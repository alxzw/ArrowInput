param(
    [Parameter(Mandatory = $true)]
    [string]$DatabasePath,

    [Parameter(Mandatory = $true, ValueFromRemainingArguments = $true)]
    [string[]]$Code
)

$ErrorActionPreference = "Stop"
$candidateScripts = @(
    (Join-Path (Split-Path -Parent $PSScriptRoot) "tools\DictionaryTools\query_sqlite_dictionary.py"),
    (Join-Path $PSScriptRoot "tools\DictionaryTools\query_sqlite_dictionary.py")
)
$script = $candidateScripts | Where-Object { Test-Path $_ } | Select-Object -First 1

if (!$script) {
    throw "SQLite query script was not found. Checked: $($candidateScripts -join ', ')"
}

& python $script --database $DatabasePath @Code
if ($LASTEXITCODE -ne 0) {
    throw "SQLite query failed with exit code $LASTEXITCODE."
}
