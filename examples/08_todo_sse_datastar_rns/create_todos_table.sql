CREATE TABLE IF NOT EXISTS todos (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  user_id INTEGER NOT NULL,
  title TEXT NOT NULL,
  finished INTEGER CHECK(finished IN (1))
);
CREATE INDEX IF NOT EXISTS idx_todos_user_id ON todos(user_id);
