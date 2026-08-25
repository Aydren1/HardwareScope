#pragma once

#include "hardwarescope/update_manifest.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>

namespace hardwarescope {

enum class UpdateCheckStatus : std::uint8_t {
    failed,
    current,
    available,
};

struct UpdateCheckResult final {
    UpdateCheckStatus status{UpdateCheckStatus::failed};
    UpdateManifest manifest{};
    std::uint32_t system_error{};
};

[[nodiscard]] UpdateCheckResult CheckStableUpdate(const SemanticVersion& installed_version) noexcept;
[[nodiscard]] std::optional<std::filesystem::path> DownloadVerifiedInstaller(const UpdateManifest& manifest) noexcept;
[[nodiscard]] bool LaunchUpdateHandoff(
    const std::filesystem::path& updater,
    const std::filesystem::path& installer,
    const UpdateManifest& manifest,
    std::uint32_t application_process_id,
    const std::filesystem::path& application_to_relaunch) noexcept;

} // namespace hardwarescope
