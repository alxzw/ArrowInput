param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath,

    [Parameter(Mandatory = $true)]
    [string]$OutputPath,

    [ValidateSet("simple", "rime")]
    [string]$SourceFormat = "simple",

    [string]$Columns = "code,text,comment,weight,user_weight",

    [ValidateSet("auto", "tab", "space")]
    [string]$Delimiter = "auto",

    [string]$DefaultWeight = ""
)

$ErrorActionPreference = "Stop"
$candidateScripts = @(
    (Join-Path (Split-Path -Parent $PSScriptRoot) "tools\DictionaryTools\convert_text_dictionary_to_tsv.py"),
    (Join-Path $PSScriptRoot "tools\DictionaryTools\convert_text_dictionary_to_tsv.py")
)
$script = $candidateScripts | Where-Object { Test-Path $_ } | Select-Object -First 1

if (!$script) {
    throw "Converter script was not found. Checked: $($candidateScripts -join ', ')"
}

$argsList = @(
    "--input", $InputPath,
    "--output", $OutputPath,
    "--source-format", $SourceFormat,
    "--columns", $Columns,
    "--delimiter", $Delimiter
)
if ($DefaultWeight -ne "") {
    $argsList += @("--default-weight", $DefaultWeight)
}

& python $script @argsList
if ($LASTEXITCODE -ne 0) {
    throw "Converter failed with exit code $LASTEXITCODE."
}
