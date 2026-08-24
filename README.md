<p align="center">
  <img src="assets/hardwarescope-logo.png" alt="HardwareScope logo" width="160">
</p>

# HardwareScope downloads

This public repository hosts the HardwareScope download website, release notes, installers, and update metadata.

Website: **https://aydren1.github.io/HardwareScope/**

HardwareScope is a lightweight, read-only Windows hardware monitor with live temperatures, usage, clock speeds, power, fan data, and a configurable on-screen display.

## Download

Use the [latest release](../../releases/latest) to download the current x64 Windows installer.

Current release: **HardwareScope 1.7.4**

Version 1.7.4 fixes the FPS counter staying at `-` in games. It replaces the unreliable PresentMon console/CSV child process with a narrowly filtered in-process Windows graphics trace, calculates FPS from original frame timestamps, follows the foreground 3D process, and keeps capture bounded and off the UI thread.

Version 1.7.3 makes blank areas across the complete top headers genuinely draggable by giving the visually transparent WPF header surfaces an active hit-test background. The main window and Settings can now be moved from any non-interactive part of their headers.

Version 1.7.2 makes the full non-interactive top header draggable in both application windows. It also replaces foreground-only FPS capture with an HWiNFO-style continuous PresentMon session that automatically follows the active 3D process with the highest frame rate. FPS reads are lock-free, rate-limited, memory-bounded, and cannot block the interface during high-frame-rate gameplay.

Version 1.7.1 gives FPS its own low-latency refresh and smoothing controls, independent from temperature and hardware polling. It also replaces the separate Windows title bars with seamless draggable headers and matching in-app window controls.

Version 1.7.0 adds an optional live FPS counter for the foreground game or 3D application. FPS is pinned first in the OSD and has independent color and 50–200% scale controls. The bundled PresentMon capture engine starts only when FPS monitoring is enabled.

Version 1.6.5 adds a fully horizontal OSD layout with Tight, Normal, or Wide spacing; 50–200% scaling; and NVIDIA-style `│` separators between CPU, GPU, storage, memory, and system readings.

Version 1.6.4 removes the redundant HardwareScope heading from the on-screen display so selected sensor readings use less space.

Version 1.6.3 prevents automatic and manual updates from downloading concurrently, adds safe retries with unique temporary files, and validates both the installer size and SHA-256 checksum.

Version 1.6.2 fixes Setup error 740 after interactive installation. The optional **Launch HardwareScope** action now uses the administrator credentials already approved for Setup.

Version 1.6.1 sends HardwareScope to the notification tray when the standard minimize button is clicked, removing it from the taskbar while monitoring continues. Double-click the tray icon to reopen it, or right-click for Open and Exit actions. This behavior is configurable under Settings > Startup & tray.

Only one HardwareScope instance runs at a time. Launching the app again restores and focuses the existing window, including when it is hidden in the tray.

Hover over any table heading or sensor for one second to see a short plain-language explanation. OSD is identified as the On-Screen Display, while recognized CPU, GPU, storage, and health readings receive more specific descriptions.

Settings includes manual and automatic stable updates. HardwareScope downloads new installers from the official GitHub release, verifies the published SHA-256 checksum, installs silently, and relaunches itself.

The stable update manifest is published automatically only after GitHub has made the release installer public and the release workflow has verified its URL, byte size, and SHA-256 checksum. See [RELEASING.md](RELEASING.md).

> HardwareScope is currently unsigned. Windows may display an **Unknown publisher** or SmartScreen warning. Verify the published SHA-256 checksum before installing.

## Requirements

- Windows 10 or Windows 11
- 64-bit processor
- Administrator access for low-level sensors

## Safety

HardwareScope reads sensor telemetry only. It does not change voltages, fan curves, clock speeds, or power limits.

## License

See [LICENSE.txt](LICENSE.txt).
