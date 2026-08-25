#include <windows.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

std::wstring Quote(const std::filesystem::path& path) {
    return L"\"" + path.native() + L"\"";
}

} // namespace

int wmain() {
    std::array<wchar_t, 32'768U> module_path{};
    const auto length = GetModuleFileNameW(nullptr, module_path.data(), static_cast<DWORD>(module_path.size()));
    if (length == 0U || length >= module_path.size()) return 2;
    const auto directory = std::filesystem::path{module_path.data()}.parent_path();
    const auto updater = directory / L"HardwareScopeNativeUpdater.exe";
    const auto fixture = directory / L"HardwareScopeNativeUpdateRelaunchFixture.exe";
    std::error_code error;
    const auto temporary = std::filesystem::temp_directory_path(error);
    if (error) return 2;
    const auto installer = temporary / L"HardwareScopeNativeInvalidUpdate.exe";
    const auto marker = temporary / L"HardwareScopeNativeUpdateRelaunchFixture.txt";
    std::filesystem::remove(marker, error);
    {
        std::ofstream stream{installer, std::ios::binary | std::ios::trunc};
        if (!stream) return 2;
        stream.put('x');
    }

    std::wstring command = Quote(updater);
    command += L" --apply " + Quote(installer);
    command += L" --size 1 --sha256 ";
    command += std::wstring(64U, L'0');
    command += L" --wait-pid 4294967295 --relaunch " + Quote(fixture);
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, directory.c_str(), &startup, &process)) {
        std::filesystem::remove(installer, error);
        return 2;
    }
    CloseHandle(process.hThread);
    const auto wait = WaitForSingleObject(process.hProcess, 10'000U);
    DWORD exit_code{};
    static_cast<void>(GetExitCodeProcess(process.hProcess, &exit_code));
    CloseHandle(process.hProcess);
    bool relaunched{};
    for (int attempt = 0; attempt < 100 && !relaunched; ++attempt) {
        Sleep(50U);
        relaunched = std::filesystem::exists(marker);
    }
    std::filesystem::remove(installer, error);
    std::filesystem::remove(marker, error);
    if (wait != WAIT_OBJECT_0 || exit_code != 14U || !relaunched) {
        std::cerr << "FAIL: updater verification failure did not relaunch the existing app; wait="
                  << wait << ", exit=" << exit_code << ", relaunched=" << relaunched << '\n';
        return 1;
    }
    std::cout << "OK: failed update handoff relaunches the existing application\n";
    return 0;
}
