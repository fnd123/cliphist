#pragma once

#include <functional>

#include "cli/CommandParser.hpp"

namespace cliphist {

class HistoryRepository;

int RunQtClipboardDaemon(const CommandOptions& options, HistoryRepository& repo,
                         const std::function<bool()>& stop_requested,
                         const std::function<bool()>& paused_requested);

int RunQtDesktopSession(const CommandOptions& options);

}  // namespace cliphist
