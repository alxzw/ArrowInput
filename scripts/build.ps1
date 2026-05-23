param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$msbuild = "${env:ProgramFiles}\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\amd64\MSBuild.exe"

if (!(Test-Path $msbuild)) {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $installPath = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
        if ($installPath) {
            $candidate = Join-Path $installPath "MSBuild\Current\Bin\amd64\MSBuild.exe"
            if (Test-Path $candidate) {
                $msbuild = $candidate
            }
        }
    }
}

if (!(Test-Path $msbuild)) {
    throw "MSBuild.exe was not found. Install Visual Studio 2022 with Desktop development with C++."
}

& $msbuild (Join-Path $root "ArrowInput.sln") /m /p:Configuration=$Configuration /p:Platform=x64
