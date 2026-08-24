# HardwareScope 1.7.5

This release fixes the automatic updater closing HardwareScope without reliably installing or reopening the app.

## Fixed

- Replaced the direct installer launch with a detached update handoff.
- The helper now waits until HardwareScope has fully exited before the installer replaces application files.
- HardwareScope reopens only after the installer reports a successful exit code.
- Failed installs reopen the existing app and display the failure instead of disappearing silently.
- Installer output is retained in the local update folder for troubleshooting.
- Old update handoff files and logs are cleaned up after seven days.

## Verification

- Added an automated handoff test that verifies the wait, install, status, and restart sequence.
