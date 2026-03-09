#include "util/Hash.hpp"

#include <cstdint>
#include <iomanip>
#include <sstream>

namespace cliphist {

std::string Fnv1a64Hex(const std::string& text) {
  constexpr std::uint64_t kOffsetBasis = 14695981039346656037ULL;
  constexpr std::uint64_t kPrime = 1099511628211ULL;

  std::uint64_t hash = kOffsetBasis;
  for (unsigned char c : text) {
    hash ^= static_cast<std::uint64_t>(c);
    hash *= kPrime;
  }

  std::ostringstream oss;
  oss << std::hex << std::setfill('0') << std::setw(16) << hash;
  return oss.str();
}

}  // namespace cliphist
