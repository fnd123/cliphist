#pragma once

#include <cstdint>
#include <string>

namespace cliphist {

class ArchiveStore {
 public:
  explicit ArchiveStore(std::string base_dir) : base_dir_(std::move(base_dir)) {}

  bool SaveText(std::int64_t unix_sec, const std::string& text);
  const std::string& LastPath() const { return last_path_; }

  static std::string ArchiveDirForDb(const std::string& db_path);

 private:
  std::string base_dir_;
  std::string last_path_;
};

}  // namespace cliphist
