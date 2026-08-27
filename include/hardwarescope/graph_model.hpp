#pragma once

#include "hardwarescope/app_settings.hpp"
#include "hardwarescope/sensor_snapshot.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace hardwarescope {

struct GraphRange final {
    double minimum{};
    double maximum{1.0};
};

struct GraphSeries final {
    static constexpr std::size_t kMaximumSamples = 6'000U;

    std::uint64_t sensor_id{};
    SensorUnit unit{SensorUnit::percent};
    std::array<wchar_t, 96U> name{};
    std::array<double, kMaximumSamples> samples{};
    std::array<std::uint64_t, kMaximumSamples> timestamps_milliseconds{};
    std::size_t first{};
    std::size_t count{};
    double current{};
    bool available{};

    [[nodiscard]] double Sample(std::size_t chronological_index) const noexcept;
    [[nodiscard]] std::uint64_t Timestamp(std::size_t chronological_index) const noexcept;
};

class GraphHistory final {
public:
    static constexpr std::size_t kMaximumSeries = AppSettings::kMaximumGraphSensors;

    void Configure(const AppSettings& settings) noexcept;
    void Update(const SensorSnapshot& snapshot, std::uint64_t tick_milliseconds) noexcept;
    void Clear() noexcept;
    void SetPaused(bool paused) noexcept { paused_ = paused; }
    [[nodiscard]] bool Paused() const noexcept { return paused_; }
    [[nodiscard]] std::size_t SeriesCount() const noexcept { return series_count_; }
    [[nodiscard]] const GraphSeries& Series(std::size_t index) const noexcept { return series_[index]; }
    [[nodiscard]] std::size_t DesiredSampleCount() const noexcept;
    [[nodiscard]] GraphRange Range() noexcept;

private:
    [[nodiscard]] GraphRange FixedRange(SensorUnit unit) const noexcept;
    [[nodiscard]] GraphRange ObservedRange() const noexcept;

    std::array<GraphSeries, kMaximumSeries> series_{};
    std::array<std::uint64_t, kMaximumSeries> configured_ids_{};
    std::size_t series_count_{};
    std::uint32_t history_seconds_{30U};
    std::uint32_t refresh_milliseconds_{100U};
    GraphScaleMode scale_mode_{GraphScaleMode::fixed};
    double custom_minimum_{};
    double custom_maximum_{100.0};
    GraphRange adaptive_range_{};
    bool adaptive_initialized_{};
    bool paused_{};
    std::uint64_t last_snapshot_sequence_{};
    std::uint64_t last_sample_tick_{};
};

[[nodiscard]] const wchar_t* GraphUnitSuffix(SensorUnit unit) noexcept;

} // namespace hardwarescope
