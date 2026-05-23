[CmdletBinding(PositionalBinding = $false)]
param(
    [string]$InstallDir = "D:\MyApp\ArrowInput",
    [string]$ConfigPath,

    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$Arguments
)

$ErrorActionPreference = "Stop"

function Read-IniValue {
    param(
        [string]$Path,
        [string]$Section,
        [string]$Key
    )

    if (!(Test-Path $Path)) {
        return ""
    }

    $inSection = $false
    foreach ($line in Get-Content -Path $Path) {
        $trimmed = $line.Trim()
        if (!$trimmed -or $trimmed.StartsWith(";") -or $trimmed.StartsWith("#")) {
            continue
        }
        if ($trimmed.StartsWith("[") -and $trimmed.EndsWith("]")) {
            $inSection = $trimmed.Substring(1, $trimmed.Length - 2) -ieq $Section
            continue
        }
        if ($inSection) {
            $separator = $trimmed.IndexOf("=")
            if ($separator -gt 0) {
                $name = $trimmed.Substring(0, $separator).Trim()
                if ($name -ieq $Key) {
                    return $trimmed.Substring($separator + 1).Trim()
                }
            }
        }
    }
    return ""
}

function Resolve-DictionaryPath {
    param(
        [string]$Path,
        [string]$BasePath
    )

    if (!$Path) {
        return ""
    }
    if ([System.IO.Path]::IsPathRooted($Path)) {
        return $Path
    }
    return [System.IO.Path]::GetFullPath((Join-Path $BasePath $Path))
}

if (!$ConfigPath) {
    $ConfigPath = Join-Path (Join-Path $env:APPDATA "ArrowInput") "config.ini"
}
if (!(Test-Path $ConfigPath)) {
    throw "Config file was not found: $ConfigPath"
}
if (!$Arguments -or $Arguments.Count -eq 0) {
    throw "A command is required. Examples: list, user-stats, deleted, undo-delete, undo-learning."
}

$configDir = Split-Path -Parent $ConfigPath
$dictionaryType = Read-IniValue -Path $ConfigPath -Section "general" -Key "dictionary_type"
$dictionaryPath = Read-IniValue -Path $ConfigPath -Section "general" -Key "dictionary_path"
if (!$dictionaryType) {
    $dictionaryType = "tsv"
}
if ($dictionaryType -ine "sqlite") {
    throw "Current dictionary is not sqlite: dictionary_type=$dictionaryType"
}

$resolvedDictionaryPath = Resolve-DictionaryPath -Path $dictionaryPath -BasePath $configDir
if (!$resolvedDictionaryPath) {
    throw "Current sqlite dictionary_path is empty."
}
if (!(Test-Path $resolvedDictionaryPath)) {
    throw "Configured dictionary was not found: $resolvedDictionaryPath"
}

$managerCandidates = @(
    (Join-Path $InstallDir "manage_user_dictionary.ps1"),
    (Join-Path $PSScriptRoot "manage_user_dictionary.ps1")
)
$manager = $managerCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (!$manager) {
    throw "manage_user_dictionary.ps1 was not found. Checked: $($managerCandidates -join ', ')"
}

& $manager -DatabasePath $resolvedDictionaryPath @Arguments
