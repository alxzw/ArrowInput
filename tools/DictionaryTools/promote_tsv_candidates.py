import argparse
from pathlib import Path


HEADER = [
    "# ArrowInput TSV dictionary",
    "# UTF-8 TSV: code\ttext\tcomment\tweight\tuser_weight",
]


def parse_weight(value: str) -> int:
    if value == "":
        return 0
    return int(value)


def read_rows(path: Path) -> list[list[str]]:
    rows: list[list[str]] = []
    with path.open("r", encoding="utf-8-sig", newline="") as file:
        for line_number, raw_line in enumerate(file, start=1):
            line = raw_line.rstrip("\r\n")
            if not line or line.startswith("#"):
                continue
            fields = line.split("\t")
            if len(fields) < 2 or not fields[0] or not fields[1]:
                raise ValueError(f"{path}:{line_number}: expected non-empty code and text fields")
            if len(fields) > 5:
                raise ValueError(f"{path}:{line_number}: extra fields after user_weight")
            normalized = fields + [""] * (5 - len(fields))
            parse_weight(normalized[3])
            parse_weight(normalized[4])
            rows.append(normalized)
    return rows


def promote(target: Path, source: Path, boost: int) -> int:
    if not target.exists():
        raise FileNotFoundError(f"Target dictionary was not found: {target}")
    if not source.exists():
        raise FileNotFoundError(f"Source dictionary was not found: {source}")
    if boost < 0:
        raise ValueError("Boost must be non-negative")

    target_rows = read_rows(target)
    source_keys = {(row[0], row[1]) for row in read_rows(source)}
    max_weight_by_code: dict[str, int] = {}
    for row in target_rows:
        code = row[0]
        weight = parse_weight(row[3])
        max_weight_by_code[code] = max(max_weight_by_code.get(code, 0), weight)

    promoted = 0
    for row in target_rows:
        code = row[0]
        text = row[1]
        if (code, text) not in source_keys:
            continue
        desired = max_weight_by_code.get(code, 0) + boost
        current = parse_weight(row[3])
        if current < desired:
            row[3] = str(desired)
            promoted += 1

    with target.open("w", encoding="utf-8", newline="\n") as file:
        for line in HEADER:
            file.write(line + "\n")
        for row in target_rows:
            file.write("\t".join(row) + "\n")

    return promoted


def main() -> int:
    parser = argparse.ArgumentParser(description="Promote selected ArrowInput TSV candidates above peers with the same code.")
    parser.add_argument("--target", required=True, help="Target TSV dictionary to update")
    parser.add_argument("--source", required=True, help="Source TSV whose code+text rows should be promoted")
    parser.add_argument("--boost", type=int, default=1000, help="Weight added above each code's current maximum")
    args = parser.parse_args()

    promoted = promote(Path(args.target), Path(args.source), args.boost)
    print(f"Promoted {promoted} candidate(s).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
