#include "app/DaemonRunner.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "core/Deduplicator.hpp"
#include "core/RetentionPolicy.hpp"
#include "db/HistoryRepository.hpp"
#include "util/ArchiveStore.hpp"
#include "util/Path.hpp"
#include "util/Time.hpp"
#include "x11/ClipboardWatcher.hpp"

namespace cliphist {

int RunDaemonLoop(const CommandOptions& options, HistoryRepository& repo,
                  const std::function<bool()>& stop_requested,
                  const std::function<bool()>& paused_requested) {
  ClipboardWatcher watcher;
  if (!watcher.Start(options.selection_mode)) {
    std::cerr << "无法初始化 X11 剪贴板监听器\n";
    return 3;
  }

  Deduplicator dedup;
  RetentionPolicy retention;
  const std::string archive_dir = ArchiveStore::ArchiveDirForDb(options.db_path);
  ArchiveStore archive_store(archive_dir);

  const std::filesystem::path marker =
      FsPathFromUtf8(archive_dir) / ".backfilled_v1";
  if (!std::filesystem::exists(marker)) {
    ListOptions all_options;
    all_options.limit = std::max(1, repo.Count());
    all_options.offset = 0;
    all_options.sort_field = SortField::kCreatedAt;
    all_options.sort_order = SortOrder::kAsc;
    const auto all_items = repo.List(all_options);
    for (const auto& item : all_items) {
      const auto ts = (item.created_at > 0) ? item.created_at : item.updated_at;
      (void)archive_store.SaveText(ts, item.content);
    }
    std::ofstream(marker, std::ios::out | std::ios::trunc) << "ok\n";
  }

  while (!stop_requested()) {
    const auto text_opt = watcher.PollTextChange();
    if (!text_opt.has_value()) {
      continue;
    }
    if (paused_requested()) {
      continue;
    }

    const std::string& text = *text_opt;
    if (text.empty()) {
      continue;
    }

    const auto now = UnixTimeSeconds();
    const auto hash = dedup.ComputeHash(text);
    if (!repo.UpsertByContentHash(text, hash, now)) {
      std::cerr << "写入剪贴板记录失败\n";
      continue;
    }
    (void)archive_store.SaveText(now, text);

    (void)retention.Apply(repo, options.max_items);
  }

  watcher.Stop();
  return 0;
}

}  // namespace cliphist
