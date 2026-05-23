param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug"
)

$ErrorActionPreference = "Stop"

Write-Host "== AlgorithmProbe regression =="
& (Join-Path $PSScriptRoot "test_algorithm_probe.ps1") -Configuration $Configuration

Write-Host "== Current dictionary probe smoke test =="
& (Join-Path $PSScriptRoot "test_probe_current_dictionary.ps1") -Configuration $Configuration

Write-Host "== Current dictionary check smoke test =="
& (Join-Path $PSScriptRoot "test_check_current_dictionary.ps1") -Configuration $Configuration

Write-Host "== Pinyin parser smoke test =="
& (Join-Path $PSScriptRoot "test_pinyin_parser.ps1") -Configuration $Configuration

Write-Host "== User dictionary smoke test =="
& (Join-Path $PSScriptRoot "test_user_dictionary.ps1") -Configuration $Configuration

Write-Host "== Current user dictionary manager smoke test =="
& (Join-Path $PSScriptRoot "test_manage_current_user_dictionary.ps1")

Write-Host "== SQLite import smoke test =="
& (Join-Path $PSScriptRoot "test_sqlite_import.ps1")

Write-Host "== SQLite query smoke test =="
& (Join-Path $PSScriptRoot "test_query_sqlite.ps1")

Write-Host "== SQLite inspect smoke test =="
& (Join-Path $PSScriptRoot "test_inspect_sqlite_dictionary.ps1")

Write-Host "== SQLite benchmark smoke test =="
& (Join-Path $PSScriptRoot "test_benchmark_sqlite_dictionary.ps1")

Write-Host "== Seed dictionary smoke test =="
& (Join-Path $PSScriptRoot "test_seed_dictionary.ps1")

Write-Host "== TSV dictionary validation test =="
& (Join-Path $PSScriptRoot "test_validate_tsv_dictionary.ps1")

Write-Host "== Text dictionary conversion test =="
& (Join-Path $PSScriptRoot "test_convert_text_dictionary_to_tsv.ps1")

Write-Host "== TSV entry append test =="
& (Join-Path $PSScriptRoot "test_add_tsv_entry.ps1")

Write-Host "== TSV dictionary merge test =="
& (Join-Path $PSScriptRoot "test_merge_tsv_dictionary.ps1")

Write-Host "== TSV dictionary merge preview test =="
& (Join-Path $PSScriptRoot "test_preview_tsv_merge.ps1")

Write-Host "== TSV candidate promotion test =="
& (Join-Path $PSScriptRoot "test_promote_tsv_candidates.ps1")

Write-Host "== Experiment dictionary test =="
& (Join-Path $PSScriptRoot "test_new_experiment_dictionary.ps1")

Write-Host "== Use experiment dictionary test =="
& (Join-Path $PSScriptRoot "test_use_experiment_dictionary.ps1")

Write-Host "== Rebuild experiment dictionary test =="
& (Join-Path $PSScriptRoot "test_rebuild_experiment_dictionary.ps1")

Write-Host "== Backup/restore experiment dictionary test =="
& (Join-Path $PSScriptRoot "test_backup_restore_experiment_dictionary.ps1")

Write-Host "== List experiment backups test =="
& (Join-Path $PSScriptRoot "test_list_experiment_backups.ps1")

Write-Host "== Import external dictionary to experiment test =="
& (Join-Path $PSScriptRoot "test_import_external_dictionary_to_experiment.ps1")

Write-Host "== Config helper smoke test =="
& (Join-Path $PSScriptRoot "test_set_dictionary.ps1")

Write-Host "== Status helper smoke test =="
& (Join-Path $PSScriptRoot "test_status.ps1")

Write-Host "== Pending helper smoke test =="
& (Join-Path $PSScriptRoot "test_apply_pending.ps1")

Write-Host "== Deploy smoke test =="
& (Join-Path $PSScriptRoot "test_deploy.ps1") -Configuration $Configuration

Write-Host "== ArrowInput DLL build =="
& (Join-Path $PSScriptRoot "build.ps1") -Configuration $Configuration

Write-Host "All ArrowInput checks passed."
