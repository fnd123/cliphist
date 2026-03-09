#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "cli/CommandParser.hpp"
#include "core/ClipboardEntry.hpp"

namespace cliphist {

class Database;

struct HistoryStats {
  int total_entries = 0;
  int total_hits = 0;
  std::int64_t oldest_created_at = 0;
  std::int64_t newest_updated_at = 0;
};

struct ListOptions {
  int limit = 20;
  int offset = 0;
  std::string contains;
  std::string exact;
  std::int64_t since = 0;
  SortField sort_field = SortField::kUpdatedAt;
  SortOrder sort_order = SortOrder::kDesc;
};

class HistoryRepository {
 public:
  explicit HistoryRepository(Database& db) : db_(db) {}

  bool UpsertByContentHash(const std::string& text, const std::string& hash,
                           std::int64_t ts);
  std::vector<ClipboardEntry> List(int limit, int offset) const;
  std::vector<ClipboardEntry> List(const ListOptions& options) const;
  int Count(const ListOptions& options) const;
  bool SetFavorite(std::int64_t id, bool favorite);
  bool DeleteById(std::int64_t id);
  bool UpdateContent(std::int64_t id, const std::string& content,
                     const std::string& content_hash, std::int64_t updated_at);
  int ClearAll(bool keep_favorites = false);
  int EnforceMaxItems(int max_items);
  int Count() const;
  HistoryStats Stats() const;

 private:
  Database& db_;
};

}  // namespace cliphist
