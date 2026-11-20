param(
    [string]$Title = "flux.stability",
    [string]$Luamake = "luamake",
    [int]$TimeoutMs = 8000
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
using System.Text;
public class StabilityWindowFinder {
    public delegate bool EnumProc(IntPtr hwnd, IntPtr lparam);
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc proc, IntPtr lparam);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetWindowText(IntPtr hwnd, StringBuilder text, int max);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint pid);
    [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr hwnd, IntPtr after, int x, int y, int cx, int cy, uint flags);
}
"@

$root = Split-Path -Parent $PSScriptRoot
$luamakePath = (Get-Command $Luamake -ErrorAction Stop).Source
$process = Start-Process -FilePath $luamakePath -ArgumentList "lua", "tools/stability_window.lua" -WorkingDirectory $root -PassThru
if ($null -eq $process) {
    throw "failed to start stability window"
}

$window = [IntPtr]::Zero
try {
    $deadline = (Get-Date).AddMilliseconds($TimeoutMs)
    while ($window -eq [IntPtr]::Zero -and (Get-Date) -lt $deadline) {
        $script:found = [IntPtr]::Zero
        $callback = [StabilityWindowFinder+EnumProc]{
            param($hwnd, $param)
            if ([StabilityWindowFinder]::IsWindowVisible($hwnd)) {
                $buffer = New-Object System.Text.StringBuilder 512
                [void][StabilityWindowFinder]::GetWindowText($hwnd, $buffer, 512)
                if ($buffer.ToString() -eq $Title) {
                    $script:found = $hwnd
                    return $false
                }
            }
            return $true
        }
        [void][StabilityWindowFinder]::EnumWindows($callback, [IntPtr]::Zero)
        $window = $script:found
        if ($window -eq [IntPtr]::Zero) {
            Start-Sleep -Milliseconds 100
        }
    }
    if ($window -eq [IntPtr]::Zero) {
        throw "window '$Title' not found"
    }

    $windowPid = [uint32]0
    [void][StabilityWindowFinder]::GetWindowThreadProcessId($window, [ref]$windowPid)

    Start-Sleep -Milliseconds 1500
    [void][StabilityWindowFinder]::SetWindowPos($window, [IntPtr](-1), 0, 0, 0, 0, 0x0013)
    Start-Sleep -Milliseconds 700
    $shot1 = Join-Path $root "build/stability_1.png"
    $shot2 = Join-Path $root "build/stability_2.png"
    & (Join-Path $PSScriptRoot "shot.ps1") -Title $Title -Out $shot1 -KeepTopmost | Out-Null
    Start-Sleep -Milliseconds 250
    & (Join-Path $PSScriptRoot "shot.ps1") -Title $Title -Out $shot2 -KeepTopmost | Out-Null

    $a = [System.Drawing.Bitmap]::FromFile($shot1)
    $b = [System.Drawing.Bitmap]::FromFile($shot2)
    try {
        if ($a.Width -ne $b.Width -or $a.Height -ne $b.Height) {
            throw "screenshot size changed"
        }
        $diff = 0
        $max = 0
        # DWM rounded borders/shadows, cursor pixels, and adjacent windows can
        # touch the outermost pixels; only the app-drawn content must be stable.
        $margin = 16
        for ($y = $margin; $y -lt ($a.Height - $margin); $y += 1) {
            for ($x = $margin; $x -lt ($a.Width - $margin); $x += 1) {
                $p = $a.GetPixel($x, $y)
                $q = $b.GetPixel($x, $y)
                $d = [math]::Abs($p.R - $q.R) + [math]::Abs($p.G - $q.G) + [math]::Abs($p.B - $q.B)
                if ($d -gt 0) {
                    $diff += 1
                    if ($d -gt $max) { $max = $d }
                }
            }
        }
        if ($diff -ne 0) {
            throw "static content changed: $diff pixels differ inside ${margin}px margins (max channel delta $max)"
        }
        Write-Output "stability ok diff=0 (content area, ${margin}px margins ignored)"
    }
    finally {
        $a.Dispose()
        $b.Dispose()
        if ($windowPid -ne 0) {
            Stop-Process -Id $windowPid -Force -ErrorAction SilentlyContinue
        }
    }
}
finally {
    if (-not $process.HasExited) {
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
    }
}
