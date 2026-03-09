#include "x11/ClipboardWatcher.hpp"

#include <X11/Xatom.h>
#include <X11/extensions/Xfixes.h>

#include <sys/select.h>
#include <unistd.h>

#include <optional>
#include <string>

namespace cliphist {

ClipboardWatcher::~ClipboardWatcher() { Stop(); }

bool ClipboardWatcher::Start(SelectionMode mode) {
  Stop();

  display_ = XOpenDisplay(nullptr);
  if (display_ == nullptr) {
    return false;
  }

  window_ = XCreateSimpleWindow(display_, DefaultRootWindow(display_), 0, 0, 1,
                                1, 0, 0, 0);

  atom_clipboard_ = XInternAtom(display_, "CLIPBOARD", False);
  atom_primary_ = XInternAtom(display_, "PRIMARY", False);
  atom_utf8_ = XInternAtom(display_, "UTF8_STRING", False);
  atom_property_ = XInternAtom(display_, "CLIPHIST_SELECTION", False);

  if (!XFixesQueryExtension(display_, &xfixes_event_base_, &xfixes_error_base_)) {
    Stop();
    return false;
  }

  watch_clipboard_ =
      (mode == SelectionMode::kClipboard || mode == SelectionMode::kBoth);
  watch_primary_ =
      (mode == SelectionMode::kPrimary || mode == SelectionMode::kBoth);

  if (watch_clipboard_) {
    XFixesSelectSelectionInput(display_, window_, atom_clipboard_,
                               XFixesSetSelectionOwnerNotifyMask);
  }
  if (watch_primary_) {
    XFixesSelectSelectionInput(display_, window_, atom_primary_,
                               XFixesSetSelectionOwnerNotifyMask);
  }

  XFlush(display_);
  return true;
}

void ClipboardWatcher::Stop() {
  if (display_ != nullptr && window_ != 0) {
    XDestroyWindow(display_, window_);
    window_ = 0;
  }

  if (display_ != nullptr) {
    XCloseDisplay(display_);
    display_ = nullptr;
  }

  last_seen_.clear();
}

bool ClipboardWatcher::RequestSelection(Atom selection) {
  if (display_ == nullptr) {
    return false;
  }

  XConvertSelection(display_, selection, atom_utf8_, atom_property_, window_,
                    CurrentTime);
  XFlush(display_);
  return true;
}

std::optional<std::string> ClipboardWatcher::ReadSelectionProperty(
    const XSelectionEvent& event) {
  if (display_ == nullptr || event.property == None) {
    return std::nullopt;
  }

  Atom actual_type = None;
  int actual_format = 0;
  unsigned long nitems = 0;
  unsigned long bytes_after = 0;
  unsigned char* prop = nullptr;

  const int rc = XGetWindowProperty(
      display_, window_, atom_property_, 0, (~0L), True, AnyPropertyType,
      &actual_type, &actual_format, &nitems, &bytes_after, &prop);
  if (rc != Success || prop == nullptr || actual_format != 8 || nitems == 0) {
    if (prop != nullptr) {
      XFree(prop);
    }
    return std::nullopt;
  }

  std::string text(reinterpret_cast<char*>(prop), nitems);
  XFree(prop);

  if (text.empty() || text == last_seen_) {
    return std::nullopt;
  }

  last_seen_ = text;
  return text;
}

std::optional<std::string> ClipboardWatcher::PollTextChange() {
  if (display_ == nullptr) {
    return std::nullopt;
  }

  const int fd = ConnectionNumber(display_);
  fd_set fds;
  FD_ZERO(&fds);
  FD_SET(fd, &fds);

  timeval tv;
  tv.tv_sec = 0;
  tv.tv_usec = 250000;

  (void)select(fd + 1, &fds, nullptr, nullptr, &tv);

  while (XPending(display_) > 0) {
    XEvent ev;
    XNextEvent(display_, &ev);

    if (ev.type == xfixes_event_base_ + XFixesSelectionNotify) {
      XFixesSelectionNotifyEvent* sev =
          reinterpret_cast<XFixesSelectionNotifyEvent*>(&ev);
      if (watch_clipboard_ && sev->selection == atom_clipboard_) {
        RequestSelection(atom_clipboard_);
      } else if (watch_primary_ && sev->selection == atom_primary_) {
        RequestSelection(atom_primary_);
      }
      continue;
    }

    if (ev.type == SelectionNotify) {
      const auto text = ReadSelectionProperty(ev.xselection);
      if (text.has_value()) {
        return text;
      }
    }
  }

  return std::nullopt;
}

}  // namespace cliphist
