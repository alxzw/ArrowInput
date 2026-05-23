param(
    [Parameter(Mandatory = $true)]
    [string]$DictionaryPath,

    [Parameter(Mandatory = $true)]
    [string]$Code,

    [Parameter(Mandatory = $true)]
    [string]$Text,

    [string]$Comment = "",
    [int]$Weight = 0,
    [int]$UserWeight = 0,
    [string]$DatabasePath,
    [switch]$AllowDuplicate
)

$ErrorActionPreference = "Stop"

if (!$Code.Trim() -or !$Text.Trim()) {
    throw "Code and Text must be non-empty."
}
if ($Code -ne $Code.Trim() -or $Text -ne $Text.Trim() -or $Comment -ne $Comment.Trim()) {
    throw "Code, Text, and Comment must not contain leading or trailing whitespace."
}
foreach ($value in @($Code, $Text, $Comment)) {
    if ($value.Contains("`t") -or $value.Contains("`r") -or $value.Contains("`n")) {
        throw "Fields must not contain tab or newline characters."
    }
}

$dictionaryParent = Split-Path -Parent $DictionaryPath
if ($dictionaryParent) {
    New-Item -ItemType Directory -Force -Path $dictionaryParent | Out-Null
}

if (!(Test-Path $DictionaryPath)) {
    @(
        "# ArrowInput TSV dictionary"
        "# UTF-8 TSV: code`ttext`tcomment`tweight`tuser_weight"
    ) | Set-Content -Path $DictionaryPath -Encoding UTF8
}

if (!$AllowDuplicate) {
    foreach ($line in Get-Content -Path $DictionaryPath -Encoding UTF8) {
        if (!$line -or $line.StartsWith("#")) {
            continue
        }
        $fields = $line -split "`t", 6
        if ($fields.Count -ge 2 -and $fields[0] -eq $Code -and $fields[1] -eq $Text) {
            throw "Duplicate candidate already exists: code=$Code text=$Text. Use -AllowDuplicate to append anyway."
        }
    }
}

$entry = "{0}`t{1}`t{2}`t{3}`t{4}" -f $Code, $Text, $Comment, $Weight, $UserWeight
Add-Content -Path $DictionaryPath -Value $entry -Encoding UTF8

& (Join-Path $PSScriptRoot "validate_tsv_dictionary.ps1") -InputPath $DictionaryPath | Out-Host

if ($DatabasePath) {
    & (Join-Path $PSScriptRoot "import_tsv_to_sqlite.ps1") -InputPath $DictionaryPath -OutputPath $DatabasePath | Out-Host
    & (Join-Path $PSScriptRoot "query_sqlite.ps1") $Code -DatabasePath $DatabasePath | Out-Host
}

Write-Host "Added TSV entry: code=$Code text=$Text weight=$Weight user_weight=$UserWeight"
