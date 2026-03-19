#include "cli/PrintFormatter.hpp"

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <sstream>

namespace cliphist {

namespace {
std::string ClipPreview(const std::string& s, std::size_t n = 80) {
  std::string out = s;
  std::replace(out.begin(), out.end(), '\n', ' ');
  std::replace(out.begin(), out.end(), '\r', ' ');
  if (out.size() <= n) {
    return out;
  }
  return out.substr(0, n - 3) + "...";
}

std::string JsonEscape(const std::string& s) {
  std::string out;
  out.reserve(s.size() + 8);
  for (unsigned char c : s) {
    switch (c) {
      case '\"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      case '\b':
        out += "\\b";
        break;
      case '\f':
        out += "\\f";
        break;
      default:
        if (c < 0x20U) {
          std::ostringstream escape;
          escape << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                 << static_cast<int>(c);
          out += escape.str();
        } else {
          out.push_back(static_cast<char>(c));
        }
        break;
    }
  }
  return out;
}
}  // namespace

std::string PrintFormatter::FormatList(
    const std::vector<ClipboardEntry>& entries) const {
  std::ostringstream oss;
  oss << "ID\tUpdated\tHits\tFav\tContent\n";
  for (const auto& e : entries) {
    oss << e.id << '\t' << e.updated_at << '\t' << e.hit_count << '\t'
        << (e.favorite ? "*" : "-") << '\t' << ClipPreview(e.content) << '\n';
  }
  return oss.str();
}

std::string PrintFormatter::FormatListJson(
    const std::vector<ClipboardEntry>& entries) const {
  std::ostringstream oss;
  oss << "[";
  for (std::size_t i = 0; i < entries.size(); ++i) {
    const auto& e = entries[i];
    if (i > 0) {
      oss << ",";
    }
    oss << "{\"id\":" << e.id << ",\"updated_at\":" << e.updated_at
        << ",\"created_at\":" << e.created_at << ",\"hit_count\":" << e.hit_count
        << ",\"favorite\":" << (e.favorite ? "true" : "false")
        << ",\"content\":\"" << JsonEscape(e.content) << "\"}";
  }
  oss << "]\n";
  return oss.str();
}

std::string PrintFormatter::FormatStats(const HistoryStats& stats) const {
  std::ostringstream oss;
  oss << "Total entries: " << stats.total_entries << '\n';
  oss << "Total hits: " << stats.total_hits << '\n';
  oss << "Oldest created_at: " << stats.oldest_created_at << '\n';
  oss << "Newest updated_at: " << stats.newest_updated_at << '\n';
  return oss.str();
}

std::string PrintFormatter::FormatStatsJson(const HistoryStats& stats) const {
  std::ostringstream oss;
  oss << "{\"total_entries\":" << stats.total_entries
      << ",\"total_hits\":" << stats.total_hits
      << ",\"oldest_created_at\":" << stats.oldest_created_at
      << ",\"newest_updated_at\":" << stats.newest_updated_at << "}\n";
  return oss.str();
}

std::string PrintFormatter::FormatCount(int count, bool json) const {
  std::ostringstream oss;
  if (json) {
    oss << "{\"count\":" << count << "}\n";
  } else {
    oss << count << '\n';
  }
  return oss.str();
}

}  // namespace cliphist
