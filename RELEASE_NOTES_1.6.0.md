# HardwareScope 1.6.0

HardwareScope 1.6.0 focuses on trustworthy sensor output and standard Windows behavior.

## Changes

- The standard minimize button now keeps HardwareScope running as a normal taskbar application.
- Fixed warning and critical temperature limits are no longer shown as live temperatures. This removes false readings such as an NVMe drive appearing to run at 93 °C.
- Missing, non-finite, invalid, library-hidden, threshold, and duplicate readings are filtered out.
- All LibreHardwareMonitor sensor types use their correct display units.
- Storage health, endurance, spare capacity, power-on time, and power-cycle labels are clearer.
- CPU multiplier, GPU temperature, drive temperature, and unlabeled motherboard sensor names are clearer.
- Corrupted hardware labels and duplicate display names are normalized.
- Settings now includes manual and automatic GitHub updates. Installers are downloaded over HTTPS, SHA-256 verified, installed silently, and HardwareScope relaunches automatically.

## Verification

- Clean self-contained Windows-x64 build with zero compiler warnings or errors.
- Administrator-level audit of 417 exposed entries on the test system.
- 313 live/current readings retained and 104 invalid, hidden, duplicate, missing, or metadata entries filtered.
- No duplicate display rows, non-finite values, metadata leaks, or corrupted labels remained.
- The packaged window passed a Windows title-bar minimize-command test.
- The update Settings UI, manifest parser, version comparison, and checksum verifier passed release tests.

HardwareScope is currently unsigned, so Windows SmartScreen may show an unknown-publisher warning.
