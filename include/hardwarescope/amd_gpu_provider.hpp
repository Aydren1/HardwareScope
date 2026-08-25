#pragma once

#include "hardwarescope/sensor_snapshot.hpp"

#include <windows.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace hardwarescope {

inline constexpr std::size_t kAmdPmLogSensorCount = 256U;

struct AmdPmLogSample final {
    int supported{};
    int value{};
};

void DecodeAmdPmLogSensors(
    const std::array<AmdPmLogSample, kAmdPmLogSensorCount>& samples,
    std::uint64_t id_base,
    const wchar_t* hardware,
    SensorSnapshot& snapshot) noexcept;

void DecodeAmdVramSensors(
    int used_megabytes,
    std::int64_t total_bytes,
    std::uint64_t id_base,
    const wchar_t* hardware,
    SensorSnapshot& snapshot) noexcept;

class AmdGpuProvider final {
public:
    AmdGpuProvider() = default;
    ~AmdGpuProvider();

    AmdGpuProvider(const AmdGpuProvider&) = delete;
    AmdGpuProvider& operator=(const AmdGpuProvider&) = delete;

    [[nodiscard]] bool Initialize() noexcept;
    void Close() noexcept;
    void Collect(SensorSnapshot& snapshot) noexcept;

    [[nodiscard]] bool Available() const noexcept { return initialized_ && gpu_count_ > 0U; }
    [[nodiscard]] std::size_t GpuCount() const noexcept { return gpu_count_; }

private:
    static constexpr std::size_t kMaximumGpus = 16U;

    struct GpuRecord final {
        int adapter_index{};
        int adapter_id{};
        std::int64_t memory_bytes{};
        std::wstring name;
    };

    HMODULE module_{};
    void* context_{};
    FARPROC destroy_{};
    FARPROC query_pm_log_{};
    FARPROC dedicated_vram_usage_{};
    std::array<GpuRecord, kMaximumGpus> gpus_{};
    std::size_t gpu_count_{};
    std::chrono::steady_clock::time_point last_refresh_{};
    std::unique_ptr<SensorSnapshot> refresh_buffer_;
    bool initialized_{};
};

} // namespace hardwarescope
