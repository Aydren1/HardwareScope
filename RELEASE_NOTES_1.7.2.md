# HardwareScope 1.7.2

## Improved window movement

- The entire top header of the main HardwareScope window can now be used to move the window.
- The entire top header of Settings is draggable as well.
- Search fields, dropdowns, toggles, buttons, and window controls remain fully interactive and do not start a drag.
- Double-clicking any non-interactive part of a header maximizes or restores the window.
- Dragging a maximized window restores it so it can be repositioned.

## Gameplay responsiveness

- FPS readings no longer make the interface wait on PresentMon's high-rate frame stream.
- FPS processing is rate-limited and memory-bounded, including in very high-frame-rate games.
- Mouse and keyboard input now take priority over FPS text refreshes.
- FPS remains independent from the slower hardware-sensor refresh timer.
- FPS monitoring now follows HWiNFO's model: one continuous capture session automatically reports the active 3D process with the highest frame rate.
- Alt-Tabbing no longer stops and restarts capture just because the foreground window changed.
