#include "core/Deduplicator.hpp"

#include "util/Hash.hpp"

namespace cliphist {

std::string Deduplicator::ComputeHash(const std::string& text) const {
  return Fnv1a64Hex(text);
}

}  // namespace cliphist
