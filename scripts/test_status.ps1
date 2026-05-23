param()

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$testDir = Join-Path $root "build\tests\status"
$installDir = Join-Path $testDir "install"
$configPath = Join-Path $testDir "config.ini"
$dbPath = Join-Path $installDir "experiments\trial.db"
$experimentDir = Join-Path $installDir "experiments"
$backupDir = Join-Path $experimentDir "backups\trial\20260523_010203"

New-Item -ItemType Directory -Force -Path $installDir | Out-Null
New-Item -ItemType Directory -Force -Path $experimentDir | Out-Null
New-Item -ItemType Directory -Force -Path $backupDir | Out-Null
New-Item -ItemType File -Force -Path (Join-Path $installDir "ArrowInputTextService.dll") | Out-Null
New-Item -ItemType File -Force -Path (Join-Path $installDir "sqlite3.dll") | Out-Null
New-Item -ItemType File -Force -Path (Join-Path $installDir "sample_dictionary.tsv") | Out-Null
New-Item -ItemType File -Force -Path $dbPath | Out-Null
New-Item -ItemType File -Force -Path (Join-Path $installDir "set_dictionary.ps1") | Out-Null
New-Item -ItemType File -Force -Path (Join-Path $experimentDir "trial.tsv") | Out-Null
New-Item -ItemType File -Force -Path (Join-Path $experimentDir "trial.db") | Out-Null

@"
[general]
dictionary_type=sqlite
dictionary_path=$dbPath
candidate_page_size=5
max_candidates_per_query=100
prefix_candidates=1
fuzzy_pinyin=1

[debug]
capture_keys=1
"@ | Set-Content -Path $configPath -Encoding UTF8

$output = & (Join-Path $PSScriptRoot "status.ps1") -InstallDir $installDir -ConfigPath $configPath -LogTail 1 6>&1
$text = ($output -join "`n")

if ($text -notmatch "dictionary_type=sqlite") {
    throw "status output did not include sqlite dictionary type."
}
if ($text -notmatch "dictionary_path_exists=yes") {
    throw "status output did not confirm dictionary path existence."
}
if ($text -notmatch "max_candidates_per_query=100") {
    throw "status output did not include max candidate query limit."
}
if ($text -notmatch "prefix_candidates=1") {
    throw "status output did not include prefix candidate setting."
}
if ($text -notmatch "fuzzy_pinyin=1") {
    throw "status output did not include fuzzy pinyin setting."
}
if ($text -notmatch "experiment=trial") {
    throw "status output did not identify active experiment."
}
if ($text -notmatch "Pending binaries: none") {
    throw "status output did not report pending state."
}
if ($text -notmatch "Experiments:" -or $text -notmatch "trial: tsv=yes db=yes") {
    throw "status output did not include experiment summary."
}
if ($text -notmatch "latest_backup=20260523_010203") {
    throw "status output did not include latest experiment backup."
}

Write-Host "status tests passed."
