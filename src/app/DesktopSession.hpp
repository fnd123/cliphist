#pragma once

#include <atomic>
#include <string>

#include "cli/CommandParser.hpp"

namespace cliphist {

class DesktopSession {
 public:
  int Run(const CommandOptions& options);

 private:
  bool LaunchUiProcess(const CommandOptions& options);
  void EnsureUiProcess(const CommandOptions& options);
  void StopUiProcess();
  void ReapUiProcess();
  void FocusUiWindow();
  std::string ExecutablePath() const;

  std::atomic_bool stop_requested_{false};
  std::atomic_bool paused_{false};
  int ui_pid_ = -1;
};

}  // namespace cliphist
