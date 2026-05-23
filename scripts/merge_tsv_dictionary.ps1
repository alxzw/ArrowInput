param(
    [Parameter(Mandatory = $true)]
    [string]$TargetPath,

    [Parameter(Mandatory = $true)]
    [string]$SourcePath,

    [string]$DatabasePath,
    [switch]$AllowDuplicate
)

$ErrorActionPreference = "Stop"
$candidateScripts = @(
    (Join-Path (Split-Path -Parent $PSScriptRoot) "tools\DictionaryTools\merge_tsv_dictionary.py"),
    (Join-Path $PSScriptRoot "tools\DictionaryTools\merge_tsv_dictionary.py")
)
$script = $candidateScripts | Where-Object { Test-Path $_ } | Select-Object -First 1

if (!$script) {
    throw "Merge script was not found. Checked: $($candidateScripts -join ', ')"
}

$argsList = @("--target", $TargetPath, "--source", $SourcePath)
if ($AllowDuplicate) {
    $argsList += "--allow-duplicate"
}

& python $script @argsList
if ($LASTEXITCODE -ne 0) {
    throw "Merge failed with exit code $LASTEXITCODE."
}
& (Join-Path $PSScriptRoot "validate_tsv_dictionary.ps1") -InputPath $TargetPath | Out-Host

if ($DatabasePath) {
    & (Join-Path $PSScriptRoot "import_tsv_to_sqlite.ps1") -InputPath $TargetPath -OutputPath $DatabasePath | Out-Host
}
