param()

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$testDir = Join-Path $root "build\tests\manage_current_user_dictionary"
$database = Join-Path $testDir "current.db"
$config = Join-Path $testDir "config.ini"

if (Test-Path $testDir) {
    Remove-Item -Recurse -Force $testDir
}
New-Item -ItemType Directory -Force -Path $testDir | Out-Null
& (Join-Path $PSScriptRoot "import_tsv_to_sqlite.ps1") -InputPath (Join-Path $root "tests\algorithm_probe_weighted_dictionary.tsv") -OutputPath $database | Out-Null

@"
[general]
dictionary_type=sqlite
dictionary_path=current.db
"@ | Set-Content -Encoding UTF8 -Path $config

& (Join-Path $PSScriptRoot "manage_current_user_dictionary.ps1") -ConfigPath $config add --code ceshi --text 测试 --comment "ce shi" --user-weight 5000 | Out-Null

$list = & (Join-Path $PSScriptRoot "manage_current_user_dictionary.ps1") -ConfigPath $config list
if (($list -join "`n") -notmatch "ceshi`t测试") {
    Write-Host ($list -join "`n") -ForegroundColor Yellow
    throw "Current user dictionary manager did not add/list through config dictionary."
}

$stats = & (Join-Path $PSScriptRoot "manage_current_user_dictionary.ps1") -ConfigPath $config user-stats
if (($stats -join "`n") -notmatch "active_user_entries=1") {
    Write-Host ($stats -join "`n") -ForegroundColor Yellow
    throw "Current user dictionary manager did not report stats through config dictionary."
}

@"
[general]
dictionary_type=tsv
dictionary_path=current.tsv
"@ | Set-Content -Encoding UTF8 -Path $config

$nonSqliteFailed = $false
try {
    & (Join-Path $PSScriptRoot "manage_current_user_dictionary.ps1") -ConfigPath $config list | Out-Null
} catch {
    $nonSqliteFailed = $true
}
if (!$nonSqliteFailed) {
    throw "Current user dictionary manager should reject non-sqlite dictionaries."
}

Write-Host "current user dictionary manager tests passed."
