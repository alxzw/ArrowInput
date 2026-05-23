param(
    [Parameter(Mandatory = $true)]
    [string]$TargetPath,

    [Parameter(Mandatory = $true)]
    [string]$SourcePath,

    [int]$Top = 10,

    [int]$HotThreshold = 50
)

$ErrorActionPreference = "Stop"
$candidateScripts = @(
    (Join-Path (Split-Path -Parent $PSScriptRoot) "tools\DictionaryTools\preview_tsv_merge.py"),
    (Join-Path $PSScriptRoot "tools\DictionaryTools\preview_tsv_merge.py")
)
$script = $candidateScripts | Where-Object { Test-Path $_ } | Select-Object -First 1

if (!$script) {
    throw "Preview script was not found. Checked: $($candidateScripts -join ', ')"
}

& python $script --target $TargetPath --source $SourcePath --top $Top --hot-threshold $HotThreshold
if ($LASTEXITCODE -ne 0) {
    throw "Merge preview failed with exit code $LASTEXITCODE."
}
