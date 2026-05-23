param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$testDir = Join-Path $root "build\tests\check_current_dictionary"
$dictionary = Join-Path $root "data\seed_pinyin.tsv"
$database = Join-Path $testDir "seed.db"
$config = Join-Path $testDir "config.ini"

& (Join-Path $PSScriptRoot "build_algorithm_probe.ps1") -Configuration $Configuration | Out-Null

if (Test-Path $testDir) {
    Remove-Item -Recurse -Force $testDir
}
New-Item -ItemType Directory -Force -Path $testDir | Out-Null

& (Join-Path $PSScriptRoot "import_tsv_to_sqlite.ps1") -InputPath $dictionary -OutputPath $database | Out-Null

@"
[general]
dictionary_type=sqlite
dictionary_path=$database
max_candidates_per_query=5
prefix_candidates=1
fuzzy_pinyin=1
"@ | Set-Content -Encoding UTF8 -Path $config

$output = & (Join-Path $PSScriptRoot "check_current_dictionary.ps1") -ConfigPath $config -Limit 3 -Iterations 1 ni nih
$actual = ($output -join "`n").TrimEnd()
if ($actual -notmatch "current dictionary check passed") {
    Write-Host "Actual dictionary check output:" -ForegroundColor Yellow
    Write-Host $actual
    throw "Current dictionary check did not complete."
}
if ($actual -notmatch "\[nih\] 1 candidate\(s\)") {
    Write-Host "Actual dictionary check output:" -ForegroundColor Yellow
    Write-Host $actual
    throw "Current dictionary check did not query through AlgorithmProbe."
}
if ($actual -notmatch "SQLite lookup benchmark") {
    Write-Host "Actual dictionary check output:" -ForegroundColor Yellow
    Write-Host $actual
    throw "Current dictionary check did not run SQLite benchmark."
}
if ($actual -notmatch "fuzzy_pinyin=1") {
    Write-Host "Actual dictionary check output:" -ForegroundColor Yellow
    Write-Host $actual
    throw "Current dictionary check did not show fuzzy pinyin setting."
}

$skipOutput = & (Join-Path $PSScriptRoot "check_current_dictionary.ps1") -ConfigPath $config -Limit 3 -SkipBenchmark ni
$skipActual = ($skipOutput -join "`n").TrimEnd()
if ($skipActual -match "SQLite lookup benchmark") {
    Write-Host "Actual skip benchmark output:" -ForegroundColor Yellow
    Write-Host $skipActual
    throw "Current dictionary check should skip benchmark when requested."
}

Write-Host "current dictionary check tests passed."
