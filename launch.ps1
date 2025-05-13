param(
    [string]$AppPath = (Resolve-Path -Path "x64\Debug").Path,
    [switch]$UseClassicConsole = $false,
    [switch]$UseSimpleFinder = $false
)

# Function to position a window
function Set-WindowPosition {
    param(
        [string]$ProcessName,
        [int]$X,
        [int]$Y,
        [int]$Width,
        [int]$Height,
        [int]$MaxRetries = 50,
        [int]$RetryIntervalMs = 100
    )
    
    Add-Type @"
    using System;
    using System.Runtime.InteropServices;
    using System.Text;
    using System.Collections.Generic;
    using System.Diagnostics;
    
    public class Win32 {
        [DllImport("user32.dll")]
        [return: MarshalAs(UnmanagedType.Bool)]
        public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);
        
        [DllImport("user32.dll")]
        [return: MarshalAs(UnmanagedType.Bool)]
        public static extern bool MoveWindow(IntPtr hWnd, int X, int Y, int nWidth, int nHeight, bool bRepaint);
        
        [DllImport("user32.dll")]
        public static extern IntPtr FindWindow(string lpClassName, string lpWindowName);
        
        [DllImport("user32.dll")]
        public static extern bool SetForegroundWindow(IntPtr hWnd);
        
        [DllImport("user32.dll")]
        public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);

        [StructLayout(LayoutKind.Sequential)]
        public struct RECT {
            public int Left;
            public int Top;
            public int Right;
            public int Bottom;
        }
        
        // Helper method to find windows by process name directly from C#
        public static IntPtr FindWindowByProcessName(string processName) {
            try {
                Process[] processes = Process.GetProcessesByName(processName);
                if (processes.Length == 0) {
                    return IntPtr.Zero;
                }
                
                foreach (Process proc in processes) {
                    if (proc.MainWindowHandle != IntPtr.Zero) {
                        return proc.MainWindowHandle;
                    }
                }
            }
            catch (Exception) {
                // Silently fail and let PowerShell handle it
            }
            return IntPtr.Zero;
        }
    }
"@
    
    # Retry loop to find and position the window
    $retryCount = 0
    $success = $false
    
    Write-Host "Looking for window for process '$ProcessName'..."
    
    while (-not $success -and $retryCount -lt $MaxRetries) {
        $handle = $null
        
        try {
            if ($UseSimpleFinder) {
                # Try to get the main window handle directly from Process object
                $proc = Get-Process -Name $ProcessName -ErrorAction SilentlyContinue
                if ($proc) {
                    Write-Host "Found process $ProcessName with ID: $($proc.Id)"
                    # Try to get its main window handle
                    $handle = $proc.MainWindowHandle
                    if ($handle -ne [IntPtr]::Zero) {
                        Write-Host "Found main window handle through .NET: $handle"
                    }
                }
            }
            else {
                # Use our built-in C# method that's safer than delegate conversion
                $handle = [Win32]::FindWindowByProcessName($ProcessName)
                if ($handle -ne [IntPtr]::Zero) {
                    Write-Host "Found window handle using FindWindowByProcessName: $handle"
                }
            }
            
            if ($handle -eq [IntPtr]::Zero) {
                # Fall back to traditional method of finding by window title
                $handle = [Win32]::FindWindow("ConsoleWindowClass", "*$ProcessName*")
                if ($handle -ne [IntPtr]::Zero) {
                    Write-Host "Found window handle using ConsoleWindowClass: $handle"
                }
                
                if ($handle -eq [IntPtr]::Zero) {
                    # Try Windows Terminal window class
                    $handle = [Win32]::FindWindow("CASCADIA_HOSTING_WINDOW_CLASS", "*$ProcessName*")
                    if ($handle -ne [IntPtr]::Zero) {
                        Write-Host "Found window handle using CASCADIA_HOSTING_WINDOW_CLASS: $handle"
                    }
                }
            }
        }
        catch {
            Write-Host "Error while finding window: $_"
        }
        
        if ($handle -ne [IntPtr]::Zero) {
            try {
                # Bring window to front
                [void][Win32]::SetForegroundWindow($handle)
                [void][Win32]::ShowWindow($handle, 5) # 5 = SW_SHOW
                
                $rect = New-Object Win32+RECT
                [void][Win32]::GetWindowRect($handle, [ref]$rect)
                [void][Win32]::MoveWindow($handle, $X, $Y, $Width, $Height, $true)
                Write-Host "Window for '$ProcessName' positioned successfully after $($retryCount+1) attempt(s)."
                $success = $true
            }
            catch {
                Write-Host "Error while positioning window: $_"
                $retryCount = $retryCount + 1
            }
        } 
        else {
            $retryCount = $retryCount + 1
            if ($retryCount -lt $MaxRetries) {
                Write-Host "Attempt $($retryCount): Window not found, retrying in $($RetryIntervalMs)ms..."
                Start-Sleep -Milliseconds $RetryIntervalMs
            }
        }
    }
    
    if (-not $success) {
        Write-Host "Window for '$ProcessName' not found after $MaxRetries attempts."
    }
}

# Load the required assembly for screen info
Add-Type -AssemblyName System.Windows.Forms

# Calculate screen dimensions for side-by-side positioning
$screenWidth = [System.Windows.Forms.Screen]::PrimaryScreen.WorkingArea.Width
$screenHeight = [System.Windows.Forms.Screen]::PrimaryScreen.WorkingArea.Height
$windowWidth = $screenWidth / 2
$windowHeight = $screenHeight

Write-Host "Starting applications from $AppPath..."

# Choose console host based on parameter
$startInfo = New-Object System.Diagnostics.ProcessStartInfo

if ($UseClassicConsole) {
    Write-Host "Using classic console host (cmd.exe)..."
    $startInfo.FileName = "cmd.exe"
    $startInfo.Arguments = "/c"
}

# Start both processes first
Write-Host "Starting RocketServer..."
if ($UseClassicConsole) {
    $startInfo.Arguments = "/c `"$AppPath\RocketServer.exe`""
    [System.Diagnostics.Process]::Start($startInfo)
} else {
    $serverProc = Start-Process -FilePath "$AppPath\RocketServer.exe" -WindowStyle Normal -PassThru
    Write-Host "RocketServer started with PID: $($serverProc.Id)"
}

Write-Host "Starting RocketConsole..."
if ($UseClassicConsole) {
    $startInfo.Arguments = "/c `"$AppPath\RocketConsole.exe`""
    [System.Diagnostics.Process]::Start($startInfo)
} else {
    $clientProc = Start-Process -FilePath "$AppPath\RocketConsole.exe" -WindowStyle Normal -PassThru
    Write-Host "RocketConsole started with PID: $($clientProc.Id)"
}

# Give both processes a moment to initialize
Write-Host "Waiting for applications to initialize..."
Start-Sleep -Seconds 3

# Then position both windows
Write-Host "Positioning RocketServer window..."
Set-WindowPosition -ProcessName "RocketServer" -X 0 -Y 0 -Width $windowWidth -Height $windowHeight -MaxRetries 15 -RetryIntervalMs 1000

Write-Host "Positioning RocketConsole window..."
Set-WindowPosition -ProcessName "RocketConsole" -X $windowWidth -Y 0 -Width $windowWidth -Height $windowHeight -MaxRetries 15 -RetryIntervalMs 1000

Write-Host "Application launch and positioning complete."
