#pragma once

#include "hardwarescope/sensor_snapshot.hpp"

#include <windows.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace hardwarescope {

struct ProcessorTimeSample final {
    std::uint64_t idle{};
    std::uint64_t kernel{};
    std::uint64_t user{};
};

[[nodiscard]] double ComputeProcessorUsage(
    const ProcessorTimeSample& previous,
    const ProcessorTimeSample& current) noexcept;

class SystemPerformanceProvider final {
public:
    SystemPerformanceProvider() noexcept;

    void Collect(SensorSnapshot& snapshot) noexcept;
    [[nodiscard]] const std::wstring& ProcessorName() const noexcept { return processor_name_; }

private:
    static constexpr std::size_t kMaximumProcessors = 256U;

    using NtQuerySystemInformationFunction = LONG(NTAPI*)(ULONG, PVOID, ULONG, PULONG);

    NtQuerySystemInformationFunction query_system_information_{};
    std::array<ProcessorTimeSample, kMaximumProcessors> previous_times_{};
    std::size_t previous_count_{};
    std::wstring processor_name_;
};

} // namespace hardwarescope
