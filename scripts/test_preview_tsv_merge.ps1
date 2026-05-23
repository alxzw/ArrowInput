param()

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$testDir = Join-Path $root "build\tests\preview_tsv_merge"
$target = Join-Path $testDir "target.tsv"
$source = Join-Path $testDir "source.tsv"

if (Test-Path $testDir) {
    Remove-Item -Recurse -Force $testDir
}
New-Item -ItemType Directory -Force -Path $testDir | Out-Null
@(
    "# target"
    "ni`tni`tni`t1000`t0"
    "ni`tni2`tni`t100`t0"
    "hao`thao`thao`t1000`t0"
) | Set-Content -Path $target -Encoding UTF8
@(
    "# source"
    "ni`tni`tni`t1000`t0"
    "ni`tni3`tni`t900`t0"
    "xin`txin`txin`t800`t0"
    "xin`txin`txin duplicate`t700`t0"
) | Set-Content -Path $source -Encoding UTF8

$output = & (Join-Path $PSScriptRoot "preview_tsv_merge.ps1") -TargetPath $target -SourcePath $source -Top 5 -HotThreshold 3
$text = ($output -join "`n")
if ($LASTEXITCODE -ne 0) {
    Write-Host $text
    throw "Preview merge command failed."
}
if ($text -notmatch "target candidate count: 3" -or
    $text -notmatch "source candidate count: 4" -or
    $text -notmatch "new candidate count: 3" -or
    $text -notmatch "duplicate target candidate count: 1" -or
    $text -notmatch "source internal duplicate extra count: 1" -or
    $text -notmatch "projected candidate count: 6" -or
    $text -notmatch "ni: 3") {
    Write-Host $text
    throw "Preview merge output was unexpected."
}

$before = [System.IO.File]::ReadAllText($target, [System.Text.Encoding]::UTF8)
& (Join-Path $PSScriptRoot "preview_tsv_merge.ps1") -TargetPath $target -SourcePath $source | Out-Null
$after = [System.IO.File]::ReadAllText($target, [System.Text.Encoding]::UTF8)
if ($before -ne $after) {
    throw "Preview merge should not modify the target dictionary."
}

Write-Host "preview_tsv_merge tests passed."
