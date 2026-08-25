#include "hardwarescope/system_performance_provider.hpp"

#include <powrprof.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cwchar>
#include <vector>

namespace hardwarescope {
namespace {

constexpr ULONG kSystemProcessorPerformanceInformation = 8U;

struct NativeProcessorPerformanceInformation final {
    LARGE_INTEGER idle_time;
    LARGE_INTEGER kernel_time;
    LARGE_INTEGER user_time;
    LARGE_INTEGER dpc_time;
    LARGE_INTEGER interrupt_time;
    ULONG interrupt_count;
};

struct NativeProcessorPowerInformation final {
    ULONG number;
    ULONG maximum_mhz;
    ULONG current_mhz;
    ULONG mhz_limit;
    ULONG maximum_idle_state;
    ULONG current_idle_state;
};

SensorValue MakeSensor(
    const std::uint64_t id,
    const SensorKind kind,
    const SensorUnit unit,
    const wchar_t* name,
    const wchar_t* hardware,
    const double current) noexcept {
    SensorValue value{};
    value.id = id;
    value.kind = kind;
    value.unit = unit;
    value.available = true;
    static_cast<void>(wcsncpy_s(value.name.data(), value.name.size(), name, _TRUNCATE));
    static_cast<void>(wcsncpy_s(value.hardware.data(), value.hardware.size(), hardware, _TRUNCATE));
    value.current = current;
    value.minimum = current;
    value.maximum = current;
    return value;
}

std::wstring ReadProcessorName() noexcept {
    HKEY key{};
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) return L"Processor";
    std::array<wchar_t, 256> value{};
    DWORD type{};
    DWORD size = static_cast<DWORD>(value.size() * sizeof(wchar_t));
    const auto status = RegQueryValueExW(key, L"ProcessorNameString", nullptr, &type, reinterpret_cast<BYTE*>(value.data()), &size);
    RegCloseKey(key);
    if (status != ERROR_SUCCESS || type != REG_SZ) return L"Processor";
    std::wstring result{value.data()};
    const auto first = result.find_first_not_of(L' ');
    const auto last = result.find_last_not_of(L' ');
    return first == std::wstring::npos ? L"Processor" : result.substr(first, last - first + 1U);
}

} // namespace

double ComputeProcessorUsage(const ProcessorTimeSample& previous, const ProcessorTimeSample& current) noexcept {
    if (current.kernel < previous.kernel || current.user < previous.user || current.idle < previous.idle) return 0.0;
    const auto kernel = current.kernel - previous.kernel;
    const auto user = current.user - previous.user;
    const auto idle = current.idle - previous.idle;
    const auto total = kernel + user;
    if (total == 0U || idle > total) return 0.0;
    return std::clamp(static_cast<double>(total - idle) * 100.0 / static_cast<double>(total), 0.0, 100.0);
}

SystemPerformanceProvider::SystemPerformanceProvider() noexcept : processor_name_(ReadProcessorName()) {
    const auto ntdll = GetModuleHandleW(L"ntdll.dll");
    if (ntdll != nullptr) query_system_information_ = reinterpret_cast<NtQuerySystemInformationFunction>(GetProcAddress(ntdll, "NtQuerySystemInformation"));
}

void SystemPerformanceProvider::Collect(SensorSnapshot& snapshot) noexcept {
    const auto append = [&](const SensorValue& sensor) {
        if (snapshot.count < snapshot.sensors.size()) snapshot.sensors[snapshot.count++] = sensor;
    };

    const auto processor_count = std::min<std::size_t>(GetActiveProcessorCount(ALL_PROCESSOR_GROUPS), kMaximumProcessors);
    std::array<ProcessorTimeSample, kMaximumProcessors> current_times{};
    std::size_t current_count = 0U;
    if (query_system_information_ != nullptr && processor_count > 0U) {
        std::array<NativeProcessorPerformanceInformation, kMaximumProcessors> native_times{};
        const auto byte_count = static_cast<ULONG>(processor_count * sizeof(NativeProcessorPerformanceInformation));
        if (query_system_information_(kSystemProcessorPerformanceInformation, native_times.data(), byte_count, nullptr) >= 0) {
            current_count = processor_count;
            double total_usage = 0.0;
            for (std::size_t index = 0; index < current_count; ++index) {
                current_times[index] = ProcessorTimeSample{
                    static_cast<std::uint64_t>(native_times[index].idle_time.QuadPart),
                    static_cast<std::uint64_t>(native_times[index].kernel_time.QuadPart),
                    static_cast<std::uint64_t>(native_times[index].user_time.QuadPart)};
                if (previous_count_ == current_count) {
                    const auto usage = ComputeProcessorUsage(previous_times_[index], current_times[index]);
                    total_usage += usage;
                    std::array<wchar_t, 32> name{};
                    static_cast<void>(swprintf_s(name.data(), name.size(), L"CPU Thread #%zu", index + 1U));
                    append(MakeSensor(0x0100'0000'0000'1100ULL + index, SensorKind::utilization, SensorUnit::percent, name.data(), processor_name_.c_str(), usage));
                }
            }
            if (previous_count_ == current_count && current_count > 0U) {
                append(MakeSensor(0x0100'0000'0000'1000ULL, SensorKind::utilization, SensorUnit::percent, L"CPU Total", processor_name_.c_str(), total_usage / static_cast<double>(current_count)));
            }
            previous_times_ = current_times;
            previous_count_ = current_count;
        }
    }

    if (processor_count > 0U) {
        std::vector<NativeProcessorPowerInformation> power(processor_count);
        if (CallNtPowerInformation(
                ProcessorInformation,
                nullptr,
                0,
                power.data(),
                static_cast<ULONG>(power.size() * sizeof(NativeProcessorPowerInformation))) == ERROR_SUCCESS) {
            double average = 0.0;
            std::size_t valid = 0U;
            for (std::size_t index = 0; index < power.size(); ++index) {
                if (power[index].current_mhz == 0U) continue;
                average += power[index].current_mhz;
                ++valid;
                std::array<wchar_t, 32> name{};
                static_cast<void>(swprintf_s(name.data(), name.size(), L"CPU Thread #%zu clock", index + 1U));
                append(MakeSensor(0x0100'0000'0000'2100ULL + index, SensorKind::clock, SensorUnit::megahertz, name.data(), processor_name_.c_str(), power[index].current_mhz));
            }
            if (valid > 0U) append(MakeSensor(0x0100'0000'0000'2000ULL, SensorKind::clock, SensorUnit::megahertz, L"CPU Clock Average", processor_name_.c_str(), average / static_cast<double>(valid)));
        }
    }

    MEMORYSTATUSEX memory{};
    memory.dwLength = sizeof(memory);
    if (GlobalMemoryStatusEx(&memory)) {
        append(MakeSensor(0x0400'0000'0000'1000ULL, SensorKind::utilization, SensorUnit::percent, L"Physical Memory Usage", L"System memory", memory.dwMemoryLoad));
    }
}

} // namespace hardwarescope
