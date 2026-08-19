<p align="center">
  <img src="assets/hardwarescope-logo.png" alt="HardwareScope logo" width="160">
</p>

# HardwareScope downloads

This public repository hosts the HardwareScope download website, release notes, installers, and update metadata.

Website: **https://aydren1.github.io/HardwareScope/**

HardwareScope is a lightweight, read-only Windows hardware monitor with live temperatures, usage, clock speeds, power, fan data, and a configurable on-screen display.

## Download

Use the [latest release](../../releases/latest) to download the current x64 Windows installer.

Current release: **HardwareScope 1.6.1**

Version 1.6.1 sends HardwareScope to the notification tray when the standard minimize button is clicked, removing it from the taskbar while monitoring continues. Double-click the tray icon to reopen it, or right-click for Open and Exit actions. This behavior is configurable under Settings > Startup & tray.

Only one HardwareScope instance runs at a time. Launching the app again restores and focuses the existing window, including when it is hidden in the tray.

Hover over any table heading or sensor for one second to see a short plain-language explanation. OSD is identified as the On-Screen Display, while recognized CPU, GPU, storage, and health readings receive more specific descriptions.

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
