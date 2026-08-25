# HardwareScope 2.0 architecture

## Non-negotiable rules

1. Hardware access never runs on the window thread.
2. Rendering never waits for hardware access, ETW, storage I/O, settings I/O, or networking.
3. Snapshot publication is bounded and cannot grow with runtime.
4. The UI paints only visible sensor rows and only after Windows asks it to paint.
5. Window movement and resizing are native non-client operations handled by DWM.
6. Expensive providers use independent polling schedules and cache static metadata.
7. The stable 1.x build remains available until native sensor and updater parity is verified.
8. The main telemetry window defaults to software Direct2D to avoid GPU-driver memory, thread, and handle overhead.

## Process model

The normal application is one native process. PawnIO remains the signed privileged hardware bridge. The updater is a separate native executable that runs only during an update.

## Data flow

```text
Native providers -> polling coordinator -> local SensorSnapshot
                                             |
                                         SnapshotStore
                                             |
                         WM_APP_SNAPSHOT -> Direct2D renderer

Game detector -> native ETW worker ---------^
```

The worker builds a complete snapshot locally and publishes it in one operation. A
short mutex protects only the bounded snapshot memory copy; it never encloses
hardware collection or I/O. The renderer copies the most recent snapshot and never
walks provider objects. This deliberately favors a small, provably coherent critical
section over a complex lock-free protocol.

## Provider boundaries

Each provider will expose discovery, static metadata, polling cadence, and value collection through a narrow native interface. CPU, GPU, storage, memory, motherboard, and FPS providers can fail independently without stopping the UI.
