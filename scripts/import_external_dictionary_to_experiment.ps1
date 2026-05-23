param(
    [Parameter(Mandatory = $true)]
    [string]$Name,

    [Parameter(Mandatory = $true)]
    [string]$SourcePath,

    [ValidateSet("text", "tsv", "rime")]
    [string]$SourceFormat = "text",

    [string]$InstallDir = "D:\MyApp\ArrowInput",

    [string]$ConfigPath,

    [string]$Columns = "code,text,comment,weight,user_weight",

    [ValidateSet("auto", "tab", "space")]
    [string]$Delimiter = "auto",

    [string]$DefaultWeight = "",

    [int]$HotThreshold = 50,

    [string]$SeedTsv,

    [string]$PromoteSourceTsv,

    [int]$PromoteBoost = 1000,

    [switch]$CreateIfMissing,

    [switch]$AllowDuplicate,

    [switch]$PreviewOnly,

    [switch]$UseNow
)

$ErrorActionPreference = "Stop"

if ($Name.IndexOfAny([System.IO.Path]::GetInvalidFileNameChars()) -ge 0) {
    throw "Experiment name contains invalid file name characters: $Name"
}
if (!(Test-Path $SourcePath)) {
    throw "Source dictionary was not found: $SourcePath"
}

$experimentDir = Join-Path $InstallDir "experiments"
$experimentTsv = Join-Path $experimentDir "$Name.tsv"
$experimentDb = Join-Path $experimentDir "$Name.db"

if (!(Test-Path $experimentTsv)) {
    if (!$CreateIfMissing) {
        throw "Experiment TSV was not found: $experimentTsv"
    }
    $newArgs = @{
        Name = $Name
        InstallDir = $InstallDir
    }
    if ($SeedTsv) {
        $newArgs.SourceTsv = $SeedTsv
    }
    & (Join-Path $PSScriptRoot "new_experiment_dictionary.ps1") @newArgs | Out-Host
    Write-Host "Created missing experiment dictionary: $Name"
}

$sourceTsv = $SourcePath
$tempDir = $null
if ($SourceFormat -eq "text" -or $SourceFormat -eq "rime") {
    $tempDir = Join-Path $env:TEMP ("ArrowInput_import_{0}_{1}" -f $Name, (Get-Date -Format "yyyyMMdd_HHmmss"))
    New-Item -ItemType Directory -Force -Path $tempDir | Out-Null
    $sourceTsv = Join-Path $tempDir "converted.tsv"
    $convertArgs = @{
        InputPath = $SourcePath
        OutputPath = $sourceTsv
        SourceFormat = if ($SourceFormat -eq "rime") { "rime" } else { "simple" }
    }
    if ($SourceFormat -eq "text") {
        $convertArgs.Columns = $Columns
        $convertArgs.Delimiter = $Delimiter
        if ($DefaultWeight -ne "") {
            $convertArgs.DefaultWeight = $DefaultWeight
        }
    }
    & (Join-Path $PSScriptRoot "convert_text_dictionary_to_tsv.ps1") @convertArgs | Out-Host
}

& (Join-Path $PSScriptRoot "validate_tsv_dictionary.ps1") -InputPath $sourceTsv | Out-Host
& (Join-Path $PSScriptRoot "preview_tsv_merge.ps1") -TargetPath $experimentTsv -SourcePath $sourceTsv -HotThreshold $HotThreshold | Out-Host

if ($PreviewOnly) {
    Write-Host "Preview only; experiment dictionary was not modified."
    Write-Host "Experiment TSV: $experimentTsv"
    Write-Host "Source TSV: $sourceTsv"
    return
}

& (Join-Path $PSScriptRoot "backup_experiment_dictionary.ps1") -Name $Name -InstallDir $InstallDir | Out-Host

$mergeArgs = @{
    TargetPath = $experimentTsv
    SourcePath = $sourceTsv
}
if ($AllowDuplicate) {
    $mergeArgs.AllowDuplicate = $true
}
& (Join-Path $PSScriptRoot "merge_tsv_dictionary.ps1") @mergeArgs | Out-Host

if ($PromoteSourceTsv) {
    if (!(Test-Path $PromoteSourceTsv)) {
        throw "Promotion source TSV was not found: $PromoteSourceTsv"
    }
    & (Join-Path $PSScriptRoot "promote_tsv_candidates.ps1") `
        -TargetPath $experimentTsv `
        -SourcePath $PromoteSourceTsv `
        -Boost $PromoteBoost | Out-Host
}

$rebuildArgs = @{
    Name = $Name
    InstallDir = $InstallDir
}
if ($ConfigPath) {
    $rebuildArgs.ConfigPath = $ConfigPath
}
if ($UseNow) {
    $rebuildArgs.UseNow = $true
}
& (Join-Path $PSScriptRoot "rebuild_experiment_dictionary.ps1") @rebuildArgs | Out-Host

Write-Host "Imported external dictionary into experiment: $Name"
Write-Host "Experiment TSV: $experimentTsv"
Write-Host "Experiment DB: $experimentDb"
Write-Host "Source TSV: $sourceTsv"
