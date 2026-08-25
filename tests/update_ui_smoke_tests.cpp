#include <windows.h>

#include "hardwarescope/app_commands.hpp"

#include <iostream>

int main() {
    constexpr wchar_t main_class[] = L"HardwareScope.Native.MainWindow";
    constexpr wchar_t settings_class[] = L"HardwareScope.Native.SettingsWindow";
    const auto main_window = FindWindowW(main_class, nullptr);
    if (main_window == nullptr) {
        std::cerr << "FAIL: HardwareScope is not running\n";
        return 1;
    }
    static_cast<void>(SendMessageW(main_window, WM_COMMAND, hardwarescope::kCommandOpen, 0U));
    static_cast<void>(PostMessageW(main_window, WM_COMMAND, hardwarescope::kCommandSettings, 0U));
    HWND settings{};
    for (int attempt = 0; attempt < 100 && (settings == nullptr || !IsWindowVisible(settings)); ++attempt) {
        Sleep(50U);
        settings = FindWindowW(settings_class, nullptr);
    }
    if (settings == nullptr || !IsWindowVisible(settings)) {
        std::cerr << "FAIL: settings did not open before the automatic update check\n";
        return 1;
    }
    Sleep(12'000U);
    DWORD_PTR result{};
    if (!IsWindow(main_window)
        || !IsWindow(settings)
        || SendMessageTimeoutW(main_window, WM_NULL, 0U, 0U, SMTO_ABORTIFHUNG, 2'000U, &result) == 0U) {
        std::cerr << "FAIL: automatic update completion destabilized the modal UI\n";
        return 1;
    }
    static_cast<void>(PostMessageW(settings, WM_COMMAND, 2U, 0U));
    for (int attempt = 0; attempt < 100 && (IsWindow(settings) || !IsWindowEnabled(main_window)); ++attempt) Sleep(25U);
    if (IsWindow(settings) || !IsWindowEnabled(main_window)) {
        std::cerr << "FAIL: settings did not close after automatic update completion\n";
        return 1;
    }
    std::cout << "OK: automatic manifest check completes while Settings is modal without blocking or crashing the UI\n";
    return 0;
}
