#pragma once

#include <functional>
#include <string>
#include <vector>

#include "core/ClipboardEntry.hpp"

namespace cliphist {

class SimpleUi {
 public:
  using FetchEntriesFn = std::function<std::vector<ClipboardEntry>()>;
  using ToggleFavoriteFn = std::function<bool(std::int64_t, bool)>;
  using TogglePauseFn = std::function<bool()>;
  using IsPausedFn = std::function<bool()>;
  using ClearHistoryFn = std::function<bool()>;
  using DeleteEntryFn = std::function<bool(std::int64_t)>;
  using ExportHistoryFn = std::function<std::string(bool)>;
  using SaveContentFn = std::function<bool(std::int64_t, const std::string&)>;

  int Run(FetchEntriesFn fetch_entries, ToggleFavoriteFn toggle_favorite,
          TogglePauseFn toggle_pause, IsPausedFn is_paused,
          ClearHistoryFn clear_history, DeleteEntryFn delete_entry,
          ExportHistoryFn export_history, SaveContentFn save_content,
          int refresh_interval_ms = 1000);
};

}  // namespace cliphist
