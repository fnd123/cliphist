#include <iostream>

#include "core/Deduplicator.hpp"

int main() {
  cliphist::Deduplicator dedup;

  const auto a1 = dedup.ComputeHash("hello");
  const auto a2 = dedup.ComputeHash("hello");
  const auto b = dedup.ComputeHash("hello world");

  if (a1 != a2) {
    std::cerr << "same content should produce same hash\n";
    return 1;
  }
  if (a1 == b) {
    std::cerr << "different content should produce different hash\n";
    return 1;
  }

  return 0;
}
