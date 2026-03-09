CREATE TABLE IF NOT EXISTS clipboard_history (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  content TEXT NOT NULL,
  content_hash TEXT NOT NULL UNIQUE,
  created_at INTEGER NOT NULL,
  updated_at INTEGER NOT NULL,
  hit_count INTEGER NOT NULL DEFAULT 1,
  favorite INTEGER NOT NULL DEFAULT 0
);

CREATE INDEX IF NOT EXISTS idx_clipboard_updated_at
  ON clipboard_history(updated_at DESC);
