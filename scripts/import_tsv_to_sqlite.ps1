param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath,

    [Parameter(Mandatory = $true)]
    [string]$OutputPath,

    [switch]$Append
)

$ErrorActionPreference = "Stop"
$candidateScripts = @(
    (Join-Path (Split-Path -Parent $PSScriptRoot) "tools\DictionaryTools\import_tsv_to_sqlite.py"),
    (Join-Path $PSScriptRoot "tools\DictionaryTools\import_tsv_to_sqlite.py")
)
$script = $candidateScripts | Where-Object { Test-Path $_ } | Select-Object -First 1

if (!$script) {
    throw "Importer script was not found. Checked: $($candidateScripts -join ', ')"
}

$argsList = @("--input", $InputPath, "--output", $OutputPath)
if ($Append) {
    $argsList += "--append"
}

& python $script @argsList
if ($LASTEXITCODE -ne 0) {
    throw "SQLite importer failed with exit code $LASTEXITCODE."
}
