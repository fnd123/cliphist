#include "app/Application.hpp"

#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "app/DaemonRunner.hpp"
#include "app/DesktopSession.hpp"
#include "cli/PrintFormatter.hpp"
#include "db/Database.hpp"
#include "db/HistoryRepository.hpp"
#include "ui/SimpleUi.hpp"
#include "util/Hash.hpp"

namespace cliphist {

namespace {
volatile std::sig_atomic_t g_stop = 0;

void SignalHandler(int) { g_stop = 1; }

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

std::string ExportEntries(const std::vector<ClipboardEntry>& entries, bool json,
                          const PrintFormatter& formatter,
                          const std::string& db_path) {
  const std::filesystem::path dir =
      std::filesystem::path(db_path).parent_path() / "exports";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  if (ec) {
    return {};
  }

  const std::string base = std::to_string(static_cast<long long>(std::time(nullptr)));
  const std::filesystem::path path =
      dir / (json ? ("cliphist-" + base + ".json") : ("cliphist-" + base + ".txt"));
  std::ofstream out(path, std::ios::out | std::ios::trunc);
  if (!out.is_open()) {
    return {};
  }
  out << (json ? formatter.FormatListJson(entries) : formatter.FormatList(entries));
  return path.string();
}
}  // namespace

int Application::Run(const CommandOptions& options) {
  Database db;
  if (!db.Open(options.db_path)) {
    std::cerr << "无法打开数据库: " << options.db_path << "\n";
    return 2;
  }
  if (!db.InitSchema()) {
    std::cerr << "初始化数据库结构失败: " << db.LastError() << "\n";
    return 2;
  }

  HistoryRepository repo(db);
  PrintFormatter formatter;

  switch (options.type) {
    case CommandType::kList: {
      ListOptions list_options;
      list_options.limit = options.limit;
      list_options.offset = options.offset;
      list_options.contains = options.contains;
      list_options.exact = options.exact;
      list_options.since = options.since;
      list_options.sort_field = options.sort_field;
      list_options.sort_order = options.sort_order;
      if (options.count_only) {
        std::cout << formatter.FormatCount(repo.Count(list_options), options.json);
      } else {
        const auto items = repo.List(list_options);
        if (options.json) {
          std::cout << formatter.FormatListJson(items);
        } else {
          std::cout << formatter.FormatList(items);
        }
      }
      return 0;
    }
    case CommandType::kStats: {
      const auto stats = repo.Stats();
      if (options.json) {
        std::cout << formatter.FormatStatsJson(stats);
      } else {
        std::cout << formatter.FormatStats(stats);
      }
      return 0;
    }
    case CommandType::kDaemon: {
      std::signal(SIGINT, SignalHandler);
      std::signal(SIGTERM, SignalHandler);
      return RunDaemonLoop(options, repo, []() { return g_stop != 0; },
                           []() { return false; });
    }
    case CommandType::kDesktop: {
      DesktopSession session;
      return session.Run(options);
    }
    case CommandType::kUi: {
      ListOptions list_options;
      list_options.limit = options.limit;
      list_options.offset = options.offset;
      list_options.contains = options.contains;
      list_options.exact = options.exact;
      list_options.since = options.since;
      list_options.sort_field = options.sort_field;
      list_options.sort_order = options.sort_order;
      SimpleUi ui;
      return ui.Run(
          [&repo, list_options]() { return repo.List(list_options); },
          [&repo](std::int64_t id, bool favorite) {
            return repo.SetFavorite(id, favorite);
          },
          [&options]() { return TogglePausedState(options.db_path); },
          [&options]() { return ReadPausedState(options.db_path); },
          [&repo]() { return repo.ClearAll(false) >= 0; },
          [&repo](std::int64_t id) { return repo.DeleteById(id); },
          [&repo, &formatter, &options, list_options](bool json) {
            return ExportEntries(repo.List(list_options), json, formatter, options.db_path);
          },
          [&repo](std::int64_t id, const std::string& content) {
            return repo.UpdateContent(id, content, Fnv1a64Hex(content),
                                      static_cast<std::int64_t>(std::time(nullptr)));
          });
    }
    case CommandType::kHelp:
    case CommandType::kInvalid:
      break;
  }

  return 1;
}

}  // namespace cliphist
