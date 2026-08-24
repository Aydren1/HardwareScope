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

## Verification

- Added classification tests confirming desktop applications are ignored and recognized game executables remain visible.
