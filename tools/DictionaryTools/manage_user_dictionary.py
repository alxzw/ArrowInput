import argparse
import csv
import sqlite3
import sys
from pathlib import Path


SCHEMA = """
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

CREATE TABLE IF NOT EXISTS user_learning_events (
    id INTEGER PRIMARY KEY,
    code TEXT NOT NULL,
    text TEXT NOT NULL,
    comment TEXT NOT NULL DEFAULT '',
    delta_user_weight INTEGER NOT NULL,
    created_at_utc TEXT NOT NULL DEFAULT (datetime('now')),
    reverted_at_utc TEXT
);

CREATE INDEX IF NOT EXISTS idx_user_learning_events_active
ON user_learning_events(reverted_at_utc, id DESC);
"""


def write_text(text: str) -> None:
    sys.stdout.buffer.write(text.encode("utf-8"))
    sys.stdout.buffer.flush()


def open_database(path: Path) -> sqlite3.Connection:
    if not path.exists():
        raise FileNotFoundError(f"Database was not found: {path}")
    connection = sqlite3.connect(path)
    connection.executescript(SCHEMA)
    return connection


def add_entry(args: argparse.Namespace) -> int:
    with open_database(Path(args.database)) as connection:
        connection.execute(
            """
            INSERT INTO user_entries(code, text, comment, weight, user_weight, deleted, updated_at_utc)
            VALUES (?, ?, ?, ?, ?, 0, datetime('now'))
            ON CONFLICT(code, text) DO UPDATE SET
                comment = excluded.comment,
                weight = excluded.weight,
                user_weight = excluded.user_weight,
                deleted = 0,
                updated_at_utc = excluded.updated_at_utc
            """,
            (args.code, args.text, args.comment or "", args.weight, args.user_weight),
        )
    write_text(f"added user entry: {args.code}\t{args.text}\n")
    return 0


def delete_entry(args: argparse.Namespace) -> int:
    with open_database(Path(args.database)) as connection:
        connection.execute(
            """
            INSERT INTO user_entries(code, text, comment, weight, user_weight, deleted, updated_at_utc)
            VALUES (?, ?, '', 0, 0, 1, datetime('now'))
            ON CONFLICT(code, text) DO UPDATE SET
                deleted = 1,
                updated_at_utc = excluded.updated_at_utc
            """,
            (args.code, args.text),
        )
    write_text(f"deleted user entry: {args.code}\t{args.text}\n")
    return 0


def restore_entry(args: argparse.Namespace) -> int:
    with open_database(Path(args.database)) as connection:
        cursor = connection.execute(
            """
            UPDATE user_entries
            SET deleted = 0,
                updated_at_utc = datetime('now')
            WHERE code = ? AND text = ? AND deleted = 1
            """,
            (args.code, args.text),
        )
        if cursor.rowcount == 0:
            write_text(f"no deleted user entry to restore: {args.code}\t{args.text}\n")
            return 0
    write_text(f"restored user entry: {args.code}\t{args.text}\n")
    return 0


def list_entries(args: argparse.Namespace) -> int:
    with open_database(Path(args.database)) as connection:
        rows = connection.execute(
            """
            SELECT code, text, comment, weight, user_weight, deleted, updated_at_utc
            FROM user_entries
            WHERE (? OR deleted = 0)
            ORDER BY code ASC, user_weight DESC, weight DESC, id ASC
            LIMIT ?
            """,
            (1 if args.include_deleted else 0, args.limit),
        ).fetchall()
    for row in rows:
        write_text("\t".join(str(value) for value in row) + "\n")
    return 0


def deleted_entries(args: argparse.Namespace) -> int:
    with open_database(Path(args.database)) as connection:
        rows = connection.execute(
            """
            SELECT code, text, comment, weight, user_weight, deleted, updated_at_utc
            FROM user_entries
            WHERE deleted = 1
            ORDER BY updated_at_utc DESC, id DESC
            LIMIT ?
            """,
            (args.limit,),
        ).fetchall()
    for row in rows:
        write_text("\t".join(str(value) for value in row) + "\n")
    return 0


def undo_delete(args: argparse.Namespace) -> int:
    with open_database(Path(args.database)) as connection:
        row = connection.execute(
            """
            SELECT code, text
            FROM user_entries
            WHERE deleted = 1
            ORDER BY updated_at_utc DESC, id DESC
            LIMIT 1
            """
        ).fetchone()
        if not row:
            write_text("no deleted user entry to restore\n")
            return 0

        code, text = row
        connection.execute(
            """
            UPDATE user_entries
            SET deleted = 0,
                updated_at_utc = datetime('now')
            WHERE code = ? AND text = ?
            """,
            (code, text),
        )
    write_text(f"restored latest deleted user entry: {code}\t{text}\n")
    return 0


def user_stats(args: argparse.Namespace) -> int:
    with open_database(Path(args.database)) as connection:
        active_entries, deleted_entries, user_weight_total = connection.execute(
            """
            SELECT
                COALESCE(SUM(CASE WHEN deleted = 0 THEN 1 ELSE 0 END), 0),
                COALESCE(SUM(CASE WHEN deleted = 1 THEN 1 ELSE 0 END), 0),
                COALESCE(SUM(CASE WHEN deleted = 0 THEN user_weight ELSE 0 END), 0)
            FROM user_entries
            """
        ).fetchone()
        learning_events, reverted_learning_events = connection.execute(
            """
            SELECT
                COUNT(*),
                COALESCE(SUM(CASE WHEN reverted_at_utc IS NOT NULL THEN 1 ELSE 0 END), 0)
            FROM user_learning_events
            """
        ).fetchone()
        top_rows = connection.execute(
            """
            SELECT code, text, user_weight
            FROM user_entries
            WHERE deleted = 0 AND user_weight > 0
            ORDER BY user_weight DESC, updated_at_utc DESC, id DESC
            LIMIT ?
            """,
            (args.top,),
        ).fetchall()

    write_text(f"active_user_entries={active_entries}\n")
    write_text(f"deleted_user_entries={deleted_entries}\n")
    write_text(f"user_weight_total={user_weight_total}\n")
    write_text(f"learning_events={learning_events}\n")
    write_text(f"reverted_learning_events={reverted_learning_events}\n")
    for index, (code, text, user_weight) in enumerate(top_rows, start=1):
        write_text(f"top_user_weight_{index}={code}\t{text}\t{user_weight}\n")
    return 0


def export_entries(args: argparse.Namespace) -> int:
    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with open_database(Path(args.database)) as connection, output_path.open("w", encoding="utf-8", newline="") as file:
        writer = csv.writer(file, delimiter="\t", lineterminator="\n")
        writer.writerow(["code", "text", "comment", "weight", "user_weight", "deleted"])
        rows = connection.execute(
            """
            SELECT code, text, comment, weight, user_weight, deleted
            FROM user_entries
            WHERE (? OR deleted = 0)
            ORDER BY code ASC, user_weight DESC, weight DESC, id ASC
            """,
            (1 if args.include_deleted else 0,),
        )
        count = 0
        for row in rows:
            writer.writerow(row)
            count += 1
    write_text(f"exported {count} user entr(y/ies): {output_path}\n")
    return 0


def import_entries(args: argparse.Namespace) -> int:
    input_path = Path(args.input)
    if not input_path.exists():
        raise FileNotFoundError(f"Input file was not found: {input_path}")

    count = 0
    with open_database(Path(args.database)) as connection, input_path.open("r", encoding="utf-8-sig", newline="") as file:
        reader = csv.DictReader(file, delimiter="\t")
        required = {"code", "text"}
        if not reader.fieldnames or not required.issubset(set(reader.fieldnames)):
            raise ValueError("Import TSV must contain at least code and text columns.")
        for row in reader:
            code = (row.get("code") or "").strip()
            text = (row.get("text") or "").strip()
            if not code or not text:
                continue
            comment = row.get("comment") or ""
            weight = int(row.get("weight") or 0)
            user_weight = int(row.get("user_weight") or 0)
            deleted = int(row.get("deleted") or 0)
            connection.execute(
                """
                INSERT INTO user_entries(code, text, comment, weight, user_weight, deleted, updated_at_utc)
                VALUES (?, ?, ?, ?, ?, ?, datetime('now'))
                ON CONFLICT(code, text) DO UPDATE SET
                    comment = excluded.comment,
                    weight = excluded.weight,
                    user_weight = excluded.user_weight,
                    deleted = excluded.deleted,
                    updated_at_utc = excluded.updated_at_utc
                """,
                (code, text, comment, weight, user_weight, deleted),
            )
            count += 1
    write_text(f"imported {count} user entr(y/ies): {input_path}\n")
    return 0


def learning_history(args: argparse.Namespace) -> int:
    with open_database(Path(args.database)) as connection:
        rows = connection.execute(
            """
            SELECT id, code, text, comment, delta_user_weight, created_at_utc, reverted_at_utc
            FROM user_learning_events
            WHERE (? OR reverted_at_utc IS NULL)
            ORDER BY id DESC
            LIMIT ?
            """,
            (1 if args.include_reverted else 0, args.limit),
        ).fetchall()
    for row in rows:
        write_text("\t".join("" if value is None else str(value) for value in row) + "\n")
    return 0


def undo_learning(args: argparse.Namespace) -> int:
    with open_database(Path(args.database)) as connection:
        row = connection.execute(
            """
            SELECT id, code, text, delta_user_weight
            FROM user_learning_events
            WHERE reverted_at_utc IS NULL
            ORDER BY id DESC
            LIMIT 1
            """
        ).fetchone()
        if not row:
            write_text("no learning event to undo\n")
            return 0

        event_id, code, text, delta_user_weight = row
        connection.execute(
            """
            UPDATE user_entries
            SET user_weight = CASE
                    WHEN user_weight > ? THEN user_weight - ?
                    ELSE 0
                END,
                updated_at_utc = datetime('now')
            WHERE code = ? AND text = ?
            """,
            (delta_user_weight, delta_user_weight, code, text),
        )
        connection.execute(
            """
            UPDATE user_learning_events
            SET reverted_at_utc = datetime('now')
            WHERE id = ?
            """,
            (event_id,),
        )
    write_text(f"undid learning event: {event_id}\t{code}\t{text}\n")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="Manage ArrowInput SQLite user entries.")
    parser.add_argument("--database", required=True, help="SQLite dictionary path")
    subparsers = parser.add_subparsers(dest="command", required=True)

    add = subparsers.add_parser("add")
    add.add_argument("--code", required=True)
    add.add_argument("--text", required=True)
    add.add_argument("--comment", default="")
    add.add_argument("--weight", type=int, default=0)
    add.add_argument("--user-weight", type=int, default=1000)
    add.set_defaults(func=add_entry)

    delete = subparsers.add_parser("delete")
    delete.add_argument("--code", required=True)
    delete.add_argument("--text", required=True)
    delete.set_defaults(func=delete_entry)

    restore = subparsers.add_parser("restore")
    restore.add_argument("--code", required=True)
    restore.add_argument("--text", required=True)
    restore.set_defaults(func=restore_entry)

    list_cmd = subparsers.add_parser("list")
    list_cmd.add_argument("--limit", type=int, default=100)
    list_cmd.add_argument("--include-deleted", action="store_true")
    list_cmd.set_defaults(func=list_entries)

    deleted = subparsers.add_parser("deleted")
    deleted.add_argument("--limit", type=int, default=20)
    deleted.set_defaults(func=deleted_entries)

    undo_delete_cmd = subparsers.add_parser("undo-delete")
    undo_delete_cmd.set_defaults(func=undo_delete)

    stats = subparsers.add_parser("user-stats")
    stats.add_argument("--top", type=int, default=5)
    stats.set_defaults(func=user_stats)

    export = subparsers.add_parser("export")
    export.add_argument("--output", required=True)
    export.add_argument("--include-deleted", action="store_true")
    export.set_defaults(func=export_entries)

    import_cmd = subparsers.add_parser("import")
    import_cmd.add_argument("--input", required=True)
    import_cmd.set_defaults(func=import_entries)

    history = subparsers.add_parser("learning-history")
    history.add_argument("--limit", type=int, default=20)
    history.add_argument("--include-reverted", action="store_true")
    history.set_defaults(func=learning_history)

    undo = subparsers.add_parser("undo-learning")
    undo.set_defaults(func=undo_learning)

    args = parser.parse_args()
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main())
