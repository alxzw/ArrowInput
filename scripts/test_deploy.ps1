param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$testInstallDir = Join-Path $root "build\tests\deploy\install"
$sampleDb = Join-Path $root "build\tools\AlgorithmProbe\sample_dictionary.db"

& (Join-Path $PSScriptRoot "build.ps1") -Configuration $Configuration | Out-Null
& (Join-Path $PSScriptRoot "build_algorithm_probe.ps1") -Configuration $Configuration | Out-Null

if (Test-Path $testInstallDir) {
    Remove-Item -Recurse -Force $testInstallDir
}
if (Test-Path $sampleDb) {
    Remove-Item -Force $sampleDb
}

& (Join-Path $PSScriptRoot "deploy.ps1") -Configuration $Configuration -InstallDir $testInstallDir | Out-Null

foreach ($name in @("ArrowInputTextService.dll", "sqlite3.dll", "AlgorithmProbe.exe", "sample_dictionary.tsv", "sample_dictionary.db", "seed_pinyin.tsv", "seed_pinyin.db", "set_dictionary.ps1", "status.ps1", "probe_current_dictionary.ps1", "check_current_dictionary.ps1", "release_loaded_binaries.ps1", "apply_pending.ps1", "query_sqlite.ps1", "query_sqlite_dictionary.ps1", "inspect_sqlite_dictionary.ps1", "benchmark_sqlite_dictionary.ps1", "manage_user_dictionary.ps1", "manage_current_user_dictionary.ps1", "validate_tsv_dictionary.ps1", "convert_text_dictionary_to_tsv.ps1", "import_tsv_to_sqlite.ps1", "add_tsv_entry.ps1", "merge_tsv_dictionary.ps1", "preview_tsv_merge.ps1", "promote_tsv_candidates.ps1", "new_experiment_dictionary.ps1", "use_experiment_dictionary.ps1", "rebuild_experiment_dictionary.ps1", "backup_experiment_dictionary.ps1", "restore_experiment_dictionary.ps1", "list_experiment_backups.ps1", "import_external_dictionary_to_experiment.ps1")) {
    $path = Join-Path $testInstallDir $name
    if (!(Test-Path $path)) {
        throw "Deploy test expected file was not found: $path"
    }
}

foreach ($name in @("import_tsv_to_sqlite.py", "query_sqlite_dictionary.py", "inspect_sqlite_dictionary.py", "benchmark_sqlite_dictionary.py", "manage_user_dictionary.py", "validate_tsv_dictionary.py", "convert_text_dictionary_to_tsv.py", "merge_tsv_dictionary.py", "preview_tsv_merge.py", "promote_tsv_candidates.py")) {
    $path = Join-Path $testInstallDir "tools\DictionaryTools\$name"
    if (!(Test-Path $path)) {
        throw "Deploy test expected tool was not found: $path"
    }
}

& (Join-Path $testInstallDir "validate_tsv_dictionary.ps1") -InputPath (Join-Path $testInstallDir "seed_pinyin.tsv") | Out-Null
& (Join-Path $testInstallDir "inspect_sqlite_dictionary.ps1") -DatabasePath (Join-Path $testInstallDir "seed_pinyin.db") -Limit 2 ni | Out-Null
& (Join-Path $testInstallDir "benchmark_sqlite_dictionary.ps1") -DatabasePath (Join-Path $testInstallDir "seed_pinyin.db") -Limit 2 -Iterations 1 ni | Out-Null
& (Join-Path $testInstallDir "AlgorithmProbe.exe") --type sqlite --dict (Join-Path $testInstallDir "seed_pinyin.db") --limit 2 ni | Out-Null
& (Join-Path $testInstallDir "manage_user_dictionary.ps1") -DatabasePath (Join-Path $testInstallDir "seed_pinyin.db") add --code ceshi --text 测试 --comment "ce shi" --user-weight 1000 | Out-Null
$userList = & (Join-Path $testInstallDir "manage_user_dictionary.ps1") -DatabasePath (Join-Path $testInstallDir "seed_pinyin.db") list
if (($userList -join "`n") -notmatch "ceshi`t测试") {
    throw "Deployed user dictionary manager did not add/list a user entry."
}
$testConfig = Join-Path $testInstallDir "probe_test_config.ini"
@"
[general]
dictionary_type=sqlite
dictionary_path=$(Join-Path $testInstallDir "seed_pinyin.db")
max_candidates_per_query=2
prefix_candidates=1
"@ | Set-Content -Encoding UTF8 -Path $testConfig
$probeOutput = & (Join-Path $testInstallDir "probe_current_dictionary.ps1") -InstallDir $testInstallDir -ConfigPath $testConfig ni
if (($probeOutput -join "`n") -notmatch "^\[ni\] 2 candidate\(s\)") {
    Write-Host "Actual current dictionary probe output:" -ForegroundColor Yellow
    Write-Host ($probeOutput -join "`n")
    throw "Deployed current dictionary probe did not use runtime config."
}
& (Join-Path $testInstallDir "check_current_dictionary.ps1") -InstallDir $testInstallDir -ConfigPath $testConfig -Limit 2 -Iterations 1 ni | Out-Null
& (Join-Path $testInstallDir "manage_current_user_dictionary.ps1") -InstallDir $testInstallDir -ConfigPath $testConfig user-stats | Out-Null
& (Join-Path $testInstallDir "release_loaded_binaries.ps1") -InstallDir $testInstallDir | Out-Null
& (Join-Path $testInstallDir "new_experiment_dictionary.ps1") -Name "deploy_test" -InstallDir $testInstallDir -Force | Out-Null

Write-Host "deploy tests passed."
