param()

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$testDir = Join-Path $root "build\tests\convert_text_dictionary"
$input = Join-Path $testDir "source.txt"
$output = Join-Path $testDir "converted.tsv"
$reverseInput = Join-Path $testDir "reverse.txt"
$reverseOutput = Join-Path $testDir "reverse.tsv"
$rimeInput = Join-Path $testDir "rime.dict.yaml"
$rimeOutput = Join-Path $testDir "rime.tsv"
$badRimeInput = Join-Path $testDir "bad_rime.dict.yaml"
$badRimeOutput = Join-Path $testDir "bad_rime.tsv"

if (Test-Path $testDir) {
    Remove-Item -Recurse -Force $testDir
}
New-Item -ItemType Directory -Force -Path $testDir | Out-Null

@(
    "# simple space-delimited dictionary"
    "ni ni ni 1000 0"
    "hao hao hao 900 0"
) | Set-Content -Path $input -Encoding UTF8

$outputText = & (Join-Path $PSScriptRoot "convert_text_dictionary_to_tsv.ps1") `
    -InputPath $input `
    -OutputPath $output `
    -Delimiter space
$outputJoined = ($outputText -join "`n")
if ($LASTEXITCODE -ne 0 -or $outputJoined -notmatch "Converted 2 row\(s\)") {
    Write-Host $outputJoined
    throw "Convert command failed."
}

$validated = & (Join-Path $PSScriptRoot "validate_tsv_dictionary.ps1") -InputPath $output
$validatedText = ($validated -join "`n")
if ($LASTEXITCODE -ne 0 -or $validatedText -notmatch "candidate count: 2" -or $validatedText -notmatch "issues: none") {
    Write-Host $validatedText
    throw "Converted TSV validation failed."
}

@(
    "# text code weight"
    "ni ni 1200"
    "hao hao 1100"
) | Set-Content -Path $reverseInput -Encoding UTF8

& (Join-Path $PSScriptRoot "convert_text_dictionary_to_tsv.ps1") `
    -InputPath $reverseInput `
    -OutputPath $reverseOutput `
    -Delimiter space `
    -Columns "text,code,weight" | Out-Null

$reverseRows = Get-Content -Path $reverseOutput | Where-Object { $_ -and !$_.StartsWith("#") }
if ($reverseRows[0] -ne "ni`tni`t`t1200`t" -or $reverseRows[1] -ne "hao`thao`t`t1100`t") {
    $reverseRows | ForEach-Object { Write-Host $_ }
    throw "Column mapping conversion output was unexpected."
}

@(
    "# Rime dictionary"
    "---"
    "name: sample"
    "version: `"1`""
    "..."
    "ni`tni`t1000"
    "nihao`tni hao`t900"
    "yibai`tyi bai`t99.93%"
) | Set-Content -Path $rimeInput -Encoding UTF8

& (Join-Path $PSScriptRoot "convert_text_dictionary_to_tsv.ps1") `
    -InputPath $rimeInput `
    -OutputPath $rimeOutput `
    -SourceFormat rime | Out-Null

$rimeRows = Get-Content -Path $rimeOutput | Where-Object { $_ -and !$_.StartsWith("#") }
if ($rimeRows[0] -ne "ni`tni`tni`t1000`t" -or
    $rimeRows[1] -ne "nihao`tnihao`tni hao`t900`t" -or
    $rimeRows[2] -ne "yibai`tyibai`tyi bai`t9993`t") {
    $rimeRows | ForEach-Object { Write-Host $_ }
    throw "Rime conversion output was unexpected."
}

$rimeValidated = & (Join-Path $PSScriptRoot "validate_tsv_dictionary.ps1") -InputPath $rimeOutput
$rimeValidatedText = ($rimeValidated -join "`n")
if ($LASTEXITCODE -ne 0 -or $rimeValidatedText -notmatch "candidate count: 3" -or $rimeValidatedText -notmatch "issues: none") {
    Write-Host $rimeValidatedText
    throw "Converted Rime TSV validation failed."
}

@(
    "---"
    "name: bad"
    "..."
    "missing-tabs"
) | Set-Content -Path $badRimeInput -Encoding UTF8

$badRimeFailed = $false
try {
    & (Join-Path $PSScriptRoot "convert_text_dictionary_to_tsv.ps1") `
        -InputPath $badRimeInput `
        -OutputPath $badRimeOutput `
        -SourceFormat rime 2>$null | Out-Null
} catch {
    $badRimeFailed = $true
}
if (!$badRimeFailed) {
    throw "Invalid Rime conversion should fail."
}

Write-Host "convert_text_dictionary_to_tsv tests passed."
