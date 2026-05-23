param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$testDir = Join-Path $root "build\tests\probe_current_dictionary"
$dictionary = Join-Path $root "tests\algorithm_probe_weighted_dictionary.tsv"
$spacedDictionary = Join-Path $root "tests\algorithm_probe_spaced_dictionary.tsv"
$database = Join-Path $testDir "weighted.db"
$spacedDatabase = Join-Path $testDir "spaced.db"
$config = Join-Path $testDir "config.ini"

& (Join-Path $PSScriptRoot "build_algorithm_probe.ps1") -Configuration $Configuration | Out-Null

if (Test-Path $testDir) {
    Remove-Item -Recurse -Force $testDir
}
New-Item -ItemType Directory -Force -Path $testDir | Out-Null

& (Join-Path $PSScriptRoot "import_tsv_to_sqlite.ps1") -InputPath $dictionary -OutputPath $database | Out-Null
& (Join-Path $PSScriptRoot "import_tsv_to_sqlite.ps1") -InputPath $spacedDictionary -OutputPath $spacedDatabase | Out-Null

@"
[general]
dictionary_type=sqlite
dictionary_path=$database
max_candidates_per_query=2
prefix_candidates=1
fuzzy_pinyin=0
"@ | Set-Content -Encoding UTF8 -Path $config

$output = & (Join-Path $PSScriptRoot "probe_current_dictionary.ps1") -ConfigPath $config ni
$actual = ($output -join "`n").TrimEnd()
if ($actual -notmatch "^\[ni\] 2 candidate\(s\)") {
    Write-Host "Actual current dictionary probe output:" -ForegroundColor Yellow
    Write-Host $actual
    throw "Current dictionary probe did not apply max_candidates_per_query from config."
}

$limitOutput = & (Join-Path $PSScriptRoot "probe_current_dictionary.ps1") -ConfigPath $config -Limit 1 ni
$limitActual = ($limitOutput -join "`n").TrimEnd()
if ($limitActual -notmatch "^\[ni\] 1 candidate\(s\)") {
    Write-Host "Actual limit override output:" -ForegroundColor Yellow
    Write-Host $limitActual
    throw "Current dictionary probe did not apply -Limit override."
}

$statsOutput = & (Join-Path $PSScriptRoot "probe_current_dictionary.ps1") -ConfigPath $config -Stats
$statsActual = ($statsOutput -join "`n").TrimEnd()
if ($statsActual -notmatch "source type: sqlite") {
    Write-Host "Actual stats output:" -ForegroundColor Yellow
    Write-Host $statsActual
    throw "Current dictionary probe did not pass sqlite dictionary config."
}

$fuzzyConfig = Join-Path $testDir "fuzzy.ini"
@"
[general]
dictionary_type=sqlite
dictionary_path=$spacedDatabase
max_candidates_per_query=5
prefix_candidates=1
fuzzy_pinyin=1
"@ | Set-Content -Encoding UTF8 -Path $fuzzyConfig

$fuzzyOutput = & (Join-Path $PSScriptRoot "probe_current_dictionary.ps1") -ConfigPath $fuzzyConfig zongguo
$fuzzyActual = ($fuzzyOutput -join "`n").TrimEnd()
if ($fuzzyActual -notmatch "^\[zongguo\] [1-9][0-9]* candidate\(s\)") {
    Write-Host "Actual fuzzy probe output:" -ForegroundColor Yellow
    Write-Host $fuzzyActual
    throw "Current dictionary probe did not apply fuzzy_pinyin from config."
}

$noFuzzyOutput = & (Join-Path $PSScriptRoot "probe_current_dictionary.ps1") -ConfigPath $fuzzyConfig -NoFuzzyPinyin zongguo
$noFuzzyActual = ($noFuzzyOutput -join "`n").TrimEnd()
if ($noFuzzyActual -notmatch "^\[zongguo\] 0 candidate\(s\)") {
    Write-Host "Actual no-fuzzy probe output:" -ForegroundColor Yellow
    Write-Host $noFuzzyActual
    throw "Current dictionary probe did not apply -NoFuzzyPinyin override."
}

$missingCodeFailed = $false
try {
    & (Join-Path $PSScriptRoot "probe_current_dictionary.ps1") -ConfigPath $config | Out-Null
} catch {
    $missingCodeFailed = $true
}
if (!$missingCodeFailed) {
    throw "Current dictionary probe should require a code unless -Stats is passed."
}

Write-Host "current dictionary probe tests passed."
