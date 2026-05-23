import argparse
import sqlite3
import sys
from pathlib import Path


def write_text(text: str) -> None:
    sys.stdout.buffer.write(text.encode("utf-8"))
    sys.stdout.buffer.flush()


def require_database(path: Path) -> None:
    if not path.exists():
        raise FileNotFoundError(f"Database was not found: {path}")


def read_global_stats(connection: sqlite3.Connection) -> tuple[int, int]:
    row = connection.execute(
        """
        SELECT COUNT(*), COUNT(DISTINCT code)
        FROM entries
        """
    ).fetchone()
    return int(row[0]), int(row[1])


def read_metadata(connection: sqlite3.Connection) -> list[tuple[str, str]]:
    return connection.execute(
        """
        SELECT key, value
        FROM metadata
        ORDER BY key ASC
        """
    ).fetchall()


def read_code_stats(connection: sqlite3.Connection, code: str) -> tuple[int, int | None, int | None]:
    row = connection.execute(
        """
        SELECT COUNT(*), MIN(weight), MAX(weight)
        FROM entries
        WHERE code = ?
        """,
        (code,),
    ).fetchone()
    total = int(row[0])
    min_weight = None if row[1] is None else int(row[1])
    max_weight = None if row[2] is None else int(row[2])
    return total, min_weight, max_weight


def next_prefix_upper_bound(prefix: str) -> str | None:
    if not prefix:
        return None
    chars = list(prefix)
    for index in range(len(chars) - 1, -1, -1):
        if ord(chars[index]) < 0x10FFFF:
            chars[index] = chr(ord(chars[index]) + 1)
            return "".join(chars[: index + 1])
    return None


def read_prefix_stats(connection: sqlite3.Connection, code: str) -> tuple[int, int | None, int | None]:
    upper_bound = next_prefix_upper_bound(code)
    if upper_bound is None:
        row = connection.execute(
            """
            SELECT COUNT(*), MIN(weight), MAX(weight)
            FROM entries
            WHERE code >= ?
            """,
            (code,),
        ).fetchone()
    else:
        row = connection.execute(
            """
            SELECT COUNT(*), MIN(weight), MAX(weight)
            FROM entries
            WHERE code >= ? AND code < ?
            """,
            (code, upper_bound),
        ).fetchone()
    total = int(row[0])
    min_weight = None if row[1] is None else int(row[1])
    max_weight = None if row[2] is None else int(row[2])
    return total, min_weight, max_weight


def read_candidates(
    connection: sqlite3.Connection,
    code: str,
    limit: int,
    prefix: bool,
) -> list[tuple[str, str, int, int]]:
    if not prefix:
        return connection.execute(
            """
            SELECT text, comment, weight, user_weight
            FROM entries
            WHERE code = ?
            ORDER BY weight DESC, user_weight DESC, id ASC
            LIMIT ?
            """,
            (code, limit),
        ).fetchall()

    rows = connection.execute(
        """
        SELECT text, comment, weight, user_weight
        FROM entries
        WHERE code = ?
        ORDER BY weight DESC, user_weight DESC, id ASC
        LIMIT ?
        """,
        (code, limit),
    ).fetchall()
    if len(rows) >= limit:
        return rows

    upper_bound = next_prefix_upper_bound(code)
    if upper_bound is None:
        rows.extend(
            connection.execute(
                """
                SELECT text, comment, weight, user_weight
                FROM entries
                WHERE code >= ? AND code <> ?
                ORDER BY weight DESC, user_weight DESC, code ASC, id ASC
                LIMIT ?
                """,
                (code, code, limit - len(rows)),
            ).fetchall()
        )
        return rows

    rows.extend(
        connection.execute(
            """
            SELECT text, comment, weight, user_weight
            FROM entries
            WHERE code >= ? AND code < ? AND code <> ?
            ORDER BY weight DESC, user_weight DESC, code ASC, id ASC
            LIMIT ?
            """,
            (code, upper_bound, code, limit - len(rows)),
        ).fetchall()
    )
    return rows


def read_top_codes(connection: sqlite3.Connection, limit: int) -> list[tuple[str, int, int, int]]:
    return connection.execute(
        """
        SELECT code, COUNT(*) AS candidate_count, MIN(weight), MAX(weight)
        FROM entries
        GROUP BY code
        ORDER BY candidate_count DESC, code ASC
        LIMIT ?
        """,
        (limit,),
    ).fetchall()


def main() -> int:
    parser = argparse.ArgumentParser(description="Inspect an ArrowInput SQLite dictionary.")
    parser.add_argument("--database", required=True, help="SQLite dictionary path")
    parser.add_argument("--limit", type=int, default=10, help="Candidate preview count per code")
    parser.add_argument("--metadata", action="store_true", help="Print dictionary metadata")
    parser.add_argument("--top-codes", type=int, default=0, help="Print codes with the most candidates")
    parser.add_argument("--prefix", action="store_true", help="Inspect prefix lookup instead of exact-code lookup")
    parser.add_argument("code", nargs="*", help="Code(s) to inspect")
    args = parser.parse_args()

    if args.limit < 1:
        raise ValueError("--limit must be at least 1")
    if args.top_codes < 0:
        raise ValueError("--top-codes cannot be negative")

    db_path = Path(args.database)
    require_database(db_path)

    with sqlite3.connect(db_path) as connection:
        entry_count, code_count = read_global_stats(connection)
        write_text(f"Database: {db_path}\n")
        write_text(f"Entries: {entry_count}\n")
        write_text(f"Codes: {code_count}\n")
        write_text(f"Lookup: {'prefix' if args.prefix else 'exact'}\n")

        if args.metadata:
            metadata = read_metadata(connection)
            write_text("Metadata:\n")
            if metadata:
                for key, value in metadata:
                    write_text(f"  {key}={value}\n")
            else:
                write_text("  <none>\n")

        if args.top_codes > 0:
            write_text(f"Top codes by candidate count:\n")
            for index, (code, candidate_count, min_weight, max_weight) in enumerate(
                read_top_codes(connection, args.top_codes),
                start=1,
            ):
                write_text(
                    f"  {index}. {code} candidates={candidate_count} weight_range={min_weight}..{max_weight}\n"
                )

        for code in args.code:
            total, min_weight, max_weight = (
                read_prefix_stats(connection, code) if args.prefix else read_code_stats(connection, code)
            )
            shown = min(total, args.limit)
            write_text(f"[{code}]\n")
            write_text(f"  total_candidates={total}\n")
            write_text(f"  shown_candidates={shown}\n")
            if total == 0:
                write_text("  weight_range=<none>\n")
                continue

            write_text(f"  weight_range={min_weight}..{max_weight}\n")
            write_text("  top:\n")
            for index, (text, comment, weight, user_weight) in enumerate(
                read_candidates(connection, code, args.limit, args.prefix),
                start=1,
            ):
                suffix = f" {comment}" if comment else ""
                write_text(f"    {index}. {text}{suffix} weight={weight} user_weight={user_weight}\n")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
