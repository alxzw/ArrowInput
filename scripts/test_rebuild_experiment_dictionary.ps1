param()

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$testDir = Join-Path $root "build\tests\rebuild_experiment"
$installDir = Join-Path $testDir "install"
$experimentDir = Join-Path $installDir "experiments"
$configPath = Join-Path $testDir "config.ini"
$experimentTsv = Join-Path $experimentDir "trial.tsv"
$experimentDb = Join-Path $experimentDir "trial.db"

if (Test-Path $testDir) {
    Remove-Item -Recurse -Force $testDir
}
New-Item -ItemType Directory -Force -Path $experimentDir | Out-Null
@(
    "# trial"
    "nihao`t你好`tni hao`t100`t0"
) | Set-Content -Path $experimentTsv -Encoding UTF8

& (Join-Path $PSScriptRoot "rebuild_experiment_dictionary.ps1") -Name trial -InstallDir $installDir -ConfigPath $configPath -UseNow | Out-Null

if (!(Test-Path $experimentDb)) {
    throw "Experiment DB was not rebuilt."
}

$query = & (Join-Path $PSScriptRoot "query_sqlite.ps1") nihao -DatabasePath $experimentDb
$queryText = ($query -join "`n")
if ($queryText -notmatch "\[nihao\] 1 candidate\(s\)" -or $queryText -notmatch "ni hao weight=100 user_weight=0") {
    Write-Host $queryText
    throw "Rebuilt experiment DB query failed."
}

$config = Get-Content -Path $configPath -Raw
if ($config -notmatch [regex]::Escape("dictionary_path=$experimentDb")) {
    throw "Rebuilt experiment was not selected with UseNow."
}

Write-Host "rebuild_experiment_dictionary tests passed."
