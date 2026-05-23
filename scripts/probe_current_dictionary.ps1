[CmdletBinding(PositionalBinding = $false)]
param(
    [string]$InstallDir = "D:\MyApp\ArrowInput",
    [string]$ConfigPath,
    [int]$Limit = 0,
    [switch]$NoPrefix,
    [switch]$FuzzyPinyin,
    [switch]$NoFuzzyPinyin,
    [switch]$Stats,
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$Code
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

$root = Split-Path -Parent $PSScriptRoot
$probeCandidates = @(
    (Join-Path $InstallDir "AlgorithmProbe.exe"),
    (Join-Path $PSScriptRoot "AlgorithmProbe.exe"),
    (Join-Path $root "build\tools\AlgorithmProbe\x64\Debug\AlgorithmProbe.exe"),
    (Join-Path $root "build\tools\AlgorithmProbe\x64\Release\AlgorithmProbe.exe")
)
$probe = $probeCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (!$probe) {
    throw "AlgorithmProbe.exe was not found. Build it or deploy it to $InstallDir."
}

$configDir = Split-Path -Parent $ConfigPath
$dictionaryType = Read-IniValue -Path $ConfigPath -Section "general" -Key "dictionary_type"
$dictionaryPath = Read-IniValue -Path $ConfigPath -Section "general" -Key "dictionary_path"
$maxCandidatesPerQuery = Read-IniValue -Path $ConfigPath -Section "general" -Key "max_candidates_per_query"
$prefixCandidates = Read-IniValue -Path $ConfigPath -Section "general" -Key "prefix_candidates"
$fuzzyPinyinConfig = Read-IniValue -Path $ConfigPath -Section "general" -Key "fuzzy_pinyin"

if (!$dictionaryType) {
    $dictionaryType = "tsv"
}

$resolvedDictionaryPath = Resolve-DictionaryPath -Path $dictionaryPath -BasePath $configDir
if ($resolvedDictionaryPath -and !(Test-Path $resolvedDictionaryPath)) {
    throw "Configured dictionary was not found: $resolvedDictionaryPath"
}

$effectiveLimit = $Limit
if ($effectiveLimit -le 0 -and $maxCandidatesPerQuery) {
    $parsedLimit = 0
    if ([int]::TryParse($maxCandidatesPerQuery, [ref]$parsedLimit) -and $parsedLimit -gt 0) {
        $effectiveLimit = $parsedLimit
    }
}

if (!$Stats -and (!$Code -or $Code.Count -eq 0)) {
    throw "Provide one or more codes, or pass -Stats."
}

$arguments = @("--type", $dictionaryType)
if ($resolvedDictionaryPath) {
    $arguments += @("--dict", $resolvedDictionaryPath)
}
if ($effectiveLimit -gt 0) {
    $arguments += @("--limit", $effectiveLimit.ToString())
}
if ($NoPrefix -or $prefixCandidates -eq "0") {
    $arguments += "--no-prefix"
}
if ($NoFuzzyPinyin) {
    $arguments += "--no-fuzzy-pinyin"
} elseif ($FuzzyPinyin -or $fuzzyPinyinConfig -eq "1") {
    $arguments += "--fuzzy-pinyin"
}
if ($Stats) {
    $arguments += "--stats"
}
if ($Code) {
    $arguments += $Code
}

& $probe @arguments
