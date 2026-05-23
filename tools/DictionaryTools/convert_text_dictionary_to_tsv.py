import argparse
from pathlib import Path
import re


HEADER = [
    "# ArrowInput TSV dictionary",
    "# UTF-8 TSV: code\ttext\tcomment\tweight\tuser_weight",
]
FIELD_NAMES = {"code", "text", "comment", "weight", "user_weight", "ignore"}


def split_line(line: str, delimiter: str) -> list[str]:
    if delimiter == "tab":
        return line.split("\t")
    if delimiter == "space":
        return re.split(r"\s+", line.strip())
    if delimiter == "auto":
        if "\t" in line:
            return line.split("\t")
        return re.split(r"\s+", line.strip())
    raise ValueError(f"Unsupported delimiter: {delimiter}")


def parse_columns(value: str) -> list[str]:
    columns = [part.strip() for part in value.split(",") if part.strip()]
    if not columns:
        raise ValueError("Columns must not be empty")
    unknown = [column for column in columns if column not in FIELD_NAMES]
    if unknown:
        raise ValueError(f"Unknown column name(s): {', '.join(unknown)}")
    if "code" not in columns or "text" not in columns:
        raise ValueError("Columns must include code and text")
    return columns


def parse_int(value: str, line_number: int, field_name: str) -> str:
    if value == "":
        return ""
    try:
        int(value)
    except ValueError as exc:
        raise ValueError(f"line {line_number}: invalid {field_name} field") from exc
    return value


def normalize_rime_code(value: str) -> str:
    return value.replace(" ", "").lower()


def normalize_rime_weight(value: str, line_number: int) -> str:
    if value == "":
        return ""
    if value.endswith("%"):
        percent = value[:-1]
        try:
            return str(round(float(percent) * 100))
        except ValueError as exc:
            raise ValueError(f"line {line_number}: invalid Rime percent weight") from exc
    return parse_int(value, line_number, "weight")


def convert_simple(
    input_path: Path,
    columns: list[str],
    delimiter: str,
    default_weight: str,
) -> tuple[list[str], int]:
    if not input_path.exists():
        raise FileNotFoundError(f"Input dictionary was not found: {input_path}")

    parse_int(default_weight, 0, "default_weight")
    output_rows: list[str] = []
    skipped = 0

    with input_path.open("r", encoding="utf-8-sig", newline="") as file:
        for line_number, raw_line in enumerate(file, start=1):
            line = raw_line.rstrip("\r\n")
            if not line or line.startswith("#"):
                skipped += 1
                continue

            parts = split_line(line, delimiter)
            if len(parts) < len(columns):
                raise ValueError(f"line {line_number}: expected at least {len(columns)} field(s)")

            row = {
                "code": "",
                "text": "",
                "comment": "",
                "weight": default_weight,
                "user_weight": "",
            }
            for index, column in enumerate(columns):
                if column == "ignore":
                    continue
                row[column] = parts[index]

            if not row["code"] or not row["text"]:
                raise ValueError(f"line {line_number}: expected non-empty code and text fields")
            if any(value != value.strip() for value in row.values()):
                raise ValueError(f"line {line_number}: leading or trailing whitespace in a mapped field")
            parse_int(row["weight"], line_number, "weight")
            parse_int(row["user_weight"], line_number, "user_weight")

            output_rows.append(
                "\t".join([row["code"], row["text"], row["comment"], row["weight"], row["user_weight"]])
            )

    return output_rows, skipped


def convert_rime(input_path: Path) -> tuple[list[str], int]:
    if not input_path.exists():
        raise FileNotFoundError(f"Input dictionary was not found: {input_path}")

    output_rows: list[str] = []
    skipped = 0
    in_entries = False

    with input_path.open("r", encoding="utf-8-sig", newline="") as file:
        for line_number, raw_line in enumerate(file, start=1):
            line = raw_line.rstrip("\r\n")
            stripped = line.strip()
            if not stripped or stripped.startswith("#"):
                skipped += 1
                continue
            if not in_entries:
                if stripped == "...":
                    in_entries = True
                skipped += 1
                continue

            fields = line.split("\t")
            if len(fields) < 2 or not fields[0] or not fields[1]:
                raise ValueError(f"line {line_number}: expected Rime text and code fields")

            text = fields[0].strip()
            rime_code = fields[1].strip()
            code = normalize_rime_code(rime_code)
            comment = rime_code
            weight = fields[2].strip() if len(fields) >= 3 else ""
            if len(fields) > 3:
                raise ValueError(f"line {line_number}: extra fields after Rime weight")
            if not text or not code:
                raise ValueError(f"line {line_number}: expected non-empty text and code fields")
            weight = normalize_rime_weight(weight, line_number)
            output_rows.append("\t".join([code, text, comment, weight, ""]))

    return output_rows, skipped


def write_tsv(output_path: Path, output_rows: list[str]) -> None:
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with output_path.open("w", encoding="utf-8", newline="\n") as file:
        for line in HEADER:
            file.write(line + "\n")
        for row in output_rows:
            file.write(row + "\n")


def main() -> int:
    parser = argparse.ArgumentParser(description="Convert a simple text dictionary to ArrowInput TSV.")
    parser.add_argument("--input", required=True, help="Input text dictionary path")
    parser.add_argument("--output", required=True, help="Output ArrowInput TSV path")
    parser.add_argument(
        "--source-format",
        choices=("simple", "rime"),
        default="simple",
        help="Input dictionary format",
    )
    parser.add_argument(
        "--columns",
        default="code,text,comment,weight,user_weight",
        help="Comma-separated input columns: code,text,comment,weight,user_weight,ignore",
    )
    parser.add_argument(
        "--delimiter",
        choices=("auto", "tab", "space"),
        default="auto",
        help="Input field delimiter",
    )
    parser.add_argument("--default-weight", default="", help="Weight used when the mapped input has no weight column")
    args = parser.parse_args()

    input_path = Path(args.input)
    output_path = Path(args.output)
    if args.source_format == "rime":
        output_rows, skipped = convert_rime(input_path)
    else:
        output_rows, skipped = convert_simple(input_path, parse_columns(args.columns), args.delimiter, args.default_weight)

    write_tsv(output_path, output_rows)
    print(f"Converted {len(output_rows)} row(s); skipped {skipped} comment/blank/header row(s).")
    print(f"Output: {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
