#include "hardwarescope/frame_rate_tracker.hpp"

#include <algorithm>
#include <cmath>
#include <cwchar>
#include <limits>

namespace hardwarescope {

FrameRateTracker::FrameRateTracker() noexcept {
    LARGE_INTEGER frequency{};
    QueryPerformanceFrequency(&frequency);
    qpc_frequency_ = frequency.QuadPart;
}

void FrameRateTracker::SetSmoothingMilliseconds(const std::uint32_t milliseconds) noexcept {
    AcquireSRWLockExclusive(&lock_);
    smoothing_milliseconds_ = static_cast<double>(std::clamp(milliseconds, 250U, 1'250U));
    ReleaseSRWLockExclusive(&lock_);
}

void FrameRateTracker::Clear() noexcept {
    AcquireSRWLockExclusive(&lock_);
    series_ = {};
    ReleaseSRWLockExclusive(&lock_);
}

void FrameRateTracker::RecordPresent(
    const std::uint32_t process_id,
    const std::uint64_t swap_chain,
    const double event_milliseconds,
    const std::int64_t callback_qpc,
    const wchar_t* const application,
    const bool known_game) noexcept {
    if (process_id == 0U || !std::isfinite(event_milliseconds) || event_milliseconds < 0.0
        || !TryAcquireSRWLockExclusive(&lock_)) return;

    auto* selected = static_cast<Series*>(nullptr);
    auto* oldest = &series_.front();
    for (auto& series : series_) {
        if (series.active && series.process_id == process_id && series.swap_chain == swap_chain) {
            selected = &series;
            break;
        }
        if (!series.active && selected == nullptr) selected = &series;
        if (series.last_qpc < oldest->last_qpc) oldest = &series;
    }
    if (selected == nullptr) selected = oldest;
    if (!selected->active || selected->process_id != process_id || selected->swap_chain != swap_chain) {
        *selected = {};
        selected->active = true;
        selected->process_id = process_id;
        selected->swap_chain = swap_chain;
        selected->known_game = known_game;
        static_cast<void>(wcsncpy_s(selected->application.data(), selected->application.size(), application == nullptr ? L"3D application" : application, _TRUNCATE));
    }

    if (selected->count != 0U) {
        const auto last_index = (selected->first + selected->count - 1U) % selected->event_milliseconds.size();
        if (event_milliseconds <= selected->event_milliseconds[last_index]) {
            ReleaseSRWLockExclusive(&lock_);
            return;
        }
    }
    if (selected->count == selected->event_milliseconds.size()) {
        selected->first = (selected->first + 1U) % selected->event_milliseconds.size();
        --selected->count;
    }
    const auto insert = (selected->first + selected->count) % selected->event_milliseconds.size();
    selected->event_milliseconds[insert] = event_milliseconds;
    ++selected->count;
    selected->last_qpc = callback_qpc;

    const auto cutoff = event_milliseconds - smoothing_milliseconds_;
    while (selected->count > 1U && selected->event_milliseconds[selected->first] < cutoff) {
        selected->first = (selected->first + 1U) % selected->event_milliseconds.size();
        --selected->count;
    }
    ReleaseSRWLockExclusive(&lock_);
}

double FrameRateTracker::Fps(const Series& series, const double window_milliseconds) noexcept {
    if (!series.active || series.count < 2U) return 0.0;
    auto first_index = series.first;
    const auto last_index = (series.first + series.count - 1U) % series.event_milliseconds.size();
    const auto last = series.event_milliseconds[last_index];
    const auto cutoff = last - window_milliseconds;
    std::size_t sample_count = series.count;
    while (sample_count > 1U && series.event_milliseconds[first_index] < cutoff) {
        first_index = (first_index + 1U) % series.event_milliseconds.size();
        --sample_count;
    }
    const auto duration = last - series.event_milliseconds[first_index];
    if (sample_count < 2U || duration <= 0.05) return 0.0;
    return 1'000.0 * static_cast<double>(sample_count - 1U) / duration;
}

FrameRateReading FrameRateTracker::Snapshot(
    const std::int64_t now_qpc,
    const std::uint32_t foreground_process_id,
    const bool game_only) const noexcept {
    FrameRateReading reading{};
    AcquireSRWLockShared(&lock_);
    double best_fps{};
    bool best_foreground{};
    for (const auto& series : series_) {
        if (!series.active || series.count < 2U) continue;
        const auto age_seconds = qpc_frequency_ > 0
            ? static_cast<double>(now_qpc - series.last_qpc) / static_cast<double>(qpc_frequency_)
            : std::numeric_limits<double>::infinity();
        if (age_seconds < 0.0 || age_seconds > 2.5 || (game_only && !series.known_game)) continue;
        const auto fps = Fps(series, smoothing_milliseconds_);
        if (!std::isfinite(fps) || fps < 1.0 || fps > 9'999.0) continue;
        const auto foreground = series.process_id == foreground_process_id;
        if (!reading.available || (foreground && !best_foreground) || foreground == best_foreground && fps > best_fps) {
            reading.available = true;
            reading.frames_per_second = static_cast<std::uint32_t>(std::clamp(std::llround(fps), 1LL, 9'999LL));
            reading.process_id = series.process_id;
            reading.application = series.application;
            best_fps = fps;
            best_foreground = foreground;
        }
    }
    ReleaseSRWLockShared(&lock_);
    return reading;
}

std::size_t FrameRateTracker::ActiveSeries() const noexcept {
    AcquireSRWLockShared(&lock_);
    const auto count = static_cast<std::size_t>(std::count_if(series_.begin(), series_.end(), [](const Series& series) { return series.active; }));
    ReleaseSRWLockShared(&lock_);
    return count;
}

} // namespace hardwarescope
