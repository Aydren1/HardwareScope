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

[[nodiscard]] bool IsPlausibleGpuTemperature(double celsius) noexcept;
[[nodiscard]] std::array<char, 16U> FormatNvmlPciBusId(std::uint32_t bus_id) noexcept;

class NvidiaGpuProvider final {
public:
    NvidiaGpuProvider() = default;
    ~NvidiaGpuProvider();

    NvidiaGpuProvider(const NvidiaGpuProvider&) = delete;
    NvidiaGpuProvider& operator=(const NvidiaGpuProvider&) = delete;

    [[nodiscard]] bool Initialize() noexcept;
    void Close() noexcept;
    void Collect(SensorSnapshot& snapshot) noexcept;

    [[nodiscard]] bool Available() const noexcept { return initialized_ && gpu_count_ > 0U; }
    [[nodiscard]] std::size_t GpuCount() const noexcept { return gpu_count_; }

private:
    static constexpr std::size_t kMaximumGpus = 64U;

    using QueryInterfaceFunction = void*(__cdecl*)(std::uint32_t);

    struct GpuRecord final {
        void* handle{};
        void* nvml_handle{};
        std::wstring name;
        std::uint32_t pci_bus_id{};
        std::uint32_t thermal_mask{};
        std::uint32_t clock_version{};
        bool has_pci_bus_id{};
        bool rtx_50_series{};
    };

    [[nodiscard]] FARPROC Interface(std::uint32_t id) const noexcept;

    HMODULE module_{};
    HMODULE nvml_module_{};
    QueryInterfaceFunction query_interface_{};
    FARPROC unload_{};
    FARPROC get_thermal_settings_{};
    FARPROC get_thermal_sensors_{};
    FARPROC get_dynamic_pstates_{};
    FARPROC get_clock_frequencies_{};
    FARPROC get_fan_status_{};
    FARPROC nvml_shutdown_{};
    FARPROC nvml_get_memory_info_{};
    FARPROC nvml_get_power_usage_{};
    std::array<GpuRecord, kMaximumGpus> gpus_{};
    std::size_t gpu_count_{};
    std::chrono::steady_clock::time_point last_refresh_{};
    std::unique_ptr<SensorSnapshot> refresh_buffer_;
    bool initialized_{};
};

} // namespace hardwarescope
