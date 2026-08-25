#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>

namespace hardwarescope {

constexpr std::size_t kMaxSensors = 512;
constexpr std::size_t kSensorNameLength = 64;
constexpr std::size_t kHardwareNameLength = 64;

enum class SensorKind : std::uint8_t {
    temperature,
    utilization,
    clock,
    fan,
    power,
    voltage,
    data,
    frame_rate,
};

enum class SensorUnit : std::uint8_t {
    celsius,
    percent,
    megahertz,
    revolutions_per_minute,
    watts,
    volts,
    megabytes,
    frames_per_second,
};

struct SensorValue final {
    std::uint64_t id{};
    SensorKind kind{};
    SensorUnit unit{};
    bool available{};
    std::array<wchar_t, kSensorNameLength> name{};
    std::array<wchar_t, kHardwareNameLength> hardware{};
    double current{};
    double minimum{};
    double maximum{};
};

struct SensorSnapshot final {
    std::uint64_t sequence{};
    std::uint64_t captured_qpc{};
    std::uint32_t collection_microseconds{};
    std::uint32_t count{};
    std::array<SensorValue, kMaxSensors> sensors{};
};

inline void ResetSnapshot(SensorSnapshot& snapshot) noexcept {
    snapshot.sequence = 0U;
    snapshot.captured_qpc = 0U;
    snapshot.collection_microseconds = 0U;
    snapshot.count = 0U;
}

inline void CopySnapshot(const SensorSnapshot& source, SensorSnapshot& destination) noexcept {
    if (&source == &destination) return;
    destination.sequence = source.sequence;
    destination.captured_qpc = source.captured_qpc;
    destination.collection_microseconds = source.collection_microseconds;
    destination.count = std::min<std::uint32_t>(source.count, static_cast<std::uint32_t>(destination.sensors.size()));
    std::copy_n(source.sensors.begin(), destination.count, destination.sensors.begin());
}

class SnapshotStore final {
public:
    SnapshotStore() : snapshot_(std::make_unique<SensorSnapshot>()) {}
    SnapshotStore(const SnapshotStore&) = delete;
    SnapshotStore& operator=(const SnapshotStore&) = delete;

    void Publish(const SensorSnapshot& snapshot) noexcept {
        const std::scoped_lock lock(mutex_);
        CopySnapshot(snapshot, *snapshot_);
    }

    [[nodiscard]] SensorSnapshot ReadLatest() const noexcept {
        SensorSnapshot result{};
        ReadLatest(result);
        return result;
    }

    void ReadLatest(SensorSnapshot& destination) const noexcept {
        const std::scoped_lock lock(mutex_);
        CopySnapshot(*snapshot_, destination);
    }

    [[nodiscard]] std::uint64_t LatestSequence() const noexcept {
        const std::scoped_lock lock(mutex_);
        return snapshot_->sequence;
    }

private:
    mutable std::mutex mutex_;
    std::unique_ptr<SensorSnapshot> snapshot_;
};

} // namespace hardwarescope
