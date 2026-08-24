# HardwareScope 1.7.0

## New

- Added an optional live FPS counter for the foreground game or 3D application.
- Added FPS settings for enabling frame monitoring and automatically showing FPS in the OSD.
- Added a dedicated Frame rate section in the sensor table with current, minimum, and maximum FPS.
- Bundled Intel PresentMon 2.4.1, so users do not need to install a separate FPS tool.
- Horizontal OSD mode places FPS first and separates it from CPU/GPU readings with the existing NVIDIA-style divider.

## Performance and behavior

- PresentMon starts only while FPS monitoring is enabled and targets only the foreground application.
- GPU and input timing collection are disabled because HardwareScope only needs frame-rate data.
- FPS is smoothed over a short rolling window and follows the busiest swap chain in the selected application.

## Compatibility

- FPS capture supports DirectX, OpenGL, and Vulkan applications that expose presentation events to Windows.
- The existing HardwareScope OSD may not appear over games using true exclusive fullscreen; borderless fullscreen is recommended for the overlay.
