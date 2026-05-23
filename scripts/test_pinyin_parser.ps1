param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$testDir = Join-Path $root "build\tests\pinyin_parser"
$source = Join-Path $root "tests\algorithm_probe_spaced_dictionary.tsv"
$database = Join-Path $testDir "spaced.db"
$probe = Join-Path $root "build\tools\AlgorithmProbe\x64\$Configuration\AlgorithmProbe.exe"

& (Join-Path $PSScriptRoot "build_algorithm_probe.ps1") -Configuration $Configuration | Out-Null

if (Test-Path $testDir) {
    Remove-Item -Recurse -Force $testDir
}
New-Item -ItemType Directory -Force -Path $testDir | Out-Null
& (Join-Path $PSScriptRoot "import_tsv_to_sqlite.ps1") -InputPath $source -OutputPath $database | Out-Null

$output = & $probe --type sqlite --dict $database --limit 3 shurufa zhongguo srf srfa shrf zguo zhg zg
$actual = ($output -join "`n").TrimEnd()
if ($actual -notmatch "\[shurufa\] 1 candidate\(s\)" -or $actual -notmatch "shu ru fa") {
    Write-Host $actual -ForegroundColor Yellow
    throw "Pinyin parser did not map shurufa to spaced pinyin lookup."
}
if ($actual -notmatch "\[zhongguo\] [12] candidate\(s\)" -or $actual -notmatch "zhong guo") {
    Write-Host $actual -ForegroundColor Yellow
    throw "Pinyin parser did not map zhongguo to spaced pinyin lookup."
}
if ($actual -notmatch "\[srf\] 1 candidate\(s\)" -or $actual -notmatch "shu ru fa") {
    Write-Host $actual -ForegroundColor Yellow
    throw "Acronym lookup did not map srf to spaced pinyin lookup."
}
if ($actual -notmatch "\[zg\] [12] candidate\(s\)" -or $actual -notmatch "zhong guo") {
    Write-Host $actual -ForegroundColor Yellow
    throw "Acronym lookup did not map zg to spaced pinyin lookup."
}
if ($actual -notmatch "\[srfa\] 1 candidate\(s\)" -or $actual -notmatch "shu ru fa") {
    Write-Host $actual -ForegroundColor Yellow
    throw "Mixed prefix lookup did not map srfa to spaced pinyin lookup."
}
if ($actual -notmatch "\[shrf\] 1 candidate\(s\)" -or $actual -notmatch "shu ru fa") {
    Write-Host $actual -ForegroundColor Yellow
    throw "Mixed prefix lookup did not map shrf to spaced pinyin lookup."
}
if ($actual -notmatch "\[zguo\] [12] candidate\(s\)" -or $actual -notmatch "zhong guo") {
    Write-Host $actual -ForegroundColor Yellow
    throw "Mixed prefix lookup did not map zguo to spaced pinyin lookup."
}
if ($actual -notmatch "\[zhg\] [12] candidate\(s\)" -or $actual -notmatch "zhong guo") {
    Write-Host $actual -ForegroundColor Yellow
    throw "Mixed prefix lookup did not map zhg to spaced pinyin lookup."
}

$noFuzzyOutput = & $probe --type sqlite --dict $database --limit 3 zongguo sen
$noFuzzyActual = ($noFuzzyOutput -join "`n").TrimEnd()
if ($noFuzzyActual -notmatch "\[zongguo\] 0 candidate\(s\)" -or
    $noFuzzyActual -notmatch "\[sen\] 0 candidate\(s\)") {
    Write-Host $noFuzzyActual -ForegroundColor Yellow
    throw "Fuzzy pinyin should be disabled by default."
}

$fuzzyOutput = & $probe --type sqlite --dict $database --limit 3 --fuzzy-pinyin zongguo sen pin
$fuzzyActual = ($fuzzyOutput -join "`n").TrimEnd()
if ($fuzzyActual -notmatch "\[zongguo\] [12] candidate\(s\)" -or $fuzzyActual -notmatch "zhong guo") {
    Write-Host $fuzzyActual -ForegroundColor Yellow
    throw "Fuzzy pinyin did not map zongguo to zhong guo."
}
if ($fuzzyActual -notmatch "\[sen\] 1 candidate\(s\)" -or $fuzzyActual -notmatch "shen") {
    Write-Host $fuzzyActual -ForegroundColor Yellow
    throw "Fuzzy pinyin did not map sen to shen."
}
if ($fuzzyActual -notmatch "\[pin\] 1 candidate\(s\)" -or $fuzzyActual -notmatch "ping") {
    Write-Host $fuzzyActual -ForegroundColor Yellow
    throw "Fuzzy pinyin did not map pin to ping."
}

Write-Host "pinyin parser tests passed."
