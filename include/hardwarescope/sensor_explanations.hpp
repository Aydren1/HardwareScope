#pragma once

#include "hardwarescope/sensor_snapshot.hpp"

#include <cstdint>
#include <string>

namespace hardwarescope {

enum class TableColumn : std::uint8_t {
    osd,
    sensor,
    current,
    minimum,
    maximum,
    hardware,
};

[[nodiscard]] const wchar_t* ColumnExplanation(TableColumn column) noexcept;
[[nodiscard]] std::wstring SensorExplanation(const SensorValue& sensor);

} // namespace hardwarescope
