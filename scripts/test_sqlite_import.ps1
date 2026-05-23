param(
    [string]$OutputPath = "build\tests\sample_dictionary.db"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$dbPath = Join-Path $root $OutputPath
$dictionary = Join-Path $root "tools\AlgorithmProbe\sample_dictionary.tsv"
$checker = Join-Path $root "tools\DictionaryTools\check_sqlite_import.py"

if (!(Test-Path $checker)) {
    throw "SQLite import checker was not found: $checker"
}

& (Join-Path $PSScriptRoot "import_tsv_to_sqlite.ps1") -InputPath $dictionary -OutputPath $dbPath

& python $checker $dbPath
