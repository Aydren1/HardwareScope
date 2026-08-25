param(
    [Parameter(Mandatory = $true)]
    [string]$InstallerPath,

    [Parameter(Mandatory = $true)]
    [string]$BuildDirectory,

    [Parameter(Mandatory = $true)]
    [string]$DeploymentScript,

    [Parameter(Mandatory = $true)]
    [string]$ResultPath
)

$ErrorActionPreference = 'Stop'
$legacyRegistryPath = 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\{31C31E44-97D3-4F1F-BB98-7C7914BE2A73}_is1'
$nativeRegistryPath = 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\{50EC77EE-2969-45F1-A6DB-A6D225C1BBF1}_is1'
$legacyUninstaller = 'C:\Program Files\HardwareScope\unins000.exe'
$legacySettings = Join-Path $env:LOCALAPPDATA 'HardwareScope\settings.json'

$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = [Security.Principal.WindowsPrincipal]::new($identity)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw 'Legacy HardwareScope removal must run as administrator.'
}

$legacyEntry = Get-ItemProperty -LiteralPath $legacyRegistryPath
if ($legacyEntry.DisplayVersion -ne '1.7.6' -or $legacyEntry.InstallLocation -ne 'C:\Program Files\HardwareScope\') {
    throw 'The registered legacy installation did not match the expected HardwareScope 1.7.6 target.'
}
if (-not (Test-Path -LiteralPath $legacyUninstaller -PathType Leaf)) {
    throw 'The registered HardwareScope 1.7.6 uninstaller is missing.'
}

$uninstall = Start-Process -FilePath $legacyUninstaller -ArgumentList @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART') -Wait -PassThru
if ($uninstall.ExitCode -ne 0) {
    throw "HardwareScope 1.7.6 uninstall failed with exit code $($uninstall.ExitCode)."
}
if (Test-Path -LiteralPath $legacyRegistryPath) {
    throw 'The HardwareScope 1.7.6 uninstall entry remained after uninstall.'
}

if (Test-Path -LiteralPath $legacySettings -PathType Leaf) {
    Remove-Item -LiteralPath $legacySettings -Force
}

& $DeploymentScript -InstallerPath $InstallerPath -BuildDirectory $BuildDirectory -ResultPath $ResultPath

if (-not (Test-Path -LiteralPath $nativeRegistryPath)) {
    throw 'HardwareScope 2.0 uninstall metadata was not restored.'
}
if (Test-Path -LiteralPath $legacyRegistryPath) {
    throw 'The legacy HardwareScope uninstall entry unexpectedly returned.'
}

$results = @(
    'Legacy version removed: HardwareScope 1.7.6'
    "Legacy uninstall entry removed: $legacyRegistryPath"
    "Legacy settings removed: $legacySettings"
    'HardwareScope 2.0 repaired after shared-file cleanup'
)
$results | Add-Content -LiteralPath $ResultPath -Encoding utf8
$results
