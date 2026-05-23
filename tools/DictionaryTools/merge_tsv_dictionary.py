import argparse
from pathlib import Path


HEADER = [
    "# ArrowInput TSV dictionary",
    "# UTF-8 TSV: code\ttext\tcomment\tweight\tuser_weight",
]


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


def merge(target: Path, source: Path, allow_duplicate: bool) -> tuple[int, int]:
    if not source.exists():
        raise FileNotFoundError(f"Source dictionary was not found: {source}")

    target_rows = read_rows(target)
    source_rows = read_rows(source)
    existing = {(code, text) for code, text, _row in target_rows}
    merged_rows = list(target_rows)
    added = 0
    skipped = 0

    for code, text, row in source_rows:
        if not allow_duplicate and (code, text) in existing:
            skipped += 1
            continue
        merged_rows.append((code, text, row))
        existing.add((code, text))
        added += 1

    target.parent.mkdir(parents=True, exist_ok=True)
    with target.open("w", encoding="utf-8", newline="\n") as file:
        for line in HEADER:
            file.write(line + "\n")
        for _code, _text, row in merged_rows:
            file.write(row + "\n")

    return added, skipped


def main() -> int:
    parser = argparse.ArgumentParser(description="Merge one ArrowInput TSV dictionary into another.")
    parser.add_argument("--target", required=True, help="Target TSV dictionary to update")
    parser.add_argument("--source", required=True, help="Source TSV dictionary to merge from")
    parser.add_argument("--allow-duplicate", action="store_true", help="Keep duplicate code+text rows")
    args = parser.parse_args()

    added, skipped = merge(Path(args.target), Path(args.source), args.allow_duplicate)
    print(f"Merged {added} row(s); skipped {skipped} duplicate row(s).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
