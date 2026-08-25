#pragma once

#include <filesystem>
#include <string>

namespace hardwarescope {

[[nodiscard]] std::wstring BuildStartupCommand(const std::filesystem::path& executable, bool start_minimized);
[[nodiscard]] bool ApplyStartupRegistration(bool enabled, bool start_minimized) noexcept;

} // namespace hardwarescope
