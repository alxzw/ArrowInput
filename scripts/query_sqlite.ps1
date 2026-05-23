param(
    [Parameter(Position = 0, ValueFromRemainingArguments = $true)]
    [string[]]$Code = @("ni"),

    [string]$DatabasePath = "D:\MyApp\ArrowInput\sample_dictionary.db"
)

$ErrorActionPreference = "Stop"

if (!(Test-Path $DatabasePath)) {
    throw "SQLite dictionary was not found: $DatabasePath"
}

$queryScript = @"
import sqlite3
import sys
from pathlib import Path

def write_text(text):
    sys.stdout.buffer.write(text.encode("utf-8"))
    sys.stdout.buffer.flush()

db_path = Path(sys.argv[1])
codes = sys.argv[2:] or ["ni"]

with sqlite3.connect(db_path) as connection:
    for code in codes:
        rows = connection.execute(
            '''
            SELECT text, comment, weight, user_weight
            FROM entries
            WHERE code = ?
            ORDER BY weight DESC, user_weight DESC, id ASC
            ''',
            (code,),
        ).fetchall()
        write_text(f"[{code}] {len(rows)} candidate(s)\n")
        for index, (text, comment, weight, user_weight) in enumerate(rows, start=1):
            suffix = f" {comment}" if comment else ""
            write_text(f"{index}. {text}{suffix} weight={weight} user_weight={user_weight}\n")
"@

$tempScript = Join-Path $env:TEMP ("arrowinput_query_sqlite_{0}.py" -f ([Guid]::NewGuid().ToString("N")))
try {
    Set-Content -Path $tempScript -Value $queryScript -Encoding UTF8
    & python $tempScript $DatabasePath @Code
    if ($LASTEXITCODE -ne 0) {
        throw "SQLite query failed with exit code $LASTEXITCODE."
    }
} finally {
    if (Test-Path $tempScript) {
        Remove-Item -Force $tempScript -ErrorAction SilentlyContinue
    }
}
