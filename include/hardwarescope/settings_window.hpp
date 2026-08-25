#pragma once

#include "hardwarescope/app_settings.hpp"
#include "hardwarescope/sensor_snapshot.hpp"

#include <windows.h>

namespace hardwarescope {

[[nodiscard]] bool ShowSettingsWindow(HWND owner, AppSettings& settings, const SensorSnapshot& snapshot) noexcept;

} // namespace hardwarescope
