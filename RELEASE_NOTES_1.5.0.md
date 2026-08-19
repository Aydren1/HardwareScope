# HardwareScope 1.5.0

The first public HardwareScope distribution package.

## Highlights

- Live CPU, GPU, motherboard, memory, storage, controller, battery, and PSU telemetry when exposed by supported hardware
- CPU temperature, usage, clock-speed, and power sections
- Collapsible sensor groups and Essentials, Temperatures, Performance, and All Sensors presets
- EZ Temps monitoring with configurable CPU Tctl/Tdie, GPU core, and GPU memory-junction selections
- Borderless OSD with selectable corners, custom position, and matching text colors
- Full dark and light themes with white, teal, blue, amber, purple, red, yellow, and orange text choices
- Configurable millisecond refresh intervals
- Windows startup, minimized-start, and startup-OSD settings
- Self-contained x64 installer with PawnIO sensor support

## Installation

Download `HardwareScope-Setup-1.5.0-x64.exe` and run it as administrator. No separate .NET installation is required.

## Important

This build is not Authenticode-signed. Windows may display an **Unknown publisher** or SmartScreen warning. Verify the installer against `SHA256SUMS.txt` before running it.

HardwareScope reads telemetry only. It does not change clocks, voltages, fan curves, or power limits.
