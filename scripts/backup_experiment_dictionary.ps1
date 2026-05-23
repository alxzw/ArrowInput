param(
    [Parameter(Mandatory = $true)]
    [string]$Name,

    [string]$InstallDir = "D:\MyApp\ArrowInput"
)

$ErrorActionPreference = "Stop"

if ($Name.IndexOfAny([System.IO.Path]::GetInvalidFileNameChars()) -ge 0) {
    throw "Experiment name contains invalid file name characters: $Name"
}

$experimentDir = Join-Path $InstallDir "experiments"
$experimentTsv = Join-Path $experimentDir "$Name.tsv"
$experimentDb = Join-Path $experimentDir "$Name.db"

if (!(Test-Path $experimentTsv)) {
    throw "Experiment TSV was not found: $experimentTsv"
}

$stamp = Get-Date -Format "yyyyMMdd_HHmmss"
$backupDir = Join-Path $experimentDir "backups\$Name\$stamp"
New-Item -ItemType Directory -Force -Path $backupDir | Out-Null

Copy-Item -Force $experimentTsv (Join-Path $backupDir "$Name.tsv")
if (Test-Path $experimentDb) {
    Copy-Item -Force $experimentDb (Join-Path $backupDir "$Name.db")
}

Write-Host "Backed up experiment dictionary: $Name"
Write-Host "Backup: $backupDir"
