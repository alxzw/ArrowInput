param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$testDir = Join-Path $root "build\tests\user_dictionary"
$source = Join-Path $root "tests\algorithm_probe_weighted_dictionary.tsv"
$database = Join-Path $testDir "weighted.db"
$exportPath = Join-Path $testDir "user_entries.tsv"

& (Join-Path $PSScriptRoot "build_algorithm_probe.ps1") -Configuration $Configuration | Out-Null

if (Test-Path $testDir) {
    Remove-Item -Recurse -Force $testDir
}
New-Item -ItemType Directory -Force -Path $testDir | Out-Null
& (Join-Path $PSScriptRoot "import_tsv_to_sqlite.ps1") -InputPath $source -OutputPath $database | Out-Null

$before = & (Join-Path $root "build\tools\AlgorithmProbe\x64\$Configuration\AlgorithmProbe.exe") --type sqlite --dict $database --limit 3 ni
if (($before -join "`n") -notmatch "1\. user high_user") {
    throw "Unexpected initial weighted candidate order."
}

$selectionOutput = & (Join-Path $root "build\tools\AlgorithmProbe\x64\$Configuration\AlgorithmProbe.exe") --type sqlite --dict $database --limit 3 --select ni 2
if (($selectionOutput -join "`n") -notmatch "selected \[ni\] 2\. high") {
    Write-Host ($selectionOutput -join "`n") -ForegroundColor Yellow
    throw "Selection command did not select the expected candidate."
}
$after = & (Join-Path $root "build\tools\AlgorithmProbe\x64\$Configuration\AlgorithmProbe.exe") --type sqlite --dict $database --limit 3 ni
if (($after -join "`n") -notmatch "1\. high high") {
    Write-Host ($after -join "`n") -ForegroundColor Yellow
    throw "Selection learning did not promote the selected candidate."
}
$stats = & (Join-Path $PSScriptRoot "manage_user_dictionary.ps1") -DatabasePath $database user-stats --top 2
$statsText = $stats -join "`n"
if ($statsText -notmatch "active_user_entries=1" -or
    $statsText -notmatch "learning_events=1" -or
    $statsText -notmatch "top_user_weight_1=ni`thigh`t1000") {
    Write-Host $statsText -ForegroundColor Yellow
    throw "User dictionary stats did not report learned candidate state."
}

$history = & (Join-Path $PSScriptRoot "manage_user_dictionary.ps1") -DatabasePath $database learning-history
if (($history -join "`n") -notmatch "ni`thigh`t") {
    Write-Host ($history -join "`n") -ForegroundColor Yellow
    throw "Selection learning did not record a learning history event."
}
$undoOutput = & (Join-Path $PSScriptRoot "manage_user_dictionary.ps1") -DatabasePath $database undo-learning
if (($undoOutput -join "`n") -notmatch "undid learning event") {
    Write-Host ($undoOutput -join "`n") -ForegroundColor Yellow
    throw "Learning undo did not report success."
}
$undone = & (Join-Path $root "build\tools\AlgorithmProbe\x64\$Configuration\AlgorithmProbe.exe") --type sqlite --dict $database --limit 3 ni
if (($undone -join "`n") -notmatch "1\. user high_user") {
    Write-Host ($undone -join "`n") -ForegroundColor Yellow
    throw "Learning undo did not restore the previous ranking."
}
$activeHistory = & (Join-Path $PSScriptRoot "manage_user_dictionary.ps1") -DatabasePath $database learning-history
if (($activeHistory -join "`n") -match "ni`thigh`t") {
    Write-Host ($activeHistory -join "`n") -ForegroundColor Yellow
    throw "Learning undo did not hide the reverted event from active history."
}
$revertedHistory = & (Join-Path $PSScriptRoot "manage_user_dictionary.ps1") -DatabasePath $database learning-history --include-reverted
if (($revertedHistory -join "`n") -notmatch "ni`thigh`t") {
    Write-Host ($revertedHistory -join "`n") -ForegroundColor Yellow
    throw "Reverted learning event was not visible with --include-reverted."
}

$deleteOutput = & (Join-Path $root "build\tools\AlgorithmProbe\x64\$Configuration\AlgorithmProbe.exe") --type sqlite --dict $database --limit 3 --delete ni 1
if (($deleteOutput -join "`n") -notmatch "deleted \[ni\] 1\. user") {
    Write-Host ($deleteOutput -join "`n") -ForegroundColor Yellow
    throw "Candidate delete command did not delete the expected candidate."
}
$afterDelete = & (Join-Path $root "build\tools\AlgorithmProbe\x64\$Configuration\AlgorithmProbe.exe") --type sqlite --dict $database --limit 3 ni
if (($afterDelete -join "`n") -match "user high_user") {
    Write-Host ($afterDelete -join "`n") -ForegroundColor Yellow
    throw "Candidate delete command did not hide the deleted candidate."
}
$deletedBaseList = & (Join-Path $PSScriptRoot "manage_user_dictionary.ps1") -DatabasePath $database list --include-deleted
if (($deletedBaseList -join "`n") -notmatch "ni`tuser`t") {
    Write-Host ($deletedBaseList -join "`n") -ForegroundColor Yellow
    throw "Deleted candidate did not appear in include-deleted list."
}
$deletedOnlyList = & (Join-Path $PSScriptRoot "manage_user_dictionary.ps1") -DatabasePath $database deleted
if (($deletedOnlyList -join "`n") -notmatch "ni`tuser`t") {
    Write-Host ($deletedOnlyList -join "`n") -ForegroundColor Yellow
    throw "Deleted candidate did not appear in deleted-only list."
}
$undoDeleteOutput = & (Join-Path $PSScriptRoot "manage_user_dictionary.ps1") -DatabasePath $database undo-delete
if (($undoDeleteOutput -join "`n") -notmatch "restored latest deleted user entry") {
    Write-Host ($undoDeleteOutput -join "`n") -ForegroundColor Yellow
    throw "Undo delete did not report success."
}
$afterUndoDelete = & (Join-Path $root "build\tools\AlgorithmProbe\x64\$Configuration\AlgorithmProbe.exe") --type sqlite --dict $database --limit 3 ni
if (($afterUndoDelete -join "`n") -notmatch "user high_user") {
    Write-Host ($afterUndoDelete -join "`n") -ForegroundColor Yellow
    throw "Undo delete did not restore the latest deleted candidate."
}
$deleteAgainOutput = & (Join-Path $root "build\tools\AlgorithmProbe\x64\$Configuration\AlgorithmProbe.exe") --type sqlite --dict $database --limit 3 --delete ni 1
if (($deleteAgainOutput -join "`n") -notmatch "deleted \[ni\] 1\. user") {
    Write-Host ($deleteAgainOutput -join "`n") -ForegroundColor Yellow
    throw "Candidate delete command did not re-delete the expected candidate."
}
$restoreBaseOutput = & (Join-Path $PSScriptRoot "manage_user_dictionary.ps1") -DatabasePath $database restore --code ni --text user
if (($restoreBaseOutput -join "`n") -notmatch "restored user entry") {
    Write-Host ($restoreBaseOutput -join "`n") -ForegroundColor Yellow
    throw "User entry restore did not report success."
}
$afterRestoreBase = & (Join-Path $root "build\tools\AlgorithmProbe\x64\$Configuration\AlgorithmProbe.exe") --type sqlite --dict $database --limit 3 ni
if (($afterRestoreBase -join "`n") -notmatch "user high_user") {
    Write-Host ($afterRestoreBase -join "`n") -ForegroundColor Yellow
    throw "Restored base entry did not return as a candidate."
}

& (Join-Path $PSScriptRoot "manage_user_dictionary.ps1") -DatabasePath $database add --code ceshi --text 测试 --comment "ce shi" --user-weight 5000 | Out-Null
$list = & (Join-Path $PSScriptRoot "manage_user_dictionary.ps1") -DatabasePath $database list
if (($list -join "`n") -notmatch "ceshi`t测试") {
    throw "User entry add/list failed."
}
$userCandidateOutput = & (Join-Path $root "build\tools\AlgorithmProbe\x64\$Configuration\AlgorithmProbe.exe") --type sqlite --dict $database --limit 3 ceshi
if (($userCandidateOutput -join "`n") -notmatch "\[ceshi\] 1 candidate\(s\)" -or ($userCandidateOutput -join "`n") -notmatch "测试 ce shi") {
    Write-Host ($userCandidateOutput -join "`n") -ForegroundColor Yellow
    throw "Standalone user entry did not appear as a candidate."
}

& (Join-Path $PSScriptRoot "manage_user_dictionary.ps1") -DatabasePath $database export --output $exportPath | Out-Null
if (!(Test-Path $exportPath)) {
    throw "User dictionary export did not write a file."
}

& (Join-Path $PSScriptRoot "manage_user_dictionary.ps1") -DatabasePath $database delete --code ceshi --text 测试 | Out-Null
$deletedList = & (Join-Path $PSScriptRoot "manage_user_dictionary.ps1") -DatabasePath $database list
if (($deletedList -join "`n") -match "ceshi`t测试") {
    throw "User entry delete did not hide the entry."
}
$deletedCandidateOutput = & (Join-Path $root "build\tools\AlgorithmProbe\x64\$Configuration\AlgorithmProbe.exe") --type sqlite --dict $database --limit 3 ceshi
if (($deletedCandidateOutput -join "`n") -notmatch "\[ceshi\] 0 candidate\(s\)") {
    Write-Host ($deletedCandidateOutput -join "`n") -ForegroundColor Yellow
    throw "Deleted standalone user entry should not appear as a candidate."
}
$restoreStandaloneOutput = & (Join-Path $PSScriptRoot "manage_user_dictionary.ps1") -DatabasePath $database restore --code ceshi --text 测试
if (($restoreStandaloneOutput -join "`n") -notmatch "restored user entry") {
    Write-Host ($restoreStandaloneOutput -join "`n") -ForegroundColor Yellow
    throw "Standalone user entry restore did not report success."
}
$restoredStandaloneOutput = & (Join-Path $root "build\tools\AlgorithmProbe\x64\$Configuration\AlgorithmProbe.exe") --type sqlite --dict $database --limit 3 ceshi
if (($restoredStandaloneOutput -join "`n") -notmatch "\[ceshi\] 1 candidate\(s\)") {
    Write-Host ($restoredStandaloneOutput -join "`n") -ForegroundColor Yellow
    throw "Restored standalone user entry did not appear as a candidate."
}

& (Join-Path $PSScriptRoot "manage_user_dictionary.ps1") -DatabasePath $database delete --code ni --text user | Out-Null
$blockedBaseOutput = & (Join-Path $root "build\tools\AlgorithmProbe\x64\$Configuration\AlgorithmProbe.exe") --type sqlite --dict $database --limit 3 ni
if (($blockedBaseOutput -join "`n") -match "user high_user") {
    Write-Host ($blockedBaseOutput -join "`n") -ForegroundColor Yellow
    throw "Deleted base entry should be blocked by user tombstone."
}

& (Join-Path $PSScriptRoot "manage_user_dictionary.ps1") -DatabasePath $database import --input $exportPath | Out-Null
$importedList = & (Join-Path $PSScriptRoot "manage_user_dictionary.ps1") -DatabasePath $database list
if (($importedList -join "`n") -notmatch "ceshi`t测试") {
    throw "User dictionary import failed."
}

Write-Host "user dictionary tests passed."
