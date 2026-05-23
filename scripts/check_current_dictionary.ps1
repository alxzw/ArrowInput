[CmdletBinding(PositionalBinding = $false)]
param(
    [string]$InstallDir = "D:\MyApp\ArrowInput",
    [string]$ConfigPath,
    [int]$Limit = 5,
    [int]$Iterations = 5,
    [switch]$SkipBenchmark,
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$Code = @("ni", "nih", "yi", "zhongguo")
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
if ($Limit -lt 1) {
    throw "-Limit must be at least 1."
}
if ($Iterations -lt 1) {
    throw "-Iterations must be at least 1."
}

$configDir = Split-Path -Parent $ConfigPath
$dictionaryType = Read-IniValue -Path $ConfigPath -Section "general" -Key "dictionary_type"
$dictionaryPath = Read-IniValue -Path $ConfigPath -Section "general" -Key "dictionary_path"
$maxCandidatesPerQuery = Read-IniValue -Path $ConfigPath -Section "general" -Key "max_candidates_per_query"
$prefixCandidates = Read-IniValue -Path $ConfigPath -Section "general" -Key "prefix_candidates"
$fuzzyPinyin = Read-IniValue -Path $ConfigPath -Section "general" -Key "fuzzy_pinyin"
if (!$dictionaryType) {
    $dictionaryType = "tsv"
}
if (!$prefixCandidates) {
    $prefixCandidates = "1"
}
if (!$fuzzyPinyin) {
    $fuzzyPinyin = "0"
}

$resolvedDictionaryPath = Resolve-DictionaryPath -Path $dictionaryPath -BasePath $configDir

Write-Output "ArrowInput current dictionary check"
Write-Output "Config: $ConfigPath"
Write-Output "dictionary_type=$dictionaryType"
Write-Output "dictionary_path=$(if ($resolvedDictionaryPath) { $resolvedDictionaryPath } else { '<empty; demo fallback>' })"
Write-Output "max_candidates_per_query=$(if ($maxCandidatesPerQuery) { $maxCandidatesPerQuery } else { '30' })"
Write-Output "prefix_candidates=$prefixCandidates"
Write-Output "fuzzy_pinyin=$fuzzyPinyin"

if ($resolvedDictionaryPath) {
    if (!(Test-Path $resolvedDictionaryPath)) {
        throw "Configured dictionary was not found: $resolvedDictionaryPath"
    }
    $dictionaryItem = Get-Item $resolvedDictionaryPath
    Write-Output ("dictionary_file=present size={0} modified={1}" -f $dictionaryItem.Length, $dictionaryItem.LastWriteTime)
}

Write-Output ""
Write-Output "Probe samples:"
& (Join-Path $PSScriptRoot "probe_current_dictionary.ps1") -InstallDir $InstallDir -ConfigPath $ConfigPath -Limit $Limit @Code

if ($dictionaryType -ieq "sqlite" -and $resolvedDictionaryPath) {
    Write-Output ""
    Write-Output "SQLite structure and sample distribution:"
    if ($prefixCandidates -ne "0") {
        & (Join-Path $PSScriptRoot "inspect_sqlite_dictionary.ps1") -DatabasePath $resolvedDictionaryPath -Limit $Limit -TopCodes 5 -Metadata -Prefix @Code
    } else {
        & (Join-Path $PSScriptRoot "inspect_sqlite_dictionary.ps1") -DatabasePath $resolvedDictionaryPath -Limit $Limit -TopCodes 5 -Metadata @Code
    }

    if (!$SkipBenchmark) {
        Write-Output ""
        Write-Output "SQLite lookup benchmark:"
        if ($prefixCandidates -ne "0") {
            & (Join-Path $PSScriptRoot "benchmark_sqlite_dictionary.ps1") -DatabasePath $resolvedDictionaryPath -Limit $Limit -Iterations $Iterations -Prefix @Code
        } else {
            & (Join-Path $PSScriptRoot "benchmark_sqlite_dictionary.ps1") -DatabasePath $resolvedDictionaryPath -Limit $Limit -Iterations $Iterations @Code
        }
    }
}

Write-Output ""
Write-Output "current dictionary check passed."
