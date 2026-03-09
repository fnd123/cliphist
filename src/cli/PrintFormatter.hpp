#pragma once

#include <string>
#include <vector>

#include "core/ClipboardEntry.hpp"
#include "db/HistoryRepository.hpp"

namespace cliphist {

class PrintFormatter {
 public:
  std::string FormatList(const std::vector<ClipboardEntry>& entries) const;
  std::string FormatListJson(const std::vector<ClipboardEntry>& entries) const;
  std::string FormatStats(const HistoryStats& stats) const;
  std::string FormatStatsJson(const HistoryStats& stats) const;
  std::string FormatCount(int count, bool json) const;
};

}  // namespace cliphist
