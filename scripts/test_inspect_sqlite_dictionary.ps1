param()

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$dbPath = Join-Path $root "build\tests\inspect_sqlite\sample_dictionary.db"
$dictionary = Join-Path $root "tests\algorithm_probe_weighted_dictionary.tsv"

& (Join-Path $PSScriptRoot "import_tsv_to_sqlite.ps1") -InputPath $dictionary -OutputPath $dbPath | Out-Null

$output = & (Join-Path $PSScriptRoot "inspect_sqlite_dictionary.ps1") -DatabasePath $dbPath -Limit 2 -TopCodes 2 -Metadata ni missing
$actual = ($output -join "`n").TrimEnd()
$expected = @"
Database: $dbPath
Entries: 5
Codes: 2
Lookup: exact
Metadata:
  imported_at_utc=
  schema_version=1
  source_format=tsv
  source_path=
Top codes by candidate count:
  1. ni candidates=3 weight_range=1..10
  2. hao candidates=2 weight_range=0..0
[ni]
  total_candidates=3
  shown_candidates=2
  weight_range=1..10
  top:
    1. user high_user weight=10 user_weight=5
    2. high high weight=10 user_weight=0
[missing]
  total_candidates=0
  shown_candidates=0
  weight_range=<none>
"@.TrimEnd()

$normalizedActual = $actual `
    -replace "imported_at_utc=[^\r\n]*", "imported_at_utc=" `
    -replace "source_path=[^\r\n]*", "source_path="

if ($normalizedActual -ne $expected) {
    Write-Host "Expected inspect output:" -ForegroundColor Yellow
    Write-Host $expected
    Write-Host "Actual inspect output:" -ForegroundColor Yellow
    Write-Host $normalizedActual
    throw "inspect_sqlite_dictionary output did not match expected output."
}

$prefixOutput = & (Join-Path $PSScriptRoot "inspect_sqlite_dictionary.ps1") -DatabasePath $dbPath -Limit 3 -Prefix n
$prefixActual = ($prefixOutput -join "`n").TrimEnd()
$prefixExpected = @"
Database: $dbPath
Entries: 5
Codes: 2
Lookup: prefix
[n]
  total_candidates=3
  shown_candidates=3
  weight_range=1..10
  top:
    1. user high_user weight=10 user_weight=5
    2. high high weight=10 user_weight=0
    3. low low weight=1 user_weight=0
"@.TrimEnd()

if ($prefixActual -ne $prefixExpected) {
    Write-Host "Expected prefix inspect output:" -ForegroundColor Yellow
    Write-Host $prefixExpected
    Write-Host "Actual prefix inspect output:" -ForegroundColor Yellow
    Write-Host $prefixActual
    throw "inspect_sqlite_dictionary prefix output did not match expected output."
}

Write-Host "inspect_sqlite_dictionary tests passed."
