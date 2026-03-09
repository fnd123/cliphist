#include "util/Time.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace cliphist {

namespace {
std::tm LocalTime(std::int64_t ts) {
  const std::time_t raw = static_cast<std::time_t>(ts);
  std::tm tm_buf{};
#if defined(_WIN32)
  if (localtime_s(&tm_buf, &raw) != 0) {
    return {};
  }
#elif defined(_POSIX_THREAD_SAFE_FUNCTIONS)
  localtime_r(&raw, &tm_buf);
#else
  const std::tm* tm_ptr = std::localtime(&raw);
  if (tm_ptr != nullptr) {
    tm_buf = *tm_ptr;
  }
#endif
  return tm_buf;
}
}  // namespace

std::int64_t UnixTimeSeconds() {
  const auto now = std::chrono::system_clock::now();
  const auto sec = std::chrono::time_point_cast<std::chrono::seconds>(now);
  return sec.time_since_epoch().count();
}

std::string FormatLocalTime(std::int64_t ts) {
  if (ts <= 0) {
    return "--:--:--";
  }
  const std::tm tm_buf = LocalTime(ts);
  std::ostringstream oss;
  oss << std::put_time(&tm_buf, "%H:%M:%S");
  return oss.str();
}

std::string FormatLocalDate(std::int64_t ts) {
  if (ts <= 0) {
    return "----/--/--";
  }
  const std::tm tm_buf = LocalTime(ts);
  std::ostringstream oss;
  oss << std::put_time(&tm_buf, "%Y/%m/%d");
  return oss.str();
}

std::string FormatLocalDateTime(std::int64_t ts) {
  if (ts <= 0) {
    return "----/--/-- --:--:--";
  }
  const std::tm tm_buf = LocalTime(ts);
  std::ostringstream oss;
  oss << std::put_time(&tm_buf, "%Y/%m/%d %H:%M:%S");
  return oss.str();
}

std::string DescribeLocalDay(std::int64_t ts) {
  if (ts <= 0) {
    return "未知日期";
  }

  const std::tm entry = LocalTime(ts);
  const std::tm now = LocalTime(UnixTimeSeconds());
  if (entry.tm_year == now.tm_year && entry.tm_yday == now.tm_yday) {
    return "今天 " + FormatLocalDate(ts);
  }
  if (entry.tm_year == now.tm_year && entry.tm_yday == now.tm_yday - 1) {
    return "昨天 " + FormatLocalDate(ts);
  }
  return FormatLocalDate(ts);
}

}  // namespace cliphist
