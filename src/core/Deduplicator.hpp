#pragma once

#include <string>

namespace cliphist {

class Deduplicator {
 public:
  std::string ComputeHash(const std::string& text) const;
};

}  // namespace cliphist
