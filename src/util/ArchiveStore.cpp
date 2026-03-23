#include "util/ArchiveStore.hpp"

#include <ctime>
#include <fstream>

#include <filesystem>
#include <iomanip>
#include <sstream>

#include "util/Path.hpp"

namespace cliphist {

namespace {
std::tm LocalTime(std::int64_t unix_sec) {
  const std::time_t t = static_cast<std::time_t>(unix_sec);
  std::tm tm_buf{};
#if defined(_WIN32)
  if (localtime_s(&tm_buf, &t) != 0) {
    return {};
  }
#elif defined(_POSIX_THREAD_SAFE_FUNCTIONS)
  localtime_r(&t, &tm_buf);
#else
  const std::tm* tm_ptr = std::localtime(&t);
  if (tm_ptr != nullptr) {
    tm_buf = *tm_ptr;
  }
#endif
  return tm_buf;
}

std::string BuildDateDir(const std::tm& tm) {
  std::ostringstream oss;
  oss << std::setfill('0') << std::setw(4) << (tm.tm_year + 1900) << "-"
      << std::setw(2) << (tm.tm_mon + 1) << "-" << std::setw(2) << tm.tm_mday;
  return oss.str();
}

std::string BuildFileStem(const std::tm& tm) {
  std::ostringstream oss;
  oss << std::setfill('0') << std::setw(2) << tm.tm_hour << std::setw(2)
      << tm.tm_min << std::setw(2) << tm.tm_sec;
  return oss.str();
}
}  // namespace

bool ArchiveStore::SaveText(std::int64_t unix_sec, const std::string& text) {
  std::error_code ec;
  const std::tm tm = LocalTime(unix_sec);
  const std::filesystem::path day_dir =
      FsPathFromUtf8(base_dir_) / BuildDateDir(tm);

  std::filesystem::create_directories(day_dir, ec);
  if (ec) {
    return false;
  }

  const std::string stem = BuildFileStem(tm);
  std::filesystem::path file = day_dir / (stem + ".txt");
  int suffix = 1;
  while (std::filesystem::exists(file, ec)) {
    if (ec) {
      return false;
    }
    file = day_dir / (stem + "_" + std::to_string(suffix++) + ".txt");
  }

  std::ofstream ofs(file, std::ios::out | std::ios::binary | std::ios::trunc);
  if (!ofs) {
    return false;
  }
  ofs << text;
  ofs.close();
  if (!ofs) {
    return false;
  }

  last_path_ = Utf8FromFsPath(file);
  return true;
}

std::string ArchiveStore::ArchiveDirForDb(const std::string& db_path) {
  const std::filesystem::path p = FsPathFromUtf8(db_path);
  if (p.has_parent_path()) {
    return Utf8FromFsPath(p.parent_path() / "archive");
  }
  return TempDirectoryUtf8() + "/cliphist_archive";
}

}  // namespace cliphist
