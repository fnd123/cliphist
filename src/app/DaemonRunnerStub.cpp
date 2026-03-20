#include "app/DaemonRunner.hpp"

#include <iostream>

#include "app/QtClipboardRuntime.hpp"
#include "db/HistoryRepository.hpp"

namespace cliphist {

int RunDaemonLoop(const CommandOptions& options, HistoryRepository& repo,
                  const std::function<bool()>& stop_requested,
                  const std::function<bool()>& paused_requested) {
#ifdef _WIN32
  return RunQtClipboardDaemon(options, repo, stop_requested, paused_requested);
#else
  (void)options;
  (void)repo;
  (void)stop_requested;
  (void)paused_requested;
  std::cerr << "当前构建未启用 X11，daemon 模式仅支持 Linux X11 环境\n";
  return 3;
#endif
}

}  // namespace cliphist
