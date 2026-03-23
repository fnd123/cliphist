#include "db/Database.hpp"

#include <filesystem>

#include "util/Path.hpp"

namespace cliphist {

namespace {
constexpr const char* kSchemaSql =
    "CREATE TABLE IF NOT EXISTS clipboard_history ("
    "id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "content TEXT NOT NULL,"
    "content_hash TEXT NOT NULL UNIQUE,"
    "created_at INTEGER NOT NULL,"
    "updated_at INTEGER NOT NULL,"
    "hit_count INTEGER NOT NULL DEFAULT 1,"
    "favorite INTEGER NOT NULL DEFAULT 0"
    ");"
    "CREATE INDEX IF NOT EXISTS idx_clipboard_updated_at "
    "ON clipboard_history(updated_at DESC);";

constexpr const char* kAddFavoriteColumnSql =
    "ALTER TABLE clipboard_history ADD COLUMN favorite INTEGER NOT NULL DEFAULT 0;";
}  // namespace

Database::~Database() { Close(); }

bool Database::Open(const std::string& db_path) {
  Close();

  const std::filesystem::path p = FsPathFromUtf8(db_path);
  if (p.has_parent_path()) {
    std::error_code ec;
    std::filesystem::create_directories(p.parent_path(), ec);
    if (ec) {
      last_error_ = "failed to create db directory: " + ec.message();
      return false;
    }
  }

  int rc = SQLITE_OK;
#ifdef _WIN32
  const std::wstring wide_db_path = WideFromUtf8(db_path);
  if (wide_db_path.empty()) {
    last_error_ = "failed to convert db path to UTF-16";
    return false;
  }
  rc = sqlite3_open16(wide_db_path.c_str(), &db_);
#else
  rc = sqlite3_open(db_path.c_str(), &db_);
#endif
  if (rc != SQLITE_OK) {
    last_error_ = sqlite3_errmsg(db_);
    Close();
    return false;
  }

  return true;
}

void Database::Close() {
  if (db_ != nullptr) {
    sqlite3_close(db_);
    db_ = nullptr;
  }
}

bool Database::Execute(const std::string& sql) const {
  if (db_ == nullptr) {
    last_error_ = "database is not open";
    return false;
  }

  char* err = nullptr;
  const int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err);
  if (rc != SQLITE_OK) {
    last_error_ = (err != nullptr) ? err : "sqlite exec error";
    sqlite3_free(err);
    return false;
  }
  return true;
}

bool Database::InitSchema() const {
  if (!Execute(kSchemaSql)) {
    return false;
  }

  char* err = nullptr;
  const int rc = sqlite3_exec(db_, kAddFavoriteColumnSql, nullptr, nullptr, &err);
  if (rc != SQLITE_OK) {
    const std::string msg = (err != nullptr) ? err : "";
    sqlite3_free(err);
    if (msg.find("duplicate column name") == std::string::npos) {
      last_error_ = msg.empty() ? "failed to ensure favorite column" : msg;
      return false;
    }
  }
  return true;
}

}  // namespace cliphist
