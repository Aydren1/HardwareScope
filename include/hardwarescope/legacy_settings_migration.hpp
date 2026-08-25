#pragma once

#include "hardwarescope/app_settings.hpp"

#include <filesystem>
#include <string_view>

namespace hardwarescope {

[[nodiscard]] bool MigrateLegacySettingsJson(std::string_view json, AppSettings& destination) noexcept;
[[nodiscard]] bool MigrateLegacySettingsFile(const std::filesystem::path& path, AppSettings& destination) noexcept;

} // namespace hardwarescope
