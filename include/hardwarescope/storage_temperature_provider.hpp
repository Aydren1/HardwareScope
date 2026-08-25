#pragma once

#include "hardwarescope/sensor_snapshot.hpp"

#include <windows.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>

namespace hardwarescope {

[[nodiscard]] bool IsPlausibleStorageTemperature(short celsius) noexcept;

class StorageTemperatureProvider final {
public:
    StorageTemperatureProvider() = default;
    ~StorageTemperatureProvider();

    StorageTemperatureProvider(const StorageTemperatureProvider&) = delete;
    StorageTemperatureProvider& operator=(const StorageTemperatureProvider&) = delete;

    void Collect(SensorSnapshot& snapshot) noexcept;
    void Reset() noexcept;
    [[nodiscard]] std::size_t DriveCount() const noexcept { return drive_count_; }

private:
    static constexpr std::size_t kMaximumDrives = 32U;
    static constexpr std::size_t kMaximumCachedSensors = 64U;

    struct DriveRecord final {
        HANDLE handle{INVALID_HANDLE_VALUE};
        std::uint32_t physical_index{};
        std::wstring model;
    };

    void EnumerateDrives() noexcept;
    void CloseDrives() noexcept;
    void RefreshTemperatures() noexcept;

    std::array<DriveRecord, kMaximumDrives> drives_{};
    std::size_t drive_count_{};
    std::array<SensorValue, kMaximumCachedSensors> cached_sensors_{};
    std::size_t cached_sensor_count_{};
    std::chrono::steady_clock::time_point last_enumeration_{};
    std::chrono::steady_clock::time_point last_temperature_refresh_{};
};

} // namespace hardwarescope
