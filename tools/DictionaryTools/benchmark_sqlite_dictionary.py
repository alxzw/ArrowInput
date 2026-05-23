import argparse
import sqlite3
import sys
import time
from pathlib import Path


def write_text(text: str) -> None:
    sys.stdout.buffer.write(text.encode("utf-8"))
    sys.stdout.buffer.flush()


def next_prefix_upper_bound(prefix: str) -> str | None:
    if not prefix:
        return None
    chars = list(prefix)
    for index in range(len(chars) - 1, -1, -1):
        if ord(chars[index]) < 0x10FFFF:
            chars[index] = chr(ord(chars[index]) + 1)
            return "".join(chars[: index + 1])
    return None


def exact_sql() -> str:
    return """
        SELECT text, comment, weight, user_weight
        FROM entries
        WHERE code = ?
        ORDER BY weight DESC, user_weight DESC, id ASC
        LIMIT ?
    """


def prefix_remainder_sql(has_upper_bound: bool) -> str:
    if has_upper_bound:
        return """
            SELECT text, comment, weight, user_weight
            FROM entries
            WHERE code >= ? AND code < ? AND code <> ?
            ORDER BY weight DESC, user_weight DESC, code ASC, id ASC
            LIMIT ?
        """
    return """
        SELECT text, comment, weight, user_weight
        FROM entries
        WHERE code >= ? AND code <> ?
        ORDER BY weight DESC, user_weight DESC, code ASC, id ASC
        LIMIT ?
    """


def query_once(
    connection: sqlite3.Connection,
    code: str,
    limit: int,
    prefix: bool,
) -> int:
    if not prefix:
        rows = connection.execute(exact_sql(), (code, limit)).fetchall()
        return len(rows)

    rows = connection.execute(exact_sql(), (code, limit)).fetchall()
    if len(rows) >= limit:
        return len(rows)

    upper_bound = next_prefix_upper_bound(code)
    if upper_bound is None:
        rows.extend(connection.execute(prefix_remainder_sql(False), (code, code, limit - len(rows))).fetchall())
    else:
        rows.extend(
            connection.execute(prefix_remainder_sql(True), (code, upper_bound, code, limit - len(rows))).fetchall()
        )
    return len(rows)


def query_plan_rows(
    connection: sqlite3.Connection,
    code: str,
    limit: int,
    prefix: bool,
) -> list[tuple[str, list[tuple[int, int, int, str]] | None]]:
    plans: list[tuple[str, list[tuple[int, int, int, str]] | None]] = [
        ("exact", connection.execute("EXPLAIN QUERY PLAN " + exact_sql(), (code, limit)).fetchall())
    ]
    if not prefix:
        return plans

    exact_rows = connection.execute(exact_sql(), (code, limit)).fetchall()
    if len(exact_rows) >= limit:
        return plans + [("prefix skipped; exact rows filled limit", None)]

    upper_bound = next_prefix_upper_bound(code)
    if upper_bound is None:
        plans.append(
            (
                "prefix",
                connection.execute("EXPLAIN QUERY PLAN " + prefix_remainder_sql(False), (code, code, limit)).fetchall(),
            )
        )
    else:
        plans.append(
            (
                "prefix",
                connection.execute(
                    "EXPLAIN QUERY PLAN " + prefix_remainder_sql(True),
                    (code, upper_bound, code, limit),
                ).fetchall(),
            )
        )
    return plans


def benchmark_code(
    connection: sqlite3.Connection,
    code: str,
    limit: int,
    iterations: int,
    prefix: bool,
) -> tuple[int, float, float, float]:
    query_once(connection, code, limit, prefix)

    durations: list[float] = []
    row_count = 0
    for _ in range(iterations):
        start = time.perf_counter()
        row_count = query_once(connection, code, limit, prefix)
        durations.append((time.perf_counter() - start) * 1000.0)

    return row_count, sum(durations) / len(durations), min(durations), max(durations)


def main() -> int:
    parser = argparse.ArgumentParser(description="Benchmark ArrowInput SQLite dictionary lookup.")
    parser.add_argument("--database", required=True, help="SQLite dictionary path")
    parser.add_argument("--limit", type=int, default=100, help="Candidate limit per lookup")
    parser.add_argument("--iterations", type=int, default=100, help="Number of timed iterations per code")
    parser.add_argument("--prefix", action="store_true", help="Benchmark prefix lookup instead of exact-code lookup")
    parser.add_argument("--explain", action="store_true", help="Print SQLite query plan for each code")
    parser.add_argument("code", nargs="+", help="Code(s) to benchmark")
    args = parser.parse_args()

    if args.limit < 1:
        raise ValueError("--limit must be at least 1")
    if args.iterations < 1:
        raise ValueError("--iterations must be at least 1")

    db_path = Path(args.database)
    if not db_path.exists():
        raise FileNotFoundError(f"Database was not found: {db_path}")

    with sqlite3.connect(db_path) as connection:
        write_text(f"Database: {db_path}\n")
        write_text(f"Lookup: {'prefix' if args.prefix else 'exact'}\n")
        write_text(f"Limit: {args.limit}\n")
        write_text(f"Iterations: {args.iterations}\n")
        for code in args.code:
            row_count, average_ms, best_ms, worst_ms = benchmark_code(
                connection,
                code,
                args.limit,
                args.iterations,
                args.prefix,
            )
            write_text(
                f"[{code}] rows={row_count} avg_ms={average_ms:.3f} best_ms={best_ms:.3f} worst_ms={worst_ms:.3f}\n"
            )
            if args.explain:
                write_text(f"[{code}] query_plan:\n")
                for phase, rows in query_plan_rows(connection, code, args.limit, args.prefix):
                    if rows is None:
                        write_text(f"  {phase}\n")
                    else:
                        write_text(f"  {phase}:\n")
                        for _, _, _, detail in rows:
                            write_text(f"    {detail}\n")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
