# HardwareScope 2.0.1

This maintenance release streamlines tray behavior, improves Settings scrolling,
and makes hardware polling controls more precise and reliable.

## Changes

- The minimize button now always hides HardwareScope in the notification tray
  and removes its taskbar button; redundant toggles were removed from Settings.
- Start-with-Windows and optional start-minimized behavior remain configurable.
- Settings scrolling now batches each wheel movement into a single layout pass
  and uses non-erasing background painting to prevent visible flicker.
- Hardware polling choices now range from 100 ms through 10,000 ms, with more
  precise millisecond options and clear fastest/recommended/lightest labels.
- Saving a polling change immediately restarts the sensor worker at the chosen
  interval and persists the exact value for the next launch.
- The privileged sensor service now receives the selected polling interval from
  the app instead of using a fixed 500 ms cadence.

Some deliberately slow devices, particularly storage SMART and DIMM sensors,
retain conservative internal safety caches to avoid unnecessary hardware access.

## Download

Download `HardwareScope-Setup-2.0.1-x64.exe` from the release assets. This build
is unsigned, so Windows may display an unknown-publisher warning.
