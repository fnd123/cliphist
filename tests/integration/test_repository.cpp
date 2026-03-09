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
        tmp_dir / ("cliphist_repo_" + std::to_string(base + i) + ".db");
    if (!std::filesystem::exists(candidate)) {
      return candidate.string();
    }
  }
  return (tmp_dir / "cliphist_repo_fallback.db").string();
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

  if (!repo.UpsertByContentHash("abc", "h_abc", 1000)) {
    std::cerr << "first upsert failed\n";
    return 1;
  }

  if (!repo.UpsertByContentHash("hello world", "h_hello_world", 1500)) {
    std::cerr << "second distinct insert failed\n";
    return 1;
  }

  if (!repo.UpsertByContentHash("abc", "h_abc", 2000)) {
    std::cerr << "third upsert failed\n";
    return 1;
  }

  if (repo.Count() != 2) {
    std::cerr << "dedup expected two rows\n";
    return 1;
  }

  const auto items = repo.List(10, 0);
  if (items.size() != 2) {
    std::cerr << "list expected two rows\n";
    return 1;
  }

  if (items[0].content != "abc") {
    std::cerr << "most recent item should be abc\n";
    return 1;
  }

  if (items[0].hit_count < 2) {
    std::cerr << "hit_count expected >=2\n";
    return 1;
  }

  if (!repo.SetFavorite(items[0].id, true)) {
    std::cerr << "set favorite failed\n";
    return 1;
  }

  const auto fav_items = repo.List(10, 0);
  if (fav_items.empty() || !fav_items[0].favorite) {
    std::cerr << "favorite ordering failed\n";
    return 1;
  }

  cliphist::ListOptions contains_filter;
  contains_filter.limit = 10;
  contains_filter.offset = 0;
  contains_filter.contains = "hello";
  const auto contains_items = repo.List(contains_filter);
  if (contains_items.size() != 1 || contains_items[0].content != "hello world") {
    std::cerr << "contains filter failed\n";
    return 1;
  }

  cliphist::ListOptions since_filter;
  since_filter.limit = 10;
  since_filter.offset = 0;
  since_filter.since = 1800;
  const auto since_items = repo.List(since_filter);
  if (since_items.size() != 1 || since_items[0].content != "abc") {
    std::cerr << "since filter failed\n";
    return 1;
  }

  cliphist::ListOptions combined_filter;
  combined_filter.limit = 10;
  combined_filter.offset = 0;
  combined_filter.contains = "hello";
  combined_filter.since = 1800;
  const auto combined_items = repo.List(combined_filter);
  if (!combined_items.empty()) {
    std::cerr << "combined filter should be empty\n";
    return 1;
  }

  cliphist::ListOptions exact_filter;
  exact_filter.limit = 10;
  exact_filter.offset = 0;
  exact_filter.exact = "hello world";
  const auto exact_items = repo.List(exact_filter);
  if (exact_items.size() != 1 || exact_items[0].content != "hello world") {
    std::cerr << "exact filter failed\n";
    return 1;
  }

  cliphist::ListOptions sort_filter;
  sort_filter.limit = 10;
  sort_filter.offset = 0;
  sort_filter.sort_field = cliphist::SortField::kCreatedAt;
  sort_filter.sort_order = cliphist::SortOrder::kAsc;
  const auto sorted_items = repo.List(sort_filter);
  if (sorted_items.size() != 2 || sorted_items[0].content != "abc") {
    std::cerr << "sort filter failed\n";
    return 1;
  }

  cliphist::ListOptions count_filter;
  count_filter.contains = "hello";
  if (repo.Count(count_filter) != 1) {
    std::cerr << "count filter failed\n";
    return 1;
  }

  if (repo.ClearAll(true) != 1 || repo.Count() != 1) {
    std::cerr << "clear non-favorites failed\n";
    return 1;
  }

  std::remove(db_path.c_str());
  return 0;
}
