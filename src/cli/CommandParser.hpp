#pragma once

#include <string>
#include <cstdint>

namespace cliphist {

enum class SelectionMode {
  kClipboard,
  kPrimary,
  kBoth,
};

enum class SortField {
  kUpdatedAt,
  kCreatedAt,
};

enum class SortOrder {
  kDesc,
  kAsc,
};

enum class CommandType {
  kDaemon,
  kDesktop,
  kList,
  kStats,
  kUi,
  kHelp,
  kInvalid,
};

struct CommandOptions {
  CommandType type = CommandType::kHelp;
  int limit = 20;
  int offset = 0;
  std::string contains;
  std::string exact;
  std::int64_t since = 0;
  bool json = false;
  bool count_only = false;
  SortField sort_field = SortField::kUpdatedAt;
  SortOrder sort_order = SortOrder::kDesc;
  int max_items = 0;
  SelectionMode selection_mode = SelectionMode::kClipboard;
  std::string db_path;
  std::string error;
};

class CommandParser {
 public:
  CommandOptions Parse(int argc, char** argv) const;
};

std::string DefaultDbPath();
std::string HelpText();

}  // namespace cliphist
