param(
    [string]$InstallDir = "D:\MyApp\ArrowInput",
    [string]$ConfigPath,
    [int]$LogTail = 30
)

$ErrorActionPreference = "Stop"

function Read-IniValue {
    param(
        [string]$Path,
        [string]$Section,
        [string]$Key
    )

    if (!(Test-Path $Path)) {
        return ""
    }

    $inSection = $false
    foreach ($line in Get-Content -Path $Path) {
        $trimmed = $line.Trim()
        if (!$trimmed -or $trimmed.StartsWith(";") -or $trimmed.StartsWith("#")) {
            continue
        }
        if ($trimmed.StartsWith("[") -and $trimmed.EndsWith("]")) {
            $inSection = $trimmed.Substring(1, $trimmed.Length - 2) -ieq $Section
            continue
        }
        if ($inSection) {
            $separator = $trimmed.IndexOf("=")
            if ($separator -gt 0) {
                $name = $trimmed.Substring(0, $separator).Trim()
                if ($name -ieq $Key) {
                    return $trimmed.Substring($separator + 1).Trim()
                }
            }
        }
    }
    return ""
}

if (!$ConfigPath) {
    $ConfigPath = Join-Path (Join-Path $env:APPDATA "ArrowInput") "config.ini"
}

$logPath = Join-Path (Join-Path $env:APPDATA "ArrowInput") "debug.log"

Write-Host "ArrowInput status"
Write-Host "InstallDir: $InstallDir"
Write-Host "Config: $ConfigPath"

Write-Host ""
Write-Host "Installed files:"
foreach ($name in @("ArrowInputTextService.dll", "sqlite3.dll", "AlgorithmProbe.exe", "sample_dictionary.tsv", "sample_dictionary.db", "seed_pinyin.tsv", "seed_pinyin.db", "set_dictionary.ps1", "status.ps1", "probe_current_dictionary.ps1", "check_current_dictionary.ps1", "release_loaded_binaries.ps1", "apply_pending.ps1", "query_sqlite.ps1", "query_sqlite_dictionary.ps1", "inspect_sqlite_dictionary.ps1", "benchmark_sqlite_dictionary.ps1", "manage_user_dictionary.ps1", "manage_current_user_dictionary.ps1", "validate_tsv_dictionary.ps1", "convert_text_dictionary_to_tsv.ps1", "import_tsv_to_sqlite.ps1", "add_tsv_entry.ps1", "merge_tsv_dictionary.ps1", "preview_tsv_merge.ps1", "promote_tsv_candidates.ps1", "new_experiment_dictionary.ps1", "use_experiment_dictionary.ps1", "rebuild_experiment_dictionary.ps1", "backup_experiment_dictionary.ps1", "restore_experiment_dictionary.ps1", "list_experiment_backups.ps1", "import_external_dictionary_to_experiment.ps1")) {
    $path = Join-Path $InstallDir $name
    if (Test-Path $path) {
        $item = Get-Item $path
        Write-Host ("  {0}: present size={1} modified={2}" -f $name, $item.Length, $item.LastWriteTime)
    } else {
        Write-Host "  ${name}: missing"
    }
}

$pending = Get-ChildItem -Path $InstallDir -Filter "*.pending" -ErrorAction SilentlyContinue
Write-Host ""
if ($pending) {
    Write-Host "Pending binaries:"
    foreach ($item in $pending) {
        Write-Host ("  {0} size={1} modified={2}" -f $item.FullName, $item.Length, $item.LastWriteTime)
    }
} else {
    Write-Host "Pending binaries: none"
}

Write-Host ""
if (Test-Path $ConfigPath) {
    $dictionaryType = Read-IniValue -Path $ConfigPath -Section "general" -Key "dictionary_type"
    $dictionaryPath = Read-IniValue -Path $ConfigPath -Section "general" -Key "dictionary_path"
    $candidatePageSize = Read-IniValue -Path $ConfigPath -Section "general" -Key "candidate_page_size"
    $maxCandidatesPerQuery = Read-IniValue -Path $ConfigPath -Section "general" -Key "max_candidates_per_query"
    $prefixCandidates = Read-IniValue -Path $ConfigPath -Section "general" -Key "prefix_candidates"
    $fuzzyPinyin = Read-IniValue -Path $ConfigPath -Section "general" -Key "fuzzy_pinyin"
    $captureKeys = Read-IniValue -Path $ConfigPath -Section "debug" -Key "capture_keys"
    if (!$dictionaryType) {
        $dictionaryType = "tsv"
    }
    Write-Host "Runtime config:"
    Write-Host "  dictionary_type=$dictionaryType"
    Write-Host "  dictionary_path=$(if ($dictionaryPath) { $dictionaryPath } else { '<empty; demo fallback>' })"
    Write-Host "  dictionary_path_exists=$(if (!$dictionaryPath) { 'n/a' } elseif (Test-Path $dictionaryPath) { 'yes' } else { 'no' })"
    Write-Host "  candidate_page_size=$(if ($candidatePageSize) { $candidatePageSize } else { '5' })"
    Write-Host "  max_candidates_per_query=$(if ($maxCandidatesPerQuery) { $maxCandidatesPerQuery } else { '30' })"
    Write-Host "  prefix_candidates=$(if ($prefixCandidates) { $prefixCandidates } else { '1' })"
    Write-Host "  fuzzy_pinyin=$(if ($fuzzyPinyin) { $fuzzyPinyin } else { '0' })"
    $experimentDirForConfig = Join-Path $InstallDir "experiments"
    $experimentPrefix = [System.IO.Path]::GetFullPath($experimentDirForConfig).TrimEnd('\') + '\'
    $experimentName = ""
    if ($dictionaryPath) {
        $fullDictionaryPath = [System.IO.Path]::GetFullPath($dictionaryPath)
        if ($fullDictionaryPath.StartsWith($experimentPrefix, [System.StringComparison]::OrdinalIgnoreCase) -and
            [System.IO.Path]::GetExtension($fullDictionaryPath) -ieq ".db") {
            $experimentName = [System.IO.Path]::GetFileNameWithoutExtension($fullDictionaryPath)
        }
    }
    Write-Host "  experiment=$(if ($experimentName) { $experimentName } else { 'n/a' })"
    Write-Host "  capture_keys=$captureKeys"

    $userManager = Join-Path $InstallDir "manage_user_dictionary.ps1"
    if ($dictionaryType -ieq "sqlite" -and $dictionaryPath -and (Test-Path $dictionaryPath) -and (Test-Path $userManager)) {
        Write-Host ""
        Write-Host "User dictionary:"
        & $userManager -DatabasePath $dictionaryPath user-stats --top 3 |
            ForEach-Object { Write-Host "  $_" }
    }
} else {
    Write-Host "Runtime config: missing"
}

$experimentDir = Join-Path $InstallDir "experiments"
Write-Host ""
if (Test-Path $experimentDir) {
    $experiments = Get-ChildItem -Path $experimentDir -Filter "*.tsv" -File -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 8
    if ($experiments) {
        Write-Host "Experiments:"
        foreach ($tsv in $experiments) {
            $name = [System.IO.Path]::GetFileNameWithoutExtension($tsv.Name)
            $db = Join-Path $experimentDir "$name.db"
            $dbStatus = if (Test-Path $db) { "db=yes" } else { "db=no" }
            $backupRoot = Join-Path $experimentDir "backups\$name"
            $latestBackup = Get-ChildItem -Path $backupRoot -Directory -ErrorAction SilentlyContinue |
                Sort-Object Name -Descending |
                Select-Object -First 1
            $backupStatus = if ($latestBackup) { "latest_backup=$($latestBackup.Name)" } else { "latest_backup=none" }
            Write-Host ("  {0}: tsv=yes {1} modified={2} {3}" -f $name, $dbStatus, $tsv.LastWriteTime, $backupStatus)
        }
    } else {
        Write-Host "Experiments: none"
    }
} else {
    Write-Host "Experiments: none"
}

Write-Host ""
if (Test-Path $logPath) {
    Write-Host "Recent debug.log:"
    Get-Content -Path $logPath -Tail $LogTail
} else {
    Write-Host "Recent debug.log: missing"
}
