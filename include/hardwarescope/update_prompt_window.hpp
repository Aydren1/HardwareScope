#pragma once

#include "hardwarescope/app_settings.hpp"
#include "hardwarescope/update_manifest.hpp"

#include <windows.h>

#include <chrono>
#include <cstdint>
#include <string_view>

namespace hardwarescope {

enum class UpdatePromptAction : std::uint8_t {
    install_now,
    remind_later,
    skip_version,
};

struct UpdatePromptResult final {
    UpdatePromptAction action{UpdatePromptAction::remind_later};
    std::chrono::seconds delay{std::chrono::hours{24}};
};

inline constexpr wchar_t kUpdatePromptWindowClass[] = L"HardwareScope.Native.UpdatePromptWindow";

[[nodiscard]] UpdatePromptResult ShowUpdatePromptWindow(
    HWND owner,
    HINSTANCE instance,
    const UpdateManifest& manifest,
    const AppSettings& settings) noexcept;

void ShowUpdateNoticeWindow(
    HWND owner,
    HINSTANCE instance,
    const AppSettings& settings,
    std::wstring_view heading,
    std::wstring_view message,
    bool warning = false) noexcept;

} // namespace hardwarescope
