#include "cli/CommandParser.hpp"

#include <cerrno>
#include <climits>
#include <cstdint>
#include <cstdlib>

#include <filesystem>
#include <string>

namespace cliphist {

namespace {
#ifndef CLIPHIST_HAS_X11
#define CLIPHIST_HAS_X11 1
#endif

bool ParseInt(const std::string& s, int* out) {
  errno = 0;
  char* end = nullptr;
  const long long v = std::strtoll(s.c_str(), &end, 10);
  if (end == s.c_str() || *end != '\0' || errno == ERANGE ||
      v < INT_MIN || v > INT_MAX) {
    return false;
  }
  *out = static_cast<int>(v);
  return true;
}

bool ParseInt64(const std::string& s, std::int64_t* out) {
  errno = 0;
  char* end = nullptr;
  const long long v = std::strtoll(s.c_str(), &end, 10);
  if (end == s.c_str() || *end != '\0' || errno == ERANGE) {
    return false;
  }
  *out = static_cast<std::int64_t>(v);
  return true;
}

bool StartsWith(const std::string& s, const std::string& prefix) {
  return s.rfind(prefix, 0) == 0;
}

bool MissingValue(int index, int argc) { return index + 1 >= argc; }
}  // namespace

std::string DefaultDbPath() {
#ifdef _WIN32
  const char* local_app_data = std::getenv("LOCALAPPDATA");
  if (local_app_data != nullptr && *local_app_data != '\0') {
    return std::string(local_app_data) + "/cliphist/cliphist.db";
  }

  const char* app_data = std::getenv("APPDATA");
  if (app_data != nullptr && *app_data != '\0') {
    return std::string(app_data) + "/cliphist/cliphist.db";
  }
#else
  const char* xdg_data = std::getenv("XDG_DATA_HOME");
  if (xdg_data != nullptr && *xdg_data != '\0') {
    return std::string(xdg_data) + "/cliphist/cliphist.db";
  }
#endif

  const char* home = std::getenv("HOME");
  if (home != nullptr && *home != '\0') {
    return std::string(home) + "/.local/share/cliphist/cliphist.db";
  }

  return (std::filesystem::temp_directory_path() / "cliphist.db").string();
}

std::string HelpText() {
#if CLIPHIST_HAS_X11
  return "用法:\n"
         "  cliphist daemon [--max-items N(0=不限)] [--selection clipboard|primary|both] [--db PATH]\n"
         "  cliphist desktop [--max-items N(0=不限)] [--selection clipboard|primary|both] [--db PATH]\n"
         "  cliphist list [--limit N] [--offset N] [--contains TEXT] [--exact TEXT] [--since UNIX_TS] [--sort updated_at|created_at] [--order asc|desc] [--count-only] [--json] [--db PATH]\n"
         "  cliphist ui [--limit N] [--db PATH]\n"
         "  cliphist stats [--json] [--db PATH]\n";
#elif defined(_WIN32)
  return "用法:\n"
         "  cliphist daemon [--max-items N(0=不限)] [--db PATH]\n"
         "  cliphist desktop [--max-items N(0=不限)] [--db PATH]\n"
         "  cliphist list [--limit N] [--offset N] [--contains TEXT] [--exact TEXT] [--since UNIX_TS] [--sort updated_at|created_at] [--order asc|desc] [--count-only] [--json] [--db PATH]\n"
         "  cliphist ui [--limit N] [--db PATH]\n"
         "  cliphist stats [--json] [--db PATH]\n"
         "说明:\n"
         "  Windows 版本基于 Qt5 剪贴板与系统托盘实现，无参数启动默认进入 desktop 模式。\n";
#else
  return "用法:\n"
         "  cliphist list [--limit N] [--offset N] [--contains TEXT] [--exact TEXT] [--since UNIX_TS] [--sort updated_at|created_at] [--order asc|desc] [--count-only] [--json] [--db PATH]\n"
         "  cliphist ui [--limit N] [--db PATH]\n"
         "  cliphist stats [--json] [--db PATH]\n"
         "说明:\n"
         "  daemon/desktop 需要 Linux X11 支持，当前构建未启用。\n";
#endif
}

CommandOptions CommandParser::Parse(int argc, char** argv) const {
  CommandOptions out;
  out.db_path = DefaultDbPath();

  if (argc < 2) {
#ifdef _WIN32
    out.type = CommandType::kDesktop;
    out.limit = 10000;
#else
    out.type = CommandType::kHelp;
#endif
    return out;
  }

  const std::string sub = argv[1];
  if (sub == "daemon") {
    out.type = CommandType::kDaemon;
  } else if (sub == "desktop") {
    out.type = CommandType::kDesktop;
    out.limit = 10000;
  } else if (sub == "list") {
    out.type = CommandType::kList;
  } else if (sub == "stats") {
    out.type = CommandType::kStats;
  } else if (sub == "ui") {
    out.type = CommandType::kUi;
    out.limit = 10000;
  } else if (sub == "help" || sub == "--help" || sub == "-h") {
    out.type = CommandType::kHelp;
    return out;
  } else {
    out.type = CommandType::kInvalid;
    out.error = "未知命令: " + sub;
    return out;
  }

  for (int i = 2; i < argc; ++i) {
    const std::string arg = argv[i];
    int parsed = 0;
    std::int64_t parsed64 = 0;
    if (arg == "--limit") {
      if (MissingValue(i, argc)) {
        out.type = CommandType::kInvalid;
        out.error = "--limit 缺少整数值";
        return out;
      }
      if (!ParseInt(argv[++i], &parsed)) {
        out.type = CommandType::kInvalid;
        out.error = "--limit 需要整数";
        return out;
      }
      out.limit = parsed;
    } else if (StartsWith(arg, "--limit=")) {
      if (!ParseInt(arg.substr(8), &parsed)) {
        out.type = CommandType::kInvalid;
        out.error = "--limit 需要整数";
        return out;
      }
      out.limit = parsed;
    } else if (arg == "--offset") {
      if (MissingValue(i, argc)) {
        out.type = CommandType::kInvalid;
        out.error = "--offset 缺少整数值";
        return out;
      }
      if (!ParseInt(argv[++i], &parsed)) {
        out.type = CommandType::kInvalid;
        out.error = "--offset 需要整数";
        return out;
      }
      out.offset = parsed;
    } else if (StartsWith(arg, "--offset=")) {
      if (!ParseInt(arg.substr(9), &parsed)) {
        out.type = CommandType::kInvalid;
        out.error = "--offset 需要整数";
        return out;
      }
      out.offset = parsed;
    } else if (arg == "--max-items") {
      if (MissingValue(i, argc)) {
        out.type = CommandType::kInvalid;
        out.error = "--max-items 缺少整数值";
        return out;
      }
      if (!ParseInt(argv[++i], &parsed)) {
        out.type = CommandType::kInvalid;
        out.error = "--max-items 需要整数";
        return out;
      }
      out.max_items = parsed;
    } else if (StartsWith(arg, "--max-items=")) {
      if (!ParseInt(arg.substr(12), &parsed)) {
        out.type = CommandType::kInvalid;
        out.error = "--max-items 需要整数";
        return out;
      }
      out.max_items = parsed;
    } else if (arg == "--selection") {
      if (MissingValue(i, argc)) {
        out.type = CommandType::kInvalid;
        out.error = "--selection 缺少取值";
        return out;
      }
      const std::string mode = argv[++i];
      if (mode == "clipboard") {
        out.selection_mode = SelectionMode::kClipboard;
      } else if (mode == "primary") {
        out.selection_mode = SelectionMode::kPrimary;
      } else if (mode == "both") {
        out.selection_mode = SelectionMode::kBoth;
      } else {
        out.type = CommandType::kInvalid;
        out.error = "--selection 仅支持 clipboard、primary 或 both";
        return out;
      }
    } else if (StartsWith(arg, "--selection=")) {
      const std::string mode = arg.substr(12);
      if (mode == "clipboard") {
        out.selection_mode = SelectionMode::kClipboard;
      } else if (mode == "primary") {
        out.selection_mode = SelectionMode::kPrimary;
      } else if (mode == "both") {
        out.selection_mode = SelectionMode::kBoth;
      } else {
        out.type = CommandType::kInvalid;
        out.error = "--selection 仅支持 clipboard、primary 或 both";
        return out;
      }
    } else if (arg == "--db") {
      if (MissingValue(i, argc)) {
        out.type = CommandType::kInvalid;
        out.error = "--db 缺少路径";
        return out;
      }
      out.db_path = argv[++i];
    } else if (StartsWith(arg, "--db=")) {
      out.db_path = arg.substr(5);
    } else if (arg == "--contains") {
      if (MissingValue(i, argc)) {
        out.type = CommandType::kInvalid;
        out.error = "--contains 缺少文本";
        return out;
      }
      out.contains = argv[++i];
    } else if (StartsWith(arg, "--contains=")) {
      out.contains = arg.substr(11);
    } else if (arg == "--exact") {
      if (MissingValue(i, argc)) {
        out.type = CommandType::kInvalid;
        out.error = "--exact 缺少文本";
        return out;
      }
      out.exact = argv[++i];
    } else if (StartsWith(arg, "--exact=")) {
      out.exact = arg.substr(8);
    } else if (arg == "--since") {
      if (MissingValue(i, argc)) {
        out.type = CommandType::kInvalid;
        out.error = "--since 缺少整数时间戳";
        return out;
      }
      if (!ParseInt64(argv[++i], &parsed64)) {
        out.type = CommandType::kInvalid;
        out.error = "--since 需要整数时间戳";
        return out;
      }
      out.since = parsed64;
    } else if (StartsWith(arg, "--since=")) {
      if (!ParseInt64(arg.substr(8), &parsed64)) {
        out.type = CommandType::kInvalid;
        out.error = "--since 需要整数时间戳";
        return out;
      }
      out.since = parsed64;
    } else if (arg == "--json") {
      out.json = true;
    } else if (arg == "--count-only") {
      out.count_only = true;
    } else if (arg == "--sort") {
      if (MissingValue(i, argc)) {
        out.type = CommandType::kInvalid;
        out.error = "--sort 缺少字段名";
        return out;
      }
      const std::string v = argv[++i];
      if (v == "updated_at") {
        out.sort_field = SortField::kUpdatedAt;
      } else if (v == "created_at") {
        out.sort_field = SortField::kCreatedAt;
      } else {
        out.type = CommandType::kInvalid;
        out.error = "--sort 仅支持 updated_at 或 created_at";
        return out;
      }
    } else if (StartsWith(arg, "--sort=")) {
      const std::string v = arg.substr(7);
      if (v == "updated_at") {
        out.sort_field = SortField::kUpdatedAt;
      } else if (v == "created_at") {
        out.sort_field = SortField::kCreatedAt;
      } else {
        out.type = CommandType::kInvalid;
        out.error = "--sort 仅支持 updated_at 或 created_at";
        return out;
      }
    } else if (arg == "--order") {
      if (MissingValue(i, argc)) {
        out.type = CommandType::kInvalid;
        out.error = "--order 缺少排序方向";
        return out;
      }
      const std::string v = argv[++i];
      if (v == "desc") {
        out.sort_order = SortOrder::kDesc;
      } else if (v == "asc") {
        out.sort_order = SortOrder::kAsc;
      } else {
        out.type = CommandType::kInvalid;
        out.error = "--order 仅支持 desc 或 asc";
        return out;
      }
    } else if (StartsWith(arg, "--order=")) {
      const std::string v = arg.substr(8);
      if (v == "desc") {
        out.sort_order = SortOrder::kDesc;
      } else if (v == "asc") {
        out.sort_order = SortOrder::kAsc;
      } else {
        out.type = CommandType::kInvalid;
        out.error = "--order 仅支持 desc 或 asc";
        return out;
      }
    } else {
      out.type = CommandType::kInvalid;
      out.error = "未知选项: " + arg;
      return out;
    }
  }

  if (out.limit <= 0) {
    out.limit =
        (out.type == CommandType::kUi || out.type == CommandType::kDesktop) ? 10000 : 20;
  }
  if ((out.type == CommandType::kUi || out.type == CommandType::kDesktop) &&
      out.limit > 10000) {
    out.limit = 10000;
  }
  if (out.offset < 0) {
    out.offset = 0;
  }
  if (out.max_items < 0) {
    out.max_items = 0;
  }

  return out;
}

}  // namespace cliphist
