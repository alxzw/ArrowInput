import sqlite3
import sys
from pathlib import Path


def main() -> int:
    if len(sys.argv) != 2:
        print("Usage: check_sqlite_import.py <database>")
        return 1

    db_path = Path(sys.argv[1])
    if not db_path.exists():
        raise FileNotFoundError(f"Database was not found: {db_path}")

    connection = sqlite3.connect(db_path)
    candidate_count = connection.execute("SELECT COUNT(*) FROM entries").fetchone()[0]
    code_count = connection.execute("SELECT COUNT(DISTINCT code) FROM entries").fetchone()[0]
    ni = connection.execute(
        "SELECT text, comment FROM entries WHERE code = ? ORDER BY weight DESC, user_weight DESC, id ASC",
        ("ni",),
    ).fetchall()
    metadata = dict(connection.execute("SELECT key, value FROM metadata"))

    assert candidate_count == 5, candidate_count
    assert code_count == 2, code_count
    assert ni == [("你", "ni"), ("呢", "ne"), ("尼", "ni")], ni
    assert metadata.get("schema_version") == "1", metadata
    assert metadata.get("source_format") == "tsv", metadata
    assert metadata.get("source_path"), metadata
    assert metadata.get("imported_at_utc"), metadata

    print("SQLite import tests passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
