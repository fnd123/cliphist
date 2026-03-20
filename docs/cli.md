# CLI

## Commands

- `cliphist daemon [--max-items N] [--selection clipboard|primary|both] [--db PATH]`
  - Run clipboard listener.
  - `--max-items` 默认 `0`（不限条数）。
  - Linux X11 build supports `clipboard|primary|both`.
  - Windows Qt5 build listens to the system clipboard.
  - Persistence:
    - SQLite history DB: path from `--db` (default `~/.local/share/cliphist/cliphist.db`).
    - File archive by date: `<db_dir>/archive/YYYY-MM-DD/HHMMSS[_N].txt`.
- `cliphist desktop [--max-items N] [--selection clipboard|primary|both] [--db PATH]`
  - Run daemon + tray + UI session.
  - `--max-items` 默认 `0`（不限条数）。
  - Linux X11 build uses the X11 tray path.
  - Windows Qt5 build uses `QSystemTrayIcon` and launches/focuses the Qt UI.
- `cliphist list [--limit N] [--offset N] [--contains TEXT] [--exact TEXT] [--since UNIX_TS] [--sort updated_at|created_at] [--order asc|desc] [--count-only] [--json] [--db PATH]`
  - Show filtered history entries, selectable sort order, or count-only result.
- `cliphist ui [--limit N] [--db PATH]`
  - Open a desktop window showing history list with auto refresh (~1s).
  - 默认显示上限 `10000` 条（可通过 `--limit` 调整，超过会自动截断为 `10000`）。
  - 分组固定为：`今天`、`昨天`、`更久以前`、`收藏`。
  - 默认仅展开 `今天` 分组。
  - 标题格式：`今天 YYYY/MM/DD`、`昨天 YYYY/MM/DD`。
  - 时间格式：`今天/昨天` 仅显示时分秒，`更久以前` 显示完整日期时间（含年份）。
  - UI actions:
    - `Up/Down/PageUp/PageDown`: select item.
    - `Enter/c`: copy selected item once.
    - Click `Auto Copy` button or press `a`: toggle auto copy on selection change.
    - `q/Esc`: quit.
- `cliphist stats [--json] [--db PATH]`
  - Show total entry count, total hits, and time range.

## Platform Notes

- Windows 下无参数启动时，默认进入 `desktop` 模式。
- Windows 仅支持系统剪贴板，不支持 X11 `primary` 选择区。

## Examples

```bash
cliphist daemon --max-items 1000 --selection clipboard
cliphist list --limit 50
cliphist list --contains hello --since 1700000000
cliphist list --exact "hello world" --json
cliphist list --contains hello --count-only --json
cliphist stats --json
cliphist ui --limit 200
```
