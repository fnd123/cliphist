#pragma once

#include <gtk/gtk.h>

#include <functional>
#include <string>
#include <vector>

#include "core/ClipboardEntry.hpp"

namespace cliphist {

class StatusTrayIcon {
 public:
  using ClickFn = std::function<void()>;
  using RecentEntriesFn = std::function<std::vector<ClipboardEntry>()>;
  using ActionFn = std::function<void()>;
  using BoolFn = std::function<bool()>;

  StatusTrayIcon() = default;
  ~StatusTrayIcon();

  StatusTrayIcon(const StatusTrayIcon&) = delete;
  StatusTrayIcon& operator=(const StatusTrayIcon&) = delete;

  bool Start(ClickFn on_activate, ClickFn on_quit,
             RecentEntriesFn recent_entries, ActionFn on_toggle_pause,
             BoolFn is_paused, std::function<bool()> on_clear_history);
  int Run();
  void Stop();

 private:
  void OnActivate();
  void OnPopupMenu(guint button, guint activate_time);
  void CopyEntry(const std::string& text);
  void RebuildMenu();
  std::string PreviewLabel(const std::string& text) const;

  static void HandleActivate(GtkStatusIcon* status_icon, gpointer user_data);
  static void HandlePopupMenu(GtkStatusIcon* status_icon, guint button,
                              guint activate_time, gpointer user_data);
  static void HandleMenuOpen(GtkWidget* item, gpointer user_data);
  static void HandleMenuQuit(GtkWidget* item, gpointer user_data);
  static void HandleMenuPause(GtkWidget* item, gpointer user_data);
  static void HandleMenuClear(GtkWidget* item, gpointer user_data);
  static void HandleHistoryCopy(GtkWidget* item, gpointer user_data);

  GtkStatusIcon* status_icon_ = nullptr;
  GtkWidget* menu_ = nullptr;
  ClickFn on_activate_;
  ClickFn on_quit_;
  RecentEntriesFn recent_entries_;
  ActionFn on_toggle_pause_;
  BoolFn is_paused_;
  std::function<bool()> on_clear_history_;
};

}  // namespace cliphist
