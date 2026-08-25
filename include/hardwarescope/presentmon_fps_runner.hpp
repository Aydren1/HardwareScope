#pragma once

#include <windows.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

namespace hardwarescope {

struct PresentMonFpsReading final {
    bool available{};
    std::uint32_t frames_per_second{};
    std::uint32_t process_id{};
    std::array<wchar_t, 64U> application{};
};

class PresentMonFpsRunner final {
public:
    PresentMonFpsRunner();
    ~PresentMonFpsRunner();

    PresentMonFpsRunner(const PresentMonFpsRunner&) = delete;
    PresentMonFpsRunner& operator=(const PresentMonFpsRunner&) = delete;

    void SetTarget(std::uint32_t process_id, std::uint32_t smoothing_milliseconds) noexcept;
    [[nodiscard]] PresentMonFpsReading Snapshot() const noexcept;
    void Stop() noexcept;

private:
    void Start(std::uint32_t process_id) noexcept;
    void ReadOutput(std::stop_token token, HANDLE pipe, std::uint32_t process_id) noexcept;
    void RecordInterval(double milliseconds, std::uint32_t process_id) noexcept;
    [[nodiscard]] std::wstring RuntimePath() const;
    static void CleanupOrphanedSessions() noexcept;

    mutable std::mutex mutex_;
    std::jthread reader_thread_{};
    HANDLE process_{};
    HANDLE pipe_{};
    std::atomic<std::uint32_t> target_process_id_{};
    std::atomic<std::uint32_t> smoothing_milliseconds_{500U};
    ULONGLONG next_start_attempt_tick_{};
    std::array<double, 512U> intervals_{};
    std::size_t interval_first_{};
    std::size_t interval_count_{};
    double interval_total_{};
    ULONGLONG last_frame_tick_{};
    std::array<wchar_t, 64U> application_{};
};

} // namespace hardwarescope
