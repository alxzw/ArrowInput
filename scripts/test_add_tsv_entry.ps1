param()

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$testDir = Join-Path $root "build\tests\add_tsv_entry"
$dictionary = Join-Path $testDir "dictionary.tsv"
$database = Join-Path $testDir "dictionary.db"

if (Test-Path $testDir) {
    Remove-Item -Recurse -Force $testDir
}
New-Item -ItemType Directory -Force -Path $testDir | Out-Null

& (Join-Path $PSScriptRoot "add_tsv_entry.ps1") -DictionaryPath $dictionary -Code ceshi -Text "测试" -Comment "ce shi" -Weight 100 -DatabasePath $database | Out-Null

$output = & (Join-Path $PSScriptRoot "query_sqlite.ps1") ceshi -DatabasePath $database
$actual = ($output -join "`n")
if ($actual -notmatch "\[ceshi\] 1 candidate\(s\)" -or $actual -notmatch "ce shi weight=100 user_weight=0") {
    Write-Host $actual
    throw "Added TSV entry was not queryable from SQLite."
}

$duplicateFailed = $false
try {
    & (Join-Path $PSScriptRoot "add_tsv_entry.ps1") -DictionaryPath $dictionary -Code ceshi -Text "测试" | Out-Null
} catch {
    $duplicateFailed = $true
}
if (!$duplicateFailed) {
    throw "Duplicate TSV entry should be rejected by default."
}

Write-Host "add_tsv_entry tests passed."
