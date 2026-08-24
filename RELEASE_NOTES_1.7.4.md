# HardwareScope 1.7.4

This release fixes the FPS counter remaining at `-` in games and removes the capture path that could become unresponsive under heavy game telemetry.

- Replaces the bundled PresentMon console/CSV child process with a lightweight in-process Windows ETW graphics listener.
- Captures narrowly filtered DXGI, D3D9, and kernel present-history events for DirectX and compatible graphics workloads.
- Calculates FPS from the original frame timestamps, so Windows event-buffer bursts no longer erase valid samples.
- Follows the foreground 3D process first and uses the highest active frame rate as a fallback.
- Keeps FPS capture independent from sensor polling, bounded in memory, and off the WPF UI thread.
- Removes the bundled PresentMon executable from the app and installer.
- Fixes post-install launch so the elevation-required app no longer triggers installer error 740.

Validation included live elevated graphics capture, a 60 FPS calculation test, automatic process selection, 100,000 synthetic frame events, and 250,000 non-blocking UI snapshot reads.
