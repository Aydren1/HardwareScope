param(
    [Parameter(Mandatory = $true)]
    [string]$SourcePath,

    [Parameter(Mandatory = $true)]
    [string]$ResultPath
)

$ErrorActionPreference = 'Stop'
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = [Security.Principal.WindowsPrincipal]::new($identity)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw 'HardwareScope application replacement must run as administrator.'
}

$source = Get-Item -LiteralPath $SourcePath
$installDirectory = Join-Path $env:ProgramFiles 'HardwareScope'
$application = Join-Path $installDirectory 'HardwareScope.exe'
if (-not (Test-Path -LiteralPath $application)) {
    throw "HardwareScope is not installed at $application."
}

Get-Process HardwareScope, HardwareScopeNative, HardwareScopeNativeInstrumented -ErrorAction SilentlyContinue |
    Stop-Process -Force -ErrorAction SilentlyContinue
Copy-Item -LiteralPath $source.FullName -Destination $application -Force

$sourceHash = (Get-FileHash -LiteralPath $source.FullName -Algorithm SHA256).Hash
$installedHash = (Get-FileHash -LiteralPath $application -Algorithm SHA256).Hash
if ($sourceHash -ne $installedHash) {
    throw 'The installed HardwareScope executable does not match the tested build.'
}

$resultDirectory = Split-Path -Parent $ResultPath
New-Item -ItemType Directory -Path $resultDirectory -Force | Out-Null
@(
    "Installed app: $application"
    "SHA-256: $installedHash"
) | Set-Content -LiteralPath $ResultPath -Encoding utf8
