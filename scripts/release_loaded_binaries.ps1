param(
    [string]$InstallDir = "D:\MyApp\ArrowInput",
    [string[]]$Name = @("ArrowInputTextService.dll", "sqlite3.dll"),
    [switch]$StopLockingProcesses
)

$ErrorActionPreference = "Stop"

if (!(Test-Path $InstallDir)) {
    throw "Install directory was not found: $InstallDir"
}

$source = @"
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Runtime.InteropServices;

public static class RestartManagerNative
{
    const int CCH_RM_SESSION_KEY = 32;
    const int RmRebootReasonNone = 0;

    [StructLayout(LayoutKind.Sequential)]
    public struct FILETIME
    {
        public uint dwLowDateTime;
        public uint dwHighDateTime;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct RM_UNIQUE_PROCESS
    {
        public int dwProcessId;
        public FILETIME ProcessStartTime;
    }

    public enum RM_APP_TYPE
    {
        RmUnknownApp = 0,
        RmMainWindow = 1,
        RmOtherWindow = 2,
        RmService = 3,
        RmExplorer = 4,
        RmConsole = 5,
        RmCritical = 1000
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public struct RM_PROCESS_INFO
    {
        public RM_UNIQUE_PROCESS Process;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 256)]
        public string strAppName;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 64)]
        public string strServiceShortName;
        public RM_APP_TYPE ApplicationType;
        public uint AppStatus;
        public uint TSSessionId;
        [MarshalAs(UnmanagedType.Bool)]
        public bool bRestartable;
    }

    [DllImport("rstrtmgr.dll", CharSet = CharSet.Unicode)]
    static extern int RmStartSession(out uint pSessionHandle, int dwSessionFlags, string strSessionKey);

    [DllImport("rstrtmgr.dll", CharSet = CharSet.Unicode)]
    static extern int RmRegisterResources(uint pSessionHandle, uint nFiles, string[] rgsFilenames, uint nApplications, IntPtr rgApplications, uint nServices, string[] rgsServiceNames);

    [DllImport("rstrtmgr.dll")]
    static extern int RmGetList(uint dwSessionHandle, out uint pnProcInfoNeeded, ref uint pnProcInfo, [In, Out] RM_PROCESS_INFO[] rgAffectedApps, ref uint lpdwRebootReasons);

    [DllImport("rstrtmgr.dll")]
    static extern int RmEndSession(uint pSessionHandle);

    public static RM_PROCESS_INFO[] GetLockingProcesses(string[] paths)
    {
        uint handle;
        string key = Guid.NewGuid().ToString("N").Substring(0, CCH_RM_SESSION_KEY);
        int result = RmStartSession(out handle, 0, key);
        if (result != 0)
        {
            throw new Win32Exception(result, "RmStartSession failed");
        }

        try
        {
            result = RmRegisterResources(handle, (uint)paths.Length, paths, 0, IntPtr.Zero, 0, null);
            if (result != 0)
            {
                throw new Win32Exception(result, "RmRegisterResources failed");
            }

            uint needed;
            uint count = 0;
            uint rebootReasons = RmRebootReasonNone;
            result = RmGetList(handle, out needed, ref count, null, ref rebootReasons);
            if (result == 234)
            {
                count = needed;
                RM_PROCESS_INFO[] processes = new RM_PROCESS_INFO[count];
                result = RmGetList(handle, out needed, ref count, processes, ref rebootReasons);
                if (result != 0)
                {
                    throw new Win32Exception(result, "RmGetList failed");
                }
                if (count == processes.Length)
                {
                    return processes;
                }
                RM_PROCESS_INFO[] trimmed = new RM_PROCESS_INFO[count];
                Array.Copy(processes, trimmed, count);
                return trimmed;
            }
            if (result != 0)
            {
                throw new Win32Exception(result, "RmGetList failed");
            }
            return new RM_PROCESS_INFO[0];
        }
        finally
        {
            RmEndSession(handle);
        }
    }
}
"@

if (!("RestartManagerNative" -as [type])) {
    Add-Type -TypeDefinition $source
}

$targets = @()
foreach ($itemName in $Name) {
    $path = Join-Path $InstallDir $itemName
    if (Test-Path $path) {
        $targets += [System.IO.Path]::GetFullPath($path)
    }
}

if ($targets.Count -eq 0) {
    Write-Host "No target binaries found in $InstallDir"
    return
}

$lockers = [RestartManagerNative]::GetLockingProcesses($targets)
if (!$lockers -or $lockers.Count -eq 0) {
    Write-Host "No locking processes found."
    return
}

Write-Host "Locking processes:"
$currentPid = $PID
$uniqueLockers = $lockers | Sort-Object { $_.Process.dwProcessId } -Unique
foreach ($locker in $uniqueLockers) {
    $pidValue = $locker.Process.dwProcessId
    $processName = $locker.strAppName
    if (!$processName) {
        try {
            $processName = (Get-Process -Id $pidValue -ErrorAction Stop).ProcessName
        } catch {
            $processName = "<unknown>"
        }
    }
    Write-Host ("  pid={0} name={1} type={2} restartable={3}" -f $pidValue, $processName, $locker.ApplicationType, $locker.bRestartable)
}

if (!$StopLockingProcesses) {
    Write-Host "Pass -StopLockingProcesses to stop these processes and release the loaded DLL."
    return
}

foreach ($locker in $uniqueLockers) {
    $pidValue = $locker.Process.dwProcessId
    if ($pidValue -eq $currentPid) {
        Write-Warning "Skipping current PowerShell process pid=$pidValue."
        continue
    }
    try {
        $process = Get-Process -Id $pidValue -ErrorAction Stop
        Write-Host ("Stopping pid={0} name={1}" -f $pidValue, $process.ProcessName)
        Stop-Process -Id $pidValue -Force -ErrorAction Stop
    } catch {
        Write-Warning "Could not stop pid=$pidValue : $($_.Exception.Message)"
    }
}

Start-Sleep -Milliseconds 500
$remaining = [RestartManagerNative]::GetLockingProcesses($targets)
if ($remaining -and $remaining.Count -gt 0) {
    Write-Warning "Some processes still lock the binaries:"
    foreach ($locker in ($remaining | Sort-Object { $_.Process.dwProcessId } -Unique)) {
        Write-Warning ("  pid={0} name={1}" -f $locker.Process.dwProcessId, $locker.strAppName)
    }
} else {
    Write-Host "Loaded ArrowInput binaries released."
}
