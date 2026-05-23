param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",
    [string]$InstallDir = "D:\MyApp\ArrowInput"
)

$ErrorActionPreference = "Stop"
$dll = Join-Path $InstallDir "ArrowInputTextService.dll"

if (-not ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw "Installing a TSF text service requires an elevated PowerShell session."
}

if (!(Test-Path $dll)) {
    throw "Build output not found: $dll"
}

$process = Start-Process -FilePath "$env:SystemRoot\System32\regsvr32.exe" -ArgumentList @("/s", $dll) -Wait -PassThru
if ($process.ExitCode -ne 0) {
    throw "regsvr32 failed with exit code $($process.ExitCode)"
}
Write-Host "ArrowInput registered: $dll"
