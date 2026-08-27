#include "hardwarescope/osd_model.hpp"
#include "hardwarescope/sensor_view_model.hpp"

#include <algorithm>
#include <array>
#include <string_view>

namespace hardwarescope {
namespace {

bool Contains(const std::wstring_view text, const std::wstring_view part) noexcept {
    return text.find(part) != std::wstring_view::npos;
}

OsdHardwareGroup GroupFor(const SensorValue& sensor) noexcept {
    const std::wstring_view name{sensor.name.data()};
    const std::wstring_view hardware{sensor.hardware.data()};
    if (sensor.kind == SensorKind::frame_rate) return OsdHardwareGroup::fps;
    if (Contains(name, L"CPU") || Contains(name, L"Tctl") || Contains(hardware, L"CPU") || Contains(hardware, L"Ryzen")) return OsdHardwareGroup::cpu;
    if (Contains(name, L"GPU") || Contains(hardware, L"NVIDIA") || Contains(hardware, L"Radeon")) return OsdHardwareGroup::gpu;
    if (Contains(name, L"Drive") || Contains(hardware, L"SSD") || Contains(hardware, L"NVMe")) return OsdHardwareGroup::storage;
    if (Contains(name, L"DIMM") || Contains(name, L"Memory")) return OsdHardwareGroup::memory;
    return OsdHardwareGroup::other;
}

std::wstring FormattedValue(const SensorValue& sensor) {
    std::array<wchar_t, 64> value{};
    switch (sensor.unit) {
    case SensorUnit::celsius: swprintf_s(value.data(), value.size(), L"%.1f °C", sensor.current); break;
    case SensorUnit::percent: swprintf_s(value.data(), value.size(), L"%.1f %%", sensor.current); break;
    case SensorUnit::megahertz: swprintf_s(value.data(), value.size(), L"%.0f MHz", sensor.current); break;
    case SensorUnit::revolutions_per_minute: swprintf_s(value.data(), value.size(), L"%.0f RPM", sensor.current); break;
    case SensorUnit::watts: swprintf_s(value.data(), value.size(), L"%.1f W", sensor.current); break;
    case SensorUnit::volts: swprintf_s(value.data(), value.size(), L"%.3f V", sensor.current); break;
    case SensorUnit::megabytes: swprintf_s(value.data(), value.size(), L"%.0f MB", sensor.current); break;
    case SensorUnit::frames_per_second: swprintf_s(value.data(), value.size(), L"%.0f", sensor.current); break;
    case SensorUnit::milliseconds: swprintf_s(value.data(), value.size(), L"%.2f ms", sensor.current); break;
    }
    return value.data();
}

std::wstring LabelFor(const SensorValue& sensor) {
    const std::wstring_view name{sensor.name.data()};
    if (sensor.id == kFpsOnePercentLowSensorId) return L"1% Low";
    if (sensor.id == kFpsFrameTimeSensorId) return L"Frame time";
    if (sensor.kind == SensorKind::frame_rate) return L"FPS";
    if (Contains(name, L"Tctl") || Contains(name, L"CPU Package")) return L"CPU Tctl/Tdie";
    if (Contains(name, L"Memory Junction")) return L"GPU Memory Junction";
    if (Contains(name, L"GPU Core temperature")) return L"GPU Core";
    return sensor.name.data();
}

bool IsCpuEasyTemperature(const SensorValue& sensor) noexcept {
    if (sensor.kind != SensorKind::temperature) return false;
    const std::wstring_view name{sensor.name.data()};
    return Contains(name, L"Tctl/Tdie") || Contains(name, L"CPU Package");
}

bool IsGpuCoreEasyTemperature(const SensorValue& sensor) noexcept {
    return sensor.kind == SensorKind::temperature && Contains(sensor.name.data(), L"GPU Core temperature");
}

bool IsGpuMemoryEasyTemperature(const SensorValue& sensor) noexcept {
    return sensor.kind == SensorKind::temperature && Contains(sensor.name.data(), L"Memory Junction");
}

} // namespace

std::vector<OsdDisplayItem> BuildOsdDisplayItems(const SensorSnapshot& snapshot, const AppSettings& settings) {
    std::vector<OsdDisplayItem> items;
    items.reserve(12U);
    auto add = [&](const SensorValue& sensor, const bool fps) {
        if (!sensor.available) return;
        if (std::any_of(items.begin(), items.end(), [&](const OsdDisplayItem& item) { return item.sensor_id == sensor.id; })) return;
        items.push_back(OsdDisplayItem{
            sensor.id,
            LabelFor(sensor) + L" " + FormattedValue(sensor),
            GroupFor(sensor),
            SensorSectionColor(ClassifySensor(sensor), settings),
            fps});
    };

    if (settings.fps_enabled) {
        for (std::uint32_t index = 0; index < snapshot.count; ++index) {
            if (snapshot.sensors[index].id == kFpsSensorId) {
                add(snapshot.sensors[index], true);
                break;
            }
        }
        if (settings.fps_one_percent_low_enabled) {
            for (std::uint32_t index = 0; index < snapshot.count; ++index) {
                if (snapshot.sensors[index].id == kFpsOnePercentLowSensorId) {
                    add(snapshot.sensors[index], true);
                    break;
                }
            }
        }
    }

    if (settings.easy_temperature_enabled) {
        bool cpu_added = false;
        bool gpu_added = false;
        bool memory_added = false;
        for (std::uint32_t index = 0; index < snapshot.count; ++index) {
            const auto& sensor = snapshot.sensors[index];
            if (!cpu_added && (settings.easy_temperature_mask & easy_cpu_package) != 0U && IsCpuEasyTemperature(sensor)) {
                add(sensor, false);
                cpu_added = true;
            } else if (!gpu_added && (settings.easy_temperature_mask & easy_gpu_core) != 0U && IsGpuCoreEasyTemperature(sensor)) {
                add(sensor, false);
                gpu_added = true;
            } else if (!memory_added && (settings.easy_temperature_mask & easy_gpu_memory_junction) != 0U && IsGpuMemoryEasyTemperature(sensor)) {
                add(sensor, false);
                memory_added = true;
            }
        }
    }

    for (std::uint32_t pinned = 0; pinned < settings.pinned_sensor_count; ++pinned) {
        for (std::uint32_t index = 0; index < snapshot.count; ++index) {
            if (snapshot.sensors[index].id == settings.pinned_sensor_ids[pinned]) {
                add(snapshot.sensors[index], snapshot.sensors[index].kind == SensorKind::frame_rate);
                break;
            }
        }
    }
    return items;
}

std::vector<OsdDisplayItem> BuildOsdSurfaceItems(
    const SensorSnapshot& snapshot,
    const AppSettings& settings,
    const bool fps_surface) {
    auto items = BuildOsdDisplayItems(snapshot, settings);
    if (!settings.fps_separate_position) {
        if (fps_surface) items.clear();
        return items;
    }
    std::erase_if(items, [fps_surface](const OsdDisplayItem& item) { return item.fps != fps_surface; });
    return items;
}

bool IsSensorSelectedForOsd(const SensorValue& sensor, const AppSettings& settings) noexcept {
    if (sensor.id == kFpsSensorId) return settings.fps_enabled;
    if (sensor.id == kFpsOnePercentLowSensorId) return settings.fps_enabled && settings.fps_one_percent_low_enabled;
    if (sensor.id == kFpsFrameTimeSensorId) return settings.osd_graph_enabled && settings.osd_graph_sensor_id == sensor.id;
    if (sensor.kind == SensorKind::frame_rate) return false;
    if (settings.IsSensorPinned(sensor.id)) return true;
    if (!settings.easy_temperature_enabled) return false;
    if (IsCpuEasyTemperature(sensor)) return (settings.easy_temperature_mask & easy_cpu_package) != 0U;
    if (IsGpuCoreEasyTemperature(sensor)) return (settings.easy_temperature_mask & easy_gpu_core) != 0U;
    if (IsGpuMemoryEasyTemperature(sensor)) return (settings.easy_temperature_mask & easy_gpu_memory_junction) != 0U;
    return false;
}

void SetSensorSelectedForOsd(const SensorValue& sensor, AppSettings& settings, const bool selected) noexcept {
    if (sensor.id == kFpsSensorId) {
        settings.fps_enabled = selected;
        return;
    }
    if (sensor.id == kFpsOnePercentLowSensorId) {
        settings.fps_one_percent_low_enabled = selected;
        return;
    }
    if (sensor.id == kFpsFrameTimeSensorId) {
        settings.osd_graph_sensor_id = sensor.id;
        settings.osd_graph_enabled = selected;
        return;
    }
    if (selected) {
        static_cast<void>(settings.PinSensor(sensor.id));
        return;
    }
    static_cast<void>(settings.UnpinSensor(sensor.id));
    if (!settings.easy_temperature_enabled) return;
    if (IsCpuEasyTemperature(sensor)) settings.easy_temperature_mask &= ~easy_cpu_package;
    if (IsGpuCoreEasyTemperature(sensor)) settings.easy_temperature_mask &= ~easy_gpu_core;
    if (IsGpuMemoryEasyTemperature(sensor)) settings.easy_temperature_mask &= ~easy_gpu_memory_junction;
}

} // namespace hardwarescope
