param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath,

    [int]$MaxIssues = 20,

    [int]$MaxCodeLength = 128,

    [int]$MaxTextLength = 64,

    [int]$MaxCommentLength = 128,

    [int]$MinWeight = -1000000000,

    [int]$MaxWeight = 1000000000,

    [int]$MinUserWeight = -1000000000,

    [int]$MaxUserWeight = 1000000000
)

$ErrorActionPreference = "Stop"
$candidateScripts = @(
    (Join-Path (Split-Path -Parent $PSScriptRoot) "tools\DictionaryTools\validate_tsv_dictionary.py"),
    (Join-Path $PSScriptRoot "tools\DictionaryTools\validate_tsv_dictionary.py")
)
$script = $candidateScripts | Where-Object { Test-Path $_ } | Select-Object -First 1

if (!$script) {
    throw "Validator script was not found. Checked: $($candidateScripts -join ', ')"
}

& python $script `
    --input $InputPath `
    --max-issues $MaxIssues `
    --max-code-length $MaxCodeLength `
    --max-text-length $MaxTextLength `
    --max-comment-length $MaxCommentLength `
    --min-weight $MinWeight `
    --max-weight $MaxWeight `
    --min-user-weight $MinUserWeight `
    --max-user-weight $MaxUserWeight
if ($LASTEXITCODE -ne 0) {
    throw "Validator failed with exit code $LASTEXITCODE."
}
