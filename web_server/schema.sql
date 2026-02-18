CREATE TABLE sqlite_sequence(name,seq);
CREATE TABLE plan(
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  object_name TEXT NOT NULL,
  is_interstellar INTEGER NOT NULL,
  obs_start_time INTEGER NOT NULL,
  rec_start_time INTEGER NOT NULL,
  end_time INTEGER NOT NULL
);
CREATE TABLE db_metadata (
  id INTEGER PRIMARY KEY CHECK (id = 1),
  version INTEGER NOT NULL DEFAULT 1
);
CREATE TRIGGER update_version_on_insert AFTER INSERT ON plan
BEGIN
  UPDATE db_metadata SET version = version + 1 WHERE id = 1;
END;
CREATE TRIGGER update_version_on_update AFTER UPDATE ON plan
BEGIN
  UPDATE db_metadata SET version = version + 1 WHERE id = 1;
END;
CREATE TRIGGER update_version_on_delete AFTER DELETE ON plan
BEGIN
  UPDATE db_metadata SET version = version + 1 WHERE id = 1;
END;
PRAGMA journal_mode=WAL;