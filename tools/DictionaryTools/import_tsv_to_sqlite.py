import argparse
import sqlite3
from datetime import datetime, timezone
from pathlib import Path


SCHEMA = """
CREATE TABLE IF NOT EXISTS entries (
    id INTEGER PRIMARY KEY,
    code TEXT NOT NULL,
    text TEXT NOT NULL,
    comment TEXT NOT NULL DEFAULT '',
    weight INTEGER NOT NULL DEFAULT 0,
    user_weight INTEGER NOT NULL DEFAULT 0,
    flags INTEGER NOT NULL DEFAULT 0
);

CREATE INDEX IF NOT EXISTS idx_entries_code_rank
ON entries(code, weight DESC, user_weight DESC, id ASC);

CREATE TABLE IF NOT EXISTS metadata (
    key TEXT PRIMARY KEY,
    value TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS user_entries (
    id INTEGER PRIMARY KEY,
    code TEXT NOT NULL,
    text TEXT NOT NULL,
    comment TEXT NOT NULL DEFAULT '',
    weight INTEGER NOT NULL DEFAULT 0,
    user_weight INTEGER NOT NULL DEFAULT 0,
    deleted INTEGER NOT NULL DEFAULT 0,
    created_at_utc TEXT NOT NULL DEFAULT (datetime('now')),
    updated_at_utc TEXT NOT NULL DEFAULT (datetime('now')),
    UNIQUE(code, text)
);

CREATE INDEX IF NOT EXISTS idx_user_entries_code_rank
ON user_entries(code, deleted, weight DESC, user_weight DESC, id ASC);
"""


def iter_tsv_rows(path: Path):
    with path.open("r", encoding="utf-8-sig", newline="") as file:
        for line_number, raw_line in enumerate(file, start=1):
            line = raw_line.rstrip("\r\n")
            if not line or line.startswith("#"):
                continue

            fields = line.split("\t")
            if len(fields) < 2 or not fields[0] or not fields[1]:
                raise ValueError(f"Invalid TSV line {line_number}: expected non-empty code and text fields")

            code = fields[0]
            text = fields[1]
            comment = fields[2] if len(fields) >= 3 else ""
            weight = int(fields[3]) if len(fields) >= 4 and fields[3] else 0
            user_weight = int(fields[4]) if len(fields) >= 5 and fields[4] else 0
            yield code, text, comment, weight, user_weight


def import_tsv(tsv_path: Path, db_path: Path, replace: bool) -> tuple[int, int]:
    if not tsv_path.exists():
        raise FileNotFoundError(f"TSV file not found: {tsv_path}")

    db_path.parent.mkdir(parents=True, exist_ok=True)

    with sqlite3.connect(db_path) as connection:
        connection.executescript(SCHEMA)
        if replace:
            connection.execute("DELETE FROM entries")

        rows = list(iter_tsv_rows(tsv_path))
        connection.executemany(
            """
            INSERT INTO entries(code, text, comment, weight, user_weight)
            VALUES (?, ?, ?, ?, ?)
            """,
            rows,
        )
        connection.execute(
            """
            INSERT INTO metadata(key, value)
            VALUES ('schema_version', '1')
            ON CONFLICT(key) DO UPDATE SET value = excluded.value
            """
        )
        connection.execute(
            """
            INSERT INTO metadata(key, value)
            VALUES ('source_format', 'tsv')
            ON CONFLICT(key) DO UPDATE SET value = excluded.value
            """
        )
        connection.execute(
            """
            INSERT INTO metadata(key, value)
            VALUES ('source_path', ?)
            ON CONFLICT(key) DO UPDATE SET value = excluded.value
            """,
            (str(tsv_path.resolve()),),
        )
        connection.execute(
            """
            INSERT INTO metadata(key, value)
            VALUES ('imported_at_utc', ?)
            ON CONFLICT(key) DO UPDATE SET value = excluded.value
            """,
            (datetime.now(timezone.utc).replace(microsecond=0).isoformat(),),
        )

        code_count = connection.execute("SELECT COUNT(DISTINCT code) FROM entries").fetchone()[0]
        candidate_count = connection.execute("SELECT COUNT(*) FROM entries").fetchone()[0]
        return code_count, candidate_count


def main() -> int:
    parser = argparse.ArgumentParser(description="Import an ArrowInput UTF-8 TSV dictionary into SQLite.")
    parser.add_argument("--input", required=True, help="Input TSV dictionary path")
    parser.add_argument("--output", required=True, help="Output SQLite database path")
    parser.add_argument("--append", action="store_true", help="Append rows instead of replacing entries")
    args = parser.parse_args()

    code_count, candidate_count = import_tsv(Path(args.input), Path(args.output), replace=not args.append)
    print(f"Imported {candidate_count} candidate(s) across {code_count} code(s).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
