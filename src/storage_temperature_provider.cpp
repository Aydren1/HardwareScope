#include "hardwarescope/storage_temperature_provider.hpp"

#include <winioctl.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cwchar>
#include <string_view>

namespace hardwarescope {
namespace {

constexpr auto kTemperatureRefreshInterval = std::chrono::seconds{5};
constexpr auto kDriveEnumerationInterval = std::chrono::minutes{1};
constexpr std::size_t kQueryBufferSize = 4096U;

std::wstring TrimAsciiDeviceText(const char* const text, const std::size_t maximum_length) {
    std::size_t length = 0U;
    while (length < maximum_length && text[length] != '\0') ++length;
    std::size_t first = 0U;
    while (first < length && (text[first] == ' ' || text[first] == '\t')) ++first;
    while (length > first && (text[length - 1U] == ' ' || text[length - 1U] == '\t')) --length;
    if (first == length) return L"Storage device";

    const std::string_view source{text + first, length - first};
    const auto required = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, source.data(), static_cast<int>(source.size()), nullptr, 0);
    const auto code_page = required > 0 ? CP_UTF8 : CP_ACP;
    const auto flags = required > 0 ? MB_ERR_INVALID_CHARS : 0U;
    const auto wide_length = MultiByteToWideChar(code_page, flags, source.data(), static_cast<int>(source.size()), nullptr, 0);
    if (wide_length <= 0) return L"Storage device";
    std::wstring result(static_cast<std::size_t>(wide_length), L'\0');
    static_cast<void>(MultiByteToWideChar(code_page, flags, source.data(), static_cast<int>(source.size()), result.data(), wide_length));
    return result;
}

std::wstring QueryDriveModel(const HANDLE handle, const std::uint32_t physical_index) noexcept {
    STORAGE_PROPERTY_QUERY query{};
    query.PropertyId = StorageDeviceProperty;
    query.QueryType = PropertyStandardQuery;
    alignas(STORAGE_DEVICE_DESCRIPTOR) std::array<std::byte, kQueryBufferSize> buffer{};
    DWORD returned{};
    if (DeviceIoControl(
            handle,
            IOCTL_STORAGE_QUERY_PROPERTY,
            &query,
            sizeof(query),
            buffer.data(),
            static_cast<DWORD>(buffer.size()),
            &returned,
            nullptr)
        && returned >= sizeof(STORAGE_DEVICE_DESCRIPTOR)) {
        const auto* const descriptor = reinterpret_cast<const STORAGE_DEVICE_DESCRIPTOR*>(buffer.data());
        const auto product_offset = static_cast<std::size_t>(descriptor->ProductIdOffset);
        if (product_offset != 0U && product_offset < returned) {
            return TrimAsciiDeviceText(
                reinterpret_cast<const char*>(buffer.data() + product_offset),
                static_cast<std::size_t>(returned) - product_offset);
        }
    }
    return L"Physical drive " + std::to_wstring(physical_index);
}

SensorValue MakeTemperatureSensor(
    const std::uint32_t physical_index,
    const STORAGE_TEMPERATURE_INFO& info,
    const wchar_t* const model) noexcept {
    SensorValue sensor{};
    sensor.id = 0x0300'0000'0000'0000ULL
        | (static_cast<std::uint64_t>(physical_index) << 16U)
        | static_cast<std::uint64_t>(info.Index);
    sensor.kind = SensorKind::temperature;
    sensor.unit = SensorUnit::celsius;
    sensor.available = true;
    if (info.Index == 0U) {
        static_cast<void>(wcscpy_s(sensor.name.data(), sensor.name.size(), L"Drive composite temperature"));
    } else {
        static_cast<void>(swprintf_s(sensor.name.data(), sensor.name.size(), L"Drive temperature sensor #%u", static_cast<unsigned>(info.Index)));
    }
    static_cast<void>(wcsncpy_s(sensor.hardware.data(), sensor.hardware.size(), model, _TRUNCATE));
    sensor.current = static_cast<double>(info.Temperature);
    sensor.minimum = sensor.current;
    sensor.maximum = sensor.current;
    return sensor;
}

} // namespace

bool IsPlausibleStorageTemperature(const short celsius) noexcept {
    return celsius != static_cast<short>(STORAGE_TEMPERATURE_VALUE_NOT_REPORTED)
        && celsius >= -20
        && celsius <= 125;
}

StorageTemperatureProvider::~StorageTemperatureProvider() {
    CloseDrives();
}

void StorageTemperatureProvider::Reset() noexcept {
    CloseDrives();
    last_enumeration_ = {};
    last_temperature_refresh_ = {};
}

void StorageTemperatureProvider::Collect(SensorSnapshot& snapshot) noexcept {
    const auto now = std::chrono::steady_clock::now();
    if (drive_count_ == 0U || last_enumeration_ == std::chrono::steady_clock::time_point{}
        || now - last_enumeration_ >= kDriveEnumerationInterval) {
        EnumerateDrives();
        last_enumeration_ = now;
        last_temperature_refresh_ = {};
    }
    if (last_temperature_refresh_ == std::chrono::steady_clock::time_point{}
        || now - last_temperature_refresh_ >= kTemperatureRefreshInterval) {
        RefreshTemperatures();
        last_temperature_refresh_ = now;
    }
    for (std::size_t index = 0U; index < cached_sensor_count_ && snapshot.count < snapshot.sensors.size(); ++index) {
        snapshot.sensors[snapshot.count++] = cached_sensors_[index];
    }
}

void StorageTemperatureProvider::EnumerateDrives() noexcept {
    CloseDrives();
    for (std::uint32_t physical_index = 0U;
         physical_index < static_cast<std::uint32_t>(kMaximumDrives) && drive_count_ < drives_.size();
         ++physical_index) {
        const auto path = L"\\\\.\\PhysicalDrive" + std::to_wstring(physical_index);
        const auto handle = CreateFileW(
            path.c_str(),
            0,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr,
            OPEN_EXISTING,
            0,
            nullptr);
        if (handle == INVALID_HANDLE_VALUE) continue;
        auto& drive = drives_[drive_count_++];
        drive.handle = handle;
        drive.physical_index = physical_index;
        drive.model = QueryDriveModel(handle, physical_index);
    }
}

void StorageTemperatureProvider::CloseDrives() noexcept {
    for (std::size_t index = 0U; index < drive_count_; ++index) {
        if (drives_[index].handle != INVALID_HANDLE_VALUE) CloseHandle(drives_[index].handle);
        drives_[index] = {};
    }
    drive_count_ = 0U;
    cached_sensor_count_ = 0U;
}

void StorageTemperatureProvider::RefreshTemperatures() noexcept {
    cached_sensor_count_ = 0U;
    for (std::size_t drive_index = 0U; drive_index < drive_count_; ++drive_index) {
        const auto& drive = drives_[drive_index];
        STORAGE_PROPERTY_QUERY query{};
        query.PropertyId = StorageDeviceTemperatureProperty;
        query.QueryType = PropertyStandardQuery;
        alignas(STORAGE_TEMPERATURE_DATA_DESCRIPTOR) std::array<std::byte, kQueryBufferSize> buffer{};
        DWORD returned{};
        if (!DeviceIoControl(
                drive.handle,
                IOCTL_STORAGE_QUERY_PROPERTY,
                &query,
                sizeof(query),
                buffer.data(),
                static_cast<DWORD>(buffer.size()),
                &returned,
                nullptr)) {
            continue;
        }

        constexpr auto header_size = offsetof(STORAGE_TEMPERATURE_DATA_DESCRIPTOR, TemperatureInfo);
        if (returned < header_size) continue;
        const auto* const descriptor = reinterpret_cast<const STORAGE_TEMPERATURE_DATA_DESCRIPTOR*>(buffer.data());
        const auto usable_size = std::min<std::size_t>({
            static_cast<std::size_t>(returned),
            static_cast<std::size_t>(descriptor->Size),
            buffer.size()});
        if (usable_size < header_size) continue;
        const auto available_count = (usable_size - header_size) / sizeof(STORAGE_TEMPERATURE_INFO);
        const auto info_count = std::min<std::size_t>(descriptor->InfoCount, available_count);
        for (std::size_t info_index = 0U;
             info_index < info_count && cached_sensor_count_ < cached_sensors_.size();
             ++info_index) {
            const auto& info = descriptor->TemperatureInfo[info_index];
            if (!IsPlausibleStorageTemperature(info.Temperature)) continue;
            cached_sensors_[cached_sensor_count_++] = MakeTemperatureSensor(
                drive.physical_index,
                info,
                drive.model.c_str());
        }
    }
}

} // namespace hardwarescope
