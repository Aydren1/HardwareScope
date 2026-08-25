param(
    [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$diagnosticsRoot = [IO.Path]::GetFullPath((Join-Path $projectRoot 'out\diagnostics'))
$stagingDirectory = [IO.Path]::GetFullPath((Join-Path $diagnosticsRoot 'HardwareScope-2.0-sensor-validation-x64'))
$archivePath = [IO.Path]::GetFullPath((Join-Path $diagnosticsRoot 'HardwareScope-2.0-sensor-validation-x64.zip'))
$exporterPath = [IO.Path]::GetFullPath((Join-Path $projectRoot "out\build\x64-release\$Configuration\HardwareScopeNativeSensorExport.exe"))

if (-not $stagingDirectory.StartsWith($diagnosticsRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'The validation staging path escaped the diagnostics directory.'
}
if (-not $archivePath.StartsWith($diagnosticsRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'The validation archive path escaped the diagnostics directory.'
}
if (-not (Test-Path -LiteralPath $exporterPath -PathType Leaf)) {
    throw "Build the sensor exporter first: $exporterPath"
}

if (Test-Path -LiteralPath $stagingDirectory) {
    Remove-Item -LiteralPath $stagingDirectory -Recurse -Force
}
New-Item -ItemType Directory -Path $stagingDirectory | Out-Null

$payload = @(
    @{ Source = $exporterPath; Destination = 'HardwareScopeSensorExport.exe' },
    @{ Source = (Join-Path $projectRoot 'tests\analyze_sensor_validation.ps1'); Destination = 'Analyze-HardwareScopeSeries.ps1' },
    @{ Source = (Join-Path $projectRoot 'docs\SECOND_PC_VALIDATION.md'); Destination = 'VALIDATION_INSTRUCTIONS.md' },
    @{ Source = (Join-Path $projectRoot 'README.md'); Destination = 'README.md' },
    @{ Source = (Join-Path $projectRoot 'THIRD_PARTY_NOTICES.md'); Destination = 'THIRD_PARTY_NOTICES.md' },
    @{ Source = (Join-Path $projectRoot 'LICENSE.txt'); Destination = 'LICENSE.txt' }
)
foreach ($item in $payload) {
    if (-not (Test-Path -LiteralPath $item.Source -PathType Leaf)) {
        throw "Validation package source is missing: $($item.Source)"
    }
    Copy-Item -LiteralPath $item.Source -Destination (Join-Path $stagingDirectory $item.Destination)
}

if (Test-Path -LiteralPath $archivePath) {
    Remove-Item -LiteralPath $archivePath -Force
}
Compress-Archive -Path (Join-Path $stagingDirectory '*') -DestinationPath $archivePath -CompressionLevel Optimal

$archive = Get-Item -LiteralPath $archivePath
$hash = (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash
Write-Output "PACKAGE=$($archive.FullName)"
Write-Output "BYTES=$($archive.Length)"
Write-Output "SHA256=$hash"
