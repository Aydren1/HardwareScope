# HardwareScope 2.0.2

This focused UI maintenance release removes the remaining flicker from the
Monitoring tab while the Settings window is scrolled.

## Changes

- Settings now uses composited child-window painting so the Additional OSD
  Sensors list and its parent background are presented as one buffered frame.
- Scrolling repositions all Settings controls without intermediate redraws,
  followed by one coordinated redraw of the window and its children.
- The native UI regression suite now opens the Monitoring tab, verifies the
  Additional OSD Sensors list, and exercises repeated buffered scrolling.

## Download

Download `HardwareScope-Setup-2.0.2-x64.exe` from the release assets. This build
is unsigned, so Windows may display an unknown-publisher warning.
