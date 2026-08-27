# HardwareScope 2.0.8

This update replaces the basic single-line graph with a native, configurable graph system inspired by HWiNFO.

- Graph up to four available sensors at once when they use the same unit.
- Show the graph inside the OSD, in a detachable resizable window, or both.
- Adds fixed sensor-aware scales, smooth adaptive scaling, and custom minimum/maximum ranges.
- Adds configurable 5-second to 5-minute history, 50–1000 ms refresh, OSD size, grid, labels, line thickness, and individual line colors.
- Adds pause/resume, reset, and time-range zoom controls to the floating graph window.
- Keeps the newest sample anchored at the right edge and preserves a stable time axis while history fills.
- Saves floating-window position, size, and always-on-top preference.
- Keeps selected graph sensors at the top of the sensor picker for faster editing.
- Uses fixed-capacity native history buffers with no per-sample memory allocation.
- Expands deterministic graph tests and Windows UI smoke coverage, including floating-window paint, sizing, visibility, DPI transitions, and resource-leak checks.
