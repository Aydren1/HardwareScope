# HardwareScope 2.0.3

This release corrects the underlying Settings-page scrolling behavior affecting
the Additional OSD Sensors list on the Monitoring tab.

## Changes

- The normal Settings window is now sized from its required client area, so it
  no longer exposes unnecessary outer-page scrolling at standard dimensions.
- Genuinely constrained Settings windows now use native pixel scrolling and
  move child controls without forcing the OSD sensor list to repaint in place.
- Scrolling the Monitoring page no longer changes the Additional OSD Sensors
  list's own internal scroll position.
- Settings controls use sibling clipping to prevent overlapping child-control
  redraws during movement.
- Regression coverage now checks both normal-size overflow and constrained
  Monitoring-page scrolling behavior.

## Download

Download `HardwareScope-Setup-2.0.3-x64.exe` from the release assets. This build
is unsigned, so Windows may display an unknown-publisher warning.
