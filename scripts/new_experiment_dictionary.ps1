param(
    [string]$Name = ("experiment_{0}" -f (Get-Date -Format "yyyyMMdd_HHmmss")),
    [string]$InstallDir = "D:\MyApp\ArrowInput",
    [string]$SourceTsv,
    [string]$ConfigPath,
    [switch]$UseNow,
    [switch]$Force
)

$ErrorActionPreference = "Stop"

if (!$SourceTsv) {
    $SourceTsv = Join-Path $InstallDir "seed_pinyin.tsv"
}
if (!(Test-Path $SourceTsv)) {
    throw "Source TSV was not found: $SourceTsv"
}

if ($Name.IndexOfAny([System.IO.Path]::GetInvalidFileNameChars()) -ge 0) {
    throw "Experiment name contains invalid file name characters: $Name"
}

$experimentDir = Join-Path $InstallDir "experiments"
New-Item -ItemType Directory -Force -Path $experimentDir | Out-Null

$targetTsv = Join-Path $experimentDir "$Name.tsv"
$targetDb = Join-Path $experimentDir "$Name.db"

if (!$Force -and ((Test-Path $targetTsv) -or (Test-Path $targetDb))) {
    throw "Experiment already exists: $Name. Use -Force to overwrite it."
}

Copy-Item -Force $SourceTsv $targetTsv
& (Join-Path $PSScriptRoot "validate_tsv_dictionary.ps1") -InputPath $targetTsv | Out-Host
& (Join-Path $PSScriptRoot "import_tsv_to_sqlite.ps1") -InputPath $targetTsv -OutputPath $targetDb | Out-Host

if ($UseNow) {
    $setArgs = @{
        Type = "sqlite"
        Path = $targetDb
        InstallDir = $InstallDir
    }
    if ($ConfigPath) {
        $setArgs.ConfigPath = $ConfigPath
    }
    & (Join-Path $PSScriptRoot "set_dictionary.ps1") @setArgs | Out-Host
}

Write-Host "Experiment TSV: $targetTsv"
Write-Host "Experiment DB: $targetDb"
