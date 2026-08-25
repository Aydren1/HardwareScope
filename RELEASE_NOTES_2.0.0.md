# HardwareScope 2.0.0

HardwareScope 2.0 is a complete native C++ rewrite focused on responsiveness,
low background overhead, accurate read-only sensors, and a compact configurable
on-screen display.

## New in 2.0

- Native Win32, Direct2D, and DirectWrite interface with no managed UI runtime
- Searchable, collapsible CPU, GPU, storage, memory, and system sensor sections
- Dark/light themes and selectable UI/OSD colors
- Configurable EZ temperatures and per-sensor OSD selection
- Vertical and horizontal OSD layouts with placement, spacing, opacity, and scale
- Independent FPS color, scale, refresh, and smoothing controls
- Game-only PresentMon FPS collection that stays off on the desktop
- Tray minimization, startup controls, and single-instance behavior
- Hover explanations for columns and sensor readings
- Verified automatic updates using GitHub asset size and SHA-256 metadata
- Native sensor service for read-only privileged CPU, DIMM, storage, and board data

## FPS reliability

The release cleans up abandoned HardwareScope ETW sessions, restarts capture if
the collector exits unexpectedly, and correctly handles PresentMon's live pipe
encoding. CS2 capture was validated end to end through the installed service and
OSD data path.

## Performance and validation

The production executable uses a statically linked MSVC runtime, fixed-capacity
sensor snapshots, virtualized sensor rows, bounded worker handoffs, and no test
hooks. Core, updater-recovery, mixed-DPI UI, tray, OSD, settings repetition, and
live sensor/FPS checks passed for this build.

## Installation

Download `HardwareScope-Setup-2.0.0-x64.exe` from the release assets. The app is
currently unsigned, so Windows may show an Unknown publisher or SmartScreen
warning. Compare the installer against `SHA256SUMS.txt` if desired.
