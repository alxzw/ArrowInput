param()

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$testDir = Join-Path $root "build\tests\merge_tsv"
$target = Join-Path $testDir "target.tsv"
$source = Join-Path $root "tests\merge_source_dictionary.tsv"
$database = Join-Path $testDir "target.db"

if (Test-Path $testDir) {
    Remove-Item -Recurse -Force $testDir
}
New-Item -ItemType Directory -Force -Path $testDir | Out-Null
@(
    "# target"
    "ni`tni`tni`t1000`t0"
) | Set-Content -Path $target -Encoding UTF8

$output = & (Join-Path $PSScriptRoot "merge_tsv_dictionary.ps1") -TargetPath $target -SourcePath $source -DatabasePath $database
$text = ($output -join "`n")
if ($text -notmatch "Merged 2 row\(s\); skipped 1 duplicate row\(s\).") {
    Write-Host $text
    throw "Merge output was unexpected."
}

$query = & (Join-Path $PSScriptRoot "query_sqlite.ps1") ceshi xin -DatabasePath $database
$queryText = ($query -join "`n")
if ($queryText -notmatch "\[ceshi\] 1 candidate\(s\)" -or $queryText -notmatch "\[xin\] 1 candidate\(s\)") {
    Write-Host $queryText
    throw "Merged rows were not queryable."
}

Write-Host "merge_tsv_dictionary tests passed."
