#pragma once

#include "hardwarescope/frame_rate_tracker.hpp"

#include <windows.h>
#include <evntrace.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <thread>

namespace hardwarescope {

class EtwFpsMonitor final {
public:
    EtwFpsMonitor() = default;
    ~EtwFpsMonitor();

    EtwFpsMonitor(const EtwFpsMonitor&) = delete;
    EtwFpsMonitor& operator=(const EtwFpsMonitor&) = delete;

    void Start(bool enabled, bool game_only, std::uint32_t smoothing_milliseconds);
    void Stop() noexcept;
    [[nodiscard]] FrameRateReading Snapshot() const noexcept;
    [[nodiscard]] bool CaptureActive() const noexcept { return capture_active_.load(std::memory_order_acquire); }

private:
    static void WINAPI EventRecordCallback(EVENT_RECORD* event) noexcept;
    void HandleEvent(const EVENT_RECORD& event) noexcept;
    void RunController(std::stop_token stop_token) noexcept;
    void StartCapture(std::uint32_t process_id, const wchar_t* application);
    void StopCapture() noexcept;
    void CaptureLoop(std::stop_token stop_token) noexcept;

    FrameRateTracker tracker_{};
    std::jthread controller_thread_{};
    std::jthread capture_thread_{};
    std::atomic<TRACEHANDLE> session_handle_{};
    std::atomic<std::uint32_t> target_process_id_{};
    std::atomic<bool> capture_active_{};
    std::array<wchar_t, 64U> target_application_{};
    std::array<wchar_t, 64U> session_name_{};
    SRWLOCK target_lock_ = SRWLOCK_INIT;
    bool enabled_{};
    bool game_only_{};
};

} // namespace hardwarescope
