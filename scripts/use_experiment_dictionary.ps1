param(
    [Parameter(Mandatory = $true)]
    [string]$Name,

    [string]$InstallDir = "D:\MyApp\ArrowInput",
    [string]$ConfigPath
)

$ErrorActionPreference = "Stop"

if ($Name.IndexOfAny([System.IO.Path]::GetInvalidFileNameChars()) -ge 0) {
    throw "Experiment name contains invalid file name characters: $Name"
}

$experimentDir = Join-Path $InstallDir "experiments"
$experimentDb = Join-Path $experimentDir "$Name.db"

if (!(Test-Path $experimentDb)) {
    throw "Experiment DB was not found: $experimentDb"
}

$setArgs = @{
    Type = "sqlite"
    Path = $experimentDb
    InstallDir = $InstallDir
}
if ($ConfigPath) {
    $setArgs.ConfigPath = $ConfigPath
}

& (Join-Path $PSScriptRoot "set_dictionary.ps1") @setArgs | Out-Host
Write-Host "Using experiment dictionary: $Name"
