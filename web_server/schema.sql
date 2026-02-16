CREATE TABLE sqlite_sequence(name,seq);
CREATE TABLE plan(
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  object_name TEXT NOT NULL,
  is_interstellar INTEGER NOT NULL,
  obs_start_time INTEGER NOT NULL,
  rec_start_time INTEGER NOT NULL,
  end_time INTEGER NOT NULL
);
