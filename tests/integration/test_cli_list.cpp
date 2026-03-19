#include <iostream>
#include <string>
#include <vector>

#include "cli/PrintFormatter.hpp"
#include "core/ClipboardEntry.hpp"
#include "db/HistoryRepository.hpp"

int main() {
  std::vector<cliphist::ClipboardEntry> entries;
  cliphist::ClipboardEntry e;
  e.id = 1;
  e.updated_at = 123;
  e.hit_count = 1;
  e.favorite = true;
  e.content = std::string("he\"llo\n") + '\b' + '\x01';
  entries.push_back(e);

  cliphist::PrintFormatter f;
  const std::string out = f.FormatList(entries);
  const std::string out_json = f.FormatListJson(entries);

  if (out.find("ID") == std::string::npos ||
      out.find("he\"llo") == std::string::npos) {
    std::cerr << "unexpected list output\n";
    return 1;
  }
  if (out_json.find("\"id\":1") == std::string::npos ||
      out_json.find("\"content\":\"he\\\"llo\\n\\b\\u0001\"") == std::string::npos ||
      out_json.find("\"favorite\":true") == std::string::npos) {
    std::cerr << "unexpected json list output\n";
    return 1;
  }

  cliphist::HistoryStats stats;
  stats.total_entries = 7;
  stats.total_hits = 9;
  stats.oldest_created_at = 100;
  stats.newest_updated_at = 200;
  const std::string stats_json = f.FormatStatsJson(stats);
  if (stats_json.find("\"total_entries\":7") == std::string::npos ||
      stats_json.find("\"total_hits\":9") == std::string::npos) {
    std::cerr << "unexpected stats json output\n";
    return 1;
  }

  const std::string count_text = f.FormatCount(3, false);
  const std::string count_json = f.FormatCount(3, true);
  if (count_text != "3\n" || count_json != "{\"count\":3}\n") {
    std::cerr << "unexpected count output\n";
    return 1;
  }

  return 0;
}
