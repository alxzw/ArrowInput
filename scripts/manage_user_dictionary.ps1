[CmdletBinding(PositionalBinding = $false)]
param(
    [Parameter(Mandatory = $true)]
    [string]$DatabasePath,

    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$Arguments
)

$ErrorActionPreference = "Stop"
$candidateScripts = @(
    (Join-Path (Split-Path -Parent $PSScriptRoot) "tools\DictionaryTools\manage_user_dictionary.py"),
    (Join-Path $PSScriptRoot "tools\DictionaryTools\manage_user_dictionary.py")
)
$script = $candidateScripts | Where-Object { Test-Path $_ } | Select-Object -First 1

if (!$script) {
    throw "User dictionary management script was not found. Checked: $($candidateScripts -join ', ')"
}
if (!$Arguments -or $Arguments.Count -eq 0) {
    throw "A command is required: add, delete, restore, deleted, undo-delete, user-stats, list, export, import, learning-history, or undo-learning."
}

& python $script --database $DatabasePath @Arguments
if ($LASTEXITCODE -ne 0) {
    throw "User dictionary management failed with exit code $LASTEXITCODE."
}
