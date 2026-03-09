#include "ui/SimpleUi.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <QApplication>
#include <QAction>
#include <QAbstractItemView>
#include <QClipboard>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QFont>
#include <QGuiApplication>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMenu>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QPointer>
#include <QSettings>
#include <QSet>
#include <QSplitter>
#include <QString>
#include <QStyle>
#include <QTimer>
#include <QTextCursor>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVariant>
#include <QVBoxLayout>
#include <QScrollBar>
#include <QWidget>

#include "util/Time.hpp"

namespace cliphist {

namespace {
constexpr const char* kWindowTitle = "剪贴板历史";
constexpr int kPreviewChars = 90;

QString ToQString(const std::string& value) {
  return QString::fromUtf8(value.c_str(), static_cast<int>(value.size()));
}

std::string ToStdString(const QString& value) {
  const QByteArray utf8 = value.toUtf8();
  return std::string(utf8.constData(), static_cast<std::size_t>(utf8.size()));
}

QString PreviewText(const std::string& text, int max_chars) {
  QString value = ToQString(text);
  value.replace('\n', ' ');
  value.replace('\r', ' ');
  if (value.size() > max_chars) {
    value = value.left(max_chars) + "...";
  }
  if (value.isEmpty()) {
    value = "(空文本)";
  }
  return value;
}

bool IsTodayLabel(const std::string& day_label) {
  return day_label.rfind("今天", 0) == 0;
}

QString EntryTimeLabel(const ClipboardEntry& entry) {
  return ToQString(FormatLocalTime(entry.updated_at));
}

struct UiGroup {
  std::string key;
  std::string title;
  bool default_collapsed = false;
  std::vector<ClipboardEntry> entries;
};

std::vector<UiGroup> BuildGroups(const std::vector<ClipboardEntry>& entries,
                                 const QString& search_query,
                                 bool favorites_only) {
  std::vector<ClipboardEntry> normal_entries;
  std::vector<ClipboardEntry> favorite_entries;
  const QString trimmed = search_query.trimmed();

  for (const auto& entry : entries) {
    if (!trimmed.isEmpty() &&
        !ToQString(entry.content).contains(trimmed, Qt::CaseInsensitive)) {
      continue;
    }
    if (favorites_only && !entry.favorite) {
      continue;
    }
    if (entry.favorite) {
      favorite_entries.push_back(entry);
    } else {
      normal_entries.push_back(entry);
    }
  }

  auto append_groups = [](const char* section_title,
                          const std::vector<ClipboardEntry>& source,
                          std::vector<UiGroup>* out) {
    if (source.empty()) {
      return;
    }
    std::string current_day;
    UiGroup current_group;
    for (const auto& entry : source) {
      const std::string day_label = DescribeLocalDay(entry.updated_at);
      if (current_day != day_label) {
        if (!current_group.entries.empty()) {
          out->push_back(current_group);
        }
        current_day = day_label;
        current_group = UiGroup{};
        current_group.key = std::string(section_title) + "/" + day_label;
        current_group.title =
            std::string(section_title) + " / " + day_label + "  " +
            std::to_string(0) + " 条";
        current_group.default_collapsed =
            std::string(section_title) == "收藏" || !IsTodayLabel(day_label);
      }
      current_group.entries.push_back(entry);
      current_group.title =
          std::string(section_title) + " / " + day_label + "  " +
          std::to_string(current_group.entries.size()) + " 条";
    }
    if (!current_group.entries.empty()) {
      out->push_back(current_group);
    }
  };

  std::vector<UiGroup> groups;
  append_groups("历史", normal_entries, &groups);
  append_groups("收藏", favorite_entries, &groups);
  return groups;
}

class ClipboardWindow final : public QMainWindow {
 public:
  ClipboardWindow(SimpleUi::FetchEntriesFn fetch_entries,
                  SimpleUi::ToggleFavoriteFn toggle_favorite,
                  SimpleUi::TogglePauseFn toggle_pause,
                  SimpleUi::IsPausedFn is_paused,
                  SimpleUi::ClearHistoryFn clear_history,
                  SimpleUi::DeleteEntryFn delete_entry,
                  SimpleUi::ExportHistoryFn export_history,
                  SimpleUi::SaveContentFn save_content,
                  int refresh_interval_ms)
      : fetch_entries_(std::move(fetch_entries)),
        toggle_favorite_(std::move(toggle_favorite)),
        toggle_pause_(std::move(toggle_pause)),
        is_paused_(std::move(is_paused)),
        clear_history_(std::move(clear_history)),
        delete_entry_(std::move(delete_entry)),
        export_history_(std::move(export_history)),
        save_content_(std::move(save_content)),
        refresh_interval_ms_(std::max(200, refresh_interval_ms)),
        settings_("cliphist", "cliphist-ui") {
    setWindowTitle(kWindowTitle);
    resize(1220, 820);
    BuildUi();
    ApplyTheme();
    LoadState();
    ConnectSignals();
    RefreshEntries();
    refresh_timer_.setInterval(refresh_interval_ms_);
    refresh_timer_.start();
  }

  ~ClipboardWindow() override { SaveState(); }

 protected:
  void closeEvent(QCloseEvent* event) override {
    SaveState();
    QMainWindow::closeEvent(event);
  }

 private:
  struct ViewAnchor {
    qint64 entry_id = -1;
    QString group_key;
    int offset_y = 0;
  };

  void BuildUi() {
    auto* central = new QWidget(this);
    auto* root = new QVBoxLayout(central);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(12);

    auto* header_box = new QWidget(central);
    auto* header_layout = new QVBoxLayout(header_box);
    header_layout->setContentsMargins(16, 14, 16, 14);
    header_layout->setSpacing(6);
    auto* title = new QLabel("剪贴板历史", header_box);
    QFont title_font = title->font();
    title_font.setPointSize(16);
    title_font.setBold(true);
    title->setFont(title_font);
    header_status_ = new QLabel("正在加载...", header_box);
    header_layout->addWidget(title);
    header_layout->addWidget(header_status_);
    root->addWidget(header_box);

    splitter_ = new QSplitter(Qt::Vertical, central);
    tree_ = new QTreeWidget(splitter_);
    tree_->setColumnCount(3);
    tree_->setHeaderLabels({"时间", "次数", "内容"});
    tree_->setRootIsDecorated(true);
    tree_->setIndentation(12);
    tree_->setUniformRowHeights(false);
    tree_->setAlternatingRowColors(false);
    tree_->setSelectionMode(QAbstractItemView::SingleSelection);
    tree_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tree_->header()->setSectionResizeMode(0, QHeaderView::Fixed);
    tree_->header()->setSectionResizeMode(1, QHeaderView::Fixed);
    tree_->header()->setSectionResizeMode(2, QHeaderView::Stretch);
    tree_->setColumnWidth(0, 110);
    tree_->setColumnWidth(1, 72);
    splitter_->addWidget(tree_);

    auto* preview_panel = new QWidget(splitter_);
    auto* preview_layout = new QVBoxLayout(preview_panel);
    preview_layout->setContentsMargins(0, 0, 0, 0);
    preview_layout->setSpacing(10);

    auto* preview_header = new QHBoxLayout();
    auto* preview_title = new QLabel("内容预览与编辑", preview_panel);
    QFont preview_font = preview_title->font();
    preview_font.setBold(true);
    preview_title->setFont(preview_font);
    preview_hint_ = new QLabel("草稿只在本次有效，点击“保存更改”才会写回历史。", preview_panel);
    preview_hint_->setObjectName("previewHint");
    preview_header->addWidget(preview_title);
    preview_header->addStretch(1);
    preview_header->addWidget(preview_hint_);
    preview_layout->addLayout(preview_header);

    preview_ = new QPlainTextEdit(preview_panel);
    preview_->setPlaceholderText("选择一条历史记录后，可在这里查看、编辑、复制草稿。");
    preview_->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    preview_layout->addWidget(preview_, 1);

    auto* preview_buttons = new QHBoxLayout();
    preview_buttons->setSpacing(8);
    copy_draft_button_ = new QPushButton("复制草稿", preview_panel);
    save_button_ = new QPushButton("保存更改", preview_panel);
    discard_button_ = new QPushButton("恢复原文", preview_panel);
    preview_buttons->addWidget(copy_draft_button_);
    preview_buttons->addWidget(save_button_);
    preview_buttons->addWidget(discard_button_);
    preview_buttons->addStretch(1);
    preview_layout->addLayout(preview_buttons);
    splitter_->addWidget(preview_panel);
    splitter_->setStretchFactor(0, 3);
    splitter_->setStretchFactor(1, 2);
    root->addWidget(splitter_, 1);

    auto* bottom_box = new QWidget(central);
    auto* bottom_layout = new QVBoxLayout(bottom_box);
    bottom_layout->setContentsMargins(16, 14, 16, 14);
    bottom_layout->setSpacing(10);

    auto* button_row = new QHBoxLayout();
    button_row->setSpacing(8);
    auto_copy_button_ = new QPushButton("自动复制: 关", bottom_box);
    copy_button_ = new QPushButton("复制当前", bottom_box);
    favorite_button_ = new QPushButton("加入收藏", bottom_box);
    delete_button_ = new QPushButton("删除当前", bottom_box);
    favorites_only_button_ = new QPushButton("仅看收藏", bottom_box);
    more_button_ = new QToolButton(bottom_box);
    more_button_->setText("更多");
    more_button_->setPopupMode(QToolButton::InstantPopup);
    more_menu_ = new QMenu(more_button_);
    pause_action_ = more_menu_->addAction("暂停监听");
    clear_history_action_ = more_menu_->addAction("清空历史");
    more_menu_->addSeparator();
    export_text_action_ = more_menu_->addAction("导出文本");
    export_json_action_ = more_menu_->addAction("导出 JSON");
    more_button_->setMenu(more_menu_);
    button_row->addWidget(auto_copy_button_);
    button_row->addWidget(copy_button_);
    button_row->addWidget(favorite_button_);
    button_row->addWidget(delete_button_);
    button_row->addWidget(favorites_only_button_);
    button_row->addStretch(1);
    button_row->addWidget(more_button_);
    bottom_layout->addLayout(button_row);

    auto* search_row = new QHBoxLayout();
    search_row->setSpacing(8);
    search_ = new QLineEdit(bottom_box);
    search_->setPlaceholderText("点击这里后输入关键字进行搜索");
    clear_search_button_ = new QPushButton("清空搜索", bottom_box);
    search_row->addWidget(search_, 1);
    search_row->addWidget(clear_search_button_);
    bottom_layout->addLayout(search_row);

    status_label_ = new QLabel("就绪", bottom_box);
    bottom_layout->addWidget(status_label_);
    root->addWidget(bottom_box);

    setCentralWidget(central);

    auto_copy_button_->setFocusPolicy(Qt::NoFocus);
    copy_button_->setFocusPolicy(Qt::NoFocus);
    favorite_button_->setFocusPolicy(Qt::NoFocus);
    delete_button_->setFocusPolicy(Qt::NoFocus);
    favorites_only_button_->setFocusPolicy(Qt::NoFocus);
    more_button_->setFocusPolicy(Qt::NoFocus);
    clear_search_button_->setFocusPolicy(Qt::NoFocus);
    copy_draft_button_->setFocusPolicy(Qt::NoFocus);
    save_button_->setFocusPolicy(Qt::NoFocus);
    discard_button_->setFocusPolicy(Qt::NoFocus);
  }

  void ApplyTheme() {
    setStyleSheet(
        "QMainWindow, QWidget { background: #eef4f8; color: #1f2d3d; }"
        "QLabel#previewHint { color: #5a7085; }"
        "QTreeWidget, QPlainTextEdit, QLineEdit {"
        "  background: #ffffff;"
        "  border: 1px solid #98b2c8;"
        "  border-radius: 12px;"
        "  padding: 6px;"
        "}"
        "QHeaderView::section {"
        "  background: #d7e7f3;"
        "  color: #23364a;"
        "  border: none;"
        "  border-bottom: 1px solid #98b2c8;"
        "  padding: 8px 10px;"
        "  font-weight: 600;"
        "}"
        "QPushButton, QToolButton {"
        "  background: #ffffff;"
        "  border: 1px solid #7f9db3;"
        "  border-radius: 12px;"
        "  padding: 8px 16px;"
        "  min-height: 18px;"
        "}"
        "QPushButton:hover, QToolButton:hover { background: #e8f2f8; }"
        "QPushButton:pressed, QToolButton:pressed { background: #d6e7f2; }"
        "QPushButton:disabled { color: #8295a6; background: #f3f6f8; }"
        "QTreeWidget::item { padding: 6px 4px; }"
        "QTreeWidget::item:selected {"
        "  background: #c9dff0;"
        "  color: #16222f;"
        "}"
        "QMenu {"
        "  background: #ffffff;"
        "  border: 1px solid #7f9db3;"
        "  border-radius: 10px;"
        "  padding: 6px;"
        "}"
        "QMenu::item { padding: 8px 16px; border-radius: 8px; }"
        "QMenu::item:selected { background: #d7e7f3; }");
  }

  void ConnectSignals() {
    connect(&refresh_timer_, &QTimer::timeout, this,
            [this]() {
              RefreshEntries(true);
              RefreshIfPending();
            });
    connect(tree_, &QTreeWidget::itemSelectionChanged, this,
            [this]() { HandleSelectionChanged(); });
    connect(tree_, &QTreeWidget::itemDoubleClicked, this,
            [this](QTreeWidgetItem* item, int) {
              if (item != nullptr && item->data(0, Qt::UserRole).isValid()) {
                CopyText(preview_->toPlainText(), "已复制当前草稿");
              }
            });
    connect(tree_, &QTreeWidget::itemExpanded, this,
            [this](QTreeWidgetItem* item) { RememberGroupState(item, false); });
    connect(tree_, &QTreeWidget::itemCollapsed, this,
            [this](QTreeWidgetItem* item) { RememberGroupState(item, true); });
    connect(preview_, &QPlainTextEdit::textChanged, this,
            [this]() { HandleDraftChanged(); });
    connect(copy_draft_button_, &QPushButton::clicked, this,
            [this]() { CopyDraft(); });
    connect(save_button_, &QPushButton::clicked, this,
            [this]() { SaveDraft(); });
    connect(discard_button_, &QPushButton::clicked, this,
            [this]() { ResetDraftFromSelection(); });
    connect(copy_button_, &QPushButton::clicked, this,
            [this]() { CopyCurrent(); });
    connect(auto_copy_button_, &QPushButton::clicked, this,
            [this]() { ToggleAutoCopy(); });
    connect(favorite_button_, &QPushButton::clicked, this,
            [this]() { ToggleFavorite(); });
    connect(delete_button_, &QPushButton::clicked, this,
            [this]() { DeleteSelected(); });
    connect(favorites_only_button_, &QPushButton::clicked, this,
            [this]() { ToggleFavoritesOnly(); });
    connect(clear_search_button_, &QPushButton::clicked, this,
            [this]() {
              search_->clear();
              search_->setFocus(Qt::OtherFocusReason);
            });
    connect(search_, &QLineEdit::textChanged, this,
            [this]() { RefreshEntries(false); });
    connect(pause_action_, &QAction::triggered, this,
            [this]() { TogglePause(); });
    connect(clear_history_action_, &QAction::triggered, this,
            [this]() { ClearHistory(); });
    connect(export_text_action_, &QAction::triggered, this,
            [this]() { ExportHistory(false); });
    connect(export_json_action_, &QAction::triggered, this,
            [this]() { ExportHistory(true); });
  }

  void LoadState() {
    restoreGeometry(settings_.value("window/geometry").toByteArray());
    const QByteArray splitter_state =
        settings_.value("window/splitter").toByteArray();
    if (!splitter_state.isEmpty()) {
      if (splitter_ != nullptr) {
        splitter_->restoreState(splitter_state);
      }
    }
    auto_copy_enabled_ = settings_.value("ui/auto_copy", false).toBool();
    favorites_only_ = settings_.value("ui/favorites_only", false).toBool();
    const QStringList groups =
        settings_.value("ui/collapsed_groups").toStringList();
    for (const QString& group : groups) {
      collapsed_groups_.insert(group);
    }
    UpdateActionButtons();
  }

  void SaveState() {
    settings_.setValue("window/geometry", saveGeometry());
    if (splitter_ != nullptr) {
      settings_.setValue("window/splitter", splitter_->saveState());
    }
    settings_.setValue("ui/auto_copy", auto_copy_enabled_);
    settings_.setValue("ui/favorites_only", favorites_only_);
    QStringList groups;
    for (const QString& group : collapsed_groups_) {
      groups.push_back(group);
    }
    settings_.setValue("ui/collapsed_groups", groups);
  }

  void UpdateHeaderStatus() {
    const int favorite_count = static_cast<int>(
        std::count_if(entries_.begin(), entries_.end(),
                      [](const ClipboardEntry& entry) { return entry.favorite; }));
    const QString monitoring =
        (is_paused_ && is_paused_()) ? "已暂停" : "运行中";
    header_status_->setText(
        QString("总条数: %1    收藏: %2    当前显示: %3    过滤: %4    监听: %5")
            .arg(entries_.size())
            .arg(favorite_count)
            .arg(visible_entries_.size())
            .arg(favorites_only_ ? "仅收藏" : "全部")
            .arg(monitoring));
    pause_action_->setText((is_paused_ && is_paused_()) ? "继续监听" : "暂停监听");
    favorite_button_->setText(CurrentEntry() != nullptr && CurrentEntry()->favorite
                                  ? "取消收藏"
                                  : "加入收藏");
    favorites_only_button_->setText(favorites_only_ ? "显示全部" : "仅看收藏");
  }

  bool IsUserInteracting() const {
    return isActiveWindow() &&
           (search_->hasFocus() || preview_->hasFocus() || tree_->hasFocus());
  }

  void RefreshEntries(bool allow_skip = false) {
    if (!fetch_entries_) {
      return;
    }
    if (allow_skip && IsUserInteracting()) {
      return;
    }
    const qint64 selected_id = SelectedEntryId();
    const ViewAnchor anchor = CaptureViewAnchor();
    const QString preview_text = preview_->toPlainText();
    const QTextCursor preview_cursor = preview_->textCursor();
    const bool keep_preview_state = selected_id > 0;
    const bool keep_draft = selected_id > 0 && draft_dirty_;

    entries_ = fetch_entries_();
    RebuildTree(selected_id, anchor);
    UpdateHeaderStatus();

    if (keep_preview_state && SelectedEntryId() == selected_id) {
      preview_->blockSignals(true);
      preview_->setPlainText(preview_text);
      preview_->blockSignals(false);
      preview_->setTextCursor(preview_cursor);
      draft_dirty_ = keep_draft;
    } else {
      ResetDraftFromSelection();
    }
    UpdateActionButtons();
  }

  void RefreshAndFollowEntry(qint64 entry_id) {
    if (!fetch_entries_) {
      return;
    }

    const QWidget* focused = focusWidget();
    const bool preview_had_focus = (focused == preview_);
    const bool search_had_focus = (focused == search_);
    const QString preview_text = preview_->toPlainText();
    const QTextCursor preview_cursor = preview_->textCursor();
    const bool keep_draft = draft_dirty_;

    entries_ = fetch_entries_();
    RebuildTree(entry_id, ViewAnchor{});
    UpdateHeaderStatus();

    QTreeWidgetItem* target = FindItemById(entry_id);
    if (target != nullptr) {
      tree_->setCurrentItem(target);
      tree_->scrollToItem(target, QAbstractItemView::PositionAtCenter);
    }

    if (SelectedEntryId() == entry_id) {
      preview_->blockSignals(true);
      preview_->setPlainText(preview_text);
      preview_->blockSignals(false);
      preview_->setTextCursor(preview_cursor);
      draft_dirty_ = keep_draft;
    } else {
      ResetDraftFromSelection();
    }

    if (preview_had_focus) {
      preview_->setFocus(Qt::OtherFocusReason);
      preview_->setTextCursor(preview_cursor);
    } else if (search_had_focus) {
      search_->setFocus(Qt::OtherFocusReason);
    } else {
      tree_->setFocus(Qt::OtherFocusReason);
    }
    UpdateActionButtons();
  }

  void RequestDeferredRefresh() { pending_refresh_ = true; }

  void RefreshIfPending() {
    if (!pending_refresh_ || IsUserInteracting()) {
      return;
    }
    pending_refresh_ = false;
    RefreshEntries(false);
  }

  ViewAnchor CaptureViewAnchor() const {
    ViewAnchor anchor;
    const int top_y = tree_->header()->height() + 4;
    QTreeWidgetItem* item = tree_->itemAt(8, top_y);
    if (item == nullptr) {
      return anchor;
    }
    const QVariant id = item->data(0, Qt::UserRole);
    if (id.isValid()) {
      anchor.entry_id = id.toLongLong();
      if (item->parent() != nullptr) {
        anchor.group_key = item->parent()->data(0, Qt::UserRole + 1).toString();
      }
    } else {
      anchor.group_key = item->data(0, Qt::UserRole + 1).toString();
    }
    anchor.offset_y = tree_->visualItemRect(item).top() - top_y;
    return anchor;
  }

  void RestoreViewAnchor(const ViewAnchor& anchor) {
    if (anchor.entry_id > 0) {
      QTreeWidgetItem* item = FindItemById(anchor.entry_id);
      if (item != nullptr) {
        tree_->scrollToItem(item, QAbstractItemView::PositionAtTop);
        tree_->verticalScrollBar()->setValue(
            tree_->verticalScrollBar()->value() + anchor.offset_y);
        return;
      }
    }
    if (!anchor.group_key.isEmpty()) {
      for (int i = 0; i < tree_->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = tree_->topLevelItem(i);
        if (item != nullptr &&
            item->data(0, Qt::UserRole + 1).toString() == anchor.group_key) {
          tree_->scrollToItem(item, QAbstractItemView::PositionAtTop);
          tree_->verticalScrollBar()->setValue(
              tree_->verticalScrollBar()->value() + anchor.offset_y);
          return;
        }
      }
    }
  }

  void RebuildTree(qint64 selected_id, const ViewAnchor& anchor) {
    tree_->setUpdatesEnabled(false);
    tree_->blockSignals(true);
    tree_->clear();
    visible_entries_.clear();
    item_lookup_.clear();

    const std::vector<UiGroup> groups =
        BuildGroups(entries_, search_->text(), favorites_only_);

    for (const auto& group : groups) {
      auto* group_item = new QTreeWidgetItem(tree_);
      group_item->setText(0, ToQString(group.title));
      group_item->setFirstColumnSpanned(true);
      group_item->setData(0, Qt::UserRole + 1, ToQString(group.key));
      QFont font = group_item->font(0);
      font.setBold(true);
      group_item->setFont(0, font);

      for (const auto& entry : group.entries) {
        visible_entries_.push_back(entry);
        auto* item = new QTreeWidgetItem(group_item);
        item->setData(0, Qt::UserRole, QVariant::fromValue<qlonglong>(entry.id));
        item->setText(0, EntryTimeLabel(entry));
        item->setText(1, QString("%1次").arg(entry.hit_count));
        const QString content_prefix = entry.favorite ? "★ " : "";
        item->setText(2, content_prefix + PreviewText(entry.content, kPreviewChars));
        item_lookup_.push_back(item);
      }

      const QString key = ToQString(group.key);
      if (!known_groups_.contains(key)) {
        known_groups_.insert(key);
        if (group.default_collapsed) {
          collapsed_groups_.insert(key);
        }
      }
      group_item->setExpanded(!collapsed_groups_.contains(key));
    }

    if (tree_->topLevelItemCount() > 0) {
      tree_->header()->setStretchLastSection(true);
    }

    RestoreSelection(selected_id);
    RestoreViewAnchor(anchor);
    tree_->blockSignals(false);
    tree_->setUpdatesEnabled(true);
  }

  void RestoreSelection(qint64 selected_id) {
    QTreeWidgetItem* target = nullptr;
    if (selected_id > 0) {
      target = FindItemById(selected_id);
    }
    if (target != nullptr) {
      QTreeWidgetItem* parent = target->parent();
      const QString group_key =
          parent != nullptr ? parent->data(0, Qt::UserRole + 1).toString() : QString();
      if (parent != nullptr && collapsed_groups_.contains(group_key)) {
        target = parent;
      }
    }
    if (target == nullptr && !item_lookup_.empty()) {
      target = item_lookup_.front();
    }
    if (target != nullptr) {
      tree_->setCurrentItem(target);
    }
  }

  void RememberGroupState(QTreeWidgetItem* item, bool collapsed) {
    if (item == nullptr) {
      return;
    }
    const QString key = item->data(0, Qt::UserRole + 1).toString();
    if (key.isEmpty()) {
      return;
    }
    if (collapsed) {
      collapsed_groups_.insert(key);
    } else {
      collapsed_groups_.remove(key);
    }
  }

  QTreeWidgetItem* FindItemById(qint64 id) const {
    for (QTreeWidgetItem* item : item_lookup_) {
      if (item != nullptr &&
          item->data(0, Qt::UserRole).toLongLong() == id) {
        return item;
      }
    }
    return nullptr;
  }

  qint64 SelectedEntryId() const {
    QTreeWidgetItem* item = tree_->currentItem();
    if (item == nullptr) {
      return -1;
    }
    return item->data(0, Qt::UserRole).toLongLong();
  }

  const ClipboardEntry* CurrentEntry() const {
    const qint64 id = SelectedEntryId();
    if (id <= 0) {
      return nullptr;
    }
    for (const auto& entry : visible_entries_) {
      if (entry.id == id) {
        return &entry;
      }
    }
    return nullptr;
  }

  void HandleSelectionChanged() {
    ResetDraftFromSelection();
    if (auto_copy_enabled_) {
      const ClipboardEntry* entry = CurrentEntry();
      if (entry != nullptr) {
        CopyText(ToQString(entry->content), "已自动复制当前条目");
      }
    }
    UpdateActionButtons();
    UpdateHeaderStatus();
  }

  ClipboardEntry* MutableEntryById(qint64 id) {
    if (id <= 0) {
      return nullptr;
    }
    for (auto& entry : entries_) {
      if (entry.id == id) {
        return &entry;
      }
    }
    return nullptr;
  }

  ClipboardEntry* MutableVisibleEntryById(qint64 id) {
    if (id <= 0) {
      return nullptr;
    }
    for (auto& entry : visible_entries_) {
      if (entry.id == id) {
        return &entry;
      }
    }
    return nullptr;
  }

  void UpdateCurrentItemTexts() {
    QTreeWidgetItem* item = tree_->currentItem();
    const ClipboardEntry* entry = CurrentEntry();
    if (item == nullptr || entry == nullptr ||
        !item->data(0, Qt::UserRole).isValid()) {
      return;
    }
    item->setText(0, EntryTimeLabel(*entry));
    item->setText(1, QString("%1次").arg(entry->hit_count));
    const QString prefix = entry->favorite ? "★ " : "";
    item->setText(2, prefix + PreviewText(entry->content, kPreviewChars));
  }

  void UpdateGroupTitle(QTreeWidgetItem* group_item) {
    if (group_item == nullptr) {
      return;
    }
    const QString key = group_item->data(0, Qt::UserRole + 1).toString();
    const QStringList parts = key.split('/');
    if (parts.size() < 2) {
      return;
    }
    group_item->setText(0, QString("%1 / %2  %3 条")
                               .arg(parts[0])
                               .arg(parts[1])
                               .arg(group_item->childCount()));
  }

  void RemoveCurrentItemInPlace() {
    QTreeWidgetItem* item = tree_->currentItem();
    if (item == nullptr || item->parent() == nullptr ||
        !item->data(0, Qt::UserRole).isValid()) {
      return;
    }
    QTreeWidgetItem* parent = item->parent();
    QTreeWidgetItem* next = tree_->itemBelow(item);
    if (next == nullptr || next == item) {
      next = tree_->itemAbove(item);
    }
    const qint64 id = item->data(0, Qt::UserRole).toLongLong();
    item_lookup_.erase(std::remove(item_lookup_.begin(), item_lookup_.end(), item),
                       item_lookup_.end());
    visible_entries_.erase(
        std::remove_if(visible_entries_.begin(), visible_entries_.end(),
                       [id](const ClipboardEntry& entry) { return entry.id == id; }),
        visible_entries_.end());
    entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
                                  [id](const ClipboardEntry& entry) {
                                    return entry.id == id;
                                  }),
                   entries_.end());
    parent->removeChild(item);
    delete item;
    if (parent->childCount() == 0) {
      collapsed_groups_.remove(parent->data(0, Qt::UserRole + 1).toString());
      delete parent;
    } else {
      UpdateGroupTitle(parent);
    }
    if (next != nullptr) {
      tree_->setCurrentItem(next);
    }
  }

  void ResetDraftFromSelection() {
    const ClipboardEntry* entry = CurrentEntry();
    preview_->blockSignals(true);
    preview_->setPlainText(entry != nullptr ? ToQString(entry->content) : QString());
    preview_->moveCursor(QTextCursor::End);
    preview_->blockSignals(false);
    draft_dirty_ = false;
    status_label_->setText(entry != nullptr ? "已载入当前记录原文" : "当前没有可预览的记录");
    UpdateActionButtons();
  }

  void HandleDraftChanged() {
    const ClipboardEntry* entry = CurrentEntry();
    if (entry == nullptr) {
      draft_dirty_ = false;
    } else {
      draft_dirty_ = (preview_->toPlainText() != ToQString(entry->content));
    }
    UpdateActionButtons();
  }

  void UpdateActionButtons() {
    auto_copy_button_->setText(auto_copy_enabled_ ? "自动复制: 开" : "自动复制: 关");
    const bool has_selection = CurrentEntry() != nullptr;
    copy_button_->setEnabled(has_selection);
    favorite_button_->setEnabled(has_selection);
    delete_button_->setEnabled(has_selection);
    copy_draft_button_->setEnabled(has_selection);
    save_button_->setEnabled(has_selection && draft_dirty_);
    discard_button_->setEnabled(has_selection && draft_dirty_);
    preview_->setEnabled(has_selection);
  }

  void CopyText(const QString& text, const QString& message) {
    QClipboard* clipboard = QGuiApplication::clipboard();
    if (clipboard == nullptr) {
      status_label_->setText("复制失败：无法访问系统剪贴板");
      return;
    }
    clipboard->setText(text, QClipboard::Clipboard);
    if (clipboard->supportsSelection()) {
      clipboard->setText(text, QClipboard::Selection);
    }
    status_label_->setText(message);
  }

  void CopyCurrent() {
    const ClipboardEntry* entry = CurrentEntry();
    if (entry == nullptr) {
      status_label_->setText("当前没有可复制的记录");
      return;
    }
    CopyText(ToQString(entry->content), "已复制原始内容");
  }

  void CopyDraft() {
    if (CurrentEntry() == nullptr) {
      status_label_->setText("当前没有可复制的草稿");
      return;
    }
    CopyText(preview_->toPlainText(), "已复制当前草稿");
  }

  void SaveDraft() {
    const ClipboardEntry* entry = CurrentEntry();
    if (entry == nullptr || !save_content_) {
      status_label_->setText("保存失败：当前没有选中记录");
      return;
    }
    const std::string content = ToStdString(preview_->toPlainText());
    if (content == entry->content) {
      status_label_->setText("没有需要保存的更改");
      return;
    }
    if (save_content_(entry->id, content)) {
      if (ClipboardEntry* mutable_entry = MutableEntryById(entry->id)) {
        mutable_entry->content = content;
      }
      if (ClipboardEntry* mutable_visible = MutableVisibleEntryById(entry->id)) {
        mutable_visible->content = content;
      }
      UpdateCurrentItemTexts();
      draft_dirty_ = false;
      status_label_->setText("已保存更改");
      UpdateActionButtons();
      RequestDeferredRefresh();
    } else {
      status_label_->setText("保存失败");
    }
  }

  void ToggleAutoCopy() {
    auto_copy_enabled_ = !auto_copy_enabled_;
    status_label_->setText(auto_copy_enabled_ ? "已开启自动复制" : "已关闭自动复制");
    UpdateActionButtons();
  }

  void ToggleFavorite() {
    const ClipboardEntry* entry = CurrentEntry();
    if (entry == nullptr || !toggle_favorite_) {
      status_label_->setText("当前没有可收藏的记录");
      return;
    }
    const qint64 entry_id = entry->id;
    if (toggle_favorite_(entry->id, !entry->favorite)) {
      const bool new_favorite = !entry->favorite;
      if (ClipboardEntry* mutable_entry = MutableEntryById(entry->id)) {
        mutable_entry->favorite = new_favorite;
      }
      if (ClipboardEntry* mutable_visible = MutableVisibleEntryById(entry->id)) {
        mutable_visible->favorite = new_favorite;
      }
      status_label_->setText(new_favorite ? "已加入收藏" : "已取消收藏");
      if (favorites_only_ && !new_favorite) {
        RemoveCurrentItemInPlace();
        UpdateHeaderStatus();
        UpdateActionButtons();
        RequestDeferredRefresh();
      } else {
        RefreshAndFollowEntry(entry_id);
      }
    } else {
      status_label_->setText("收藏操作失败");
    }
  }

  void DeleteSelected() {
    const ClipboardEntry* entry = CurrentEntry();
    if (entry == nullptr || !delete_entry_) {
      status_label_->setText("当前没有可删除的记录");
      return;
    }
    const auto result = QMessageBox::question(
        this, "删除记录", "确定要删除当前这条剪贴板记录吗？");
    if (result != QMessageBox::Yes) {
      status_label_->setText("已取消删除");
      return;
    }
    if (delete_entry_(entry->id)) {
      status_label_->setText("已删除当前记录");
      RemoveCurrentItemInPlace();
      UpdateHeaderStatus();
      UpdateActionButtons();
      RequestDeferredRefresh();
    } else {
      status_label_->setText("删除失败");
    }
  }

  void ToggleFavoritesOnly() {
    favorites_only_ = !favorites_only_;
    status_label_->setText(favorites_only_ ? "已切换为仅显示收藏" : "已切换为显示全部");
    RefreshEntries();
  }

  void TogglePause() {
    if (!toggle_pause_) {
      status_label_->setText("当前不支持暂停监听");
      return;
    }
    const bool paused = toggle_pause_();
    status_label_->setText(paused ? "已暂停监听剪贴板" : "已恢复监听剪贴板");
    UpdateHeaderStatus();
  }

  void ClearHistory() {
    if (!clear_history_) {
      status_label_->setText("当前不支持清空历史");
      return;
    }
    const auto result = QMessageBox::question(
        this, "清空历史", "确定要清空当前所有剪贴板历史吗？");
    if (result != QMessageBox::Yes) {
      status_label_->setText("已取消清空历史");
      return;
    }
    if (clear_history_()) {
      status_label_->setText("已清空历史");
      RefreshEntries();
    } else {
      status_label_->setText("清空历史失败");
    }
  }

  void ExportHistory(bool json) {
    if (!export_history_) {
      status_label_->setText("当前不支持导出");
      return;
    }
    const std::string path = export_history_(json);
    if (path.empty()) {
      status_label_->setText("导出失败");
    } else {
      status_label_->setText(QString("已导出到 %1").arg(ToQString(path)));
    }
  }

  SimpleUi::FetchEntriesFn fetch_entries_;
  SimpleUi::ToggleFavoriteFn toggle_favorite_;
  SimpleUi::TogglePauseFn toggle_pause_;
  SimpleUi::IsPausedFn is_paused_;
  SimpleUi::ClearHistoryFn clear_history_;
  SimpleUi::DeleteEntryFn delete_entry_;
  SimpleUi::ExportHistoryFn export_history_;
  SimpleUi::SaveContentFn save_content_;

  int refresh_interval_ms_ = 1000;
  QSettings settings_;
  QTimer refresh_timer_;
  bool auto_copy_enabled_ = false;
  bool favorites_only_ = false;
  bool draft_dirty_ = false;
  bool pending_refresh_ = false;

  QLabel* header_status_ = nullptr;
  QSplitter* splitter_ = nullptr;
  QTreeWidget* tree_ = nullptr;
  QPlainTextEdit* preview_ = nullptr;
  QLabel* preview_hint_ = nullptr;
  QPushButton* copy_draft_button_ = nullptr;
  QPushButton* save_button_ = nullptr;
  QPushButton* discard_button_ = nullptr;
  QPushButton* auto_copy_button_ = nullptr;
  QPushButton* copy_button_ = nullptr;
  QPushButton* favorite_button_ = nullptr;
  QPushButton* delete_button_ = nullptr;
  QPushButton* favorites_only_button_ = nullptr;
  QToolButton* more_button_ = nullptr;
  QMenu* more_menu_ = nullptr;
  QAction* pause_action_ = nullptr;
  QAction* clear_history_action_ = nullptr;
  QAction* export_text_action_ = nullptr;
  QAction* export_json_action_ = nullptr;
  QLineEdit* search_ = nullptr;
  QPushButton* clear_search_button_ = nullptr;
  QLabel* status_label_ = nullptr;

  std::vector<ClipboardEntry> entries_;
  std::vector<ClipboardEntry> visible_entries_;
  std::vector<QTreeWidgetItem*> item_lookup_;
  QSet<QString> collapsed_groups_;
  QSet<QString> known_groups_;
};
}  // namespace

int SimpleUi::Run(FetchEntriesFn fetch_entries, ToggleFavoriteFn toggle_favorite,
                  TogglePauseFn toggle_pause, IsPausedFn is_paused,
                  ClearHistoryFn clear_history, DeleteEntryFn delete_entry,
                  ExportHistoryFn export_history, SaveContentFn save_content,
                  int refresh_interval_ms) {
  int argc = 1;
  char arg0[] = "cliphist-ui";
  char* argv[] = {arg0, nullptr};
  std::unique_ptr<QApplication> owned_app;
  QApplication* app = qobject_cast<QApplication*>(QCoreApplication::instance());
  if (app == nullptr) {
    owned_app = std::make_unique<QApplication>(argc, argv);
    app = owned_app.get();
  }
  app->setQuitOnLastWindowClosed(true);

  ClipboardWindow window(std::move(fetch_entries), std::move(toggle_favorite),
                         std::move(toggle_pause), std::move(is_paused),
                         std::move(clear_history), std::move(delete_entry),
                         std::move(export_history), std::move(save_content),
                         refresh_interval_ms);
  window.show();
  window.raise();
  window.activateWindow();
  return app->exec();
}

}  // namespace cliphist
