# CLI

## Commands

- `cliphist daemon [--max-items N] [--selection clipboard|primary|both] [--db PATH]`
  - Run clipboard listener.
  - Linux X11 only.
  - Persistence:
    - SQLite history DB: path from `--db` (default `~/.local/share/cliphist/cliphist.db`).
    - File archive by date: `<db_dir>/archive/YYYY-MM-DD/HHMMSS[_N].txt`.
- `cliphist desktop [--max-items N] [--selection clipboard|primary|both] [--db PATH]`
  - Run daemon + tray + UI session.
  - Linux X11 only.
- `cliphist list [--limit N] [--offset N] [--contains TEXT] [--exact TEXT] [--since UNIX_TS] [--sort updated_at|created_at] [--order asc|desc] [--count-only] [--json] [--db PATH]`
  - Show filtered history entries, selectable sort order, or count-only result.
- `cliphist ui [--limit N] [--db PATH]`
  - Open a desktop window showing history list with auto refresh (~1s).
  - UI actions:
    - `Up/Down/PageUp/PageDown`: select item.
    - `Enter/c`: copy selected item once.
    - Click `Auto Copy` button or press `a`: toggle auto copy on selection change.
    - `q/Esc`: quit.
- `cliphist stats [--json] [--db PATH]`
  - Show total entry count, total hits, and time range.

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
