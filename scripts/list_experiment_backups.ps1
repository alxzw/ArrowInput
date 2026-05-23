param(
    [Parameter(Mandatory = $true)]
    [string]$Name,

    [string]$InstallDir = "D:\MyApp\ArrowInput",

    [int]$Limit = 10
)

$ErrorActionPreference = "Stop"

if ($Name.IndexOfAny([System.IO.Path]::GetInvalidFileNameChars()) -ge 0) {
    throw "Experiment name contains invalid file name characters: $Name"
}

$experimentDir = Join-Path $InstallDir "experiments"
$backupRoot = Join-Path $experimentDir "backups\$Name"

Write-Host "Experiment backups: $Name"
Write-Host "Backup root: $backupRoot"

if (!(Test-Path $backupRoot)) {
    Write-Host "Backups: none"
    return
}

$backups = Get-ChildItem -Path $backupRoot -Directory -ErrorAction SilentlyContinue |
    Sort-Object Name -Descending |
    Select-Object -First $Limit

if (!$backups) {
    Write-Host "Backups: none"
    return
}

foreach ($backup in $backups) {
    $tsv = Join-Path $backup.FullName "$Name.tsv"
    $db = Join-Path $backup.FullName "$Name.db"
    $tsvInfo = if (Test-Path $tsv) {
        $item = Get-Item $tsv
        "tsv=yes size=$($item.Length) modified=$($item.LastWriteTime)"
    } else {
        "tsv=no"
    }
    $dbInfo = if (Test-Path $db) {
        $item = Get-Item $db
        "db=yes size=$($item.Length) modified=$($item.LastWriteTime)"
    } else {
        "db=no"
    }
    Write-Host ("  {0}: {1} {2}" -f $backup.Name, $tsvInfo, $dbInfo)
}
