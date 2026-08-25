#pragma once

#include "hardwarescope/sensor_snapshot.hpp"

#include <array>
#include <cstdint>
#include <string_view>

namespace hardwarescope {

enum class SensorSection : std::uint8_t {
    cpu_temperatures,
    cpu_usage,
    cpu_clock_speeds,
    cpu_power_and_voltage,
    graphics,
    storage,
    memory,
    system_and_other,
    frame_rate,
    count,
};

struct SensorViewRow final {
    SensorSection section{};
    std::uint16_t sensor_index{};
    std::uint16_t matching_sensor_count{};
    bool is_section{};
    bool is_placeholder{};
};

struct SensorView final {
    static constexpr std::size_t kMaximumRows = kMaxSensors + static_cast<std::size_t>(SensorSection::count) + 1U;
    std::array<SensorViewRow, kMaximumRows> rows{};
    std::uint32_t count{};
};

[[nodiscard]] SensorSection ClassifySensor(const SensorValue& sensor) noexcept;
[[nodiscard]] const wchar_t* SensorSectionName(SensorSection section) noexcept;
[[nodiscard]] SensorView BuildSensorView(
    const SensorSnapshot& snapshot,
    std::uint32_t collapsed_sections,
    std::wstring_view query,
    bool show_unavailable_cpu_temperature = false) noexcept;

} // namespace hardwarescope
