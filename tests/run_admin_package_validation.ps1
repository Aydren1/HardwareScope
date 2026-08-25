param(
    [Parameter(Mandatory = $true)]
    [string]$InstallerPath,

    [Parameter(Mandatory = $true)]
    [string]$BridgeProbePath,

    [Parameter(Mandatory = $true)]
    [string]$LogPath,

    [string]$InstallDirectory = 'C:\Program Files\HardwareScope Native 2.0 Admin Smoke',

    [string]$ExpectedInstallerSha256 = 'B79CA88A7628F2F88FB18E6B3DBF76D4FC1E60A4AFBF103BD26F83B6726EFDE0'
)

$ErrorActionPreference = 'Stop'
$serviceName = 'HardwareScopeSensorService'
$expectedServicePath = '"' + (Join-Path $InstallDirectory 'HardwareScopeSensorService.exe') + '"'
$messages = [System.Collections.Generic.List[string]]::new()
$failure = $null
$installed = $false

function Add-Result([string]$Message) {
    $messages.Add($Message)
    Write-Output $Message
}

function Wait-ServiceState([string]$DesiredState, [int]$TimeoutSeconds) {
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        $service = Get-Service -Name $serviceName -ErrorAction SilentlyContinue
        if ($DesiredState -eq 'Absent') {
            if ($null -eq $service) { return $true }
        }
        elseif (($null -ne $service) -and ($service.Status.ToString() -eq $DesiredState)) {
            return $true
        }
        Start-Sleep -Milliseconds 200
    } while ([DateTime]::UtcNow -lt $deadline)
    return $false
}

try {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = [Security.Principal.WindowsPrincipal]::new($identity)
    if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
        throw 'The package validator must run from an elevated administrator process.'
    }

    $installer = Get-Item -LiteralPath $InstallerPath
    $bridgeProbe = Get-Item -LiteralPath $BridgeProbePath
    if (Test-Path -LiteralPath $InstallDirectory) {
        throw "The isolated validation directory already exists: $InstallDirectory"
    }
    if ($null -ne (Get-Service -Name $serviceName -ErrorAction SilentlyContinue)) {
        throw "The $serviceName service already exists; validation stopped without changing it."
    }

    $actualHash = (Get-FileHash -LiteralPath $installer.FullName -Algorithm SHA256).Hash
    if ($actualHash -ne $ExpectedInstallerSha256) {
        throw "Installer hash mismatch. Expected $ExpectedInstallerSha256 but found $actualHash."
    }
    Add-Result "PASS installer hash: $actualHash"

    $setupLog = Join-Path ([IO.Path]::GetDirectoryName($LogPath)) 'admin-package-install.log'
    $arguments = @(
        '/VERYSILENT',
        '/SUPPRESSMSGBOXES',
        '/NORESTART',
        "/DIR=`"$InstallDirectory`"",
        "/LOG=`"$setupLog`""
    )
    $setup = Start-Process -FilePath $installer.FullName -ArgumentList $arguments -Wait -PassThru
    if ($setup.ExitCode -ne 0) {
        throw "The isolated installer returned exit code $($setup.ExitCode)."
    }
    $installed = $true
    Add-Result 'PASS isolated installer completed'

    if (-not (Wait-ServiceState -DesiredState 'Running' -TimeoutSeconds 20)) {
        throw "$serviceName did not reach Running state."
    }
    $service = Get-Service -Name $serviceName
    if ($service.StartType -ne [System.ServiceProcess.ServiceStartMode]::Automatic) {
        throw "$serviceName startup mode was $($service.StartType), not Automatic."
    }
    Add-Result 'PASS service state: Running / Automatic'

    $serviceRegistryPath = "Registry::HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\$serviceName"
    $imagePath = (Get-ItemProperty -LiteralPath $serviceRegistryPath -Name ImagePath).ImagePath
    if ($imagePath -cne $expectedServicePath) {
        throw "Service ImagePath was '$imagePath'; expected '$expectedServicePath'."
    }
    Add-Result "PASS quoted service ImagePath: $imagePath"

    foreach ($requiredName in @('HardwareScope.exe', 'HardwareScopeUpdater.exe', 'HardwareScopeSensorService.exe', 'LICENSE.txt', 'THIRD_PARTY_NOTICES.md')) {
        if (-not (Test-Path -LiteralPath (Join-Path $InstallDirectory $requiredName) -PathType Leaf)) {
            throw "The installed package is missing $requiredName."
        }
    }
    Add-Result 'PASS installed payload is complete'

    $bridgeOutput = (& $bridgeProbe.FullName 2>&1 | Out-String).Trim()
    $bridgeExitCode = $LASTEXITCODE
    if ($bridgeExitCode -ne 0) {
        throw "Bridge probe failed with exit code $bridgeExitCode`: $bridgeOutput"
    }
    $countMatch = [regex]::Match($bridgeOutput, 'Bridge sensors:\s*(\d+)')
    if ((-not $countMatch.Success) -or ([int]$countMatch.Groups[1].Value -lt 1)) {
        throw "Bridge probe did not report a positive sensor count: $bridgeOutput"
    }
    if ($bridgeOutput -match '(?i)(^|[^a-z])(nan|inf|infinity)([^a-z]|$)') {
        throw "Bridge probe returned a non-finite value: $bridgeOutput"
    }
    Add-Result "PASS privileged bridge readings: $($countMatch.Groups[1].Value) sensors"
    $messages.Add('BRIDGE OUTPUT:')
    $messages.Add($bridgeOutput)
}
catch {
    $failure = $_.Exception.Message
    Add-Result "FAIL $failure"
}
finally {
    try {
        $uninstaller = Join-Path $InstallDirectory 'unins000.exe'
        if (Test-Path -LiteralPath $uninstaller -PathType Leaf) {
            $uninstall = Start-Process -FilePath $uninstaller -ArgumentList @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART') -Wait -PassThru
            if ($uninstall.ExitCode -ne 0) {
                throw "The isolated uninstaller returned exit code $($uninstall.ExitCode)."
            }
        }
        elseif ($installed) {
            throw 'The isolated installation completed but its uninstaller was missing.'
        }

        if (-not (Wait-ServiceState -DesiredState 'Absent' -TimeoutSeconds 20)) {
            throw "$serviceName remained installed after cleanup."
        }
        $deadline = [DateTime]::UtcNow.AddSeconds(10)
        while ((Test-Path -LiteralPath $InstallDirectory) -and ([DateTime]::UtcNow -lt $deadline)) {
            Start-Sleep -Milliseconds 200
        }
        if (Test-Path -LiteralPath $InstallDirectory) {
            throw "The isolated install directory remained after cleanup: $InstallDirectory"
        }
        Add-Result 'PASS cleanup removed the service and isolated files'
    }
    catch {
        $cleanupFailure = $_.Exception.Message
        Add-Result "FAIL cleanup: $cleanupFailure"
        if ($null -eq $failure) { $failure = $cleanupFailure }
    }

    $logDirectory = [IO.Path]::GetDirectoryName($LogPath)
    if (-not [string]::IsNullOrWhiteSpace($logDirectory)) {
        [IO.Directory]::CreateDirectory($logDirectory) | Out-Null
    }
    [IO.File]::WriteAllLines($LogPath, $messages, [Text.UTF8Encoding]::new($false))
}

if ($null -ne $failure) { exit 1 }
Add-Result 'OK exact elevated package validation passed'
[IO.File]::WriteAllLines($LogPath, $messages, [Text.UTF8Encoding]::new($false))
exit 0
