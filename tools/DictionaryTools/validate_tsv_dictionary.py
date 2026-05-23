import argparse
from collections import Counter, defaultdict
from pathlib import Path
import re


CODE_PATTERN = re.compile(r"^[a-z']+$")


def parse_int(value: str, line_number: int, field_name: str, issues: list[str]) -> int:
    if value == "":
        return 0
    try:
        return int(value)
    except ValueError:
        issues.append(f"line {line_number}: invalid {field_name} field")
        return 0


def validate(
    path: Path,
    max_issues: int,
    max_code_length: int,
    max_text_length: int,
    max_comment_length: int,
    min_weight: int,
    max_weight: int,
    min_user_weight: int,
    max_user_weight: int,
) -> int:
    if not path.exists():
        raise FileNotFoundError(f"Dictionary file was not found: {path}")

    issues: list[str] = []
    code_counts: Counter[str] = Counter()
    exact_counts: Counter[tuple[str, str]] = Counter()
    weight_by_code: defaultdict[str, list[tuple[int, int]]] = defaultdict(list)
    code_lengths: Counter[int] = Counter()
    text_lengths: Counter[int] = Counter()
    weight_values: list[int] = []
    user_weight_values: list[int] = []
    data_lines = 0
    skipped_lines = 0

    with path.open("r", encoding="utf-8-sig", newline="") as file:
        for line_number, raw_line in enumerate(file, start=1):
            line = raw_line.rstrip("\r\n")
            if not line or line.startswith("#"):
                skipped_lines += 1
                continue

            data_lines += 1
            fields = line.split("\t")
            if len(fields) < 2 or not fields[0] or not fields[1]:
                issues.append(f"line {line_number}: expected non-empty code and text fields")
                continue

            code = fields[0]
            text = fields[1]
            comment = fields[2] if len(fields) >= 3 else ""
            weight = parse_int(fields[3], line_number, "weight", issues) if len(fields) >= 4 else 0
            user_weight = parse_int(fields[4], line_number, "user_weight", issues) if len(fields) >= 5 else 0

            code_counts[code] += 1
            exact_counts[(code, text)] += 1
            weight_by_code[code].append((weight, user_weight))
            code_lengths[len(code)] += 1
            text_lengths[len(text)] += 1
            weight_values.append(weight)
            user_weight_values.append(user_weight)

            if len(fields) > 5:
                issues.append(f"line {line_number}: extra fields after user_weight")
            if any(field != field.strip() for field in fields[:5]):
                issues.append(f"line {line_number}: leading or trailing whitespace in a field")
            if not CODE_PATTERN.match(code):
                issues.append(f"line {line_number}: code must contain only lowercase letters and apostrophes")
            if len(code) > max_code_length:
                issues.append(f"line {line_number}: code length exceeds {max_code_length}")
            if len(text) > max_text_length:
                issues.append(f"line {line_number}: text length exceeds {max_text_length}")
            if len(comment) > max_comment_length:
                issues.append(f"line {line_number}: comment length exceeds {max_comment_length}")
            if weight < min_weight or weight > max_weight:
                issues.append(f"line {line_number}: weight is outside [{min_weight}, {max_weight}]")
            if user_weight < min_user_weight or user_weight > max_user_weight:
                issues.append(f"line {line_number}: user_weight is outside [{min_user_weight}, {max_user_weight}]")

    duplicate_exact = [(code, text, count) for (code, text), count in exact_counts.items() if count > 1]
    for code, _text, count in duplicate_exact[:max_issues]:
        issues.append(f"duplicate exact candidate: code={code} count={count}")

    print(f"path: {path}")
    print(f"data line count: {data_lines}")
    print(f"skipped line count: {skipped_lines}")
    print(f"code count: {len(code_counts)}")
    print(f"candidate count: {sum(code_counts.values())}")
    print(f"duplicate exact candidate count: {len(duplicate_exact)}")
    print(f"max candidates per code: {max(code_counts.values(), default=0)}")
    print(f"max code length: {max(code_lengths, default=0)}")
    print(f"max text length: {max(text_lengths, default=0)}")
    print(f"weight range: {min(weight_values, default=0)}..{max(weight_values, default=0)}")
    print(f"user_weight range: {min(user_weight_values, default=0)}..{max(user_weight_values, default=0)}")

    print("top codes:")
    for code, count in code_counts.most_common(10):
        print(f"  {code}: {count}")

    if issues:
        print("issues:")
        for issue in issues[:max_issues]:
            print(f"  {issue}")
        if len(issues) > max_issues:
            print(f"  ... {len(issues) - max_issues} more issue(s)")
        return 1

    print("issues: none")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate an ArrowInput UTF-8 TSV dictionary.")
    parser.add_argument("--input", required=True, help="Input TSV dictionary path")
    parser.add_argument("--max-issues", type=int, default=20, help="Maximum issue lines to print")
    parser.add_argument("--max-code-length", type=int, default=128, help="Maximum accepted code length")
    parser.add_argument("--max-text-length", type=int, default=64, help="Maximum accepted text length")
    parser.add_argument("--max-comment-length", type=int, default=128, help="Maximum accepted comment length")
    parser.add_argument("--min-weight", type=int, default=-1000000000, help="Minimum accepted weight")
    parser.add_argument("--max-weight", type=int, default=1000000000, help="Maximum accepted weight")
    parser.add_argument("--min-user-weight", type=int, default=-1000000000, help="Minimum accepted user_weight")
    parser.add_argument("--max-user-weight", type=int, default=1000000000, help="Maximum accepted user_weight")
    args = parser.parse_args()
    return validate(
        Path(args.input),
        args.max_issues,
        args.max_code_length,
        args.max_text_length,
        args.max_comment_length,
        args.min_weight,
        args.max_weight,
        args.min_user_weight,
        args.max_user_weight,
    )


if __name__ == "__main__":
    raise SystemExit(main())
