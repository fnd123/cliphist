#include "app/DesktopSession.hpp"

#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <future>
#include <iostream>
#include <limits.h>
#include <filesystem>
#include <fstream>
#include <vector>
#include <thread>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <spawn.h>

#include "app/DaemonRunner.hpp"
#include "cli/CommandParser.hpp"
#include "db/Database.hpp"
#include "db/HistoryRepository.hpp"
#include "x11/StatusTrayIcon.hpp"

extern char** environ;

namespace cliphist {

namespace {
int OpenDatabase(const std::string& db_path, Database* db) {
  if (!db->Open(db_path)) {
    std::cerr << "无法打开数据库: " << db_path << "\n";
    return 2;
  }
  if (!db->InitSchema()) {
    std::cerr << "初始化数据库结构失败: " << db->LastError() << "\n";
    return 2;
  }
  return 0;
}

std::vector<ClipboardEntry> LoadRecentEntries(const std::string& db_path) {
  Database db;
  if (OpenDatabase(db_path, &db) != 0) {
    return {};
  }

  HistoryRepository repo(db);
  ListOptions options;
  options.limit = 15;
  options.offset = 0;
  options.sort_field = SortField::kUpdatedAt;
  options.sort_order = SortOrder::kDesc;
  return repo.List(options);
}

std::filesystem::path PauseStatePath(const std::string& db_path) {
  return std::filesystem::path(db_path).parent_path() / "pause.state";
}

bool ReadPausedState(const std::string& db_path) {
  std::ifstream in(PauseStatePath(db_path));
  std::string value;
  return (in >> value) && value == "1";
}

bool TogglePausedState(const std::string& db_path) {
  const bool paused = !ReadPausedState(db_path);
  std::ofstream out(PauseStatePath(db_path), std::ios::out | std::ios::trunc);
  out << (paused ? "1\n" : "0\n");
  return paused;
}

bool ClearHistory(const std::string& db_path) {
  Database db;
  if (OpenDatabase(db_path, &db) != 0) {
    return false;
  }
  HistoryRepository repo(db);
  return repo.ClearAll(false) >= 0;
}
}  // namespace

int DesktopSession::Run(const CommandOptions& options) {
  std::promise<int> ready_promise;
  auto ready_future = ready_promise.get_future();
  std::thread daemon_thread([this, &options, ready = std::move(ready_promise)]() mutable {
    Database db;
    const int open_rc = OpenDatabase(options.db_path, &db);
    if (open_rc != 0) {
      ready.set_value(open_rc);
      return;
    }

    HistoryRepository repo(db);
    ready.set_value(0);
    (void)RunDaemonLoop(options, repo, [this]() { return stop_requested_.load(); },
                        [&options]() { return ReadPausedState(options.db_path); });
  });

  const int daemon_ready = ready_future.get();
  if (daemon_ready != 0) {
    stop_requested_.store(true);
    daemon_thread.join();
    return daemon_ready;
  }

  StatusTrayIcon tray;
  bool quit_requested = false;
  if (!tray.Start([this, &options]() { EnsureUiProcess(options); },
                  [this, &quit_requested]() {
                    quit_requested = true;
                    stop_requested_.store(true);
                  },
                  [&options]() { return LoadRecentEntries(options.db_path); },
                  [&options]() { (void)TogglePausedState(options.db_path); },
                  [&options]() { return ReadPausedState(options.db_path); },
                  [&options]() { return ClearHistory(options.db_path); })) {
    std::cerr << "无法创建系统托盘图标，回退为直接打开主窗口\n";
    (void)LaunchUiProcess(options);
    while (!stop_requested_.load()) {
      ReapUiProcess();
      if (ui_pid_ <= 0) {
        stop_requested_.store(true);
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
    StopUiProcess();
    daemon_thread.join();
    return 0;
  }

  // Start menu launches the app entrypoint directly. Open the main window once
  // after the tray is ready so "desktop" mode is visible even on shells where
  // the tray icon is delayed, collapsed, or unsupported by default.
  EnsureUiProcess(options);

  const int tray_rc = tray.Run();
  if (!quit_requested) {
    stop_requested_.store(true);
  }
  StopUiProcess();
  tray.Stop();
  daemon_thread.join();
  return tray_rc;
}

bool DesktopSession::LaunchUiProcess(const CommandOptions& options) {
  const std::string exe = ExecutablePath();
  if (exe.empty()) {
    return false;
  }

  const std::string limit_arg = "--limit=" + std::to_string(options.limit);
  const std::string db_arg = "--db=" + options.db_path;
  std::vector<char*> argv;
  argv.push_back(const_cast<char*>(exe.c_str()));
  argv.push_back(const_cast<char*>("ui"));
  argv.push_back(const_cast<char*>(limit_arg.c_str()));
  argv.push_back(const_cast<char*>(db_arg.c_str()));
  argv.push_back(nullptr);

  pid_t pid = -1;
  if (posix_spawn(&pid, exe.c_str(), nullptr, nullptr, argv.data(), environ) != 0) {
    return false;
  }

  ui_pid_ = static_cast<int>(pid);
  return true;
}

void DesktopSession::EnsureUiProcess(const CommandOptions& options) {
  ReapUiProcess();
  if (ui_pid_ > 0) {
    FocusUiWindow();
    return;
  }
  if (!LaunchUiProcess(options)) {
    std::cerr << "启动中文界面失败\n";
  }
}

void DesktopSession::FocusUiWindow() {
  if (ui_pid_ <= 0) {
    return;
  }

  Display* display = XOpenDisplay(nullptr);
  if (display == nullptr) {
    return;
  }

  const Window root = DefaultRootWindow(display);
  Atom client_list = XInternAtom(display, "_NET_CLIENT_LIST", False);
  Atom wm_pid = XInternAtom(display, "_NET_WM_PID", False);
  Atom active_window = XInternAtom(display, "_NET_ACTIVE_WINDOW", False);

  Atom actual_type = None;
  int actual_format = 0;
  unsigned long nitems = 0;
  unsigned long bytes_after = 0;
  unsigned char* data = nullptr;
  if (XGetWindowProperty(display, root, client_list, 0, 4096, False, XA_WINDOW,
                         &actual_type, &actual_format, &nitems, &bytes_after,
                         &data) != Success ||
      data == nullptr) {
    XCloseDisplay(display);
    return;
  }

  Window target = 0;
  Window* windows = reinterpret_cast<Window*>(data);
  for (unsigned long i = 0; i < nitems; ++i) {
    unsigned char* pid_data = nullptr;
    Atom pid_type = None;
    int pid_format = 0;
    unsigned long pid_items = 0;
    unsigned long pid_bytes_after = 0;
    if (XGetWindowProperty(display, windows[i], wm_pid, 0, 1, False, XA_CARDINAL,
                           &pid_type, &pid_format, &pid_items, &pid_bytes_after,
                           &pid_data) == Success &&
        pid_data != nullptr && pid_items == 1) {
      const unsigned long pid = *reinterpret_cast<unsigned long*>(pid_data);
      XFree(pid_data);
      if (static_cast<int>(pid) == ui_pid_) {
        target = windows[i];
        break;
      }
    } else if (pid_data != nullptr) {
      XFree(pid_data);
    }
  }
  XFree(data);

  if (target != 0) {
    XMapRaised(display, target);
    XEvent event{};
    event.xclient.type = ClientMessage;
    event.xclient.window = target;
    event.xclient.message_type = active_window;
    event.xclient.format = 32;
    event.xclient.data.l[0] = 1;
    event.xclient.data.l[1] = CurrentTime;
    XSendEvent(display, root, False, SubstructureRedirectMask | SubstructureNotifyMask,
               &event);
    XFlush(display);
  }

  XCloseDisplay(display);
}

void DesktopSession::StopUiProcess() {
  ReapUiProcess();
  if (ui_pid_ <= 0) {
    return;
  }

  (void)kill(static_cast<pid_t>(ui_pid_), SIGTERM);
  (void)waitpid(static_cast<pid_t>(ui_pid_), nullptr, 0);
  ui_pid_ = -1;
}

void DesktopSession::ReapUiProcess() {
  if (ui_pid_ <= 0) {
    return;
  }

  const pid_t rc = waitpid(static_cast<pid_t>(ui_pid_), nullptr, WNOHANG);
  if (rc == static_cast<pid_t>(ui_pid_)) {
    ui_pid_ = -1;
  }
}

std::string DesktopSession::ExecutablePath() const {
  char buf[PATH_MAX] = {};
  const ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
  if (len <= 0) {
    return {};
  }
  buf[len] = '\0';
  return buf;
}

}  // namespace cliphist
