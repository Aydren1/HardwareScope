# HardwareScope downloads

This public repository hosts the HardwareScope download website, release notes, installers, and update metadata.

Website: **https://aydren1.github.io/HardwareScope/**

HardwareScope is a lightweight, read-only Windows hardware monitor with live temperatures, usage, clock speeds, power, fan data, and a configurable on-screen display.

## Download

Use the [latest release](../../releases/latest) to download the current x64 Windows installer.

Current release: **HardwareScope 1.6.0**

Version 1.6.0 improves sensor accuracy by separating live telemetry from fixed warning/critical thresholds, filtering duplicate or invalid readings, correcting units and labels, and keeping the app on the Windows taskbar when minimized.

Settings includes manual and automatic stable updates. HardwareScope downloads new installers from the official GitHub release, verifies the published SHA-256 checksum, installs silently, and relaunches itself.

> HardwareScope is currently unsigned. Windows may display an **Unknown publisher** or SmartScreen warning. Verify the published SHA-256 checksum before installing.

## Requirements

- Windows 10 or Windows 11
- 64-bit processor
- Administrator access for low-level sensors

## Safety

HardwareScope reads sensor telemetry only. It does not change voltages, fan curves, clock speeds, or power limits.

## License

See [LICENSE.txt](LICENSE.txt).
