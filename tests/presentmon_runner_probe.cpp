#include "hardwarescope/presentmon_fps_runner.hpp"

#include <windows.h>

#include <iostream>

int main(const int argument_count, char** arguments) {
    if (argument_count != 2) return 2;
    hardwarescope::PresentMonFpsRunner runner;
    runner.SetTarget(strtoul(arguments[1], nullptr, 10), 500U);
    hardwarescope::PresentMonFpsReading last{};
    for (int attempt{}; attempt < 150; ++attempt) {
        const auto reading = runner.Snapshot();
        if (reading.available) last = reading;
        if (reading.available && reading.one_percent_low_frames_per_second != 0U) {
            std::wcout << L"FPS: " << reading.frames_per_second
                       << L" | 1% low: " << reading.one_percent_low_frames_per_second
                       << L" | frame time: " << reading.frame_time_milliseconds
                       << L" ms | " << reading.application.data() << L"\n";
            return reading.frames_per_second >= 10U && reading.frame_time_milliseconds > 0.05 ? 0 : 1;
        }
        Sleep(100U);
    }
    std::cerr << "FAIL: PresentMon produced no complete reading (available=" << last.available
              << ", fps=" << last.frames_per_second
              << ", low=" << last.one_percent_low_frames_per_second
              << ", frame_ms=" << last.frame_time_milliseconds << ")\n";
    return 1;
}
