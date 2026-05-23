param()

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$dbPath = Join-Path $root "build\tests\query_sqlite\sample_dictionary.db"
$dictionary = Join-Path $root "tests\algorithm_probe_weighted_dictionary.tsv"

& (Join-Path $PSScriptRoot "import_tsv_to_sqlite.ps1") -InputPath $dictionary -OutputPath $dbPath | Out-Null

$output = & (Join-Path $PSScriptRoot "query_sqlite.ps1") -DatabasePath $dbPath ni
$actual = ($output -join "`n").TrimEnd()
$expected = @"
[ni] 3 candidate(s)
1. user high_user weight=10 user_weight=5
2. high high weight=10 user_weight=0
3. low low weight=1 user_weight=0
"@.TrimEnd()

if ($actual -ne $expected) {
    Write-Host "Expected query output:" -ForegroundColor Yellow
    Write-Host $expected
    Write-Host "Actual query output:" -ForegroundColor Yellow
    Write-Host $actual
    throw "query_sqlite output did not match expected output."
}

$positionOutput = & (Join-Path $PSScriptRoot "query_sqlite.ps1") ni -DatabasePath $dbPath
$positionActual = ($positionOutput -join "`n").TrimEnd()
if ($positionActual -ne $expected) {
    throw "query_sqlite positional code argument did not match expected output."
}

Write-Host "query_sqlite tests passed."
