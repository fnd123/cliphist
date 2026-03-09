#pragma once

#include <sqlite3.h>

#include <string>

namespace cliphist {

class Database {
 public:
  Database() = default;
  ~Database();

  Database(const Database&) = delete;
  Database& operator=(const Database&) = delete;

  bool Open(const std::string& db_path);
  void Close();
  bool Execute(const std::string& sql) const;
  bool InitSchema() const;

  sqlite3* Handle() const { return db_; }
  const std::string& LastError() const { return last_error_; }

 private:
  sqlite3* db_ = nullptr;
  mutable std::string last_error_;
};

}  // namespace cliphist
