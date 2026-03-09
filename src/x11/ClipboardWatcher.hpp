#pragma once

#include <X11/Xlib.h>

#include <optional>
#include <string>

#include "cli/CommandParser.hpp"

namespace cliphist {

class ClipboardWatcher {
 public:
  ClipboardWatcher() = default;
  ~ClipboardWatcher();

  ClipboardWatcher(const ClipboardWatcher&) = delete;
  ClipboardWatcher& operator=(const ClipboardWatcher&) = delete;

  bool Start(SelectionMode mode);
  void Stop();
  std::optional<std::string> PollTextChange();

 private:
  bool RequestSelection(Atom selection);
  std::optional<std::string> ReadSelectionProperty(const XSelectionEvent& event);

  Display* display_ = nullptr;
  Window window_ = 0;

  Atom atom_clipboard_ = None;
  Atom atom_primary_ = None;
  Atom atom_utf8_ = None;
  Atom atom_property_ = None;

  int xfixes_event_base_ = 0;
  int xfixes_error_base_ = 0;
  bool watch_clipboard_ = true;
  bool watch_primary_ = false;
  std::string last_seen_;
};

}  // namespace cliphist
