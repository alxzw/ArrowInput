param()

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$testDir = Join-Path $root "build\tests\list_experiment_backups"
$installDir = Join-Path $testDir "install"
$experimentDir = Join-Path $installDir "experiments"
$experimentTsv = Join-Path $experimentDir "trial.tsv"

if (Test-Path $testDir) {
    Remove-Item -Recurse -Force $testDir
}
New-Item -ItemType Directory -Force -Path $experimentDir | Out-Null
@(
    "# trial"
    "nihao`tnihao`tni hao`t100`t0"
) | Set-Content -Path $experimentTsv -Encoding UTF8

& (Join-Path $PSScriptRoot "rebuild_experiment_dictionary.ps1") -Name trial -InstallDir $installDir | Out-Null
& (Join-Path $PSScriptRoot "backup_experiment_dictionary.ps1") -Name trial -InstallDir $installDir | Out-Null

$output = & (Join-Path $PSScriptRoot "list_experiment_backups.ps1") -Name trial -InstallDir $installDir 6>&1
$text = ($output -join "`n")
if ($text -notmatch "Experiment backups: trial" -or
    $text -notmatch "tsv=yes" -or
    $text -notmatch "db=yes") {
    Write-Host $text
    throw "Backup list output was unexpected."
}

$none = & (Join-Path $PSScriptRoot "list_experiment_backups.ps1") -Name missing -InstallDir $installDir 6>&1
$noneText = ($none -join "`n")
if ($noneText -notmatch "Backups: none") {
    Write-Host $noneText
    throw "Missing backup list output was unexpected."
}

Write-Host "list_experiment_backups tests passed."
