#include "hardwarescope/sensor_view_model.hpp"

#include <cwctype>

namespace hardwarescope {
namespace {

bool ContainsInsensitive(const wchar_t* const text, const std::wstring_view query) noexcept {
    if (query.empty()) return true;
    if (text == nullptr) return false;
    const std::wstring_view value{text};
    if (query.size() > value.size()) return false;
    for (std::size_t start = 0U; start + query.size() <= value.size(); ++start) {
        bool match = true;
        for (std::size_t index = 0U; index < query.size(); ++index) {
            if (std::towlower(value[start + index]) != std::towlower(query[index])) {
                match = false;
                break;
            }
        }
        if (match) return true;
    }
    return false;
}

bool Matches(const SensorValue& sensor, const std::wstring_view query) noexcept {
    return ContainsInsensitive(sensor.name.data(), query) || ContainsInsensitive(sensor.hardware.data(), query);
}

} // namespace

SensorSection ClassifySensor(const SensorValue& sensor) noexcept {
    if (sensor.kind == SensorKind::frame_rate) return SensorSection::frame_rate;
    const auto domain = static_cast<std::uint8_t>(sensor.id >> 56U);
    if (domain == 0x01U) {
        if (sensor.kind == SensorKind::temperature) return SensorSection::cpu_temperatures;
        if (sensor.kind == SensorKind::utilization) return SensorSection::cpu_usage;
        if (sensor.kind == SensorKind::clock) return SensorSection::cpu_clock_speeds;
        return SensorSection::cpu_power_and_voltage;
    }
    if (domain == 0x02U) return SensorSection::graphics;
    if (domain == 0x03U) return SensorSection::storage;
    if (domain == 0x04U) return SensorSection::memory;
    return SensorSection::system_and_other;
}

const wchar_t* SensorSectionName(const SensorSection section) noexcept {
    switch (section) {
    case SensorSection::cpu_temperatures: return L"CPU temperatures";
    case SensorSection::cpu_usage: return L"CPU usage";
    case SensorSection::cpu_clock_speeds: return L"CPU clock speeds";
    case SensorSection::cpu_power_and_voltage: return L"CPU power & voltage";
    case SensorSection::graphics: return L"Graphics";
    case SensorSection::storage: return L"Storage & drives";
    case SensorSection::memory: return L"Memory";
    case SensorSection::system_and_other: return L"System & other";
    case SensorSection::frame_rate: return L"Frame rate";
    case SensorSection::count: break;
    }
    return L"Sensors";
}

SensorView BuildSensorView(
    const SensorSnapshot& snapshot,
    const std::uint32_t collapsed_sections,
    const std::wstring_view query,
    const bool show_unavailable_cpu_temperature) noexcept {
    SensorView view{};
    for (std::uint32_t raw_section = 0U; raw_section < static_cast<std::uint32_t>(SensorSection::count); ++raw_section) {
        const auto section = static_cast<SensorSection>(raw_section);
        std::uint16_t matching_count{};
        for (std::uint32_t index = 0U; index < snapshot.count; ++index) {
            if (ClassifySensor(snapshot.sensors[index]) == section && Matches(snapshot.sensors[index], query)) ++matching_count;
        }
        const auto cpu_placeholder = show_unavailable_cpu_temperature
            && section == SensorSection::cpu_temperatures
            && matching_count == 0U
            && query.empty();
        if (matching_count == 0U && !cpu_placeholder) continue;
        view.rows[view.count++] = SensorViewRow{section, 0U, matching_count, true, false};
        const auto collapsed = (collapsed_sections & (1U << raw_section)) != 0U;
        if (collapsed && query.empty()) continue;
        if (cpu_placeholder) {
            view.rows[view.count++] = SensorViewRow{section, 0U, 0U, false, true};
            continue;
        }
        for (std::uint32_t index = 0U; index < snapshot.count; ++index) {
            if (ClassifySensor(snapshot.sensors[index]) != section || !Matches(snapshot.sensors[index], query)) continue;
            view.rows[view.count++] = SensorViewRow{section, static_cast<std::uint16_t>(index), 0U, false, false};
        }
    }
    return view;
}

} // namespace hardwarescope
