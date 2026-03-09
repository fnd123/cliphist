#pragma once

#include <cstdint>
#include <string>

namespace cliphist {

struct ClipboardEntry {
  std::int64_t id = 0;
  std::string content;
  std::string content_hash;
  std::int64_t created_at = 0;
  std::int64_t updated_at = 0;
  int hit_count = 1;
  bool favorite = false;
};

}  // namespace cliphist
