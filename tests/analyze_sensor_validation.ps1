param(
    [Parameter(Mandatory = $true)]
    [string]$IdlePath,

    [Parameter(Mandatory = $true)]
    [string]$LoadPath,

    [string]$OutputPath = 'HardwareScope-series-summary.csv',

    [ValidateRange(1, 36000)]
    [int]$ExpectedSamples = 60
)

$ErrorActionPreference = 'Stop'
$culture = [Globalization.CultureInfo]::InvariantCulture
$timestampFormat = "yyyy-MM-dd'T'HH:mm:ss.fff'Z'"
$timestampStyles = [Globalization.DateTimeStyles]::AssumeUniversal -bor [Globalization.DateTimeStyles]::AdjustToUniversal
$requiredColumns = @('timestamp_utc', 'sample_index', 'sensor_id', 'kind', 'unit', 'current', 'minimum', 'maximum', 'sensor', 'hardware')

function Read-SensorSeries([string]$Path, [string]$CaptureName) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "$CaptureName capture does not exist: $Path"
    }
    $rows = @(Import-Csv -LiteralPath $Path)
    if ($rows.Count -eq 0) {
        throw "$CaptureName capture is empty."
    }
    foreach ($column in $requiredColumns) {
        if ($rows[0].PSObject.Properties.Name -cnotcontains $column) {
            throw "$CaptureName capture is missing the '$column' column."
        }
    }

    $sampleGroups = @($rows | Group-Object sample_index | Sort-Object { [int]$_.Name })
    if ($sampleGroups.Count -ne $ExpectedSamples) {
        throw "$CaptureName capture has $($sampleGroups.Count) samples; expected $ExpectedSamples."
    }
    for ($index = 0; $index -lt $sampleGroups.Count; ++$index) {
        if ([int]$sampleGroups[$index].Name -ne $index) {
            throw "$CaptureName sample indexes are not contiguous from zero."
        }
        $duplicates = @($sampleGroups[$index].Group | Group-Object sensor_id | Where-Object Count -gt 1)
        if ($duplicates.Count -ne 0) {
            throw "$CaptureName sample $index contains duplicate sensor IDs."
        }
    }

    $previousTimestamp = [DateTimeOffset]::MinValue
    foreach ($row in $rows) {
        $timestamp = [DateTimeOffset]::MinValue
        if (-not [DateTimeOffset]::TryParseExact($row.timestamp_utc, $timestampFormat, $culture, $timestampStyles, [ref]$timestamp)) {
            throw "$CaptureName contains an invalid UTC timestamp: $($row.timestamp_utc)"
        }
        if ($timestamp -lt $previousTimestamp) {
            throw "$CaptureName timestamps move backward."
        }
        $previousTimestamp = $timestamp

        if ($row.sensor_id -notmatch '^0x[0-9A-F]+$') {
            throw "$CaptureName contains an invalid sensor ID: $($row.sensor_id)"
        }
        foreach ($field in @('kind', 'unit', 'sensor', 'hardware')) {
            if ([string]::IsNullOrWhiteSpace($row.$field)) {
                throw "$CaptureName contains an empty '$field' field for $($row.sensor_id)."
            }
        }
        foreach ($field in @('current', 'minimum', 'maximum')) {
            $value = 0.0
            $parsed = [double]::TryParse($row.$field, [Globalization.NumberStyles]::Float, $culture, [ref]$value)
            if (-not $parsed -or [double]::IsNaN($value) -or [double]::IsInfinity($value)) {
                throw "$CaptureName contains a non-finite '$field' value for $($row.sensor_id)."
            }
        }
    }

    $summary = foreach ($sensorGroup in ($rows | Group-Object sensor_id | Sort-Object Name)) {
        $metadata = @($sensorGroup.Group | ForEach-Object {
            "$($_.kind)|$($_.unit)|$($_.sensor)|$($_.hardware)"
        } | Sort-Object -Unique)
        if ($metadata.Count -ne 1) {
            throw "$CaptureName metadata changed for sensor $($sensorGroup.Name)."
        }
        $values = @($sensorGroup.Group | ForEach-Object { [double]::Parse($_.current, $culture) })
        $measure = $values | Measure-Object -Minimum -Maximum -Average
        [pscustomobject]@{
            capture = $CaptureName
            sensor_id = $sensorGroup.Name
            hardware = $sensorGroup.Group[0].hardware
            sensor = $sensorGroup.Group[0].sensor
            kind = $sensorGroup.Group[0].kind
            unit = $sensorGroup.Group[0].unit
            samples = $values.Count
            minimum = [Math]::Round($measure.Minimum, 3)
            average = [Math]::Round($measure.Average, 3)
            maximum = [Math]::Round($measure.Maximum, 3)
        }
    }

    [pscustomobject]@{
        Rows = $rows
        Summary = @($summary)
        SensorIds = @($summary.sensor_id | Sort-Object)
    }
}

$idle = Read-SensorSeries -Path $IdlePath -CaptureName 'idle'
$load = Read-SensorSeries -Path $LoadPath -CaptureName 'load'
$sensorSetDifference = @(Compare-Object -ReferenceObject $idle.SensorIds -DifferenceObject $load.SensorIds)
if ($sensorSetDifference.Count -ne 0) {
    $details = ($sensorSetDifference | ForEach-Object { "$($_.InputObject) $($_.SideIndicator)" }) -join ', '
    throw "Sensor IDs changed between idle and load captures: $details"
}

$outputDirectory = [IO.Path]::GetDirectoryName([IO.Path]::GetFullPath($OutputPath))
[IO.Directory]::CreateDirectory($outputDirectory) | Out-Null
@($idle.Summary + $load.Summary) | Export-Csv -LiteralPath $OutputPath -NoTypeInformation -Encoding UTF8

Write-Output "PASS idle samples: $ExpectedSamples; sensors per sample: $($idle.SensorIds.Count)"
Write-Output "PASS load samples: $ExpectedSamples; sensors per sample: $($load.SensorIds.Count)"
Write-Output 'PASS timestamps, values, metadata, IDs, and sample bounds are valid'
Write-Output "SUMMARY=$([IO.Path]::GetFullPath($OutputPath))"
