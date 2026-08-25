// ADL structure definitions are derived from LibreHardwareMonitor v0.9.6
// (MPL-2.0). Sensor indices follow AMD's public ADL 18.1 ADL_PMLOG_SENSORS
// enum, including board power (73) and intake temperature (74). This provider
// only calls read APIs.

#include "hardwarescope/amd_gpu_provider.hpp"
#include "hardwarescope/nvidia_gpu_provider.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cwchar>
#include <new>
#include <tuple>

namespace hardwarescope {
namespace {

constexpr int kAdlOk = 0;
constexpr int kAmdVendorId = 0x1002;
constexpr auto kRefreshInterval = std::chrono::milliseconds{250};

struct AdlAdapterInfo final {
    int size{};
    int adapter_index{};
    std::array<char, 256U> udid{};
    int bus_number{};
    int device_number{};
    int function_number{};
    int vendor_id{};
    std::array<char, 256U> adapter_name{};
    std::array<char, 256U> display_name{};
    int present{};
    int exist{};
    std::array<char, 256U> driver_path{};
    std::array<char, 256U> driver_path_ext{};
    std::array<char, 256U> pnp_string{};
    int os_display_index{};
};

struct AdlMemoryInfoX4 final {
    std::int64_t memory_size{};
    std::array<char, 256U> memory_type{};
    std::int64_t memory_bandwidth{};
    std::int64_t hyper_memory_size{};
    std::int64_t invisible_memory_size{};
    std::int64_t visible_memory_size{};
    std::int64_t vram_vendor_revision_id{};
    std::int64_t memory_bandwidth_x2{};
    std::int64_t memory_bit_rate_x2{};
};

struct AdlPmLogDataOutput final {
    int size{};
    std::array<AmdPmLogSample, kAmdPmLogSensorCount> sensors{};
};

static_assert(sizeof(AdlAdapterInfo) == 1'572U);
static_assert(sizeof(AdlMemoryInfoX4) == 320U);
static_assert(sizeof(AmdPmLogSample) == 8U);
static_assert(sizeof(AdlPmLogDataOutput) == 2'052U);

using MemoryAllocateFunction = void*(__stdcall*)(int);
using MainControlCreateFunction = int(__cdecl*)(MemoryAllocateFunction, int, void**);
using MainControlDestroyFunction = int(__cdecl*)(void*);
using NumberOfAdaptersFunction = int(__cdecl*)(void*, int*);
using AdapterInfoFunction = int(__cdecl*)(void*, AdlAdapterInfo*, int);
using AdapterActiveFunction = int(__cdecl*)(void*, int, int*);
using AdapterIdFunction = int(__cdecl*)(void*, int, int*);
using QueryPmLogFunction = int(__cdecl*)(void*, int, AdlPmLogDataOutput*);
using DedicatedVramUsageFunction = int(__cdecl*)(void*, int, int*);
using MemoryInfoX4Function = int(__cdecl*)(void*, int, AdlMemoryInfoX4*);

void* __stdcall AllocateAdlMemory(const int size) noexcept {
    return size > 0 ? std::calloc(1U, static_cast<std::size_t>(size)) : nullptr;
}

std::wstring WidenName(const char* const text, const std::size_t capacity) {
    const auto length = strnlen_s(text, capacity);
    if (length == 0U) return L"AMD Radeon GPU";
    const auto wide_length = MultiByteToWideChar(CP_ACP, 0, text, static_cast<int>(length), nullptr, 0);
    if (wide_length <= 0) return L"AMD Radeon GPU";
    std::wstring result(static_cast<std::size_t>(wide_length), L'\0');
    static_cast<void>(MultiByteToWideChar(CP_ACP, 0, text, static_cast<int>(length), result.data(), wide_length));
    return result;
}

void AppendSensor(
    SensorSnapshot& snapshot,
    const std::uint64_t id,
    const SensorKind kind,
    const SensorUnit unit,
    const wchar_t* const name,
    const wchar_t* const hardware,
    const double current) noexcept {
    if (snapshot.count >= snapshot.sensors.size()) return;
    auto& sensor = snapshot.sensors[snapshot.count++];
    sensor.id = id;
    sensor.kind = kind;
    sensor.unit = unit;
    sensor.available = true;
    static_cast<void>(wcsncpy_s(sensor.name.data(), sensor.name.size(), name, _TRUNCATE));
    static_cast<void>(wcsncpy_s(sensor.hardware.data(), sensor.hardware.size(), hardware, _TRUNCATE));
    sensor.current = current;
    sensor.minimum = current;
    sensor.maximum = current;
}

bool ValidPercent(const int value) noexcept { return value >= 0 && value <= 100; }
bool ValidClock(const int value) noexcept { return value > 0 && value <= 20'000; }
bool ValidFan(const int value) noexcept { return value >= 0 && value <= 20'000; }
bool ValidVoltageMillivolts(const int value) noexcept { return value > 0 && value <= 5'000; }
bool ValidPower(const int value) noexcept { return value >= 0 && value <= 2'000; }

} // namespace

void DecodeAmdPmLogSensors(
    const std::array<AmdPmLogSample, kAmdPmLogSensorCount>& samples,
    const std::uint64_t id_base,
    const wchar_t* const hardware,
    SensorSnapshot& snapshot) noexcept {
    const auto add_temperature = [&](const std::size_t sensor_index, const std::uint64_t id, const wchar_t* const name) {
        const auto& value = samples[sensor_index];
        if (value.supported != 0 && IsPlausibleGpuTemperature(value.value)) {
            AppendSensor(snapshot, id_base | id, SensorKind::temperature, SensorUnit::celsius, name, hardware, value.value);
        }
    };
    add_temperature(8U, 0x0001U, L"GPU Core temperature");
    add_temperature(9U, 0x0002U, L"GPU Memory Junction temperature");
    add_temperature(27U, 0x0003U, L"GPU Hot Spot temperature");
    add_temperature(10U, 0x0004U, L"GPU VR VDDC temperature");
    add_temperature(11U, 0x0005U, L"GPU VR Memory temperature");
    add_temperature(24U, 0x0006U, L"GPU VR SoC temperature");
    add_temperature(29U, 0x0007U, L"GPU SoC temperature");
    add_temperature(12U, 0x0008U, L"GPU Liquid temperature");
    add_temperature(13U, 0x0009U, L"GPU PLX temperature");
    add_temperature(25U, 0x000AU, L"GPU VR Memory #1 temperature");
    add_temperature(26U, 0x000BU, L"GPU VR Memory #2 temperature");
    add_temperature(28U, 0x000CU, L"GPU Graphics temperature");
    add_temperature(32U, 0x000DU, L"GPU APU CPU temperature");
    add_temperature(42U, 0x000EU, L"GPU Liquid #1 temperature");
    add_temperature(43U, 0x000FU, L"GPU Liquid #2 temperature");
    add_temperature(50U, 0x0010U, L"GPU GCD Hot Spot temperature");
    add_temperature(51U, 0x0011U, L"GPU MCD Hot Spot temperature");
    add_temperature(74U, 0x0012U, L"GPU Intake temperature");

    constexpr std::array<std::pair<std::size_t, const wchar_t*>, 2U> usage{{
        {19U, L"GPU Core usage"}, {20U, L"GPU Memory usage"}}};
    for (std::size_t index = 0U; index < usage.size(); ++index) {
        const auto [sensor_index, name] = usage[index];
        const auto& value = samples[sensor_index];
        if (value.supported != 0 && ValidPercent(value.value)) {
            AppendSensor(snapshot, id_base | (0x0100U + index), SensorKind::utilization, SensorUnit::percent, name, hardware, value.value);
        }
    }

    constexpr std::array<std::pair<std::size_t, const wchar_t*>, 3U> clocks{{
        {1U, L"GPU Core clock"}, {2U, L"GPU Memory clock"}, {3U, L"GPU SoC clock"}}};
    for (std::size_t index = 0U; index < clocks.size(); ++index) {
        const auto [sensor_index, name] = clocks[index];
        const auto& value = samples[sensor_index];
        if (value.supported != 0 && ValidClock(value.value)) {
            AppendSensor(snapshot, id_base | (0x0200U + index), SensorKind::clock, SensorUnit::megahertz, name, hardware, value.value);
        }
    }

    if (samples[14U].supported != 0 && ValidFan(samples[14U].value)) {
        AppendSensor(snapshot, id_base | 0x0300U, SensorKind::fan, SensorUnit::revolutions_per_minute, L"GPU Fan", hardware, samples[14U].value);
    }
    if (samples[15U].supported != 0 && ValidPercent(samples[15U].value)) {
        AppendSensor(snapshot, id_base | 0x0301U, SensorKind::utilization, SensorUnit::percent, L"GPU Fan speed", hardware, samples[15U].value);
    }

    constexpr std::array<std::pair<std::size_t, const wchar_t*>, 3U> voltages{{
        {21U, L"GPU Core voltage"}, {16U, L"GPU SoC voltage"}, {22U, L"GPU Memory voltage"}}};
    for (std::size_t index = 0U; index < voltages.size(); ++index) {
        const auto [sensor_index, name] = voltages[index];
        const auto& value = samples[sensor_index];
        if (value.supported != 0 && ValidVoltageMillivolts(value.value)) {
            AppendSensor(snapshot, id_base | (0x0400U + index), SensorKind::voltage, SensorUnit::volts, name, hardware, static_cast<double>(value.value) / 1'000.0);
        }
    }

    constexpr std::array<std::tuple<std::size_t, std::uint64_t, const wchar_t*>, 4U> powers{{
        {73U, 0x0500U, L"GPU Board Power"},
        {30U, 0x0501U, L"GPU Core Power"},
        {17U, 0x0502U, L"GPU SoC Power"},
        {23U, 0x0503U, L"GPU ASIC Power"}}};
    for (const auto& [sensor_index, id, name] : powers) {
        const auto& value = samples[sensor_index];
        if (value.supported != 0 && ValidPower(value.value)) {
            AppendSensor(snapshot, id_base | id, SensorKind::power, SensorUnit::watts, name, hardware, value.value);
        }
    }
}

void DecodeAmdVramSensors(
    const int used_megabytes,
    const std::int64_t total_bytes,
    const std::uint64_t id_base,
    const wchar_t* const hardware,
    SensorSnapshot& snapshot) noexcept {
    if (used_megabytes < 0 || total_bytes <= 0) return;
    const auto total_megabytes = static_cast<double>(total_bytes) / (1024.0 * 1024.0);
    if (static_cast<double>(used_megabytes) > total_megabytes) return;
    AppendSensor(snapshot, id_base | 0x0600U, SensorKind::data, SensorUnit::megabytes, L"GPU Memory Used", hardware, used_megabytes);
    AppendSensor(snapshot, id_base | 0x0601U, SensorKind::data, SensorUnit::megabytes, L"GPU Memory Total", hardware, total_megabytes);
    AppendSensor(snapshot, id_base | 0x0602U, SensorKind::utilization, SensorUnit::percent, L"GPU Memory Usage", hardware, static_cast<double>(used_megabytes) * 100.0 / total_megabytes);
}

AmdGpuProvider::~AmdGpuProvider() {
    Close();
}

bool AmdGpuProvider::Initialize() noexcept {
    if (initialized_) return Available();
    initialized_ = true;
    module_ = LoadLibraryExW(L"atiadlxx.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (module_ == nullptr) return false;

    const auto create = reinterpret_cast<MainControlCreateFunction>(GetProcAddress(module_, "ADL2_Main_Control_Create"));
    destroy_ = GetProcAddress(module_, "ADL2_Main_Control_Destroy");
    const auto number = reinterpret_cast<NumberOfAdaptersFunction>(GetProcAddress(module_, "ADL2_Adapter_NumberOfAdapters_Get"));
    const auto info = reinterpret_cast<AdapterInfoFunction>(GetProcAddress(module_, "ADL2_Adapter_AdapterInfo_Get"));
    const auto active = reinterpret_cast<AdapterActiveFunction>(GetProcAddress(module_, "ADL2_Adapter_Active_Get"));
    const auto get_id = reinterpret_cast<AdapterIdFunction>(GetProcAddress(module_, "ADL2_Adapter_ID_Get"));
    query_pm_log_ = GetProcAddress(module_, "ADL2_New_QueryPMLogData_Get");
    dedicated_vram_usage_ = GetProcAddress(module_, "ADL2_Adapter_DedicatedVRAMUsage_Get");
    const auto memory_info = reinterpret_cast<MemoryInfoX4Function>(GetProcAddress(module_, "ADL2_Adapter_MemoryInfoX4_Get"));
    if (create == nullptr || destroy_ == nullptr || number == nullptr || info == nullptr
        || active == nullptr || query_pm_log_ == nullptr || create(&AllocateAdlMemory, 1, &context_) != kAdlOk
        || context_ == nullptr) {
        Close();
        initialized_ = true;
        return false;
    }

    int adapter_count{};
    if (number(context_, &adapter_count) != kAdlOk || adapter_count <= 0 || adapter_count > 128) return false;
    std::array<AdlAdapterInfo, 128U> adapters{};
    for (auto& adapter : adapters) adapter.size = sizeof(AdlAdapterInfo);
    if (info(context_, adapters.data(), adapter_count * static_cast<int>(sizeof(AdlAdapterInfo))) != kAdlOk) return false;

    for (int index = 0; index < adapter_count && gpu_count_ < gpus_.size(); ++index) {
        const auto& adapter = adapters[static_cast<std::size_t>(index)];
        int is_active{};
        if (adapter.vendor_id != kAmdVendorId || adapter.present == 0
            || active(context_, adapter.adapter_index, &is_active) != kAdlOk || is_active == 0) {
            continue;
        }
        int adapter_id = (adapter.bus_number << 16) | (adapter.device_number << 8) | adapter.function_number;
        if (get_id != nullptr) static_cast<void>(get_id(context_, adapter.adapter_index, &adapter_id));
        const auto duplicate = std::find_if(gpus_.begin(), gpus_.begin() + gpu_count_, [adapter_id](const GpuRecord& gpu) {
            return gpu.adapter_id == adapter_id;
        });
        if (duplicate != gpus_.begin() + gpu_count_) continue;

        auto& gpu = gpus_[gpu_count_++];
        gpu.adapter_index = adapter.adapter_index;
        gpu.adapter_id = adapter_id;
        gpu.name = WidenName(adapter.adapter_name.data(), adapter.adapter_name.size());
        if (memory_info != nullptr) {
            AdlMemoryInfoX4 memory{};
            if (memory_info(context_, adapter.adapter_index, &memory) == kAdlOk && memory.memory_size > 0) {
                gpu.memory_bytes = memory.memory_size;
            }
        }
    }
    return Available();
}

void AmdGpuProvider::Close() noexcept {
    if (context_ != nullptr && destroy_ != nullptr) {
        static_cast<void>(reinterpret_cast<MainControlDestroyFunction>(destroy_)(context_));
    }
    context_ = nullptr;
    destroy_ = nullptr;
    query_pm_log_ = nullptr;
    dedicated_vram_usage_ = nullptr;
    if (module_ != nullptr) FreeLibrary(module_);
    module_ = nullptr;
    gpus_ = {};
    gpu_count_ = 0U;
    if (refresh_buffer_ != nullptr) ResetSnapshot(*refresh_buffer_);
    last_refresh_ = {};
    initialized_ = false;
}

void AmdGpuProvider::Collect(SensorSnapshot& snapshot) noexcept {
    if (!initialized_) static_cast<void>(Initialize());
    if (!Available()) return;
    const auto append_cached = [&] {
        if (refresh_buffer_ == nullptr) return;
        for (std::uint32_t index = 0U; index < refresh_buffer_->count && snapshot.count < snapshot.sensors.size(); ++index) {
            snapshot.sensors[snapshot.count++] = refresh_buffer_->sensors[index];
        }
    };
    const auto now = std::chrono::steady_clock::now();
    if (last_refresh_ != std::chrono::steady_clock::time_point{} && now - last_refresh_ < kRefreshInterval) {
        append_cached();
        return;
    }

    const auto query = reinterpret_cast<QueryPmLogFunction>(query_pm_log_);
    const auto vram_usage = reinterpret_cast<DedicatedVramUsageFunction>(dedicated_vram_usage_);
    if (refresh_buffer_ == nullptr) refresh_buffer_.reset(new (std::nothrow) SensorSnapshot{});
    if (refresh_buffer_ == nullptr) return;
    auto& refreshed = *refresh_buffer_;
    ResetSnapshot(refreshed);
    for (std::size_t gpu_index = 0U; gpu_index < gpu_count_; ++gpu_index) {
        const auto& gpu = gpus_[gpu_index];
        const auto id_base = 0x0201'0000'0000'0000ULL | (static_cast<std::uint64_t>(gpu_index) << 16U);
        AdlPmLogDataOutput log{};
        log.size = sizeof(log);
        if (query(context_, gpu.adapter_index, &log) == kAdlOk) {
            DecodeAmdPmLogSensors(log.sensors, id_base, gpu.name.c_str(), refreshed);
        }

        int used_megabytes{};
        if (vram_usage != nullptr && vram_usage(context_, gpu.adapter_index, &used_megabytes) == kAdlOk) {
            DecodeAmdVramSensors(used_megabytes, gpu.memory_bytes, id_base, gpu.name.c_str(), refreshed);
        }
    }
    last_refresh_ = now;
    append_cached();
}

} // namespace hardwarescope
