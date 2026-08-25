#include "hardwarescope/startup_registration.hpp"

#include <windows.h>

#include <array>

namespace hardwarescope {

std::wstring BuildStartupCommand(const std::filesystem::path& executable, const bool start_minimized) {
    auto command = L"\"" + executable.wstring() + L"\"";
    if (start_minimized) command += L" --minimized";
    return command;
}

bool ApplyStartupRegistration(const bool enabled, const bool start_minimized) noexcept {
    constexpr wchar_t run_key[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    constexpr wchar_t value_name[] = L"HardwareScope";
    HKEY key{};
    const auto access = KEY_SET_VALUE | KEY_WOW64_64KEY;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, run_key, 0U, nullptr, REG_OPTION_NON_VOLATILE, access, nullptr, &key, nullptr) != ERROR_SUCCESS) return false;
    LSTATUS status{};
    if (!enabled) {
        status = RegDeleteValueW(key, value_name);
        if (status == ERROR_FILE_NOT_FOUND) status = ERROR_SUCCESS;
    } else {
        std::array<wchar_t, 32'768U> path{};
        const auto length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
        if (length == 0U || length >= path.size()) {
            RegCloseKey(key);
            return false;
        }
        const auto command = BuildStartupCommand(std::filesystem::path{path.data()}, start_minimized);
        status = RegSetValueExW(
            key,
            value_name,
            0U,
            REG_SZ,
            reinterpret_cast<const BYTE*>(command.c_str()),
            static_cast<DWORD>((command.size() + 1U) * sizeof(wchar_t)));
    }
    RegCloseKey(key);
    return status == ERROR_SUCCESS;
}

} // namespace hardwarescope
