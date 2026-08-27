#include "hardwarescope/amd_gpu_provider.hpp"
#include "hardwarescope/nvidia_gpu_provider.hpp"
#include "hardwarescope/privileged_sensor_collector.hpp"
#include "hardwarescope/sensor_snapshot.hpp"
#include "hardwarescope/storage_temperature_provider.hpp"
#include "hardwarescope/system_performance_provider.hpp"

#include <windows.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <thread>

namespace {

std::string Utf8(const std::wstring_view text) {
    if (text.empty()) return {};
    const auto size = WideCharToMultiByte(CP_UTF8, 0U, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) return {};
    std::string result(static_cast<std::size_t>(size), '\0');
    static_cast<void>(WideCharToMultiByte(CP_UTF8, 0U, text.data(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr));
    return result;
}

void CsvField(std::ostream& output, const std::string_view value) {
    output << '"';
    for (const auto character : value) {
        if (character == '"') output << '"';
        output << character;
    }
    output << '"';
}

std::string_view KindName(const hardwarescope::SensorKind kind) noexcept {
    using enum hardwarescope::SensorKind;
    switch (kind) {
    case temperature: return "temperature";
    case utilization: return "utilization";
    case clock: return "clock";
    case fan: return "fan";
    case power: return "power";
    case voltage: return "voltage";
    case data: return "data";
    case frame_rate: return "frame_rate";
    }
    return "unknown";
}

std::string_view UnitName(const hardwarescope::SensorUnit unit) noexcept {
    using enum hardwarescope::SensorUnit;
    switch (unit) {
    case celsius: return "C";
    case percent: return "%";
    case megahertz: return "MHz";
    case revolutions_per_minute: return "RPM";
    case watts: return "W";
    case volts: return "V";
    case megabytes: return "MB";
    case frames_per_second: return "FPS";
    case milliseconds: return "ms";
    }
    return "unknown";
}

bool IsElevated() noexcept {
    HANDLE token{};
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
    TOKEN_ELEVATION elevation{};
    DWORD size{};
    const auto success = GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size);
    CloseHandle(token);
    return success && elevation.TokenIsElevated != 0U;
}

bool ParseUnsigned(const wchar_t* text, std::uint32_t& value) noexcept {
    if (text == nullptr || *text == L'\0' || *text == L'-') return false;
    wchar_t* end{};
    const auto parsed = std::wcstoul(text, &end, 10);
    if (end == text || *end != L'\0' || parsed > std::numeric_limits<std::uint32_t>::max()) return false;
    value = static_cast<std::uint32_t>(parsed);
    return true;
}

void WriteUtcTimestamp(std::ostream& output) {
    SYSTEMTIME now{};
    GetSystemTime(&now);
    output << std::setfill('0')
           << std::setw(4) << now.wYear << '-'
           << std::setw(2) << now.wMonth << '-'
           << std::setw(2) << now.wDay << 'T'
           << std::setw(2) << now.wHour << ':'
           << std::setw(2) << now.wMinute << ':'
           << std::setw(2) << now.wSecond << '.'
           << std::setw(3) << now.wMilliseconds << 'Z'
           << std::setfill(' ');
}

} // namespace

int wmain(const int argument_count, wchar_t** arguments) {
    auto output_path = std::wstring{L"HardwareScope-Sensors.csv"};
    auto allow_unprivileged = false;
    std::uint32_t duration_seconds{};
    std::uint32_t interval_milliseconds{1'000U};
    for (int index = 1; index < argument_count; ++index) {
        const std::wstring_view option{arguments[index]};
        if (option == L"--allow-unprivileged") {
            allow_unprivileged = true;
        } else if (option == L"--output" && index + 1 < argument_count) {
            output_path = arguments[++index];
        } else if (option == L"--duration-seconds" && index + 1 < argument_count) {
            if (!ParseUnsigned(arguments[++index], duration_seconds) || duration_seconds > 3'600U) {
                std::cerr << "FAIL: --duration-seconds must be from 0 through 3600\n";
                return 2;
            }
        } else if (option == L"--interval-ms" && index + 1 < argument_count) {
            if (!ParseUnsigned(arguments[++index], interval_milliseconds)
                || interval_milliseconds < 100U || interval_milliseconds > 60'000U) {
                std::cerr << "FAIL: --interval-ms must be from 100 through 60000\n";
                return 2;
            }
        } else if (option == L"--help") {
            std::cout << "Usage: HardwareScopeSensorExport.exe [--output FILE] [--duration-seconds 0..3600] "
                         "[--interval-ms 100..60000] [--allow-unprivileged]\n";
            return 0;
        } else {
            std::cerr << "FAIL: unknown or incomplete command-line option\n";
            return 2;
        }
    }
    if (!IsElevated() && !allow_unprivileged) {
        std::cerr << "FAIL: run this exporter as administrator for a complete sensor audit\n";
        return 2;
    }

    hardwarescope::SystemPerformanceProvider system;
    hardwarescope::StorageTemperatureProvider storage;
    hardwarescope::NvidiaGpuProvider nvidia;
    hardwarescope::AmdGpuProvider amd_gpu;
    hardwarescope::PrivilegedSensorCollector privileged;
    static_cast<void>(nvidia.Initialize());
    static_cast<void>(amd_gpu.Initialize());
    static_cast<void>(privileged.Initialize(GetModuleHandleW(nullptr)));
    const auto close_providers = [&]() noexcept {
        privileged.Close();
        amd_gpu.Close();
        nvidia.Close();
    };

    const auto collect = [&]() {
        hardwarescope::SensorSnapshot snapshot{};
        system.Collect(snapshot);
        storage.Collect(snapshot);
        nvidia.Collect(snapshot);
        amd_gpu.Collect(snapshot);
        privileged.Collect(snapshot);
        return snapshot;
    };
    static_cast<void>(collect());
    Sleep(1'000U);

    std::ofstream output{output_path, std::ios::binary | std::ios::trunc};
    if (!output) {
        close_providers();
        std::cerr << "FAIL: sensor report could not be created\n";
        return 1;
    }
    output << "timestamp_utc,sample_index,sensor_id,kind,unit,current,minimum,maximum,sensor,hardware\r\n";
    output << std::fixed << std::setprecision(3);

    const auto duration_milliseconds = duration_seconds * 1'000U;
    const auto sample_count = duration_seconds == 0U
        ? 1U
        : std::max(1U, (duration_milliseconds + interval_milliseconds - 1U) / interval_milliseconds);
    const auto started = std::chrono::steady_clock::now();
    std::uint64_t exported_readings{};
    for (std::uint32_t sample_index = 0U; sample_index < sample_count; ++sample_index) {
        if (sample_index > 0U) {
            const auto deadline = started + std::chrono::milliseconds{
                static_cast<std::uint64_t>(sample_index) * interval_milliseconds};
            std::this_thread::sleep_until(deadline);
        }
        const auto snapshot = collect();
        if (snapshot.count == 0U) {
            close_providers();
            std::cerr << "FAIL: no native sensors were detected\n";
            return 1;
        }
        for (std::uint32_t index = 0U; index < snapshot.count; ++index) {
            const auto& sensor = snapshot.sensors[index];
            WriteUtcTimestamp(output);
            output << ',' << sample_index << ",0x" << std::hex << std::uppercase << sensor.id << std::dec << ',';
            CsvField(output, KindName(sensor.kind));
            output << ',';
            CsvField(output, UnitName(sensor.unit));
            output << ',' << sensor.current << ',' << sensor.minimum << ',' << sensor.maximum << ',';
            CsvField(output, Utf8(sensor.name.data()));
            output << ',';
            CsvField(output, Utf8(sensor.hardware.data()));
            output << "\r\n";
            ++exported_readings;
        }
        output.flush();
        if (!output) {
            close_providers();
            std::cerr << "FAIL: sensor report could not be completed\n";
            return 1;
        }
    }
    close_providers();
    output.close();
    if (!output) {
        std::cerr << "FAIL: sensor report could not be completed\n";
        return 1;
    }
    std::wcout << L"OK: exported " << exported_readings << L" readings across "
               << sample_count << L" samples to " << output_path << L'\n';
    return 0;
}
