#include "hardwarescope/sensor_worker.hpp"

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <iostream>
#include <string_view>
#include <thread>

namespace {

void Published(void* const context, const std::uint64_t sequence) noexcept {
    static_cast<std::atomic<std::uint64_t>*>(context)->store(sequence, std::memory_order_release);
}

} // namespace

int wmain(const int argument_count, wchar_t** arguments) {
    const auto expect_fps = argument_count == 2 && std::wstring_view{arguments[1]} == L"--expect-fps";
    hardwarescope::SnapshotStore store;
    std::atomic<std::uint64_t> sequence{};
    hardwarescope::SensorWorker worker(store, &Published, &sequence, hardwarescope::SensorWorkerMode::native, GetModuleHandleW(nullptr));
    worker.ConfigureFps(true, true, 100U, 500U);
    worker.Start(std::chrono::milliseconds{750});
    std::uint64_t sequence_when_fps_appeared{};
    if (expect_fps) {
        for (int attempt = 0; attempt < 240 && sequence_when_fps_appeared == 0U; ++attempt) {
            std::this_thread::sleep_for(std::chrono::milliseconds{25});
            const auto candidate = store.ReadLatest();
            const auto frame = std::find_if(candidate.sensors.begin(), candidate.sensors.begin() + candidate.count, [](const auto& sensor) {
                return sensor.kind == hardwarescope::SensorKind::frame_rate;
            });
            if (frame != candidate.sensors.begin() + candidate.count) {
                sequence_when_fps_appeared = sequence.load(std::memory_order_acquire);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{1'200});
    } else {
        std::this_thread::sleep_for(std::chrono::milliseconds{4'000});
    }
    worker.Stop();
    const auto snapshot = store.ReadLatest();
    const auto fps = std::find_if(snapshot.sensors.begin(), snapshot.sensors.begin() + snapshot.count, [](const auto& sensor) {
        return sensor.kind == hardwarescope::SensorKind::frame_rate;
    });
    std::wcout << L"Published snapshots: " << sequence.load(std::memory_order_acquire) << L"\n"
               << L"Latest sensor count: " << snapshot.count << L"\n"
               << L"FPS: " << (fps == snapshot.sensors.begin() + snapshot.count ? L"-" : std::to_wstring(fps->current)) << L"\n";
    const auto published = sequence.load(std::memory_order_acquire);
    const auto fps_cadence_samples = sequence_when_fps_appeared == 0U ? 0U : published - sequence_when_fps_appeared;
    if (expect_fps && (fps_cadence_samples < 8U || fps_cadence_samples > 16U)) {
        std::cerr << "FAIL: active FPS publishing did not settle near its independent 100 ms cadence\n";
        return 1;
    }
    if (!expect_fps && (published < 4U || published > 10U)) {
        std::cerr << "FAIL: desktop publishing did not fall back to the lightweight hardware cadence\n";
        return 1;
    }
    if (expect_fps != (fps != snapshot.sensors.begin() + snapshot.count)) {
        std::cerr << "FAIL: game-only frame sensor presence did not match the running-game state\n";
        return 1;
    }
    return 0;
}
