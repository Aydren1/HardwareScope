#include <windows.h>
#include <psapi.h>

#include "hardwarescope/app_commands.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string_view>

#ifndef HARDWARESCOPE_SOAK_DEFAULT_STRESS
#define HARDWARESCOPE_SOAK_DEFAULT_STRESS 0
#endif

namespace {

constexpr wchar_t kMainWindowClass[] = L"HardwareScope.Native.MainWindow";
constexpr wchar_t kSettingsWindowClass[] = L"HardwareScope.Native.SettingsWindow";

struct ProcessMetrics final {
    std::uint64_t cpu_ticks{};
    SIZE_T working_set{};
    SIZE_T private_bytes{};
    DWORD handles{};
    DWORD gdi_objects{};
    DWORD user_objects{};
};

std::uint64_t FileTimeTicks(const FILETIME value) noexcept {
    return (static_cast<std::uint64_t>(value.dwHighDateTime) << 32U) | value.dwLowDateTime;
}

bool ReadMetrics(const HANDLE process, ProcessMetrics& metrics) noexcept {
    FILETIME creation{};
    FILETIME exit{};
    FILETIME kernel{};
    FILETIME user{};
    PROCESS_MEMORY_COUNTERS_EX memory{};
    memory.cb = sizeof(memory);
    if (!GetProcessTimes(process, &creation, &exit, &kernel, &user)
        || !GetProcessMemoryInfo(process, reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory), sizeof(memory))
        || !GetProcessHandleCount(process, &metrics.handles)) {
        return false;
    }
    metrics.cpu_ticks = FileTimeTicks(kernel) + FileTimeTicks(user);
    metrics.working_set = memory.WorkingSetSize;
    metrics.private_bytes = memory.PrivateUsage;
    metrics.gdi_objects = GetGuiResources(process, GR_GDIOBJECTS);
    metrics.user_objects = GetGuiResources(process, GR_USEROBJECTS);
    return true;
}

bool StabilizeProductionResources(
    const HANDLE process,
    ProcessMetrics& baseline,
    double& elapsed_seconds) noexcept {
    if (!ReadMetrics(process, baseline)) return false;
    const auto started = GetTickCount64();
    auto stable_samples = 0U;
    while (GetTickCount64() - started < 90'000ULL) {
        Sleep(1'000U);
        ProcessMetrics current{};
        if (!ReadMetrics(process, current)) return false;
        const auto elapsed = GetTickCount64() - started;
        const auto stable = current.handles == baseline.handles
            && current.gdi_objects == baseline.gdi_objects
            && current.user_objects == baseline.user_objects;
        stable_samples = elapsed >= 30'000ULL && stable ? stable_samples + 1U : 0U;
        baseline = current;
        if (stable_samples >= 5U) {
            elapsed_seconds = static_cast<double>(elapsed) / 1'000.0;
            return true;
        }
    }
    elapsed_seconds = static_cast<double>(GetTickCount64() - started) / 1'000.0;
    return false;
}

bool Responsive(const HWND window) noexcept {
    DWORD_PTR ignored{};
    return SendMessageTimeoutW(window, WM_NULL, 0U, 0U, SMTO_ABORTIFHUNG, 2'000U, &ignored) != 0U;
}

bool CycleSettings(const HWND main_window) noexcept {
    static_cast<void>(PostMessageW(main_window, WM_COMMAND, hardwarescope::kCommandSettings, 0U));
    HWND settings{};
    for (int attempt = 0; attempt < 120 && (settings == nullptr || !IsWindowVisible(settings)); ++attempt) {
        Sleep(25U);
        settings = FindWindowW(kSettingsWindowClass, nullptr);
    }
    if (settings == nullptr || !IsWindowVisible(settings)) return false;
    static_cast<void>(PostMessageW(settings, WM_COMMAND, 2U, 0U));
    for (int attempt = 0; attempt < 120 && (IsWindow(settings) || !IsWindowEnabled(main_window)); ++attempt) Sleep(25U);
    return !IsWindow(settings) && IsWindowEnabled(main_window);
}

long long Growth(const DWORD final_value, const DWORD initial_value) noexcept {
    return static_cast<long long>(final_value) - static_cast<long long>(initial_value);
}

} // namespace

int wmain(const int argument_count, wchar_t** arguments) {
    std::ofstream result_log(
        std::filesystem::temp_directory_path() / "HardwareScopeNativeSoakProbe.log",
        std::ios::out | std::ios::trunc);
    if (!result_log) return 2;
    struct StreamRestore final {
        std::streambuf* output;
        std::streambuf* error;
        ~StreamRestore() {
            std::cout.rdbuf(output);
            std::cerr.rdbuf(error);
        }
    } restore{std::cout.rdbuf(), std::cerr.rdbuf()};
    std::cout.rdbuf(result_log.rdbuf());
    std::cerr.rdbuf(result_log.rdbuf());

    auto duration_seconds = 60UL;
    if (argument_count >= 2) duration_seconds = std::clamp(wcstoul(arguments[1], nullptr, 10), 10UL, 300UL);
    auto stress_settings = HARDWARESCOPE_SOAK_DEFAULT_STRESS != 0;
    auto tray_mode = false;
    auto production_mode = false;
    for (int index = 2; index < argument_count; ++index) {
        const std::wstring_view option{arguments[index]};
        if (option == L"--stress-settings") stress_settings = true;
        if (option == L"--tray") tray_mode = true;
        if (option == L"--production") production_mode = true;
    }
    const auto strict_resources = duration_seconds >= 300UL;

    const auto window = FindWindowW(kMainWindowClass, nullptr);
    if (window == nullptr) {
        std::cerr << "FAIL: HardwareScope native window is not running\n";
        return 1;
    }
    const auto update_hook_result = SendMessageW(window, hardwarescope::kDisableAutomaticUpdateTestMessage, 0U, 0U);
    if ((!production_mode && update_hook_result != 1) || (production_mode && update_hook_result != 0)) {
        std::cerr << "FAIL: app instrumentation did not match the requested soak mode\n";
        return 1;
    }

    DWORD process_id{};
    static_cast<void>(GetWindowThreadProcessId(window, &process_id));
    const auto process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_id);
    if (process == nullptr) {
        std::cerr << "FAIL: HardwareScope process metrics are unavailable\n";
        return 1;
    }

    if (production_mode) {
        // The packaged binary deliberately exposes no snapshot test message.
        // Its normal update timer and lazy OSD surface are covered by the
        // resource-stability warmup immediately before the measured interval.
    } else {
        const auto snapshot_deadline = GetTickCount64() + 120'000ULL;
        while (static_cast<unsigned long long>(SendMessageW(
                   window, hardwarescope::kQuerySnapshotSequenceMessage, 0U, 0U)) < 2ULL
               && GetTickCount64() < snapshot_deadline) {
            Sleep(250U);
        }
        if (static_cast<unsigned long long>(SendMessageW(
                window, hardwarescope::kQuerySnapshotSequenceMessage, 0U, 0U)) < 2ULL) {
            CloseHandle(process);
            std::cerr << "FAIL: sensor startup did not publish two snapshots within 120 seconds\n";
            return 1;
        }
    }
    Sleep(1'000U);
    for (int paint = 0; paint < 160; ++paint) static_cast<void>(SendMessageW(window, WM_PAINT, 0U, 0U));
    if (stress_settings && !CycleSettings(window)) {
        CloseHandle(process);
        std::cerr << "FAIL: Settings warmup cycle did not complete\n";
        return 1;
    }
    if (tray_mode) {
        static_cast<void>(SendMessageW(window, WM_SYSCOMMAND, SC_MINIMIZE, 0U));
        Sleep(250U);
        if (IsWindowVisible(window)) {
            CloseHandle(process);
            std::cerr << "FAIL: tray-soak mode could not hide the main window\n";
            return 1;
        }
    }
    Sleep(500U);
    ProcessMetrics initial{};
    auto production_warmup_seconds = 0.0;
    const auto initial_read = production_mode
        ? StabilizeProductionResources(process, initial, production_warmup_seconds)
        : ReadMetrics(process, initial);
    if (!initial_read) {
        CloseHandle(process);
        std::cerr << "FAIL: initial process resources did not stabilize\n";
        return 1;
    }
    auto maximum = initial;
    auto previous = initial;
    auto responsive = true;
    auto settings_cycles = 0U;
    const auto started = GetTickCount64();
    auto next_settings = started + 15'000ULL;
    while (GetTickCount64() - started < static_cast<ULONGLONG>(duration_seconds) * 1'000ULL) {
        if (!IsWindow(window) || !Responsive(window)) {
            responsive = false;
            break;
        }
        if (stress_settings && GetTickCount64() >= next_settings) {
            if (!CycleSettings(window)) {
                responsive = false;
                break;
            }
            ++settings_cycles;
            next_settings += 15'000ULL;
        }
        ProcessMetrics current{};
        if (!ReadMetrics(process, current)) {
            responsive = false;
            break;
        }
        if (current.handles != previous.handles
            || current.gdi_objects != previous.gdi_objects
            || current.user_objects != previous.user_objects) {
            std::cout << "Resource transition at "
                      << static_cast<double>(GetTickCount64() - started) / 1'000.0
                      << " s: handles " << previous.handles << " -> " << current.handles
                      << ", GDI " << previous.gdi_objects << " -> " << current.gdi_objects
                      << ", USER " << previous.user_objects << " -> " << current.user_objects << '\n';
        }
        previous = current;
        maximum.working_set = std::max(maximum.working_set, current.working_set);
        maximum.private_bytes = std::max(maximum.private_bytes, current.private_bytes);
        maximum.handles = std::max(maximum.handles, current.handles);
        maximum.gdi_objects = std::max(maximum.gdi_objects, current.gdi_objects);
        maximum.user_objects = std::max(maximum.user_objects, current.user_objects);
        Sleep(1'000U);
    }

    ProcessMetrics final{};
    const auto final_read = ReadMetrics(process, final);
    CloseHandle(process);
    if (!final_read) {
        std::cerr << "FAIL: final process metrics could not be read\n";
        return 1;
    }
    const auto elapsed_seconds = std::max(0.001, static_cast<double>(GetTickCount64() - started) / 1'000.0);
    const auto logical_processors = std::max<DWORD>(1UL, GetActiveProcessorCount(ALL_PROCESSOR_GROUPS));
    const auto cpu_percent = static_cast<double>(final.cpu_ticks - initial.cpu_ticks)
        / 10'000'000.0 / elapsed_seconds / static_cast<double>(logical_processors) * 100.0;
    const auto paint_p95 = production_mode
        ? 0ULL
        : static_cast<unsigned long long>(SendMessageW(window, hardwarescope::kQueryPaintP95Message, 0U, 0U));
    const auto handle_growth = Growth(final.handles, initial.handles);
    const auto gdi_growth = Growth(final.gdi_objects, initial.gdi_objects);
    const auto user_growth = Growth(final.user_objects, initial.user_objects);

    std::cout << "Duration seconds: " << elapsed_seconds << '\n'
              << "App mode: " << (production_mode ? "hook-free production" : "instrumented") << '\n'
              << "Production warmup seconds: " << production_warmup_seconds << '\n'
              << "Responsive: " << (responsive ? "yes" : "no") << '\n'
              << "Settings cycles: " << settings_cycles << '\n'
              << "Total CPU: " << cpu_percent << "%\n"
              << "Final/max working set: " << final.working_set / (1024U * 1024U) << '/' << maximum.working_set / (1024U * 1024U) << " MB\n"
              << "Final/max private memory: " << final.private_bytes / (1024U * 1024U) << '/' << maximum.private_bytes / (1024U * 1024U) << " MB\n"
              << "Handle initial/final/max: " << initial.handles << '/' << final.handles << '/' << maximum.handles << '\n'
              << "GDI initial/final/max: " << initial.gdi_objects << '/' << final.gdi_objects << '/' << maximum.gdi_objects << '\n'
              << "USER initial/final/max: " << initial.user_objects << '/' << final.user_objects << '/' << maximum.user_objects << '\n'
              << "Strict final resource growth: " << (strict_resources ? "yes" : "no") << '\n'
              << "Paint p95: " << paint_p95 << " us\n";

    const auto resource_growth_failed = strict_resources
        ? handle_growth > 0 || gdi_growth > 0 || user_growth > 0
        : handle_growth > 4 || gdi_growth > 2 || user_growth > 2;
    if (!responsive || resource_growth_failed
        || final.working_set > 80ULL * 1024ULL * 1024ULL
        || final.private_bytes > 60ULL * 1024ULL * 1024ULL
        || cpu_percent > 0.10
        || (paint_p95 != 0U && paint_p95 > 4'000U)) {
        std::cerr << "FAIL: native soak exceeded a release budget\n";
        return 1;
    }
    std::cout << "OK: native soak remained responsive and inside release budgets\n";
    return 0;
}
