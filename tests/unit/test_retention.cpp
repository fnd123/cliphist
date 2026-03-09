#include <chrono>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>

#include "db/Database.hpp"
#include "db/HistoryRepository.hpp"

namespace {
std::string TempDbPath() {
  const auto base = std::chrono::steady_clock::now().time_since_epoch().count();
  const std::filesystem::path tmp_dir = std::filesystem::temp_directory_path();
  for (int i = 0; i < 64; ++i) {
    const auto candidate =
        tmp_dir / ("cliphist_retention_" + std::to_string(base + i) + ".db");
    if (!std::filesystem::exists(candidate)) {
      return candidate.string();
    }
  }
  return (tmp_dir / "cliphist_retention_fallback.db").string();
}
}  // namespace

int main() {
  const std::string db_path = TempDbPath();

  cliphist::Database db;
  if (!db.Open(db_path) || !db.InitSchema()) {
    std::cerr << "db init failed\n";
    return 1;
  }

  cliphist::HistoryRepository repo(db);
  for (int i = 0; i < 5; ++i) {
    const std::string content = "item_" + std::to_string(i);
    const std::string hash = "hash_" + std::to_string(i);
    if (!repo.UpsertByContentHash(content, hash, 100 + i)) {
      std::cerr << "insert failed\n";
      return 1;
    }
  }

  const int removed = repo.EnforceMaxItems(3);
  if (removed < 2) {
    std::cerr << "expected at least two rows removed\n";
    return 1;
  }

  if (repo.Count() != 3) {
    std::cerr << "expected exactly 3 rows after retention\n";
    return 1;
  }

  std::remove(db_path.c_str());
  return 0;
}
