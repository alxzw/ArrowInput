param()

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$testDir = Join-Path $root "build\tests\set_dictionary"
$configPath = Join-Path $testDir "config.ini"
$dbPath = Join-Path $testDir "sample_dictionary.db"
$tsvPath = Join-Path $testDir "sample_dictionary.tsv"
$missingPath = Join-Path $testDir "missing_dictionary.tsv"

New-Item -ItemType Directory -Force -Path $testDir | Out-Null
if (Test-Path $configPath) {
    Remove-Item -Force $configPath
}
New-Item -ItemType File -Force -Path $dbPath | Out-Null
New-Item -ItemType File -Force -Path $tsvPath | Out-Null

& (Join-Path $PSScriptRoot "set_dictionary.ps1") -Type sqlite -Path $dbPath -ConfigPath $configPath | Out-Null

$type = Get-Content $configPath | Where-Object { $_ -like "dictionary_type=*" }
$path = Get-Content $configPath | Where-Object { $_ -like "dictionary_path=*" }
$limit = Get-Content $configPath | Where-Object { $_ -like "max_candidates_per_query=*" }
$prefixCandidates = Get-Content $configPath | Where-Object { $_ -like "prefix_candidates=*" }

if ($type -ne "dictionary_type=sqlite") {
    throw "dictionary_type was not written correctly: $type"
}
if ($path -ne "dictionary_path=$dbPath") {
    throw "dictionary_path was not written correctly: $path"
}
if ($limit -ne "max_candidates_per_query=30") {
    throw "max_candidates_per_query default was not written correctly: $limit"
}
if ($prefixCandidates -ne "prefix_candidates=1") {
    throw "prefix_candidates default was not written correctly: $prefixCandidates"
}

& (Join-Path $PSScriptRoot "set_dictionary.ps1") -Type tsv -Path $tsvPath -ConfigPath $configPath | Out-Null

$type = Get-Content $configPath | Where-Object { $_ -like "dictionary_type=*" }
$path = Get-Content $configPath | Where-Object { $_ -like "dictionary_path=*" }

if ($type -ne "dictionary_type=tsv") {
    throw "dictionary_type was not updated correctly: $type"
}
if ($path -ne "dictionary_path=$tsvPath") {
    throw "dictionary_path was not updated correctly: $path"
}

$missingFailed = $false
try {
    & (Join-Path $PSScriptRoot "set_dictionary.ps1") -Type tsv -Path $missingPath -ConfigPath $configPath | Out-Null
} catch {
    $missingFailed = $true
}
if (!$missingFailed) {
    throw "set_dictionary should reject missing dictionary paths by default."
}

& (Join-Path $PSScriptRoot "set_dictionary.ps1") -Type tsv -Path $missingPath -ConfigPath $configPath -AllowMissing | Out-Null
$path = Get-Content $configPath | Where-Object { $_ -like "dictionary_path=*" }
if ($path -ne "dictionary_path=$missingPath") {
    throw "dictionary_path was not written with AllowMissing: $path"
}

Write-Host "set_dictionary tests passed."
