CREATE TABLE plan(
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  object_name TEXT NOT NULL,
  is_interstellar INTEGER NOT NULL,
  obs_start_time INTEGER NOT NULL,
  rec_start_time INTEGER NOT NULL,
  end_time INTEGER NOT NULL,
  record INTEGER DEFAULT 1
);

CREATE TABLE db_metadata (
  id INTEGER PRIMARY KEY CHECK (id = 1),
  version INTEGER NOT NULL DEFAULT 1
);

INSERT INTO db_metadata (id) VALUES (1);