import argparse
from collections import Counter
from pathlib import Path


def normalize_row(line: str, line_number: int, source: Path) -> tuple[str, str, str]:
    fields = line.rstrip("\r\n").split("\t")
    if len(fields) < 2 or not fields[0] or not fields[1]:
        raise ValueError(f"{source}:{line_number}: expected non-empty code and text fields")
    if len(fields) > 5:
        raise ValueError(f"{source}:{line_number}: extra fields after user_weight")
    if any(field != field.strip() for field in fields):
        raise ValueError(f"{source}:{line_number}: leading or trailing whitespace in a field")

    normalized = fields + [""] * (5 - len(fields))
    for index, field_name in ((3, "weight"), (4, "user_weight")):
        if normalized[index]:
            try:
                int(normalized[index])
            except ValueError as exc:
                raise ValueError(f"{source}:{line_number}: invalid {field_name} field") from exc

    return normalized[0], normalized[1], "\t".join(normalized)


def read_rows(path: Path) -> list[tuple[str, str, str]]:
    rows: list[tuple[str, str, str]] = []
    if not path.exists():
        return rows

    with path.open("r", encoding="utf-8-sig", newline="") as file:
        for line_number, raw_line in enumerate(file, start=1):
            line = raw_line.rstrip("\r\n")
            if not line or line.startswith("#"):
                continue
            rows.append(normalize_row(line, line_number, path))
    return rows


def print_counter(title: str, counter: Counter[str], limit: int) -> None:
    print(f"{title}:")
    if not counter:
        print("  none")
        return
    for code, count in counter.most_common(limit):
        print(f"  {code}: {count}")


def preview(target: Path, source: Path, top: int, hot_threshold: int) -> int:
    if not source.exists():
        raise FileNotFoundError(f"Source dictionary was not found: {source}")

    target_rows = read_rows(target)
    source_rows = read_rows(source)
    target_keys = {(code, text) for code, text, _row in target_rows}
    source_keys = Counter((code, text) for code, text, _row in source_rows)

    new_rows = [(code, text, row) for code, text, row in source_rows if (code, text) not in target_keys]
    duplicate_rows = [(code, text, row) for code, text, row in source_rows if (code, text) in target_keys]
    internal_duplicates = sum(count - 1 for count in source_keys.values() if count > 1)

    target_code_counts = Counter(code for code, _text, _row in target_rows)
    source_code_counts = Counter(code for code, _text, _row in source_rows)
    added_code_counts = Counter(code for code, _text, _row in new_rows)
    projected_code_counts = target_code_counts.copy()
    projected_code_counts.update(added_code_counts)
    hot_projected = Counter(
        {code: count for code, count in projected_code_counts.items() if count >= hot_threshold}
    )

    print(f"target: {target}")
    print(f"source: {source}")
    print(f"target candidate count: {len(target_rows)}")
    print(f"source candidate count: {len(source_rows)}")
    print(f"new candidate count: {len(new_rows)}")
    print(f"duplicate target candidate count: {len(duplicate_rows)}")
    print(f"source internal duplicate extra count: {internal_duplicates}")
    print(f"projected candidate count: {len(target_rows) + len(new_rows)}")
    print(f"hot code threshold: {hot_threshold}")
    print_counter("top source codes", source_code_counts, top)
    print_counter("top added codes", added_code_counts, top)
    print_counter("top projected hot codes", hot_projected, top)
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="Preview merging one ArrowInput TSV dictionary into another.")
    parser.add_argument("--target", required=True, help="Target TSV dictionary to compare against")
    parser.add_argument("--source", required=True, help="Source TSV dictionary to preview")
    parser.add_argument("--top", type=int, default=10, help="Number of top codes to print")
    parser.add_argument("--hot-threshold", type=int, default=50, help="Projected candidates per code threshold")
    args = parser.parse_args()

    return preview(Path(args.target), Path(args.source), args.top, args.hot_threshold)


if __name__ == "__main__":
    raise SystemExit(main())
