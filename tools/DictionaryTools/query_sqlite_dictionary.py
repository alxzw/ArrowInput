import argparse
import sqlite3
import sys
from pathlib import Path


def write_text(text: str) -> None:
    sys.stdout.buffer.write(text.encode("utf-8"))
    sys.stdout.buffer.flush()


def query(db_path: Path, code: str) -> list[tuple[str, str, int, int]]:
    if not db_path.exists():
        raise FileNotFoundError(f"Database was not found: {db_path}")

    with sqlite3.connect(db_path) as connection:
        return connection.execute(
            """
            SELECT text, comment, weight, user_weight
            FROM entries
            WHERE code = ?
            ORDER BY weight DESC, user_weight DESC, id ASC
            """,
            (code,),
        ).fetchall()


def main() -> int:
    parser = argparse.ArgumentParser(description="Query an ArrowInput SQLite dictionary.")
    parser.add_argument("--database", required=True, help="SQLite dictionary path")
    parser.add_argument("code", nargs="+", help="Code(s) to query")
    args = parser.parse_args()

    db_path = Path(args.database)
    for code in args.code:
        rows = query(db_path, code)
        write_text(f"[{code}] {len(rows)} candidate(s)\n")
        for index, (text, comment, weight, user_weight) in enumerate(rows, start=1):
            suffix = f" {comment}" if comment else ""
            write_text(f"{index}. {text}{suffix} weight={weight} user_weight={user_weight}\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
