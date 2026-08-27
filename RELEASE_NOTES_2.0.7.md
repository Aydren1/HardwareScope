# HardwareScope 2.0.7

This update adds deeper game-performance context without turning HardwareScope into a heavy dashboard.

- Adds a rolling 1% low FPS counter beside the normal FPS reading.
- Calculates 1% lows from an allocation-free PresentMon frame-time history of up to 60 seconds and refreshes the percentile once per second.
- Adds one optional live OSD graph for true FPS frame time or any available hardware sensor.
- Adds graph source, 5–60 second history, 100–500 ms refresh, width, and height controls on a dedicated Graphs tab.
- Keeps graph history in a fixed 600-sample ring buffer with no background thread or per-sample allocation.
- Keeps FPS and graph updates independent from the hardware polling interval.
- Adds frame-time and 1% low sensor explanations and deterministic statistics tests.
