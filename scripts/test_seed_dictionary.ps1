param()

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$seedTsv = Join-Path $root "data\seed_pinyin.tsv"
$seedDb = Join-Path $root "build\tests\seed_pinyin.db"

if (!(Test-Path $seedTsv)) {
    throw "Seed dictionary was not found: $seedTsv"
}

& (Join-Path $PSScriptRoot "import_tsv_to_sqlite.ps1") -InputPath $seedTsv -OutputPath $seedDb | Out-Null

$output = & (Join-Path $PSScriptRoot "query_sqlite.ps1") nihao shurufa -DatabasePath $seedDb
$actual = ($output -join "`n").TrimEnd()

if ($actual -notmatch "\[nihao\] 1 candidate\(s\)" -or
    $actual -notmatch "ni hao weight=2000 user_weight=0" -or
    $actual -notmatch "\[shurufa\] 1 candidate\(s\)" -or
    $actual -notmatch "shu ru fa weight=1500 user_weight=0") {
    Write-Host "Actual seed output:" -ForegroundColor Yellow
    Write-Host $actual
    throw "Seed dictionary output did not match expected output."
}

Write-Host "seed dictionary tests passed."
