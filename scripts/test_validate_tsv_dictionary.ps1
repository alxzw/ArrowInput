param()

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$seed = Join-Path $root "data\seed_pinyin.tsv"
$invalid = Join-Path $root "tests\invalid_validate_dictionary.tsv"

$seedOutput = & (Join-Path $PSScriptRoot "validate_tsv_dictionary.ps1") -InputPath $seed
$seedText = ($seedOutput -join "`n")
if ($LASTEXITCODE -ne 0) {
    Write-Host $seedText
    throw "Seed dictionary validation failed."
}
if ($seedText -notmatch "candidate count: 116" -or $seedText -notmatch "code count: 74" -or $seedText -notmatch "issues: none") {
    Write-Host $seedText
    throw "Seed dictionary validation stats were unexpected."
}

$previousErrorActionPreference = $ErrorActionPreference
$ErrorActionPreference = "Continue"
$invalidOutput = & powershell -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "validate_tsv_dictionary.ps1") -InputPath $invalid -MaxCodeLength 8 2>&1
$invalidExit = $LASTEXITCODE
$ErrorActionPreference = $previousErrorActionPreference
$invalidText = ($invalidOutput -join "`n")
if ($invalidExit -eq 0) {
    Write-Host $invalidText
    throw "Invalid dictionary validation should fail."
}
if ($invalidText -notmatch "invalid weight field" -or
    $invalidText -notmatch "extra fields after user_weight" -or
    $invalidText -notmatch "leading or trailing whitespace" -or
    $invalidText -notmatch "code must contain only lowercase letters and apostrophes" -or
    $invalidText -notmatch "code length exceeds 8" -or
    $invalidText -notmatch "weight is outside" -or
    $invalidText -notmatch "duplicate exact candidate") {
    Write-Host $invalidText
    throw "Invalid dictionary validation issues were unexpected."
}

Write-Host "TSV dictionary validation tests passed."
