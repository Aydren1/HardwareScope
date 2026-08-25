param(
    [Parameter(Mandatory = $true)]
    [string]$AppPath,

    [Parameter(Mandatory = $true)]
    [string]$ProbePath,

    [Parameter(Mandatory = $true)]
    [ValidateRange(10, 300)]
    [int]$DurationSeconds,

    [Parameter(Mandatory = $true)]
    [string]$ResultPath,

    [switch]$Production
)

$ErrorActionPreference = 'Stop'
$resultDirectory = Split-Path -Parent $ResultPath
New-Item -ItemType Directory -Path $resultDirectory -Force | Out-Null

$startedAt = Get-Date
$appHash = (Get-FileHash -LiteralPath $AppPath -Algorithm SHA256).Hash
$probeHash = (Get-FileHash -LiteralPath $ProbePath -Algorithm SHA256).Hash
$appProcess = $null
$probeExit = 1
$probeLog = Join-Path $env:TEMP 'HardwareScopeNativeSoakProbe.log'
if (Test-Path -LiteralPath $probeLog) {
    Remove-Item -LiteralPath $probeLog -Force
}

try {
    $appProcess = Start-Process -FilePath $AppPath -WorkingDirectory (Split-Path -Parent $AppPath) -WindowStyle Hidden -PassThru
    Start-Sleep -Seconds 2

    $probeArguments = @($DurationSeconds, '--tray')
    if ($Production) { $probeArguments += '--production' }
    & $ProbePath $probeArguments
    $probeExit = $LASTEXITCODE
}
catch {
    "Harness failure: $($_.Exception.Message)" | Set-Content -LiteralPath $ResultPath -Encoding utf8
}
finally {
    $completedAt = Get-Date
    $metadata = @(
        "Harness started: $($startedAt.ToString('o'))"
        "Harness completed: $($completedAt.ToString('o'))"
        "Requested seconds: $DurationSeconds"
        "Application SHA-256: $appHash"
        "Probe SHA-256: $probeHash"
        "Probe exit code: $probeExit"
        ''
    )
    $probeOutput = if (Test-Path -LiteralPath $probeLog) { Get-Content -LiteralPath $probeLog } else { 'Probe log was not created.' }
    @($metadata + $probeOutput) | Set-Content -LiteralPath $ResultPath -Encoding utf8

    if ($null -ne $appProcess -and !$appProcess.HasExited) {
        $null = $appProcess.CloseMainWindow()
        if (!$appProcess.WaitForExit(5000)) {
            $appProcess.Kill()
            $appProcess.WaitForExit()
        }
    }
}

exit $probeExit
