# HardwareScope 1.6.1

HardwareScope 1.6.1 adds lightweight notification-tray behavior, single-instance protection, and refreshed public GitHub branding.

## Changes

- Clicking the standard Windows minimize button now hides HardwareScope from the taskbar and keeps monitoring in the notification tray.
- Double-clicking the HardwareScope tray icon restores the main window.
- The tray menu includes **Open HardwareScope** and **Exit HardwareScope** actions.
- A first-use notification explains that HardwareScope is still running after it is minimized.
- Settings includes a **Hide in the notification tray when minimized** option under **Startup & tray**. It is enabled by default.
- Startup-minimized launches use the same tray behavior when the option is enabled.
- Only one HardwareScope instance can run in a Windows user session. Launching it again restores and focuses the existing window, including when it is hidden in the tray.
- Every sensor-table column now has a one-second hover explanation, including a plain-language definition of OSD (On-Screen Display).
- Every displayed sensor has a concise hover explanation. Common CPU, GPU, memory-junction, storage, power-delivery, and health readings receive specific descriptions; all other readings receive an explanation based on their sensor type.
- The existing HardwareScope logo is used by the tray icon and public GitHub branding.

## Verification

- Clean Release and self-contained Windows-x64 builds with zero compiler warnings or errors.
- A Windows title-bar minimize-command test confirmed that the window becomes invisible, leaves the taskbar, and the process remains running.
- A two-launch test confirmed that the second process exits and the original instance continues with its window restored.
- The full sensor-type description fallback covers every sensor type supported by the monitoring library.
- The installer and application report version 1.6.1.

HardwareScope 1.6.1 is currently unsigned, so Windows SmartScreen may show an unknown-publisher warning.
