# HardwareScope 1.7.1

## Improved FPS responsiveness

- FPS now refreshes independently from temperature and hardware sensor polling.
- The default FPS display refresh is 100 ms, while hardware sensors keep their existing configured refresh interval.
- The default FPS smoothing window is reduced from 1.25 seconds to 500 ms for faster response.
- Added separate FPS refresh options from 50–500 ms.
- Added separate FPS smoothing options from 250–1250 ms.
- The OSD changes only the FPS text during these fast updates, avoiding repeated hardware scans or overlay rebuilds.

## Performance

- PresentMon still runs only while FPS monitoring is enabled.
- Faster FPS display updates do not increase CPU, GPU, temperature, storage, or motherboard polling frequency.
