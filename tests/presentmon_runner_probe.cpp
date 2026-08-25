#include "hardwarescope/presentmon_fps_runner.hpp"

#include <windows.h>

#include <iostream>

int main(const int argument_count, char** arguments) {
    if (argument_count != 2) return 2;
    hardwarescope::PresentMonFpsRunner runner;
    runner.SetTarget(strtoul(arguments[1], nullptr, 10), 500U);
    for (int attempt{}; attempt < 100; ++attempt) {
        const auto reading = runner.Snapshot();
        if (reading.available) {
            std::wcout << L"FPS: " << reading.frames_per_second << L" | " << reading.application.data() << L"\n";
            return reading.frames_per_second >= 10U ? 0 : 1;
        }
        Sleep(100U);
    }
    std::cerr << "FAIL: PresentMon produced no frame reading\n";
    return 1;
}
