param()

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$testDir = Join-Path $root "build\tests\apply_pending"
$target = Join-Path $testDir "sample.dll"
$pending = "$target.pending"

New-Item -ItemType Directory -Force -Path $testDir | Out-Null
Set-Content -Path $target -Value "old" -Encoding ASCII
Set-Content -Path $pending -Value "new" -Encoding ASCII

& (Join-Path $PSScriptRoot "apply_pending.ps1") -InstallDir $testDir | Out-Null

$actual = (Get-Content -Path $target -Raw).Trim()
if ($actual -ne "new") {
    throw "Pending binary was not applied. Actual content: $actual"
}
if (Test-Path $pending) {
    throw "Pending file was not removed after apply."
}

Write-Host "apply_pending tests passed."
