param(
    [string]$Out = "build\flux.png",
    [string]$Title = "flux.lua",
    [int]$TimeoutMs = 8000,
    [switch]$NoTopmost,
    [switch]$KeepTopmost
)

Add-Type -AssemblyName System.Drawing

Add-Type @"
using System;
using System.Runtime.InteropServices;
using System.Text;

public class FluxShot {
    public delegate bool EnumProc(IntPtr hwnd, IntPtr lparam);

    [DllImport("user32.dll")]
    public static extern bool SetProcessDpiAwarenessContext(IntPtr value);

    [DllImport("user32.dll")]
    public static extern bool EnumWindows(EnumProc proc, IntPtr lparam);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetWindowText(IntPtr hwnd, StringBuilder text, int max);

    [DllImport("user32.dll")]
    public static extern bool IsWindowVisible(IntPtr hwnd);

    [DllImport("user32.dll")]
    public static extern bool GetClientRect(IntPtr hwnd, out RECT rect);

    [DllImport("user32.dll")]
    public static extern bool ClientToScreen(IntPtr hwnd, ref POINT point);

    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr hwnd);

    [DllImport("user32.dll")]
    public static extern bool BringWindowToTop(IntPtr hwnd);

    [DllImport("user32.dll")]
    public static extern bool SetWindowPos(IntPtr hwnd, IntPtr insertAfter, int x, int y, int cx, int cy, uint flags);

    [DllImport("user32.dll")]
    public static extern bool PrintWindow(IntPtr hwnd, IntPtr hdc, uint flags);

    [StructLayout(LayoutKind.Sequential)]
    public struct RECT {
        public int Left;
        public int Top;
        public int Right;
        public int Bottom;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct POINT {
        public int X;
        public int Y;
    }
}
"@

[void][FluxShot]::SetProcessDpiAwarenessContext([IntPtr](-4))

function Find-FluxWindow([string]$title) {
    $script:fluxFound = [IntPtr]::Zero
    $callback = [FluxShot+EnumProc]{
        param($hwnd, $param)
        if ([FluxShot]::IsWindowVisible($hwnd)) {
            $buffer = New-Object System.Text.StringBuilder 512
            [void][FluxShot]::GetWindowText($hwnd, $buffer, 512)
            if ($buffer.ToString() -eq $title) {
                $script:fluxFound = $hwnd
                return $false
            }
        }
        return $true
    }
    [void][FluxShot]::EnumWindows($callback, [IntPtr]::Zero)
    return $script:fluxFound
}

$hwnd = [IntPtr]::Zero
$deadline = (Get-Date).AddMilliseconds($TimeoutMs)
while ($hwnd -eq [IntPtr]::Zero -and (Get-Date) -lt $deadline) {
    $hwnd = Find-FluxWindow $Title
    if ($hwnd -eq [IntPtr]::Zero) {
        Start-Sleep -Milliseconds 100
    }
}
if ($hwnd -eq [IntPtr]::Zero) {
    throw "window '$Title' not found"
}

$topmost = [IntPtr](-1)
$notTopmost = [IntPtr](-2)
$noMoveNoSize = 0x0002 -bor 0x0001 -bor 0x0010
if (-not $NoTopmost) {
    [void][FluxShot]::SetWindowPos($hwnd, $topmost, 0, 0, 0, 0, $noMoveNoSize)
    [void][FluxShot]::BringWindowToTop($hwnd)
    [void][FluxShot]::SetForegroundWindow($hwnd)
    Start-Sleep -Milliseconds 700
} else {
    Start-Sleep -Milliseconds 150
}

$client = New-Object FluxShot+RECT
[void][FluxShot]::GetClientRect($hwnd, [ref]$client)
$width = $client.Right - $client.Left
$height = $client.Bottom - $client.Top
if ($width -le 0 -or $height -le 0) {
    throw "invalid client size"
}

$origin = New-Object FluxShot+POINT
$origin.X = 0
$origin.Y = 0
[void][FluxShot]::ClientToScreen($hwnd, [ref]$origin)

function New-PrintWindowBitmap {
    $bmp = New-Object System.Drawing.Bitmap $width, $height
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $hdc = $g.GetHdc()
    $ok = [FluxShot]::PrintWindow($hwnd, $hdc, 2)
    $g.ReleaseHdc($hdc)
    $g.Dispose()
    Write-Host ("printwindow={0}" -f $ok)
    return ,$bmp
}

$bitmap = New-Object System.Drawing.Bitmap $width, $height
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
$size = New-Object System.Drawing.Size $width, $height
$captured = $false
try {
    $graphics.CopyFromScreen($origin.X, $origin.Y, 0, 0, $size)
    $captured = $true
} catch {
    Write-Output "CopyFromScreen failed; falling back to PrintWindow"
}
$graphics.Dispose()
if (-not $captured) {
    $bitmap.Dispose()
    $bitmap = New-PrintWindowBitmap
}
if (-not $NoTopmost -and -not $KeepTopmost) {
    [void][FluxShot]::SetWindowPos($hwnd, $notTopmost, 0, 0, 0, 0, $noMoveNoSize)
}

$samples = @()
$samples += ,@(10, 10)
$samples += ,@([int]($width / 2), [int]($height / 2))
$samples += ,@(($width - 10), ($height - 10))
$lit = 0
foreach ($sample in $samples) {
    $pixel = $bitmap.GetPixel($sample[0], $sample[1])
    if ($pixel.R -gt 8 -or $pixel.G -gt 8 -or $pixel.B -gt 8) {
        $lit++
    }
}
if ($lit -eq 0) {
    $bitmap.Dispose()
    $bitmap = New-PrintWindowBitmap
    $pixel = $bitmap.GetPixel([int]($width / 2), [int]($height / 2))
    if ($pixel.R -le 8 -and $pixel.G -le 8 -and $pixel.B -le 8) {
        $bitmap.Dispose()
        throw "captured image is black; the window may be occluded or minimized"
    }
}

$directory = Split-Path -Parent $Out
if ([string]::IsNullOrEmpty($directory)) {
    $directory = "."
}
if (-not (Test-Path $directory)) {
    New-Item -ItemType Directory -Force -Path $directory | Out-Null
}
$path = Join-Path (Resolve-Path $directory).Path (Split-Path -Leaf $Out)
$bitmap.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
$bitmap.Dispose()
Write-Output "saved $path"
