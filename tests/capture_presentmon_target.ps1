param(
    [Parameter(Mandatory = $true)]
    [int]$TargetProcessId,

    [Parameter(Mandatory = $true)]
    [string]$OutputDirectory,

    [string]$PresentMonPath = "C:\Program Files\HardwareScope\PresentMon.exe",

    [int]$DurationSeconds = 15
)

$ErrorActionPreference = 'Stop'

if (-not ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
        [Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw 'This diagnostic must run elevated.'
}

New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$csvPath = Join-Path $OutputDirectory 'presentmon-live.csv'
$stdoutPath = Join-Path $OutputDirectory 'presentmon-live.stdout.txt'
$stderrPath = Join-Path $OutputDirectory 'presentmon-live.stderr.txt'

Remove-Item -LiteralPath $csvPath, $stdoutPath, $stderrPath -Force -ErrorAction SilentlyContinue
Get-Process PresentMon -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 500

$arguments = '--process_id {0} --output_file "{1}" --no_console_stats --qpc_time_ms --no_track_display --no_track_gpu --no_track_input --timed {2} --terminate_after_timed --stop_existing_session --session_name HardwareScopeManualDiagnostic' -f `
    $TargetProcessId, $csvPath, $DurationSeconds

$process = Start-Process -FilePath $PresentMonPath -ArgumentList $arguments -PassThru -Wait `
    -RedirectStandardOutput $stdoutPath -RedirectStandardError $stderrPath -WindowStyle Hidden

"PresentMon exit code: $($process.ExitCode)" | Add-Content -LiteralPath $stdoutPath
