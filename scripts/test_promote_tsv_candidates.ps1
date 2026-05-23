param()

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$testDir = Join-Path $root "build\tests\promote_tsv_candidates"
$target = Join-Path $testDir "target.tsv"
$source = Join-Path $testDir "source.tsv"
$database = Join-Path $testDir "target.db"

if (Test-Path $testDir) {
    Remove-Item -Recurse -Force $testDir
}
New-Item -ItemType Directory -Force -Path $testDir | Out-Null

@(
    "# target"
    "ni`tni`tni`t1000`t0"
    "ni`thigh`thigh`t10000`t0"
    "hao`thao`thao`t100`t0"
) | Set-Content -Path $target -Encoding UTF8
@(
    "# source"
    "ni`tni`tni`t1000`t0"
) | Set-Content -Path $source -Encoding UTF8

$output = & (Join-Path $PSScriptRoot "promote_tsv_candidates.ps1") -TargetPath $target -SourcePath $source -Boost 1000
$text = ($output -join "`n")
if ($LASTEXITCODE -ne 0 -or $text -notmatch "Promoted 1 candidate\(s\)") {
    Write-Host $text
    throw "Promotion output was unexpected."
}

& (Join-Path $PSScriptRoot "import_tsv_to_sqlite.ps1") -InputPath $target -OutputPath $database | Out-Null
$query = & (Join-Path $PSScriptRoot "query_sqlite.ps1") ni -DatabasePath $database
$queryText = ($query -join "`n")
if ($queryText -notmatch "1\. ni ni weight=11000 user_weight=0") {
    Write-Host $queryText
    throw "Promoted candidate was not ranked first."
}

Write-Host "promote_tsv_candidates tests passed."
