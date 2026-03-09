# Architecture

## Overview
`cliphist` is a local-only clipboard history manager.

Platform support:
- Linux (X11 build): full feature set (`daemon`, `desktop`, `ui`, `list`, `stats`).
- Windows (non-X11 build): `list`, `stats`, `ui` available; `daemon` and `desktop` are disabled.

Layers:
- `x11`: Linux X11-only clipboard and tray integration.
- `ui`: Qt desktop viewer/editor for clipboard history and copy-back action.
- `core`: hashing and retention policy.
- `db`: SQLite schema/init and history repository.
- `cli`: command parsing and terminal formatting.
- `app`: orchestrates daemon loop and CLI actions.

## Data Flow
1. `daemon` starts `ClipboardWatcher` with selected source (`clipboard`, `primary`, or `both`).
2. On clipboard text change, text is hashed.
3. Repository performs upsert by hash (dedup).
4. Retention policy enforces max item count.

## Persistence
Database table: `clipboard_history`
- unique `content_hash` guarantees dedup key.
- `updated_at` determines recency ordering.
- `hit_count` tracks repeated copies.
