param()

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$testDir = Join-Path $root "build\tests\import_external_dictionary"
$installDir = Join-Path $testDir "install"
$configPath = Join-Path $testDir "config.ini"
$source = Join-Path $testDir "external.txt"
$rimeSource = Join-Path $testDir "external_rime.dict.yaml"
$experimentTsv = Join-Path $installDir "experiments\trial.tsv"
$experimentDb = Join-Path $installDir "experiments\trial.db"
$autoExperimentDb = Join-Path $installDir "experiments\auto_trial.db"
$promoteSource = Join-Path $testDir "promote_source.tsv"

if (Test-Path $testDir) {
    Remove-Item -Recurse -Force $testDir
}
New-Item -ItemType Directory -Force -Path $testDir | Out-Null
New-Item -ItemType Directory -Force -Path $installDir | Out-Null

& (Join-Path $PSScriptRoot "new_experiment_dictionary.ps1") `
    -Name trial `
    -InstallDir $installDir `
    -SourceTsv (Join-Path $root "data\seed_pinyin.tsv") `
    -Force | Out-Null

@(
    "# external source"
    "ni ni ni 1000 0"
    "ceshi ceshi ce-shi 1200 0"
    "xin xin xin 1100 0"
) | Set-Content -Path $source -Encoding UTF8
@(
    "# promote"
    "ni`tni`tni`t100`t0"
) | Set-Content -Path $promoteSource -Encoding UTF8

$before = [System.IO.File]::ReadAllText($experimentTsv, [System.Text.Encoding]::UTF8)
& (Join-Path $PSScriptRoot "import_external_dictionary_to_experiment.ps1") `
    -Name trial `
    -SourcePath $source `
    -InstallDir $installDir `
    -Delimiter space `
    -PreviewOnly
$afterPreview = [System.IO.File]::ReadAllText($experimentTsv, [System.Text.Encoding]::UTF8)
if ($before -ne $afterPreview) {
    throw "Preview import should not modify the experiment TSV."
}

& (Join-Path $PSScriptRoot "import_external_dictionary_to_experiment.ps1") `
    -Name trial `
    -SourcePath $source `
    -InstallDir $installDir `
    -ConfigPath $configPath `
    -Delimiter space `
    -PromoteSourceTsv $promoteSource `
    -UseNow

$query = & (Join-Path $PSScriptRoot "query_sqlite.ps1") ni ceshi xin -DatabasePath $experimentDb
$queryText = ($query -join "`n")
if ($queryText -notmatch "\[ceshi\] 1 candidate\(s\)" -or
    $queryText -notmatch "\[xin\] 1 candidate\(s\)" -or
    $queryText -notmatch "1\. ni ni weight=2000 user_weight=0") {
    Write-Host $queryText
    throw "Imported external rows were not queryable."
}

$configText = Get-Content -Path $configPath -Raw
if ($configText -notmatch [regex]::Escape($experimentDb)) {
    Write-Host $configText
    throw "Import with -UseNow did not update config."
}

@(
    "# Rime source"
    "---"
    "name: sample"
    "..."
    "rimeci`trime ci`t1300"
) | Set-Content -Path $rimeSource -Encoding UTF8

& (Join-Path $PSScriptRoot "import_external_dictionary_to_experiment.ps1") `
    -Name trial `
    -SourcePath $rimeSource `
    -SourceFormat rime `
    -InstallDir $installDir | Out-Null

$rimeQuery = & (Join-Path $PSScriptRoot "query_sqlite.ps1") rimeci -DatabasePath $experimentDb
$rimeQueryText = ($rimeQuery -join "`n")
if ($rimeQueryText -notmatch "\[rimeci\] 1 candidate\(s\)") {
    Write-Host $rimeQueryText
    throw "Imported Rime row was not queryable."
}

& (Join-Path $PSScriptRoot "import_external_dictionary_to_experiment.ps1") `
    -Name auto_trial `
    -SourcePath $source `
    -InstallDir $installDir `
    -Delimiter space `
    -SeedTsv (Join-Path $root "data\seed_pinyin.tsv") `
    -CreateIfMissing | Out-Null

$autoQuery = & (Join-Path $PSScriptRoot "query_sqlite.ps1") ceshi -DatabasePath $autoExperimentDb
$autoQueryText = ($autoQuery -join "`n")
if ($autoQueryText -notmatch "\[ceshi\] 1 candidate\(s\)") {
    Write-Host $autoQueryText
    throw "CreateIfMissing import was not queryable."
}

Write-Host "import_external_dictionary_to_experiment tests passed."
