#include "hardwarescope/etw_fps_monitor.hpp"
#include "hardwarescope/game_detector.hpp"

#include <windows.h>

#include <chrono>
#include <iomanip>
#include <iostream>
#include <string_view>

int main(const int argument_count, char** arguments) {
    const auto capture_all = argument_count == 2 && std::string_view{arguments[1]} == "--all";
    const auto game = hardwarescope::FindGameProcess(GetCurrentProcessId());
    hardwarescope::EtwFpsMonitor monitor;
    const auto process = GetCurrentProcess();
    FILETIME creation{};
    FILETIME exit{};
    FILETIME kernel_before{};
    FILETIME user_before{};
    static_cast<void>(GetProcessTimes(process, &creation, &exit, &kernel_before, &user_before));
    const auto started = std::chrono::steady_clock::now();
    monitor.Start(true, !capture_all, 500U);
    Sleep(capture_all ? 5'000U : 3'500U);
    const auto reading = monitor.Snapshot();
    const auto active = monitor.CaptureActive();
    const auto stop_started = std::chrono::steady_clock::now();
    monitor.Stop();
    const auto stop_milliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - stop_started).count();
    FILETIME kernel_after{};
    FILETIME user_after{};
    static_cast<void>(GetProcessTimes(process, &creation, &exit, &kernel_after, &user_after));
    const auto to_ticks = [](const FILETIME value) {
        return (static_cast<std::uint64_t>(value.dwHighDateTime) << 32U) | value.dwLowDateTime;
    };
    const auto cpu_ms = static_cast<double>(
        to_ticks(kernel_after) + to_ticks(user_after) - to_ticks(kernel_before) - to_ticks(user_before)) / 10'000.0;
    const auto wall_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
    std::wcout << L"Detected game: " << (game.process_id == 0U ? L"none" : game.application.data()) << L"\n"
               << L"Capture active: " << (active ? L"yes" : L"no") << L"\n"
               << L"FPS available: " << (reading.available ? L"yes" : L"no") << L"\n"
               << L"FPS value: " << (reading.available ? std::to_wstring(reading.frames_per_second) : L"-") << L"\n"
               << L"Stop latency: " << std::fixed << std::setprecision(1) << stop_milliseconds << L" ms\n"
               << L"Wall time: " << wall_ms << L" ms\n"
               << L"CPU: " << std::fixed << std::setprecision(3) << cpu_ms / wall_ms * 100.0 << L"% of one logical core\n";
    if (!capture_all && game.process_id == 0U && (active || reading.available)) {
        std::cerr << "FAIL: game-only FPS capture did not remain off on the desktop\n";
        return 1;
    }
    if ((!capture_all && game.process_id != 0U || capture_all) && (!active || !reading.available || reading.frames_per_second < 10U)) {
        std::cerr << "FAIL: active DirectX game did not produce a plausible FPS reading\n";
        return 1;
    }
    return 0;
}
