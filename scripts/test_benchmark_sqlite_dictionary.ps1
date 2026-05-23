param()

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$dbPath = Join-Path $root "build\tests\benchmark_sqlite\sample_dictionary.db"
$dictionary = Join-Path $root "tests\algorithm_probe_weighted_dictionary.tsv"

& (Join-Path $PSScriptRoot "import_tsv_to_sqlite.ps1") -InputPath $dictionary -OutputPath $dbPath | Out-Null

$output = & (Join-Path $PSScriptRoot "benchmark_sqlite_dictionary.ps1") -DatabasePath $dbPath -Limit 2 -Iterations 2 ni
$text = ($output -join "`n")

if ($text -notmatch [regex]::Escape("Database: $dbPath")) {
    throw "benchmark output did not include database path."
}
if ($text -notmatch "Lookup: exact") {
    throw "benchmark output did not include exact lookup mode."
}
if ($text -notmatch "\[ni\] rows=2 avg_ms=[0-9]+\.[0-9]{3} best_ms=[0-9]+\.[0-9]{3} worst_ms=[0-9]+\.[0-9]{3}") {
    throw "benchmark output did not include expected ni timing row."
}

$prefixOutput = & (Join-Path $PSScriptRoot "benchmark_sqlite_dictionary.ps1") -DatabasePath $dbPath -Limit 5 -Iterations 2 -Prefix n
$prefixText = ($prefixOutput -join "`n")

if ($prefixText -notmatch "Lookup: prefix") {
    throw "benchmark output did not include prefix lookup mode."
}
if ($prefixText -notmatch "\[n\] rows=3 avg_ms=[0-9]+\.[0-9]{3} best_ms=[0-9]+\.[0-9]{3} worst_ms=[0-9]+\.[0-9]{3}") {
    throw "benchmark output did not include expected prefix timing row."
}

$explainOutput = & (Join-Path $PSScriptRoot "benchmark_sqlite_dictionary.ps1") -DatabasePath $dbPath -Limit 2 -Iterations 1 -Explain ni
$explainText = ($explainOutput -join "`n")

if ($explainText -notmatch "\[ni\] query_plan:" -or $explainText -notmatch "entries") {
    throw "benchmark explain output did not include expected query plan."
}

Write-Host "benchmark_sqlite_dictionary tests passed."
