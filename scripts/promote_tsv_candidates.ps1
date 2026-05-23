param(
    [Parameter(Mandatory = $true)]
    [string]$TargetPath,

    [Parameter(Mandatory = $true)]
    [string]$SourcePath,

    [int]$Boost = 1000
)

$ErrorActionPreference = "Stop"
$candidateScripts = @(
    (Join-Path (Split-Path -Parent $PSScriptRoot) "tools\DictionaryTools\promote_tsv_candidates.py"),
    (Join-Path $PSScriptRoot "tools\DictionaryTools\promote_tsv_candidates.py")
)
$script = $candidateScripts | Where-Object { Test-Path $_ } | Select-Object -First 1

if (!$script) {
    throw "Promotion script was not found. Checked: $($candidateScripts -join ', ')"
}

& python $script --target $TargetPath --source $SourcePath --boost $Boost
if ($LASTEXITCODE -ne 0) {
    throw "Candidate promotion failed with exit code $LASTEXITCODE."
}

& (Join-Path $PSScriptRoot "validate_tsv_dictionary.ps1") -InputPath $TargetPath | Out-Host
