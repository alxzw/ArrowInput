param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",
    [string]$InstallDir = "D:\MyApp\ArrowInput",
    [switch]$ReleaseLoadedBinaries,
    [switch]$StopLockingProcesses
)

$ErrorActionPreference = "Stop"
$dll = Join-Path $InstallDir "ArrowInputTextService.dll"

if (-not ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw "Uninstalling a TSF text service requires an elevated PowerShell session."
}

if (!(Test-Path $dll)) {
    throw "Build output not found: $dll"
}

$process = Start-Process -FilePath "$env:SystemRoot\System32\regsvr32.exe" -ArgumentList @("/s", "/u", $dll) -Wait -PassThru
if ($process.ExitCode -ne 0) {
    throw "regsvr32 /u failed with exit code $($process.ExitCode)"
}
Write-Host "ArrowInput unregistered: $dll"

if ($ReleaseLoadedBinaries -or $StopLockingProcesses) {
    & (Join-Path $PSScriptRoot "release_loaded_binaries.ps1") -InstallDir $InstallDir -StopLockingProcesses:$StopLockingProcesses
}
