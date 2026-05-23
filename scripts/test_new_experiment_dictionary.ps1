param()

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$testInstallDir = Join-Path $root "build\tests\new_experiment\install"
$configPath = Join-Path $root "build\tests\new_experiment\config.ini"
$sourceTsv = Join-Path $testInstallDir "seed_pinyin.tsv"

if (Test-Path $testInstallDir) {
    Remove-Item -Recurse -Force $testInstallDir
}
New-Item -ItemType Directory -Force -Path $testInstallDir | Out-Null
Copy-Item -Force (Join-Path $root "data\seed_pinyin.tsv") $sourceTsv

& (Join-Path $PSScriptRoot "new_experiment_dictionary.ps1") -Name "case1" -InstallDir $testInstallDir -ConfigPath $configPath -UseNow | Out-Null

$targetTsv = Join-Path $testInstallDir "experiments\case1.tsv"
$targetDb = Join-Path $testInstallDir "experiments\case1.db"
if (!(Test-Path $targetTsv) -or !(Test-Path $targetDb)) {
    throw "Experiment TSV or DB was not created."
}

$query = & (Join-Path $PSScriptRoot "query_sqlite.ps1") nihao -DatabasePath $targetDb
$queryText = ($query -join "`n")
if ($queryText -notmatch "\[nihao\] 1 candidate\(s\)") {
    Write-Host $queryText
    throw "Experiment DB query failed."
}

$config = Get-Content -Path $configPath -Raw
if ($config -notmatch [regex]::Escape("dictionary_path=$targetDb")) {
    throw "Experiment config path was not written."
}

Write-Host "new_experiment_dictionary tests passed."
