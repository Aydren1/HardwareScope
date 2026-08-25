# HardwareScope 2.0.4

This release improves HardwareScope's native interface, overlay flexibility,
and update experience while preserving its lightweight sensor collection path.

## Changes

- Added a true pitch-black Midnight theme alongside the existing Dark and
  Light themes.
- Added a dedicated Colors page with independent colors for CPU temperatures,
  CPU usage, CPU clocks, CPU power and voltage, graphics, storage, memory,
  system sensors, and FPS.
- Applied category colors consistently to sensor headings, current readings,
  OSD selections, and OSD text.
- Added optional independent FPS OSD positioning, allowing telemetry and FPS
  to occupy different screen corners without duplicating FPS collection.
- Slimmed the main native window and refined its custom caption, resizing, DPI
  behavior, and settings layout.
- Added quieter automatic-update notifications and explicit Update now,
  24-hour, 3-day, 1-week, and per-version skip choices.
- Corrected the embedded Windows manifest so modern Common Controls styling,
  requested elevation, and DPI declarations are retained in packaged builds.
- Expanded persistence and regression coverage for theme, category color,
  polling, OSD, settings, and update behavior.

## Download

Download `HardwareScope-Setup-2.0.4-x64.exe` from the release assets. This build
is unsigned, so Windows may display an unknown-publisher warning.
