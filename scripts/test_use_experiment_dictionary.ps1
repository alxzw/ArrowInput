param()

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$testDir = Join-Path $root "build\tests\use_experiment"
$installDir = Join-Path $testDir "install"
$experimentDir = Join-Path $installDir "experiments"
$configPath = Join-Path $testDir "config.ini"
$dbPath = Join-Path $experimentDir "trial.db"

if (Test-Path $testDir) {
    Remove-Item -Recurse -Force $testDir
}
New-Item -ItemType Directory -Force -Path $experimentDir | Out-Null
New-Item -ItemType File -Force -Path $dbPath | Out-Null

& (Join-Path $PSScriptRoot "use_experiment_dictionary.ps1") -Name trial -InstallDir $installDir -ConfigPath $configPath | Out-Null

$config = Get-Content -Path $configPath -Raw
if ($config -notmatch "dictionary_type=sqlite" -or $config -notmatch [regex]::Escape("dictionary_path=$dbPath")) {
    throw "Experiment dictionary config was not written."
}

$missingFailed = $false
try {
    & (Join-Path $PSScriptRoot "use_experiment_dictionary.ps1") -Name missing -InstallDir $installDir -ConfigPath $configPath | Out-Null
} catch {
    $missingFailed = $true
}
if (!$missingFailed) {
    throw "Missing experiment DB should be rejected."
}

Write-Host "use_experiment_dictionary tests passed."
