#include <windows.h>

#include <filesystem>
#include <fstream>

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    std::error_code error;
    const auto marker = std::filesystem::temp_directory_path(error) / L"HardwareScopeNativeUpdateRelaunchFixture.txt";
    if (error) return 2;
    std::ofstream stream{marker, std::ios::out | std::ios::trunc};
    if (!stream) return 3;
    stream << "restarted\n";
    return 0;
}
