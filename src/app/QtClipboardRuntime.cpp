#include "app/QtClipboardRuntime.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QIcon>
#include <QMenu>
#include <QProcess>
#include <QStyle>
#include <QSystemTrayIcon>
#include <QTimer>

#ifdef _WIN32
#include <windows.h>
#endif

#include "core/Deduplicator.hpp"
#include "core/RetentionPolicy.hpp"
#include "db/Database.hpp"
#include "db/HistoryRepository.hpp"
#include "util/ArchiveStore.hpp"
#include "util/Time.hpp"

namespace cliphist {

namespace {
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

std::vector<ClipboardEntry> LoadRecentEntries(const std::string& db_path, int limit = 15) {
  Database db;
  if (OpenDatabase(db_path, &db) != 0) {
    return {};
  }

  HistoryRepository repo(db);
  ListOptions options;
  options.limit = limit;
  options.offset = 0;
  options.sort_field = SortField::kUpdatedAt;
  options.sort_order = SortOrder::kDesc;
  return repo.List(options);
}

bool ClearHistoryForDb(const std::string& db_path) {
  Database db;
  if (OpenDatabase(db_path, &db) != 0) {
    return false;
  }
  HistoryRepository repo(db);
  return repo.ClearAll(false) != -1;
}

void BackfillArchiveIfNeeded(HistoryRepository& repo, const std::string& db_path,
                             ArchiveStore* archive_store) {
  const std::string archive_dir = ArchiveStore::ArchiveDirForDb(db_path);
  const std::filesystem::path marker =
      std::filesystem::path(archive_dir) / ".backfilled_v1";
  if (std::filesystem::exists(marker)) {
    return;
  }

  ListOptions all_options;
  all_options.limit = std::max(1, repo.Count());
  all_options.offset = 0;
  all_options.sort_field = SortField::kCreatedAt;
  all_options.sort_order = SortOrder::kAsc;
  const auto all_items = repo.List(all_options);
  for (const auto& item : all_items) {
    const auto ts = (item.created_at > 0) ? item.created_at : item.updated_at;
    (void)archive_store->SaveText(ts, item.content);
  }
  std::ofstream(marker, std::ios::out | std::ios::trunc) << "ok\n";
}

std::string PreviewLabel(const std::string& text, std::size_t max_chars = 36) {
  std::string out = text;
  std::replace(out.begin(), out.end(), '\n', ' ');
  std::replace(out.begin(), out.end(), '\r', ' ');
  if (out.size() > max_chars) {
    out = out.substr(0, max_chars) + "...";
  }
  if (out.empty()) {
    out = "(空文本)";
  }
  return out;
}

QString ToQString(const std::string& value) {
  return QString::fromUtf8(value.c_str(), static_cast<int>(value.size()));
}

std::string ToStdString(const QString& value) {
  const QByteArray utf8 = value.toUtf8();
  return std::string(utf8.constData(), static_cast<std::size_t>(utf8.size()));
}

#ifdef _WIN32
struct FocusWindowPayload {
  DWORD process_id = 0;
  HWND hwnd = nullptr;
};

BOOL CALLBACK FindProcessWindow(HWND hwnd, LPARAM lparam) {
  auto* payload = reinterpret_cast<FocusWindowPayload*>(lparam);
  if (payload == nullptr || !IsWindowVisible(hwnd)) {
    return TRUE;
  }

  DWORD window_process_id = 0;
  GetWindowThreadProcessId(hwnd, &window_process_id);
  if (window_process_id == payload->process_id) {
    payload->hwnd = hwnd;
    return FALSE;
  }
  return TRUE;
}

void FocusProcessWindow(qint64 process_id) {
  if (process_id <= 0) {
    return;
  }

  FocusWindowPayload payload;
  payload.process_id = static_cast<DWORD>(process_id);
  EnumWindows(FindProcessWindow, reinterpret_cast<LPARAM>(&payload));
  if (payload.hwnd == nullptr) {
    return;
  }

  ShowWindow(payload.hwnd, SW_RESTORE);
  SetForegroundWindow(payload.hwnd);
}
#endif

class QtClipboardMonitor : public QObject {
 public:
  QtClipboardMonitor(const CommandOptions& options, HistoryRepository& repo,
                     const std::function<bool()>& paused_requested)
      : QObject(nullptr),
        options_(options),
        repo_(repo),
        paused_requested_(paused_requested),
        archive_store_(ArchiveStore::ArchiveDirForDb(options.db_path)) {}

  bool Start() {
    clipboard_ = QGuiApplication::clipboard();
    if (clipboard_ == nullptr) {
      return false;
    }

    BackfillArchiveIfNeeded(repo_, options_.db_path, &archive_store_);
    QObject::connect(clipboard_, &QClipboard::dataChanged,
                     [this]() { HandleClipboardChanged(); });
    return true;
  }

 private:
  void HandleClipboardChanged() {
    if (paused_requested_ && paused_requested_()) {
      return;
    }
    if (clipboard_ == nullptr) {
      return;
    }

    const std::string text = ToStdString(clipboard_->text(QClipboard::Clipboard));
    if (text.empty()) {
      return;
    }

    const auto now = UnixTimeSeconds();
    const auto hash = dedup_.ComputeHash(text);
    if (!repo_.UpsertByContentHash(text, hash, now)) {
      std::cerr << "写入剪贴板记录失败\n";
      return;
    }
    (void)archive_store_.SaveText(now, text);
    (void)retention_.Apply(repo_, options_.max_items);
  }

  CommandOptions options_;
  HistoryRepository& repo_;
  std::function<bool()> paused_requested_;
  QClipboard* clipboard_ = nullptr;
  Deduplicator dedup_;
  RetentionPolicy retention_;
  ArchiveStore archive_store_;
};

int RunQtAppLoop(const std::function<void(QApplication&)>& setup_app) {
  int argc = 1;
  char arg0[] = "cliphist";
  char* argv[] = {arg0, nullptr};
  std::unique_ptr<QApplication> owned_app;
  QApplication* app = qobject_cast<QApplication*>(QCoreApplication::instance());
  if (app == nullptr) {
    owned_app = std::make_unique<QApplication>(argc, argv);
    app = owned_app.get();
  }
  setup_app(*app);
  return app->exec();
}
}  // namespace

int RunQtClipboardDaemon(const CommandOptions& options, HistoryRepository& repo,
                         const std::function<bool()>& stop_requested,
                         const std::function<bool()>& paused_requested) {
  return RunQtAppLoop([&](QApplication& app) {
    app.setQuitOnLastWindowClosed(false);

    auto* monitor = new QtClipboardMonitor(options, repo, paused_requested);
    if (!monitor->Start()) {
      std::cerr << "无法初始化 Qt 剪贴板监听器\n";
      QTimer::singleShot(0, &app, [&app]() { app.exit(3); });
      return;
    }
    monitor->setParent(&app);

    auto* stop_timer = new QTimer(&app);
    stop_timer->setInterval(200);
    QObject::connect(stop_timer, &QTimer::timeout, [&app, stop_requested]() {
      if (stop_requested && stop_requested()) {
        app.quit();
      }
    });
    stop_timer->start();
  });
}

int RunQtDesktopSession(const CommandOptions& options) {
  Database db;
  const int open_rc = OpenDatabase(options.db_path, &db);
  if (open_rc != 0) {
    return open_rc;
  }

  HistoryRepository repo(db);
  return RunQtAppLoop([&](QApplication& app) {
    app.setQuitOnLastWindowClosed(false);

    auto* monitor = new QtClipboardMonitor(
        options, repo, [&options]() { return ReadPausedState(options.db_path); });
    if (!monitor->Start()) {
      std::cerr << "无法初始化 Qt 剪贴板监听器\n";
      QTimer::singleShot(0, &app, [&app]() { app.exit(3); });
      return;
    }
    monitor->setParent(&app);

    auto* ui_process = new QProcess(&app);
    ui_process->setProgram(QCoreApplication::applicationFilePath());
    ui_process->setArguments(
        {QStringLiteral("ui"),
         QStringLiteral("--limit=%1").arg(options.limit),
         QStringLiteral("--db=%1").arg(ToQString(options.db_path))});

    auto launch_or_focus_ui = [ui_process]() {
      if (ui_process->state() == QProcess::Running) {
#ifdef _WIN32
        FocusProcessWindow(ui_process->processId());
#endif
        return;
      }
      ui_process->start();
      if (!ui_process->waitForStarted(3000)) {
        std::cerr << "启动中文界面失败\n";
      }
    };

    auto* tray_menu = new QMenu();
    auto rebuild_menu = [tray_menu, ui_process, options, &app]() {
      tray_menu->clear();

      QAction* open_action = tray_menu->addAction("打开窗口");
      QObject::connect(open_action, &QAction::triggered, tray_menu,
                       [ui_process]() {
                         if (ui_process->state() == QProcess::Running) {
#ifdef _WIN32
                           FocusProcessWindow(ui_process->processId());
#endif
                           return;
                         }
                         ui_process->start();
                         if (!ui_process->waitForStarted(3000)) {
                           std::cerr << "启动中文界面失败\n";
                         }
                       });

      QAction* pause_action = tray_menu->addAction(
          ReadPausedState(options.db_path) ? "继续监听" : "暂停监听");
      QObject::connect(pause_action, &QAction::triggered, tray_menu,
                       [options]() { (void)TogglePausedState(options.db_path); });

      QAction* clear_action = tray_menu->addAction("清空历史");
      QObject::connect(clear_action, &QAction::triggered, tray_menu,
                       [options]() { (void)ClearHistoryForDb(options.db_path); });

      tray_menu->addSeparator();

      const auto entries = LoadRecentEntries(options.db_path);
      if (entries.empty()) {
        QAction* empty_action = tray_menu->addAction("暂无剪贴板历史");
        empty_action->setEnabled(false);
      } else {
        const std::size_t max_items = std::min<std::size_t>(15, entries.size());
        for (std::size_t i = 0; i < max_items; ++i) {
          const std::string label =
              std::to_string(i + 1) + ". " +
              (entries[i].favorite ? "[*] " : "") +
              "[" + FormatLocalDateTime(entries[i].updated_at) + "] " +
              "[" + std::to_string(entries[i].hit_count) + "次] " +
              PreviewLabel(entries[i].content);
          QAction* entry_action = tray_menu->addAction(ToQString(label));
          QObject::connect(entry_action, &QAction::triggered, tray_menu,
                           [content = entries[i].content]() {
                             QClipboard* clipboard = QGuiApplication::clipboard();
                             if (clipboard != nullptr) {
                               clipboard->setText(ToQString(content), QClipboard::Clipboard);
                             }
                           });
        }
      }

      tray_menu->addSeparator();
      QAction* quit_action = tray_menu->addAction("退出");
      QObject::connect(quit_action, &QAction::triggered, tray_menu,
                       [&app]() { app.quit(); });
    };

    auto tray_icon = QIcon::fromTheme("cliphist");
    if (tray_icon.isNull()) {
      tray_icon = app.style()->standardIcon(QStyle::SP_FileDialogDetailedView);
    }
    QSystemTrayIcon* tray = nullptr;
    if (QSystemTrayIcon::isSystemTrayAvailable()) {
      tray = new QSystemTrayIcon(tray_icon, &app);
      tray->setToolTip("剪贴板历史");
      QObject::connect(tray_menu, &QMenu::aboutToShow, tray_menu, rebuild_menu);
      QObject::connect(tray, &QSystemTrayIcon::activated, tray,
                       [launch_or_focus_ui](QSystemTrayIcon::ActivationReason reason) {
                         if (reason == QSystemTrayIcon::Trigger ||
                             reason == QSystemTrayIcon::DoubleClick) {
                           launch_or_focus_ui();
                         }
                       });
      tray->setContextMenu(tray_menu);
      tray->show();
    }

    QObject::connect(&app, &QCoreApplication::aboutToQuit, [&]() {
      if (tray != nullptr) {
        tray->hide();
      }
      if (ui_process->state() != QProcess::NotRunning) {
        ui_process->terminate();
        if (!ui_process->waitForFinished(3000)) {
          ui_process->kill();
          (void)ui_process->waitForFinished(1000);
        }
      }
    });

    launch_or_focus_ui();
  });
}

}  // namespace cliphist
