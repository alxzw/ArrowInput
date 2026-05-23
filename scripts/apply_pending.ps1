param(
    [string]$InstallDir = "D:\MyApp\ArrowInput"
)

$ErrorActionPreference = "Stop"

if (!(Test-Path $InstallDir)) {
    throw "Install directory was not found: $InstallDir"
}

$pendingFiles = Get-ChildItem -Path $InstallDir -Filter "*.dll.pending" -File -ErrorAction SilentlyContinue
if (!$pendingFiles) {
    Write-Host "No pending binaries found in $InstallDir"
    return
}

foreach ($pending in $pendingFiles) {
    $targetName = $pending.Name.Substring(0, $pending.Name.Length - ".pending".Length)
    $target = Join-Path $InstallDir $targetName
    $temp = "$target.apply_tmp"

    if (Test-Path $temp) {
        Remove-Item -Force $temp
    }

    Copy-Item -Force $pending.FullName $temp
    try {
        Move-Item -Force $temp $target
    } catch {
        if (Test-Path $temp) {
            Remove-Item -Force $temp -ErrorAction SilentlyContinue
        }
        throw "Could not apply $($pending.FullName). The target may still be loaded: $target"
    }

    Remove-Item -Force $pending.FullName
    Write-Host "Applied pending binary: $target"
}
