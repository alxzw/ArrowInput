param(
    [Parameter(Mandatory = $true)]
    [string]$Name,

    [string]$Backup,
    [string]$InstallDir = "D:\MyApp\ArrowInput",
    [string]$ConfigPath,
    [switch]$UseNow
)

$ErrorActionPreference = "Stop"

if ($Name.IndexOfAny([System.IO.Path]::GetInvalidFileNameChars()) -ge 0) {
    throw "Experiment name contains invalid file name characters: $Name"
}

$experimentDir = Join-Path $InstallDir "experiments"
$backupRoot = Join-Path $experimentDir "backups\$Name"

if ($Backup) {
    $backupDir = Join-Path $backupRoot $Backup
} else {
    $latest = Get-ChildItem -Path $backupRoot -Directory -ErrorAction SilentlyContinue |
        Sort-Object Name -Descending |
        Select-Object -First 1
    if (!$latest) {
        throw "No backups found for experiment: $Name"
    }
    $backupDir = $latest.FullName
}

$backupTsv = Join-Path $backupDir "$Name.tsv"
if (!(Test-Path $backupTsv)) {
    throw "Backup TSV was not found: $backupTsv"
}

New-Item -ItemType Directory -Force -Path $experimentDir | Out-Null
Copy-Item -Force $backupTsv (Join-Path $experimentDir "$Name.tsv")

$rebuildArgs = @{
    Name = $Name
    InstallDir = $InstallDir
}
if ($ConfigPath) {
    $rebuildArgs.ConfigPath = $ConfigPath
}
if ($UseNow) {
    $rebuildArgs.UseNow = $true
}
& (Join-Path $PSScriptRoot "rebuild_experiment_dictionary.ps1") @rebuildArgs | Out-Host

Write-Host "Restored experiment dictionary: $Name"
Write-Host "Backup: $backupDir"
