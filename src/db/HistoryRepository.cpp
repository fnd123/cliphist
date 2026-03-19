#include "db/HistoryRepository.hpp"

#include <sqlite3.h>

#include <algorithm>

#include "db/Database.hpp"

namespace cliphist {

namespace {
std::string BuildWhereClause(const ListOptions& options) {
  std::string where;
  bool has_where = false;
  if (!options.exact.empty()) {
    where += "WHERE content = ? ";
    has_where = true;
  }
  if (!options.contains.empty()) {
    where += has_where ? "AND " : "WHERE ";
    where += "content LIKE ? ";
    has_where = true;
  }
  if (options.since > 0) {
    where += has_where ? "AND " : "WHERE ";
    where += "updated_at >= ? ";
  }
  return where;
}

void BindWhereParams(sqlite3_stmt* stmt, const ListOptions& options,
                     int* next_index) {
  if (!options.exact.empty()) {
    sqlite3_bind_text(stmt, (*next_index)++, options.exact.c_str(),
                      static_cast<int>(options.exact.size()), SQLITE_TRANSIENT);
  }
  if (!options.contains.empty()) {
    const std::string like = "%" + options.contains + "%";
    sqlite3_bind_text(stmt, (*next_index)++, like.c_str(),
                      static_cast<int>(like.size()), SQLITE_TRANSIENT);
  }
  if (options.since > 0) {
    sqlite3_bind_int64(stmt, (*next_index)++, options.since);
  }
}
}  // namespace

bool HistoryRepository::UpsertByContentHash(const std::string& text,
                                            const std::string& hash,
                                            std::int64_t ts) {
  static constexpr const char* kSql =
      "INSERT INTO clipboard_history(content, content_hash, created_at, "
      "updated_at, hit_count) VALUES(?1, ?2, ?3, ?4, 1) "
      "ON CONFLICT(content_hash) DO UPDATE SET "
      "updated_at=excluded.updated_at, "
      "hit_count=clipboard_history.hit_count+1;";

  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_.Handle(), kSql, -1, &stmt, nullptr) != SQLITE_OK) {
    return false;
  }

  sqlite3_bind_text(stmt, 1, text.c_str(), static_cast<int>(text.size()),
                    SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, hash.c_str(), static_cast<int>(hash.size()),
                    SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 3, ts);
  sqlite3_bind_int64(stmt, 4, ts);

  const int rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  return rc == SQLITE_DONE;
}

std::vector<ClipboardEntry> HistoryRepository::List(int limit, int offset) const {
  ListOptions options;
  options.limit = limit;
  options.offset = offset;
  return List(options);
}

std::vector<ClipboardEntry> HistoryRepository::List(
    const ListOptions& options) const {
  std::string sql =
      "SELECT id, content, content_hash, created_at, updated_at, hit_count, favorite "
      "FROM clipboard_history " +
      BuildWhereClause(options) + "ORDER BY ";
  sql += "favorite DESC, ";
  sql += (options.sort_field == SortField::kCreatedAt) ? "created_at " : "updated_at ";
  sql += (options.sort_order == SortOrder::kAsc) ? "ASC, id ASC " : "DESC, id DESC ";
  sql += "LIMIT ? OFFSET ?;";

  std::vector<ClipboardEntry> items;
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_.Handle(), sql.c_str(), -1, &stmt, nullptr) !=
      SQLITE_OK) {
    return items;
  }

  int next_index = 1;
  BindWhereParams(stmt, options, &next_index);
  sqlite3_bind_int(stmt, next_index++, std::max(1, options.limit));
  sqlite3_bind_int(stmt, next_index, std::max(0, options.offset));

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    ClipboardEntry e;
    e.id = sqlite3_column_int64(stmt, 0);
    const auto* txt =
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    const auto* hash =
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
    e.content = (txt != nullptr) ? txt : "";
    e.content_hash = (hash != nullptr) ? hash : "";
    e.created_at = sqlite3_column_int64(stmt, 3);
    e.updated_at = sqlite3_column_int64(stmt, 4);
    e.hit_count = sqlite3_column_int(stmt, 5);
    e.favorite = sqlite3_column_int(stmt, 6) != 0;
    items.push_back(std::move(e));
  }

  sqlite3_finalize(stmt);
  return items;
}

int HistoryRepository::Count(const ListOptions& options) const {
  const std::string sql = "SELECT COUNT(*) FROM clipboard_history " +
                          BuildWhereClause(options) + ";";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_.Handle(), sql.c_str(), -1, &stmt, nullptr) !=
      SQLITE_OK) {
    return 0;
  }

  int next_index = 1;
  BindWhereParams(stmt, options, &next_index);

  int cnt = 0;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    cnt = sqlite3_column_int(stmt, 0);
  }
  sqlite3_finalize(stmt);
  return cnt;
}

bool HistoryRepository::SetFavorite(std::int64_t id, bool favorite) {
  static constexpr const char* kSql =
      "UPDATE clipboard_history SET favorite = ?1 WHERE id = ?2;";

  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_.Handle(), kSql, -1, &stmt, nullptr) != SQLITE_OK) {
    return false;
  }
  sqlite3_bind_int(stmt, 1, favorite ? 1 : 0);
  sqlite3_bind_int64(stmt, 2, id);
  const int rc = sqlite3_step(stmt);
  const int changes = sqlite3_changes(db_.Handle());
  sqlite3_finalize(stmt);
  return rc == SQLITE_DONE && changes > 0;
}

bool HistoryRepository::DeleteById(std::int64_t id) {
  static constexpr const char* kSql =
      "DELETE FROM clipboard_history WHERE id = ?1;";

  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_.Handle(), kSql, -1, &stmt, nullptr) != SQLITE_OK) {
    return false;
  }
  sqlite3_bind_int64(stmt, 1, id);
  const int rc = sqlite3_step(stmt);
  const int changes = sqlite3_changes(db_.Handle());
  sqlite3_finalize(stmt);
  return rc == SQLITE_DONE && changes > 0;
}

bool HistoryRepository::UpdateContent(std::int64_t id, const std::string& content,
                                      const std::string& content_hash,
                                      std::int64_t updated_at) {
  static constexpr const char* kSql =
      "UPDATE clipboard_history "
      "SET content = ?1, content_hash = ?2, updated_at = ?3 "
      "WHERE id = ?4;";

  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_.Handle(), kSql, -1, &stmt, nullptr) != SQLITE_OK) {
    return false;
  }
  sqlite3_bind_text(stmt, 1, content.c_str(), static_cast<int>(content.size()),
                    SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, content_hash.c_str(),
                    static_cast<int>(content_hash.size()), SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 3, updated_at);
  sqlite3_bind_int64(stmt, 4, id);
  const int rc = sqlite3_step(stmt);
  const int changes = sqlite3_changes(db_.Handle());
  sqlite3_finalize(stmt);
  return rc == SQLITE_DONE && changes > 0;
}

int HistoryRepository::ClearAll(bool keep_favorites) {
  const char* sql =
      keep_favorites ? "DELETE FROM clipboard_history WHERE favorite = 0;"
                     : "DELETE FROM clipboard_history;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_.Handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return -1;
  }
  const int rc = sqlite3_step(stmt);
  const int changes = sqlite3_changes(db_.Handle());
  sqlite3_finalize(stmt);
  return (rc == SQLITE_DONE) ? changes : -1;
}

int HistoryRepository::EnforceMaxItems(int max_items) {
  if (max_items <= 0) {
    return 0;
  }

  static constexpr const char* kSql =
      "DELETE FROM clipboard_history WHERE id IN ("
      "SELECT id FROM clipboard_history "
      "WHERE favorite = 0 "
      "ORDER BY updated_at DESC, id DESC "
      "LIMIT -1 OFFSET ?1"
      ");";

  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_.Handle(), kSql, -1, &stmt, nullptr) != SQLITE_OK) {
    return 0;
  }

  sqlite3_bind_int(stmt, 1, max_items);

  const int rc = sqlite3_step(stmt);
  const int changes = sqlite3_changes(db_.Handle());
  sqlite3_finalize(stmt);
  return (rc == SQLITE_DONE) ? changes : 0;
}

int HistoryRepository::Count() const {
  static constexpr const char* kSql = "SELECT COUNT(*) FROM clipboard_history;";

  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_.Handle(), kSql, -1, &stmt, nullptr) != SQLITE_OK) {
    return 0;
  }

  int cnt = 0;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    cnt = sqlite3_column_int(stmt, 0);
  }
  sqlite3_finalize(stmt);
  return cnt;
}

HistoryStats HistoryRepository::Stats() const {
  static constexpr const char* kSql =
      "SELECT COUNT(*), COALESCE(SUM(hit_count), 0), "
      "COALESCE(MIN(created_at), 0), COALESCE(MAX(updated_at), 0) "
      "FROM clipboard_history;";

  HistoryStats stats;
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_.Handle(), kSql, -1, &stmt, nullptr) != SQLITE_OK) {
    return stats;
  }

  if (sqlite3_step(stmt) == SQLITE_ROW) {
    stats.total_entries = sqlite3_column_int(stmt, 0);
    stats.total_hits = sqlite3_column_int(stmt, 1);
    stats.oldest_created_at = sqlite3_column_int64(stmt, 2);
    stats.newest_updated_at = sqlite3_column_int64(stmt, 3);
  }
  sqlite3_finalize(stmt);
  return stats;
}

}  // namespace cliphist
