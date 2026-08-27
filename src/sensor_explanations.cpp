#include "hardwarescope/sensor_explanations.hpp"

#include <string_view>

namespace hardwarescope {
namespace {

bool Contains(const std::wstring_view value, const std::wstring_view part) noexcept {
    return value.find(part) != std::wstring_view::npos;
}

} // namespace

const wchar_t* ColumnExplanation(const TableColumn column) noexcept {
    switch (column) {
    case TableColumn::osd: return L"OSD means On-Screen Display. Check a sensor to show it over games and other applications.";
    case TableColumn::sensor: return L"The hardware measurement reported by the device or Windows.";
    case TableColumn::current: return L"The newest valid reading collected for this sensor.";
    case TableColumn::minimum: return L"The lowest valid reading observed since HardwareScope started.";
    case TableColumn::maximum: return L"The highest valid reading observed since HardwareScope started.";
    case TableColumn::hardware: return L"The processor, graphics card, drive, memory module, or controller that provides the reading.";
    }
    return L"HardwareScope sensor information.";
}

std::wstring SensorExplanation(const SensorValue& sensor) {
    const std::wstring_view name{sensor.name.data()};
    if (Contains(name, L"Tctl/Tdie") || Contains(name, L"CPU Package")) return L"The CPU control/package temperature used for thermal management; it represents the main CPU die reading.";
    if (Contains(name, L"CCD")) return L"Temperature of a CPU core complex die, which contains a group of processor cores and cache.";
    if (Contains(name, L"GPU Memory Junction")) return L"The hottest measured junction inside the graphics card's memory chips.";
    if (Contains(name, L"GPU Hot Spot")) return L"The hottest measured point on the graphics processor die.";
    if (Contains(name, L"GPU Core temperature")) return L"The main temperature reported for the graphics processor core.";
    if (Contains(name, L"Drive composite")) return L"The drive's primary combined temperature reading reported by its firmware.";
    if (Contains(name, L"Drive temperature sensor")) return L"An additional physical temperature sensor exposed by this drive.";
    if (Contains(name, L"DIMM") && sensor.kind == SensorKind::temperature) return L"Temperature reported by the thermal sensor on this memory module.";
    if (sensor.id == kFpsOnePercentLowSensorId) return L"Average frame rate across the slowest one percent of recent frames. Lower values reveal stutter.";
    if (sensor.id == kFpsFrameTimeSensorId) return L"Time used to present the latest game frame. Lower and steadier values feel smoother.";
    if (sensor.kind == SensorKind::frame_rate) return L"Frames presented each second by the active game, measured from native Windows graphics events.";
    if (sensor.kind == SensorKind::fan) return L"The measured rotational speed of this cooling fan.";
    if (sensor.kind == SensorKind::clock) return L"The current operating frequency reported for this processor or graphics clock domain.";
    if (sensor.kind == SensorKind::utilization) return L"The percentage of this hardware resource currently in use.";
    if (sensor.kind == SensorKind::power) return L"The electrical power currently reported for this hardware component.";
    if (sensor.kind == SensorKind::voltage) return L"The electrical voltage currently reported for this hardware rail or component.";
    if (sensor.kind == SensorKind::temperature) return L"A live physical temperature reported by this hardware sensor.";
    if (sensor.kind == SensorKind::data) return L"A live capacity or data-size reading reported for this hardware resource.";
    return L"A live measurement reported by this hardware device.";
}

} // namespace hardwarescope
