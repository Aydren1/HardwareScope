#pragma once

#include "hardwarescope/app_settings.hpp"
#include "hardwarescope/sensor_snapshot.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace hardwarescope {

enum class OsdHardwareGroup : std::uint8_t {
    fps,
    cpu,
    gpu,
    storage,
    memory,
    other,
};

struct OsdDisplayItem final {
    std::uint64_t sensor_id{};
    std::wstring text;
    OsdHardwareGroup group{};
    std::uint32_t color_rgb{};
    bool fps{};
};

[[nodiscard]] std::vector<OsdDisplayItem> BuildOsdDisplayItems(
    const SensorSnapshot& snapshot,
    const AppSettings& settings);

[[nodiscard]] bool IsSensorSelectedForOsd(const SensorValue& sensor, const AppSettings& settings) noexcept;
void SetSensorSelectedForOsd(const SensorValue& sensor, AppSettings& settings, bool selected) noexcept;

} // namespace hardwarescope
