[CmdletBinding(PositionalBinding = $false)]
param(
    [Parameter(Mandatory = $true)]
    [string]$DatabasePath,

    [int]$Limit = 10,

    [int]$TopCodes = 0,

    [switch]$Prefix,

    [switch]$Metadata,

    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$Code = @()
)

$ErrorActionPreference = "Stop"
$candidateScripts = @(
    (Join-Path (Split-Path -Parent $PSScriptRoot) "tools\DictionaryTools\inspect_sqlite_dictionary.py"),
    (Join-Path $PSScriptRoot "tools\DictionaryTools\inspect_sqlite_dictionary.py")
)
$script = $candidateScripts | Where-Object { Test-Path $_ } | Select-Object -First 1

if (!$script) {
    throw "SQLite inspect script was not found. Checked: $($candidateScripts -join ', ')"
}

$arguments = @($script, "--database", $DatabasePath, "--limit", $Limit, "--top-codes", $TopCodes)
if ($Metadata) {
    $arguments += "--metadata"
}
if ($Prefix) {
    $arguments += "--prefix"
}
$arguments += $Code

& python @arguments
if ($LASTEXITCODE -ne 0) {
    throw "SQLite inspect failed with exit code $LASTEXITCODE."
}
