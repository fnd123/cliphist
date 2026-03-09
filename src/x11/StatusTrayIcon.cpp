#include "x11/StatusTrayIcon.hpp"

#include <algorithm>
#include <string>
#include <vector>

#include "util/Time.hpp"

namespace cliphist {

namespace {
struct HistoryMenuPayload {
  StatusTrayIcon* tray = nullptr;
  std::string text;
};
}  // namespace

StatusTrayIcon::~StatusTrayIcon() { Stop(); }

bool StatusTrayIcon::Start(ClickFn on_activate, ClickFn on_quit,
                           RecentEntriesFn recent_entries,
                           ActionFn on_toggle_pause, BoolFn is_paused,
                           std::function<bool()> on_clear_history) {
  Stop();

  int argc = 0;
  char** argv = nullptr;
  if (!gtk_init_check(&argc, &argv)) {
    return false;
  }

  on_activate_ = std::move(on_activate);
  on_quit_ = std::move(on_quit);
  recent_entries_ = std::move(recent_entries);
  on_toggle_pause_ = std::move(on_toggle_pause);
  is_paused_ = std::move(is_paused);
  on_clear_history_ = std::move(on_clear_history);

  status_icon_ = gtk_status_icon_new_from_icon_name("cliphist");
  if (status_icon_ == nullptr) {
    return false;
  }
  gtk_status_icon_set_title(status_icon_, "剪贴板历史");
  gtk_status_icon_set_tooltip_text(status_icon_, "剪贴板历史");
  gtk_status_icon_set_visible(status_icon_, TRUE);

  g_signal_connect(status_icon_, "activate", G_CALLBACK(HandleActivate), this);
  g_signal_connect(status_icon_, "popup-menu", G_CALLBACK(HandlePopupMenu), this);

  RebuildMenu();
  return true;
}

void StatusTrayIcon::OnActivate() {
  if (on_activate_) {
    on_activate_();
  }
}

void StatusTrayIcon::OnPopupMenu(guint button, guint activate_time) {
  RebuildMenu();
  if (menu_ != nullptr) {
    gtk_menu_popup(GTK_MENU(menu_), nullptr, nullptr,
                   gtk_status_icon_position_menu, status_icon_, button,
                   activate_time);
  }
}

void StatusTrayIcon::CopyEntry(const std::string& text) {
  GtkClipboard* clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
  GtkClipboard* primary = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
  gtk_clipboard_set_text(clipboard, text.c_str(), static_cast<int>(text.size()));
  gtk_clipboard_store(clipboard);
  gtk_clipboard_set_text(primary, text.c_str(), static_cast<int>(text.size()));
  gtk_clipboard_store(primary);
}

void StatusTrayIcon::RebuildMenu() {
  if (menu_ == nullptr) {
    menu_ = gtk_menu_new();
  }

  GList* children = gtk_container_get_children(GTK_CONTAINER(menu_));
  for (GList* item = children; item != nullptr; item = item->next) {
    gtk_widget_destroy(GTK_WIDGET(item->data));
  }
  g_list_free(children);

  GtkWidget* open_item = gtk_menu_item_new_with_label("打开窗口");
  gtk_menu_shell_append(GTK_MENU_SHELL(menu_), open_item);
  g_signal_connect(open_item, "activate", G_CALLBACK(HandleMenuOpen), this);

  GtkWidget* pause_item = gtk_menu_item_new_with_label(
      (is_paused_ && is_paused_()) ? "继续监听" : "暂停监听");
  gtk_menu_shell_append(GTK_MENU_SHELL(menu_), pause_item);
  g_signal_connect(pause_item, "activate", G_CALLBACK(HandleMenuPause), this);

  GtkWidget* clear_item = gtk_menu_item_new_with_label("清空历史");
  gtk_menu_shell_append(GTK_MENU_SHELL(menu_), clear_item);
  g_signal_connect(clear_item, "activate", G_CALLBACK(HandleMenuClear), this);

  gtk_menu_shell_append(GTK_MENU_SHELL(menu_), gtk_separator_menu_item_new());

  const std::vector<ClipboardEntry> entries =
      recent_entries_ ? recent_entries_() : std::vector<ClipboardEntry>{};
  if (entries.empty()) {
    GtkWidget* empty_item = gtk_menu_item_new_with_label("暂无剪贴板历史");
    gtk_widget_set_sensitive(empty_item, FALSE);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu_), empty_item);
  } else {
    const std::size_t max_items = std::min<std::size_t>(15, entries.size());
    for (std::size_t i = 0; i < max_items; ++i) {
      const std::string label =
          std::to_string(i + 1) + ". " +
          (entries[i].favorite ? "[*] " : "") +
          "[" + FormatLocalDateTime(entries[i].updated_at) + "] " +
          "[" + std::to_string(entries[i].hit_count) + "次] " +
          PreviewLabel(entries[i].content);
      GtkWidget* item = gtk_menu_item_new_with_label(label.c_str());
      HistoryMenuPayload* payload = new HistoryMenuPayload{this, entries[i].content};
      g_object_set_data_full(G_OBJECT(item), "cliphist_payload", payload,
                             [](gpointer data) {
                               delete static_cast<HistoryMenuPayload*>(data);
                             });
      g_signal_connect(item, "activate", G_CALLBACK(HandleHistoryCopy), payload);
      gtk_menu_shell_append(GTK_MENU_SHELL(menu_), item);
    }
  }

  gtk_menu_shell_append(GTK_MENU_SHELL(menu_), gtk_separator_menu_item_new());

  GtkWidget* quit_item = gtk_menu_item_new_with_label("退出");
  gtk_menu_shell_append(GTK_MENU_SHELL(menu_), quit_item);
  g_signal_connect(quit_item, "activate", G_CALLBACK(HandleMenuQuit), this);
  gtk_widget_show_all(menu_);
}

std::string StatusTrayIcon::PreviewLabel(const std::string& text) const {
  constexpr std::size_t kMaxChars = 36;
  std::string out = text;
  std::replace(out.begin(), out.end(), '\n', ' ');
  std::replace(out.begin(), out.end(), '\r', ' ');
  if (out.size() > kMaxChars) {
    out = out.substr(0, kMaxChars) + "...";
  }
  if (out.empty()) {
    out = "(空文本)";
  }
  return out;
}

int StatusTrayIcon::Run() {
  if (status_icon_ == nullptr) {
    return 4;
  }
  gtk_main();
  return 0;
}

void StatusTrayIcon::Stop() {
  if (gtk_main_level() > 0) {
    gtk_main_quit();
  }
  if (menu_ != nullptr) {
    gtk_widget_destroy(menu_);
    menu_ = nullptr;
  }
  if (status_icon_ != nullptr) {
    g_object_unref(status_icon_);
    status_icon_ = nullptr;
  }
}

void StatusTrayIcon::HandleActivate(GtkStatusIcon* status_icon, gpointer user_data) {
  (void)status_icon;
  static_cast<StatusTrayIcon*>(user_data)->OnActivate();
}

void StatusTrayIcon::HandlePopupMenu(GtkStatusIcon* status_icon, guint button,
                                     guint activate_time, gpointer user_data) {
  (void)status_icon;
  static_cast<StatusTrayIcon*>(user_data)->OnPopupMenu(button, activate_time);
}

void StatusTrayIcon::HandleMenuOpen(GtkWidget* item, gpointer user_data) {
  (void)item;
  static_cast<StatusTrayIcon*>(user_data)->OnActivate();
}

void StatusTrayIcon::HandleMenuQuit(GtkWidget* item, gpointer user_data) {
  (void)item;
  StatusTrayIcon* tray = static_cast<StatusTrayIcon*>(user_data);
  if (tray->on_quit_) {
    tray->on_quit_();
  }
  if (gtk_main_level() > 0) {
    gtk_main_quit();
  }
}

void StatusTrayIcon::HandleMenuPause(GtkWidget* item, gpointer user_data) {
  (void)item;
  StatusTrayIcon* tray = static_cast<StatusTrayIcon*>(user_data);
  if (tray->on_toggle_pause_) {
    tray->on_toggle_pause_();
  }
}

void StatusTrayIcon::HandleMenuClear(GtkWidget* item, gpointer user_data) {
  (void)item;
  StatusTrayIcon* tray = static_cast<StatusTrayIcon*>(user_data);
  if (tray->on_clear_history_) {
    (void)tray->on_clear_history_();
  }
}

void StatusTrayIcon::HandleHistoryCopy(GtkWidget* item, gpointer user_data) {
  (void)item;
  HistoryMenuPayload* payload = static_cast<HistoryMenuPayload*>(user_data);
  if (payload != nullptr && payload->tray != nullptr) {
    payload->tray->CopyEntry(payload->text);
  }
}

}  // namespace cliphist
