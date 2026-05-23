param(
    [ValidateSet("tsv", "sqlite")]
    [string]$Type = "sqlite",
    [string]$Path,
    [string]$InstallDir = "D:\MyApp\ArrowInput",
    [string]$ConfigPath,
    [switch]$AllowMissing
)

$ErrorActionPreference = "Stop"

if (!$ConfigPath) {
    $configDir = Join-Path $env:APPDATA "ArrowInput"
    $ConfigPath = Join-Path $configDir "config.ini"
}

if (!$Path) {
    if ($Type -eq "sqlite") {
        $Path = Join-Path $InstallDir "sample_dictionary.db"
    } else {
        $Path = Join-Path $InstallDir "sample_dictionary.tsv"
    }
}

if (!$AllowMissing -and !(Test-Path $Path)) {
    throw "Dictionary path does not exist: $Path. Use -AllowMissing to write it anyway."
}

$configParent = Split-Path -Parent $ConfigPath
New-Item -ItemType Directory -Force -Path $configParent | Out-Null

$nativeMethods = @"
using System;
using System.Runtime.InteropServices;

public static class ArrowInputIni {
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    public static extern bool WritePrivateProfileString(string section, string key, string value, string fileName);
}
"@

if (-not ("ArrowInputIni" -as [type])) {
    Add-Type $nativeMethods
}

function Set-IniValue {
    param(
        [string]$Section,
        [string]$Key,
        [string]$Value
    )

    if (-not [ArrowInputIni]::WritePrivateProfileString($Section, $Key, $Value, $ConfigPath)) {
        throw "Failed to write $Key to $ConfigPath"
    }
}

if (!(Test-Path $ConfigPath)) {
    New-Item -ItemType File -Force -Path $ConfigPath | Out-Null
    Set-IniValue "general" "mode" "wubi"
    Set-IniValue "general" "candidate_page_size" "5"
    Set-IniValue "general" "max_candidates_per_query" "30"
    Set-IniValue "general" "prefix_candidates" "1"
    Set-IniValue "general" "chinese_punctuation" "1"
    Set-IniValue "general" "full_shape" "0"
    Set-IniValue "general" "dictionary_type" "tsv"
    Set-IniValue "general" "dictionary_path" ""
    Set-IniValue "debug" "capture_keys" "0"
    Set-IniValue "debug" "preedit_markers" "1"
}

Set-IniValue "general" "dictionary_type" $Type
Set-IniValue "general" "dictionary_path" $Path

Write-Host "ArrowInput dictionary set: type=$Type path=$Path"
Write-Host "Config: $ConfigPath"
