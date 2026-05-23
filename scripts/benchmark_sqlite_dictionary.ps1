[CmdletBinding(PositionalBinding = $false)]
param(
    [Parameter(Mandatory = $true)]
    [string]$DatabasePath,

    [int]$Limit = 100,

    [int]$Iterations = 100,

    [switch]$Prefix,

    [switch]$Explain,

    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$Code
)

$ErrorActionPreference = "Stop"
$candidateScripts = @(
    (Join-Path (Split-Path -Parent $PSScriptRoot) "tools\DictionaryTools\benchmark_sqlite_dictionary.py"),
    (Join-Path $PSScriptRoot "tools\DictionaryTools\benchmark_sqlite_dictionary.py")
)
$script = $candidateScripts | Where-Object { Test-Path $_ } | Select-Object -First 1

if (!$script) {
    throw "SQLite benchmark script was not found. Checked: $($candidateScripts -join ', ')"
}
if (!$Code -or $Code.Count -eq 0) {
    throw "At least one code is required."
}

$arguments = @($script, "--database", $DatabasePath, "--limit", $Limit, "--iterations", $Iterations)
if ($Prefix) {
    $arguments += "--prefix"
}
if ($Explain) {
    $arguments += "--explain"
}
$arguments += $Code

& python @arguments
if ($LASTEXITCODE -ne 0) {
    throw "SQLite benchmark failed with exit code $LASTEXITCODE."
}
