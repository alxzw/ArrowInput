param()

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$testDir = Join-Path $root "build\tests\backup_restore_experiment"
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

& (Join-Path $PSScriptRoot "rebuild_experiment_dictionary.ps1") -Name trial -InstallDir $installDir | Out-Null
& (Join-Path $PSScriptRoot "backup_experiment_dictionary.ps1") -Name trial -InstallDir $installDir | Out-Null

@(
    "# trial"
    "shurufa`t输入法`tshu ru fa`t900`t0"
) | Set-Content -Path $experimentTsv -Encoding UTF8
& (Join-Path $PSScriptRoot "restore_experiment_dictionary.ps1") -Name trial -InstallDir $installDir -ConfigPath $configPath -UseNow | Out-Null

$query = & (Join-Path $PSScriptRoot "query_sqlite.ps1") nihao shurufa -DatabasePath $experimentDb
$queryText = ($query -join "`n")
if ($queryText -notmatch "\[nihao\] 1 candidate\(s\)" -or $queryText -notmatch "\[shurufa\] 0 candidate\(s\)") {
    Write-Host $queryText
    throw "Restored experiment DB content was unexpected."
}

$config = Get-Content -Path $configPath -Raw
if ($config -notmatch [regex]::Escape("dictionary_path=$experimentDb")) {
    throw "Restored experiment was not selected with UseNow."
}

Write-Host "backup_restore_experiment_dictionary tests passed."
