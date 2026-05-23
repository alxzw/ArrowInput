param(
    [Parameter(Mandatory = $true)]
    [string]$Name,

    [string]$InstallDir = "D:\MyApp\ArrowInput",
    [string]$ConfigPath,
    [switch]$UseNow
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

& (Join-Path $PSScriptRoot "validate_tsv_dictionary.ps1") -InputPath $experimentTsv | Out-Host
& (Join-Path $PSScriptRoot "import_tsv_to_sqlite.ps1") -InputPath $experimentTsv -OutputPath $experimentDb | Out-Host

if ($UseNow) {
    $setArgs = @{
        Type = "sqlite"
        Path = $experimentDb
        InstallDir = $InstallDir
    }
    if ($ConfigPath) {
        $setArgs.ConfigPath = $ConfigPath
    }
    & (Join-Path $PSScriptRoot "set_dictionary.ps1") @setArgs | Out-Host
}

Write-Host "Rebuilt experiment dictionary: $Name"
Write-Host "Experiment TSV: $experimentTsv"
Write-Host "Experiment DB: $experimentDb"
