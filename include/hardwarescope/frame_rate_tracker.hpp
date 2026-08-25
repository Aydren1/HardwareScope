#pragma once

#include <windows.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace hardwarescope {

struct FrameRateReading final {
    bool available{};
    std::uint32_t frames_per_second{};
    std::uint32_t process_id{};
    std::array<wchar_t, 64U> application{};
};

class FrameRateTracker final {
public:
    FrameRateTracker() noexcept;

    FrameRateTracker(const FrameRateTracker&) = delete;
    FrameRateTracker& operator=(const FrameRateTracker&) = delete;

    void SetSmoothingMilliseconds(std::uint32_t milliseconds) noexcept;
    void Clear() noexcept;
    void RecordPresent(
        std::uint32_t process_id,
        std::uint64_t swap_chain,
        double event_milliseconds,
        std::int64_t callback_qpc,
        const wchar_t* application,
        bool known_game) noexcept;
    [[nodiscard]] FrameRateReading Snapshot(std::int64_t now_qpc, std::uint32_t foreground_process_id, bool game_only) const noexcept;
    [[nodiscard]] std::size_t ActiveSeries() const noexcept;

private:
    static constexpr std::size_t kMaximumSeries = 16U;
    static constexpr std::size_t kMaximumSamples = 2'048U;

    struct Series final {
        std::uint32_t process_id{};
        std::uint64_t swap_chain{};
        std::array<wchar_t, 64U> application{};
        std::array<double, kMaximumSamples> event_milliseconds{};
        std::size_t first{};
        std::size_t count{};
        std::int64_t last_qpc{};
        bool known_game{};
        bool active{};
    };

    [[nodiscard]] static double Fps(const Series& series, double window_milliseconds) noexcept;

    mutable SRWLOCK lock_ = SRWLOCK_INIT;
    std::array<Series, kMaximumSeries> series_{};
    std::int64_t qpc_frequency_{};
    double smoothing_milliseconds_{500.0};
};

} // namespace hardwarescope
