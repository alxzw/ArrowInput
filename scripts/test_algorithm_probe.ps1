param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot

& (Join-Path $PSScriptRoot "build_algorithm_probe.ps1") -Configuration $Configuration

$probe = Join-Path $root "build\tools\AlgorithmProbe\x64\$Configuration\AlgorithmProbe.exe"
$dictionary = Join-Path $root "tools\AlgorithmProbe\sample_dictionary.tsv"
$expectedPath = Join-Path $root "tests\algorithm_probe_expected.txt"
$statsExpectedPath = Join-Path $root "tests\algorithm_probe_stats_expected.txt"
$invalidDictionary = Join-Path $root "tests\algorithm_probe_invalid_dictionary.tsv"
$invalidStatsExpectedPath = Join-Path $root "tests\algorithm_probe_invalid_stats_expected.txt"
$unsupportedSourceExpectedPath = Join-Path $root "tests\algorithm_probe_unsupported_source_expected.txt"
$weightedDictionary = Join-Path $root "tests\algorithm_probe_weighted_dictionary.tsv"
$weightedExpectedPath = Join-Path $root "tests\algorithm_probe_weighted_expected.txt"
$seedDictionary = Join-Path $root "data\seed_pinyin.tsv"
$invalidWeightDictionary = Join-Path $root "tests\algorithm_probe_invalid_weight_dictionary.tsv"
$invalidWeightStatsExpectedPath = Join-Path $root "tests\algorithm_probe_invalid_weight_stats_expected.txt"
$sqliteDictionary = Join-Path $root "build\tests\algorithm_probe_sample.db"
$sqliteExpectedPath = Join-Path $root "tests\algorithm_probe_sqlite_expected.txt"
$missingDictionary = Join-Path $root "build\tests\missing_dictionary.tsv"
$missingDictionaryExpectedPath = Join-Path $root "tests\algorithm_probe_missing_dictionary_expected.txt"

if (!(Test-Path $probe)) {
    throw "AlgorithmProbe.exe was not found: $probe"
}
if (!(Test-Path $dictionary)) {
    throw "Sample dictionary was not found: $dictionary"
}
if (!(Test-Path $expectedPath)) {
    throw "Expected output was not found: $expectedPath"
}
if (!(Test-Path $statsExpectedPath)) {
    throw "Expected stats output was not found: $statsExpectedPath"
}
if (!(Test-Path $invalidDictionary)) {
    throw "Invalid sample dictionary was not found: $invalidDictionary"
}
if (!(Test-Path $invalidStatsExpectedPath)) {
    throw "Expected invalid stats output was not found: $invalidStatsExpectedPath"
}
if (!(Test-Path $unsupportedSourceExpectedPath)) {
    throw "Expected unsupported source output was not found: $unsupportedSourceExpectedPath"
}
if (!(Test-Path $weightedDictionary)) {
    throw "Weighted sample dictionary was not found: $weightedDictionary"
}
if (!(Test-Path $weightedExpectedPath)) {
    throw "Expected weighted output was not found: $weightedExpectedPath"
}
if (!(Test-Path $seedDictionary)) {
    throw "Seed dictionary was not found: $seedDictionary"
}
if (!(Test-Path $invalidWeightDictionary)) {
    throw "Invalid weighted sample dictionary was not found: $invalidWeightDictionary"
}
if (!(Test-Path $invalidWeightStatsExpectedPath)) {
    throw "Expected invalid weighted stats output was not found: $invalidWeightStatsExpectedPath"
}
if (!(Test-Path $sqliteExpectedPath)) {
    throw "Expected SQLite output was not found: $sqliteExpectedPath"
}
if (!(Test-Path $missingDictionaryExpectedPath)) {
    throw "Expected missing dictionary output was not found: $missingDictionaryExpectedPath"
}

& (Join-Path $PSScriptRoot "import_tsv_to_sqlite.ps1") -InputPath $weightedDictionary -OutputPath $sqliteDictionary

$output = & $probe --dict $dictionary ni hao abc
$actual = ($output -join "`n").TrimEnd()
$expected = ([System.IO.File]::ReadAllText($expectedPath, [System.Text.Encoding]::UTF8)).Replace("`r`n", "`n").TrimEnd()

if ($actual -ne $expected) {
    Write-Host "Expected:" -ForegroundColor Yellow
    Write-Host $expected
    Write-Host "Actual:" -ForegroundColor Yellow
    Write-Host $actual
    throw "AlgorithmProbe output did not match expected output."
}

$statsOutput = & $probe --dict $dictionary --stats
$statsActual = ($statsOutput -join "`n").TrimEnd()
$statsExpected = ([System.IO.File]::ReadAllText($statsExpectedPath, [System.Text.Encoding]::UTF8)).Replace("`r`n", "`n").TrimEnd()

if ($statsActual -ne $statsExpected) {
    Write-Host "Expected stats:" -ForegroundColor Yellow
    Write-Host $statsExpected
    Write-Host "Actual stats:" -ForegroundColor Yellow
    Write-Host $statsActual
    throw "AlgorithmProbe stats output did not match expected output."
}

$invalidStatsOutput = & $probe --dict $invalidDictionary --stats
$invalidStatsActual = ($invalidStatsOutput -join "`n").TrimEnd()
$invalidStatsExpected = ([System.IO.File]::ReadAllText($invalidStatsExpectedPath, [System.Text.Encoding]::UTF8)).Replace("`r`n", "`n").TrimEnd()

if ($invalidStatsActual -ne $invalidStatsExpected) {
    Write-Host "Expected invalid stats:" -ForegroundColor Yellow
    Write-Host $invalidStatsExpected
    Write-Host "Actual invalid stats:" -ForegroundColor Yellow
    Write-Host $invalidStatsActual
    throw "AlgorithmProbe invalid stats output did not match expected output."
}

$unsupportedSourceOutput = & $probe --type unsupported --dict $dictionary --stats ni
$unsupportedSourceActual = ($unsupportedSourceOutput -join "`n").TrimEnd()
$unsupportedSourceExpected = ([System.IO.File]::ReadAllText($unsupportedSourceExpectedPath, [System.Text.Encoding]::UTF8)).Replace("`r`n", "`n").TrimEnd()

if ($unsupportedSourceActual -ne $unsupportedSourceExpected) {
    Write-Host "Expected unsupported source:" -ForegroundColor Yellow
    Write-Host $unsupportedSourceExpected
    Write-Host "Actual unsupported source:" -ForegroundColor Yellow
    Write-Host $unsupportedSourceActual
    throw "AlgorithmProbe unsupported source output did not match expected output."
}

$demoFallbackOutput = & $probe ni
$demoFallbackActual = ($demoFallbackOutput -join "`n").TrimEnd()

if ($demoFallbackActual -notmatch "^\[ni\] 7 candidate\(s\)") {
    Write-Host "Actual demo fallback:" -ForegroundColor Yellow
    Write-Host $demoFallbackActual
    throw "AlgorithmProbe demo fallback did not return 7 candidate(s)."
}

if (Test-Path $missingDictionary) {
    Remove-Item -Force $missingDictionary
}
$missingDictionaryOutput = & $probe --dict $missingDictionary --stats ni
$missingDictionaryActual = ($missingDictionaryOutput -join "`n").TrimEnd()
$missingDictionaryExpected = ([System.IO.File]::ReadAllText($missingDictionaryExpectedPath, [System.Text.Encoding]::UTF8)).Replace("`r`n", "`n").TrimEnd()

if ($missingDictionaryActual -ne $missingDictionaryExpected) {
    Write-Host "Expected missing dictionary:" -ForegroundColor Yellow
    Write-Host $missingDictionaryExpected
    Write-Host "Actual missing dictionary:" -ForegroundColor Yellow
    Write-Host $missingDictionaryActual
    throw "AlgorithmProbe missing dictionary output did not match expected output."
}

$sqliteOutput = & $probe --type sqlite --dict $sqliteDictionary --stats ni
$sqliteActual = ($sqliteOutput -join "`n").TrimEnd()
$sqliteExpected = ([System.IO.File]::ReadAllText($sqliteExpectedPath, [System.Text.Encoding]::UTF8)).Replace("`r`n", "`n").TrimEnd()

if ($sqliteActual -ne $sqliteExpected) {
    Write-Host "Expected SQLite output:" -ForegroundColor Yellow
    Write-Host $sqliteExpected
    Write-Host "Actual SQLite output:" -ForegroundColor Yellow
    Write-Host $sqliteActual
    throw "AlgorithmProbe SQLite output did not match expected output."
}

$weightedOutput = & $probe --dict $weightedDictionary ni hao
$weightedActual = ($weightedOutput -join "`n").TrimEnd()
$weightedExpected = ([System.IO.File]::ReadAllText($weightedExpectedPath, [System.Text.Encoding]::UTF8)).Replace("`r`n", "`n").TrimEnd()

if ($weightedActual -ne $weightedExpected) {
    Write-Host "Expected weighted output:" -ForegroundColor Yellow
    Write-Host $weightedExpected
    Write-Host "Actual weighted output:" -ForegroundColor Yellow
    Write-Host $weightedActual
    throw "AlgorithmProbe weighted output did not match expected output."
}

$prefixOutput = & $probe --dict $seedDictionary nih
$prefixActual = ($prefixOutput -join "`n").TrimEnd()
if ($prefixActual -notmatch "^\[nih\] 1 candidate\(s\)") {
    Write-Host "Actual prefix output:" -ForegroundColor Yellow
    Write-Host $prefixActual
    throw "AlgorithmProbe prefix lookup did not return the nihao candidate."
}

$limitedPrefixOutput = & $probe --dict $seedDictionary --limit 2 ni
$limitedPrefixActual = ($limitedPrefixOutput -join "`n").TrimEnd()
if ($limitedPrefixActual -notmatch "^\[ni\] 2 candidate\(s\)" -or $limitedPrefixActual -match "ni hao") {
    Write-Host "Actual limited prefix output:" -ForegroundColor Yellow
    Write-Host $limitedPrefixActual
    throw "AlgorithmProbe limited prefix lookup should fill with exact candidates before prefix candidates."
}

$exactOnlyOutput = & $probe --dict $seedDictionary --no-prefix nih
$exactOnlyActual = ($exactOnlyOutput -join "`n").TrimEnd()
if ($exactOnlyActual -ne "[nih] 0 candidate(s)") {
    Write-Host "Actual exact-only output:" -ForegroundColor Yellow
    Write-Host $exactOnlyActual
    throw "AlgorithmProbe --no-prefix did not disable prefix lookup."
}

$invalidWeightStatsOutput = & $probe --dict $invalidWeightDictionary --stats
$invalidWeightStatsActual = ($invalidWeightStatsOutput -join "`n").TrimEnd()
$invalidWeightStatsExpected = ([System.IO.File]::ReadAllText($invalidWeightStatsExpectedPath, [System.Text.Encoding]::UTF8)).Replace("`r`n", "`n").TrimEnd()

if ($invalidWeightStatsActual -ne $invalidWeightStatsExpected) {
    Write-Host "Expected invalid weighted stats:" -ForegroundColor Yellow
    Write-Host $invalidWeightStatsExpected
    Write-Host "Actual invalid weighted stats:" -ForegroundColor Yellow
    Write-Host $invalidWeightStatsActual
    throw "AlgorithmProbe invalid weighted stats output did not match expected output."
}

Write-Host "AlgorithmProbe tests passed."
