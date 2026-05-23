param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",
    [string]$InstallDir = "D:\MyApp\ArrowInput",
    [switch]$StopLockingProcesses
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $root "build\x64\$Configuration"
$dll = Join-Path $buildDir "ArrowInputTextService.dll"
$probe = Join-Path $root "build\tools\AlgorithmProbe\x64\$Configuration\AlgorithmProbe.exe"
$pendingFiles = @()

if (!(Test-Path $dll)) {
    throw "Build output not found: $dll"
}

function Copy-BinaryWithPending {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Source,
        [Parameter(Mandatory = $true)]
        [string]$DestinationDirectory,
        [Parameter(Mandatory = $true)]
        [string]$Name
    )

    $destination = Join-Path $DestinationDirectory $Name
    try {
        Copy-Item -Force $Source $destination
        $stalePending = "$destination.pending"
        if (Test-Path $stalePending) {
            Remove-Item -Force $stalePending
        }
    } catch {
        $pending = "$destination.pending"
        Copy-Item -Force $Source $pending
        $script:pendingFiles += $pending
        Write-Warning "Could not replace $Name because it is loaded. Wrote pending file: $pending"
    }
}

New-Item -ItemType Directory -Force -Path $InstallDir | Out-Null

if ($StopLockingProcesses -and (Test-Path (Join-Path $PSScriptRoot "release_loaded_binaries.ps1"))) {
    & (Join-Path $PSScriptRoot "release_loaded_binaries.ps1") -InstallDir $InstallDir -StopLockingProcesses
}

$pdb = Join-Path $buildDir "ArrowInputTextService.pdb"
if (Test-Path $pdb) {
    Copy-Item -Force $pdb $InstallDir -ErrorAction SilentlyContinue
}

$sqliteDll = "L:\vcpkg\installed\x64-windows\bin\sqlite3.dll"
if (Test-Path $sqliteDll) {
    Copy-BinaryWithPending -Source $sqliteDll -DestinationDirectory $InstallDir -Name "sqlite3.dll"
}

Copy-Item -Force (Join-Path $root "config\config.ini") $InstallDir
$sampleDict = Join-Path $root "tools\AlgorithmProbe\sample_dictionary.tsv"
if (Test-Path $sampleDict) {
    Copy-Item -Force $sampleDict $InstallDir
}
$seedDict = Join-Path $root "data\seed_pinyin.tsv"
if (Test-Path $seedDict) {
    Copy-Item -Force $seedDict $InstallDir
}
$sampleDb = Join-Path $root "build\tools\AlgorithmProbe\sample_dictionary.db"
if ((Test-Path $sampleDict) -and (Test-Path (Join-Path $PSScriptRoot "import_tsv_to_sqlite.ps1"))) {
    & (Join-Path $PSScriptRoot "import_tsv_to_sqlite.ps1") -InputPath $sampleDict -OutputPath $sampleDb | Out-Host
}
if (Test-Path $sampleDb) {
    Copy-Item -Force $sampleDb $InstallDir
}
$seedDb = Join-Path $root "build\tools\AlgorithmProbe\seed_pinyin.db"
if ((Test-Path $seedDict) -and (Test-Path (Join-Path $PSScriptRoot "import_tsv_to_sqlite.ps1"))) {
    & (Join-Path $PSScriptRoot "import_tsv_to_sqlite.ps1") -InputPath $seedDict -OutputPath $seedDb | Out-Host
}
if (Test-Path $seedDb) {
    Copy-Item -Force $seedDb $InstallDir
}
if (Test-Path $probe) {
    Copy-Item -Force $probe (Join-Path $InstallDir "AlgorithmProbe.exe")
}
Copy-Item -Force (Join-Path $PSScriptRoot "install.ps1") $InstallDir
Copy-Item -Force (Join-Path $PSScriptRoot "uninstall.ps1") $InstallDir
Copy-Item -Force (Join-Path $PSScriptRoot "set_dictionary.ps1") $InstallDir
Copy-Item -Force (Join-Path $PSScriptRoot "status.ps1") $InstallDir
Copy-Item -Force (Join-Path $PSScriptRoot "probe_current_dictionary.ps1") $InstallDir
Copy-Item -Force (Join-Path $PSScriptRoot "check_current_dictionary.ps1") $InstallDir
Copy-Item -Force (Join-Path $PSScriptRoot "release_loaded_binaries.ps1") $InstallDir
Copy-Item -Force (Join-Path $PSScriptRoot "apply_pending.ps1") $InstallDir
Copy-Item -Force (Join-Path $PSScriptRoot "query_sqlite.ps1") $InstallDir
Copy-Item -Force (Join-Path $PSScriptRoot "query_sqlite_dictionary.ps1") $InstallDir
Copy-Item -Force (Join-Path $PSScriptRoot "inspect_sqlite_dictionary.ps1") $InstallDir
Copy-Item -Force (Join-Path $PSScriptRoot "benchmark_sqlite_dictionary.ps1") $InstallDir
Copy-Item -Force (Join-Path $PSScriptRoot "manage_user_dictionary.ps1") $InstallDir
Copy-Item -Force (Join-Path $PSScriptRoot "manage_current_user_dictionary.ps1") $InstallDir
Copy-Item -Force (Join-Path $PSScriptRoot "validate_tsv_dictionary.ps1") $InstallDir
Copy-Item -Force (Join-Path $PSScriptRoot "convert_text_dictionary_to_tsv.ps1") $InstallDir
Copy-Item -Force (Join-Path $PSScriptRoot "import_tsv_to_sqlite.ps1") $InstallDir
Copy-Item -Force (Join-Path $PSScriptRoot "add_tsv_entry.ps1") $InstallDir
Copy-Item -Force (Join-Path $PSScriptRoot "merge_tsv_dictionary.ps1") $InstallDir
Copy-Item -Force (Join-Path $PSScriptRoot "preview_tsv_merge.ps1") $InstallDir
Copy-Item -Force (Join-Path $PSScriptRoot "promote_tsv_candidates.ps1") $InstallDir
Copy-Item -Force (Join-Path $PSScriptRoot "new_experiment_dictionary.ps1") $InstallDir
Copy-Item -Force (Join-Path $PSScriptRoot "use_experiment_dictionary.ps1") $InstallDir
Copy-Item -Force (Join-Path $PSScriptRoot "rebuild_experiment_dictionary.ps1") $InstallDir
Copy-Item -Force (Join-Path $PSScriptRoot "backup_experiment_dictionary.ps1") $InstallDir
Copy-Item -Force (Join-Path $PSScriptRoot "restore_experiment_dictionary.ps1") $InstallDir
Copy-Item -Force (Join-Path $PSScriptRoot "list_experiment_backups.ps1") $InstallDir
Copy-Item -Force (Join-Path $PSScriptRoot "import_external_dictionary_to_experiment.ps1") $InstallDir
$toolInstallDir = Join-Path $InstallDir "tools\DictionaryTools"
New-Item -ItemType Directory -Force -Path $toolInstallDir | Out-Null
Copy-Item -Force (Join-Path $root "tools\DictionaryTools\*.py") $toolInstallDir

Copy-BinaryWithPending -Source $dll -DestinationDirectory $InstallDir -Name "ArrowInputTextService.dll"

Write-Host "ArrowInput deployed to $InstallDir"
if ($pendingFiles.Count -gt 0) {
    Write-Warning "Pending binaries need a sign-out/unload before taking effect:"
    foreach ($pending in $pendingFiles) {
        Write-Warning "  $pending"
    }
}
