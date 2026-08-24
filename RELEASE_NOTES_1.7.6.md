# HardwareScope 1.7.6

This release adds default-on game-only FPS display behavior.

## Added

- A **Show FPS only while a game is running** setting, enabled by default.
- Cached game-process detection for common Steam, Epic, GOG, Xbox, Riot, EA, Ubisoft, and standalone game locations.
- Large foreground game-window detection for games outside recognized library folders.

## Improved

- Browsers, ChatGPT, Discord, launchers, Windows shell processes, and other common desktop renderers no longer trigger the FPS display in game-only mode.
- The FPS row disappears completely from both the sensor list and OSD while no game is detected instead of displaying `-`.
- Users can disable game-only mode to retain FPS monitoring for every detected 3D application.
- FPS capture now remains completely stopped on the desktop and starts only after the lightweight game detector finds a running game.
- DXGI and Direct3D 9 capture is filtered to the selected game's process instead of processing every graphical application on the PC.
- The higher-overhead graphics-kernel provider is now a delayed compatibility fallback and is enabled only if direct game events are unavailable.
- The FPS ETW buffer was reduced from 32 MB to 8 MB normally and 16 MB only when compatibility fallback is required.
- Capture stops within approximately one second after the selected game exits.

## Verification

- Added classification tests confirming desktop applications are ignored and recognized game executables remain visible.
- Added an idle-efficiency benchmark. On the final self-contained release build, FPS control used 15.6 ms of CPU across 30 seconds (0.052% of one logical core) with desktop capture confirmed inactive.
