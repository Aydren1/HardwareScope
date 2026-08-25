#include "hardwarescope/app_settings.hpp"
#include "hardwarescope/amd_gpu_provider.hpp"
#include "hardwarescope/ddr5_temperature_provider.hpp"
#include "hardwarescope/frame_rate_tracker.hpp"
#include "hardwarescope/file_verification.hpp"
#include "hardwarescope/game_detector.hpp"
#include "hardwarescope/legacy_settings_migration.hpp"
#include "hardwarescope/osd_model.hpp"
#include "hardwarescope/nvidia_gpu_provider.hpp"
#include "hardwarescope/nct6687_provider.hpp"
#include "hardwarescope/sensor_snapshot.hpp"
#include "hardwarescope/sensor_explanations.hpp"
#include "hardwarescope/service_path.hpp"
#include "hardwarescope/sensor_view_model.hpp"
#include "hardwarescope/sensor_worker.hpp"
#include "hardwarescope/storage_temperature_provider.hpp"
#include "hardwarescope/startup_registration.hpp"
#include "hardwarescope/ui_palette.hpp"
#include "hardwarescope/update_manifest.hpp"
#include "hardwarescope/window_regions.hpp"

#include <algorithm>
#include <atomic>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <thread>

namespace {

int failures = 0;

void Expect(const bool condition, const char* const message) {
    if (condition) return;
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
}

void TestWindowRegions() {
    using hardwarescope::HitTestWindowRegion;
    Expect(HitTestWindowRegion(1200, 800, 1, 1, 7, 86, 138) == HTTOPLEFT, "top-left resize corner");
    Expect(HitTestWindowRegion(1200, 800, 1198, 1, 7, 86, 138) == HTTOPRIGHT, "top-right resize corner");
    Expect(HitTestWindowRegion(1200, 800, 400, 40, 7, 86, 138) == HTCAPTION, "empty header is native caption");
    Expect(HitTestWindowRegion(1200, 800, 1100, 40, 7, 86, 138) == HTCLIENT, "window controls remain interactive");
    Expect(HitTestWindowRegion(1200, 800, 400, 200, 7, 86, 138) == HTCLIENT, "sensor content remains client area");
    Expect(HitTestWindowRegion(1200, 800, 1, 400, 7, 86, 138) == HTLEFT, "left resize edge");
    Expect(HitTestWindowRegion(1200, 800, 400, 798, 7, 86, 138) == HTBOTTOM, "bottom resize edge");
}

void TestSnapshotStore() {
    hardwarescope::SnapshotStore store;
    hardwarescope::SensorSnapshot first{};
    first.sequence = 42;
    first.count = 1;
    first.sensors[0].id = 7;
    first.sensors[0].current = 55.5;
    store.Publish(first);
    const auto copy = store.ReadLatest();
    Expect(copy.sequence == 42, "snapshot sequence survives publication");
    Expect(copy.count == 1, "snapshot sensor count survives publication");
    Expect(copy.sensors[0].current == 55.5, "snapshot value survives publication");
    Expect(hardwarescope::kMaxSensors == 512U, "snapshot capacity covers high-thread-count systems");
    Expect(sizeof(hardwarescope::SensorSnapshot) <= 160U * 1024U, "snapshot stays compact and bounded");

    first.count = static_cast<std::uint32_t>(hardwarescope::kMaxSensors);
    first.sensors.back().id = 0x512U;
    first.sensors.back().current = 99.5;
    store.Publish(first);
    hardwarescope::SensorSnapshot full_copy{};
    store.ReadLatest(full_copy);
    Expect(full_copy.count == hardwarescope::kMaxSensors, "full 512-sensor snapshot survives publication");
    Expect(full_copy.sensors.back().id == 0x512U && full_copy.sensors.back().current == 99.5,
        "last full-capacity sensor survives bounded copy");

    hardwarescope::SnapshotStore concurrent_store;
    constexpr std::uint64_t iterations = 20'000;
    std::atomic<bool> writer_done{};
    std::atomic<bool> monotonic{true};
    std::thread writer([&] {
        hardwarescope::SensorSnapshot snapshot{};
        snapshot.count = 1;
        for (std::uint64_t sequence = 1; sequence <= iterations; ++sequence) {
            snapshot.sequence = sequence;
            snapshot.sensors[0].current = static_cast<double>(sequence);
            concurrent_store.Publish(snapshot);
        }
        writer_done.store(true, std::memory_order_release);
    });

    std::array<std::thread, 4> readers;
    for (auto& reader : readers) {
        reader = std::thread([&] {
            std::uint64_t previous = 0;
            while (!writer_done.load(std::memory_order_acquire)) {
                const auto snapshot = concurrent_store.ReadLatest();
                if (snapshot.sequence < previous) monotonic.store(false, std::memory_order_release);
                previous = snapshot.sequence;
            }
        });
    }
    writer.join();
    for (auto& reader : readers) reader.join();
    const auto final = concurrent_store.ReadLatest();
    Expect(monotonic.load(std::memory_order_acquire), "concurrent snapshot reads never move backward");
    Expect(final.sequence == iterations, "concurrent publication exposes final snapshot");
}

void WorkerCallback(void* const context, const std::uint64_t sequence) noexcept {
    static_cast<std::atomic<std::uint64_t>*>(context)->store(sequence, std::memory_order_release);
}

void TestSensorWorker() {
    hardwarescope::SnapshotStore store;
    std::atomic<std::uint64_t> sequence{};
    hardwarescope::SensorWorker worker(store, &WorkerCallback, &sequence, hardwarescope::SensorWorkerMode::synthetic);
    worker.Start(std::chrono::milliseconds{100});
    std::this_thread::sleep_for(std::chrono::milliseconds{360});
    worker.Stop();
    Expect(!worker.Running(), "sensor worker stops cleanly");
    Expect(sequence.load(std::memory_order_acquire) >= 3, "sensor worker publishes on its independent schedule");
    const auto snapshot = store.ReadLatest();
    Expect(snapshot.sequence == sequence.load(std::memory_order_acquire), "worker callback and snapshot agree");
    Expect(snapshot.count == 7, "synthetic provider supplies expected foundation sensors");
    Expect(snapshot.collection_microseconds < 50'000, "synthetic collection stays outside a UI-frame budget");
}

void TestSensorPublishCadence() {
    using namespace std::chrono_literals;
    Expect(hardwarescope::SelectSensorPublishInterval(750ms, 100U, true, true, false) == 750ms,
        "game-only FPS keeps the desktop on the lightweight hardware cadence");
    Expect(hardwarescope::SelectSensorPublishInterval(750ms, 100U, true, true, true) == 100ms,
        "a real game FPS sample switches publishing to the independent low-latency cadence");
    Expect(hardwarescope::SelectSensorPublishInterval(750ms, 100U, false, true, true) == 750ms,
        "disabled FPS never accelerates sensor publishing");
}

void TestSettingsStore() {
    namespace fs = std::filesystem;
    auto path = fs::temp_directory_path() / (L"HardwareScopeNativeSettings-" + std::to_wstring(GetCurrentProcessId()) + L".ini");
    hardwarescope::SettingsStore store{path};
    hardwarescope::AppSettings settings{};
    settings.refresh_interval_ms = 50U;
    settings.theme = hardwarescope::Theme::midnight;
    settings.text_color_rgb = 0x1ABCDEFU;
    settings.cpu_temperature_color_rgb = 0x123456U;
    settings.cpu_usage_color_rgb = hardwarescope::AppSettings::kMatchAccentColor;
    settings.graphics_color_rgb = 0x1ABCDEFU;
    settings.start_with_windows = true;
    settings.start_minimized = true;
    settings.osd_position = hardwarescope::OsdPosition::bottom_right;
    settings.osd_layout = hardwarescope::OsdLayout::horizontal;
    settings.osd_opacity_percent = 7U;
    settings.osd_scale_percent = 999U;
    settings.osd_spacing_px = 999U;
    settings.osd_background = true;
    settings.easy_temperature_mask = hardwarescope::easy_cpu_package | hardwarescope::easy_gpu_core;
    settings.fps_color_rgb = 0xFF7700U;
    settings.fps_scale_percent = 175U;
    settings.fps_separate_position = true;
    settings.fps_osd_position = hardwarescope::OsdPosition::top_right;
    settings.fps_refresh_interval_ms = 1U;
    settings.fps_smoothing_interval_ms = 9'999U;
    settings.automatic_updates = false;
    settings.update_snooze_until_unix_seconds = 4'102'444'800ULL;
    settings.skipped_update_major = 2U;
    settings.skipped_update_minor = 1U;
    settings.skipped_update_patch = 7U;
    settings.collapsed_sections = 0x15U;
    Expect(settings.PinSensor(10U), "first sensor can be pinned");
    Expect(settings.PinSensor(22U), "second sensor can be pinned");
    Expect(!settings.PinSensor(10U), "duplicate pinned sensor is rejected");
    Expect(settings.UnpinSensor(10U) && !settings.IsSensorPinned(10U), "pinned sensor can be removed without disturbing other selections");
    Expect(!settings.UnpinSensor(99U), "removing an unknown pinned sensor is a no-op");
    Expect(settings.PinSensor(10U), "removed sensor can be pinned again");
    Expect(store.Save(settings), "settings save is atomic and succeeds");

    hardwarescope::AppSettings loaded{};
    loaded.show_osd = false;
    Expect(store.Load(loaded), "saved settings load successfully");
    Expect(loaded.refresh_interval_ms == 100U, "refresh milliseconds clamp to safe minimum");
    Expect(loaded.theme == hardwarescope::Theme::midnight, "midnight theme round-trips");
    Expect(loaded.text_color_rgb == 0xABCDEFU, "text color is normalized to RGB");
    Expect(loaded.cpu_temperature_color_rgb == 0x123456U, "CPU temperature color round-trips independently");
    Expect(loaded.cpu_usage_color_rgb == hardwarescope::AppSettings::kMatchAccentColor, "category colors can continue matching the accent");
    Expect(loaded.graphics_color_rgb == 0xABCDEFU, "category colors are normalized to RGB");
    Expect(loaded.start_with_windows && loaded.start_minimized, "startup settings round-trip");
    Expect(loaded.show_osd, "unmodified OSD setting retains its saved default");
    Expect(loaded.osd_position == hardwarescope::OsdPosition::bottom_right, "OSD position round-trips");
    Expect(loaded.osd_layout == hardwarescope::OsdLayout::horizontal, "horizontal OSD layout round-trips");
    Expect(loaded.osd_opacity_percent == 10U, "OSD opacity clamps to safe minimum");
    Expect(loaded.osd_scale_percent == 250U, "OSD scale clamps to safe maximum");
    Expect(loaded.osd_spacing_px == 64U, "OSD spacing clamps to safe maximum");
    Expect(loaded.osd_background, "OSD background preference round-trips");
    Expect(loaded.easy_temperature_mask == (hardwarescope::easy_cpu_package | hardwarescope::easy_gpu_core), "EZ Temp selection round-trips");
    Expect(loaded.fps_color_rgb == 0xFF7700U && loaded.fps_scale_percent == 175U, "independent FPS appearance round-trips");
    Expect(loaded.fps_separate_position && loaded.fps_osd_position == hardwarescope::OsdPosition::top_right,
        "independent FPS OSD corner round-trips");
    Expect(loaded.fps_refresh_interval_ms == 50U && loaded.fps_smoothing_interval_ms == 1'250U, "independent FPS timing settings clamp and round-trip");
    Expect(!loaded.automatic_updates, "update preference round-trips");
    Expect(loaded.update_snooze_until_unix_seconds == 4'102'444'800ULL,
        "update reminder time round-trips without precision loss");
    Expect(loaded.skipped_update_major == 2U && loaded.skipped_update_minor == 1U && loaded.skipped_update_patch == 7U,
        "skipped update version round-trips");
    Expect(loaded.collapsed_sections == 0x15U, "collapsed sensor sections round-trip");
    Expect(loaded.pinned_sensor_count == 2U && loaded.IsSensorPinned(10U) && loaded.IsSensorPinned(22U), "pinned sensor IDs round-trip");
    loaded.refresh_interval_ms = 333U;
    Expect(store.Save(loaded), "a changed millisecond polling interval saves successfully");
    hardwarescope::AppSettings changed_polling{};
    Expect(store.Load(changed_polling) && changed_polling.refresh_interval_ms == 333U,
        "a changed millisecond polling interval persists exactly");
    auto temporary = path;
    temporary += L".tmp";
    Expect(!fs::exists(temporary), "atomic save leaves no temporary settings file");

    std::error_code error;
    static_cast<void>(fs::remove(path, error));
}

void TestOsdModel() {
    hardwarescope::SensorSnapshot snapshot{};
    auto set_sensor = [&](const std::uint32_t index,
                          const std::uint64_t id,
                          const hardwarescope::SensorKind kind,
                          const hardwarescope::SensorUnit unit,
                          const wchar_t* name,
                          const wchar_t* hardware,
                          const double current) {
        auto& sensor = snapshot.sensors[index];
        sensor.id = id;
        sensor.kind = kind;
        sensor.unit = unit;
        sensor.available = true;
        static_cast<void>(wcscpy_s(sensor.name.data(), sensor.name.size(), name));
        static_cast<void>(wcscpy_s(sensor.hardware.data(), sensor.hardware.size(), hardware));
        sensor.current = current;
    };
    set_sensor(0, 1U, hardwarescope::SensorKind::temperature, hardwarescope::SensorUnit::celsius, L"Core (Tctl/Tdie)", L"AMD Ryzen CPU", 51.25);
    set_sensor(1, 2U, hardwarescope::SensorKind::temperature, hardwarescope::SensorUnit::celsius, L"GPU Core temperature", L"NVIDIA GPU", 43.5);
    set_sensor(2, 3U, hardwarescope::SensorKind::temperature, hardwarescope::SensorUnit::celsius, L"GPU Memory Junction", L"NVIDIA GPU", 58.0);
    set_sensor(3, 4U, hardwarescope::SensorKind::frame_rate, hardwarescope::SensorUnit::frames_per_second, L"Frame rate", L"Game", 144.0);
    snapshot.count = 4;

    hardwarescope::AppSettings settings{};
    settings.text_color_rgb = 0x52E0D4U;
    settings.fps_color_rgb = 0xFF7700U;
    settings.osd_layout = hardwarescope::OsdLayout::horizontal;
    const auto items = hardwarescope::BuildOsdDisplayItems(snapshot, settings);
    Expect(items.size() == 4U, "EZ Temp and FPS create four default OSD items");
    Expect(items[0].sensor_id == 4U && items[0].fps && items[0].text == L"FPS 144", "FPS is first and has a compact label");
    Expect(items[0].color_rgb == 0xFF7700U && items[1].color_rgb == 0x52E0D4U, "FPS and telemetry use independent colors");
    Expect(items[1].group == hardwarescope::OsdHardwareGroup::cpu, "CPU temperature is grouped as CPU");
    Expect(items[2].group == hardwarescope::OsdHardwareGroup::gpu && items[3].group == hardwarescope::OsdHardwareGroup::gpu, "GPU temperatures share the GPU group");
    Expect(items[1].text.find(L"CPU Tctl/Tdie") == 0U, "CPU OSD label identifies the Tctl/Tdie reading");
    settings.fps_separate_position = true;
    settings.fps_osd_position = hardwarescope::OsdPosition::top_right;
    const auto telemetry_surface = hardwarescope::BuildOsdSurfaceItems(snapshot, settings, false);
    const auto fps_surface = hardwarescope::BuildOsdSurfaceItems(snapshot, settings, true);
    Expect(telemetry_surface.size() == 3U && std::none_of(telemetry_surface.begin(), telemetry_surface.end(), [](const auto& item) { return item.fps; }),
        "a split OSD keeps hardware telemetry on the primary surface");
    Expect(fps_surface.size() == 1U && fps_surface.front().fps,
        "a split OSD moves only FPS to its independent surface");
    settings.fps_separate_position = false;
    Expect(hardwarescope::IsSensorSelectedForOsd(snapshot.sensors[0], settings), "EZ CPU temperature reports selected in the main OSD column");
    hardwarescope::SetSensorSelectedForOsd(snapshot.sensors[0], settings, false);
    Expect(!hardwarescope::IsSensorSelectedForOsd(snapshot.sensors[0], settings), "main OSD toggle can disable an EZ temperature");
    hardwarescope::SetSensorSelectedForOsd(snapshot.sensors[0], settings, true);
    Expect(settings.IsSensorPinned(snapshot.sensors[0].id), "main OSD toggle can explicitly pin a sensor");

    settings.easy_temperature_mask = hardwarescope::easy_cpu_package | hardwarescope::easy_gpu_core;
    Expect(settings.PinSensor(3U), "disabled EZ temperature can be pinned explicitly");
    const auto pinned_items = hardwarescope::BuildOsdDisplayItems(snapshot, settings);
    Expect(pinned_items.size() == 4U && pinned_items.back().sensor_id == 3U, "pinned sensor is appended without duplicating EZ items");
    settings.fps_enabled = false;
    const auto no_fps_items = hardwarescope::BuildOsdDisplayItems(snapshot, settings);
    Expect(no_fps_items.size() == 3U && std::none_of(no_fps_items.begin(), no_fps_items.end(), [](const auto& item) { return item.fps; }), "disabled FPS is absent from OSD");
}

void TestProcessorUsageMath() {
    const hardwarescope::ProcessorTimeSample previous{100U, 500U, 500U};
    Expect(std::abs(hardwarescope::ComputeProcessorUsage(previous, {150U, 600U, 600U}) - 75.0) < 0.001, "processor usage subtracts idle time from kernel plus user time");
    Expect(hardwarescope::ComputeProcessorUsage(previous, previous) == 0.0, "processor usage handles a zero-duration sample");
    Expect(hardwarescope::ComputeProcessorUsage(previous, {50U, 100U, 100U}) == 0.0, "processor usage rejects counter rollback");
    Expect(hardwarescope::ComputeProcessorUsage(previous, {900U, 600U, 600U}) == 0.0, "processor usage rejects impossible idle deltas");
}

void TestStorageTemperatureValidation() {
    Expect(hardwarescope::IsPlausibleStorageTemperature(45), "normal drive temperature is accepted");
    Expect(hardwarescope::IsPlausibleStorageTemperature(0), "zero Celsius remains a valid live temperature");
    Expect(!hardwarescope::IsPlausibleStorageTemperature(126), "impossible drive temperature is rejected");
    Expect(!hardwarescope::IsPlausibleStorageTemperature(static_cast<short>(0x8000)), "not-reported sentinel is rejected");
}

void TestGpuTemperatureValidation() {
    Expect(hardwarescope::IsPlausibleGpuTemperature(52.5), "normal GPU temperature is accepted");
    Expect(!hardwarescope::IsPlausibleGpuTemperature(126.0), "impossible GPU temperature is rejected");
    Expect(!hardwarescope::IsPlausibleGpuTemperature(std::numeric_limits<double>::quiet_NaN()), "non-finite GPU temperature is rejected");
}

void TestAmdPmLogDecoding() {
    using hardwarescope::SensorKind;
    using hardwarescope::SensorUnit;
    constexpr std::uint64_t id_base = 0x0201'0000'0000'0000ULL;
    std::array<hardwarescope::AmdPmLogSample, hardwarescope::kAmdPmLogSensorCount> samples{};
    const auto support = [&](const std::size_t index, const int value) { samples[index] = {1, value}; };
    support(8U, 41); support(9U, 62); support(27U, 79); support(10U, 45); support(11U, 47); support(24U, 49);
    support(29U, 51); support(12U, 35); support(13U, 36); support(25U, 52); support(26U, 53); support(28U, 54);
    support(32U, 55); support(42U, 37); support(43U, 38); support(50U, 81); support(51U, 76); support(74U, 29);
    support(19U, 73); support(20U, 42);
    support(1U, 2'450); support(2U, 18'000); support(3U, 1'200);
    support(14U, 1'567); support(15U, 38);
    support(21U, 1'025); support(16U, 950); support(22U, 1'350);
    support(73U, 310); support(30U, 215); support(17U, 42); support(23U, 290);

    hardwarescope::SensorSnapshot snapshot{};
    hardwarescope::DecodeAmdPmLogSensors(samples, id_base, L"AMD Radeon Test", snapshot);
    Expect(snapshot.count == 32U, "AMD PMLog decoder emits every supported validated reading");

    struct Expected final {
        std::uint64_t id;
        const wchar_t* name;
        SensorKind kind;
        SensorUnit unit;
        double current;
    };
    constexpr std::array expected{
        Expected{0x0001U, L"GPU Core temperature", SensorKind::temperature, SensorUnit::celsius, 41.0},
        Expected{0x0002U, L"GPU Memory Junction temperature", SensorKind::temperature, SensorUnit::celsius, 62.0},
        Expected{0x0003U, L"GPU Hot Spot temperature", SensorKind::temperature, SensorUnit::celsius, 79.0},
        Expected{0x0004U, L"GPU VR VDDC temperature", SensorKind::temperature, SensorUnit::celsius, 45.0},
        Expected{0x0005U, L"GPU VR Memory temperature", SensorKind::temperature, SensorUnit::celsius, 47.0},
        Expected{0x0006U, L"GPU VR SoC temperature", SensorKind::temperature, SensorUnit::celsius, 49.0},
        Expected{0x0007U, L"GPU SoC temperature", SensorKind::temperature, SensorUnit::celsius, 51.0},
        Expected{0x0008U, L"GPU Liquid temperature", SensorKind::temperature, SensorUnit::celsius, 35.0},
        Expected{0x0009U, L"GPU PLX temperature", SensorKind::temperature, SensorUnit::celsius, 36.0},
        Expected{0x000AU, L"GPU VR Memory #1 temperature", SensorKind::temperature, SensorUnit::celsius, 52.0},
        Expected{0x000BU, L"GPU VR Memory #2 temperature", SensorKind::temperature, SensorUnit::celsius, 53.0},
        Expected{0x000CU, L"GPU Graphics temperature", SensorKind::temperature, SensorUnit::celsius, 54.0},
        Expected{0x000DU, L"GPU APU CPU temperature", SensorKind::temperature, SensorUnit::celsius, 55.0},
        Expected{0x000EU, L"GPU Liquid #1 temperature", SensorKind::temperature, SensorUnit::celsius, 37.0},
        Expected{0x000FU, L"GPU Liquid #2 temperature", SensorKind::temperature, SensorUnit::celsius, 38.0},
        Expected{0x0010U, L"GPU GCD Hot Spot temperature", SensorKind::temperature, SensorUnit::celsius, 81.0},
        Expected{0x0011U, L"GPU MCD Hot Spot temperature", SensorKind::temperature, SensorUnit::celsius, 76.0},
        Expected{0x0012U, L"GPU Intake temperature", SensorKind::temperature, SensorUnit::celsius, 29.0},
        Expected{0x0100U, L"GPU Core usage", SensorKind::utilization, SensorUnit::percent, 73.0},
        Expected{0x0101U, L"GPU Memory usage", SensorKind::utilization, SensorUnit::percent, 42.0},
        Expected{0x0200U, L"GPU Core clock", SensorKind::clock, SensorUnit::megahertz, 2'450.0},
        Expected{0x0201U, L"GPU Memory clock", SensorKind::clock, SensorUnit::megahertz, 18'000.0},
        Expected{0x0202U, L"GPU SoC clock", SensorKind::clock, SensorUnit::megahertz, 1'200.0},
        Expected{0x0300U, L"GPU Fan", SensorKind::fan, SensorUnit::revolutions_per_minute, 1'567.0},
        Expected{0x0301U, L"GPU Fan speed", SensorKind::utilization, SensorUnit::percent, 38.0},
        Expected{0x0400U, L"GPU Core voltage", SensorKind::voltage, SensorUnit::volts, 1.025},
        Expected{0x0401U, L"GPU SoC voltage", SensorKind::voltage, SensorUnit::volts, 0.950},
        Expected{0x0402U, L"GPU Memory voltage", SensorKind::voltage, SensorUnit::volts, 1.350},
        Expected{0x0500U, L"GPU Board Power", SensorKind::power, SensorUnit::watts, 310.0},
        Expected{0x0501U, L"GPU Core Power", SensorKind::power, SensorUnit::watts, 215.0},
        Expected{0x0502U, L"GPU SoC Power", SensorKind::power, SensorUnit::watts, 42.0},
        Expected{0x0503U, L"GPU ASIC Power", SensorKind::power, SensorUnit::watts, 290.0}};
    for (const auto& expected_sensor : expected) {
        const auto sensor = std::find_if(snapshot.sensors.begin(), snapshot.sensors.begin() + snapshot.count, [&](const auto& candidate) {
            return candidate.id == (id_base | expected_sensor.id);
        });
        Expect(sensor != snapshot.sensors.begin() + snapshot.count, "AMD PMLog sensor keeps its stable ID");
        if (sensor == snapshot.sensors.begin() + snapshot.count) continue;
        Expect(std::wstring_view{sensor->name.data()} == expected_sensor.name, "AMD PMLog sensor keeps the vendor-accurate label");
        Expect(std::wstring_view{sensor->hardware.data()} == L"AMD Radeon Test", "AMD PMLog sensor keeps the adapter identity");
        Expect(sensor->kind == expected_sensor.kind && sensor->unit == expected_sensor.unit, "AMD PMLog sensor kind and unit are correct");
        Expect(std::abs(sensor->current - expected_sensor.current) < 0.0001, "AMD PMLog sensor scaling is correct");
    }
    for (std::uint32_t left = 0U; left < snapshot.count; ++left) {
        for (std::uint32_t right = left + 1U; right < snapshot.count; ++right) {
            Expect(snapshot.sensors[left].id != snapshot.sensors[right].id, "AMD PMLog sensor IDs are unique");
        }
    }

    hardwarescope::DecodeAmdVramSensors(4'096, 16LL * 1'024LL * 1'024LL * 1'024LL, id_base, L"AMD Radeon Test", snapshot);
    Expect(snapshot.count == 35U, "AMD VRAM decoder adds used, total, and utilization readings");
    Expect(std::abs(snapshot.sensors[32].current - 4'096.0) < 0.0001
        && std::abs(snapshot.sensors[33].current - 16'384.0) < 0.0001
        && std::abs(snapshot.sensors[34].current - 25.0) < 0.0001,
        "AMD VRAM byte-to-megabyte and utilization scaling are correct");

    std::array<hardwarescope::AmdPmLogSample, hardwarescope::kAmdPmLogSensorCount> invalid{};
    invalid[8U] = {1, 200}; invalid[19U] = {1, 101}; invalid[1U] = {1, 20'001}; invalid[14U] = {1, -1};
    invalid[21U] = {1, 5'001}; invalid[73U] = {1, 2'001};
    hardwarescope::SensorSnapshot rejected{};
    hardwarescope::DecodeAmdPmLogSensors(invalid, id_base, L"AMD Radeon Test", rejected);
    hardwarescope::DecodeAmdVramSensors(-1, 16LL * 1'024LL * 1'024LL * 1'024LL, id_base, L"AMD Radeon Test", rejected);
    hardwarescope::DecodeAmdVramSensors(17'000, 16LL * 1'024LL * 1'024LL * 1'024LL, id_base, L"AMD Radeon Test", rejected);
    Expect(rejected.count == 0U, "AMD decoder omits invalid, sentinel, and impossible readings");

    hardwarescope::SensorSnapshot bounded{};
    bounded.count = static_cast<std::uint32_t>(hardwarescope::kMaxSensors - 1U);
    hardwarescope::DecodeAmdPmLogSensors(samples, id_base, L"AMD Radeon Test", bounded);
    Expect(bounded.count == hardwarescope::kMaxSensors, "AMD decoder respects the shared snapshot capacity without overflow");
}

void TestGpuAndBoardIdentityMappings() {
    const auto bus_id = hardwarescope::FormatNvmlPciBusId(0x65U);
    Expect(std::string_view{bus_id.data()} == "0000:65:00.0", "NVML PCI bus ID uses canonical domain:bus:device.function format");
    const auto invalid_bus_id = hardwarescope::FormatNvmlPciBusId(0x100U);
    Expect(invalid_bus_id[0] == '\0', "invalid PCI bus IDs are rejected");

    Expect(hardwarescope::IsNct6687DrBoardIdentity(L"Micro-Star International Co., Ltd.", L"MAG X870 TOMAHAWK WIFI"), "MSI X870 selects the NCT6687D-R layout");
    Expect(hardwarescope::IsNct6687DrBoardIdentity(L"MSI", L"PRO Z890-A WIFI"), "short MSI identity selects the NCT6687D-R layout");
    Expect(!hardwarescope::IsNct6687DrBoardIdentity(L"ASUSTeK COMPUTER INC.", L"ROG STRIX X870-E"), "non-MSI X870 remains on the generic NCT6687D layout");
    Expect(!hardwarescope::IsNct6687DrBoardIdentity(L"Micro-Star International Co., Ltd.", L"MEG Z790 GODLIKE MAX"), "older MSI boards remain on the generic NCT6687D layout");
}

void TestDdr5TemperatureConversion() {
    Expect(std::abs(hardwarescope::ConvertDdr5TemperatureRaw(0x02D0U) - 45.0) < 0.001, "DDR5 positive temperature converts at one-sixteenth degree resolution");
    Expect(std::abs(hardwarescope::ConvertDdr5TemperatureRaw(0x1FF0U) - (-1.0)) < 0.001, "DDR5 signed temperature conversion handles below-zero values");
    Expect(hardwarescope::IsPlausibleDdr5Temperature(52.25), "normal DIMM temperature is accepted");
    Expect(!hardwarescope::IsPlausibleDdr5Temperature(512.0), "impossible DIMM temperature is rejected");
    Expect(!hardwarescope::IsPlausibleDdr5Temperature(std::numeric_limits<double>::quiet_NaN()), "non-finite DIMM temperature is rejected");
}

void TestFrameRateTracker() {
    LARGE_INTEGER frequency{};
    LARGE_INTEGER now{};
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&now);
    hardwarescope::FrameRateTracker tracker;
    tracker.SetSmoothingMilliseconds(500U);
    for (int frame = 0; frame <= 30; ++frame) tracker.RecordPresent(1001U, 0x1111U, static_cast<double>(frame) * (1'000.0 / 60.0), now.QuadPart, L"TestGame.exe", true);
    auto reading = tracker.Snapshot(now.QuadPart, 1001U, true);
    Expect(reading.available && reading.frames_per_second == 60U && reading.process_id == 1001U, "bounded frame tracker calculates a 60 FPS game");
    for (int frame = 0; frame <= 72; ++frame) tracker.RecordPresent(1002U, 0x2222U, static_cast<double>(frame) * (500.0 / 72.0), now.QuadPart, L"FasterGame.exe", true);
    reading = tracker.Snapshot(now.QuadPart, 0U, true);
    Expect(reading.available && reading.frames_per_second == 144U && reading.process_id == 1002U, "automatic FPS selection chooses the busiest active swap chain");
    reading = tracker.Snapshot(now.QuadPart, 1001U, true);
    Expect(reading.available && reading.process_id == 1001U, "foreground game wins over a faster background game");
    for (std::uint32_t series = 0U; series < 40U; ++series) tracker.RecordPresent(2'000U + series, series, 1.0, now.QuadPart, L"Stress.exe", true);
    Expect(tracker.ActiveSeries() <= 16U, "frame tracker keeps a fixed maximum number of swap chains");
    const auto stale = tracker.Snapshot(now.QuadPart + frequency.QuadPart * 3, 0U, true);
    Expect(!stale.available, "stale frame samples disappear instead of leaving a frozen FPS value");
}

void TestGameClassification() {
    Expect(hardwarescope::IsExcludedGameExecutable(L"chrome.exe"), "browser is excluded from game-only FPS");
    Expect(hardwarescope::IsExcludedGameExecutable(L"HardwareScopeNative.exe"), "HardwareScope cannot select itself as a game");
    Expect(!hardwarescope::IsKnownGameExecutable(L"ChatGPT.exe", L"C:\\Program Files\\ChatGPT.exe"), "desktop app is not classified as a game");
    Expect(hardwarescope::IsKnownGameExecutable(L"Example-Win64-Shipping.exe", L"D:\\Standalone\\Example-Win64-Shipping.exe"), "Unreal shipping executable is classified as a game");
    Expect(hardwarescope::IsKnownGameExecutable(L"SampleGame.exe", L"D:\\SteamLibrary\\steamapps\\common\\Sample Game\\SampleGame.exe"), "Steam library path is classified as a game");
}

void TestStartupCommand() {
    Expect(hardwarescope::BuildStartupCommand(L"C:\\Program Files\\HardwareScope\\HardwareScope.exe", false)
        == L"\"C:\\Program Files\\HardwareScope\\HardwareScope.exe\"", "startup command safely quotes a spaced executable path");
    Expect(hardwarescope::BuildStartupCommand(L"C:\\HardwareScope.exe", true)
        == L"\"C:\\HardwareScope.exe\" --minimized", "minimized startup command appends an explicit argument");
}

void TestServiceBinaryPath() {
    Expect(hardwarescope::QuoteServiceBinaryPath(L"C:\\Program Files\\HardwareScope\\HardwareScopeSensorService.exe")
        == L"\"C:\\Program Files\\HardwareScope\\HardwareScopeSensorService.exe\"", "service executable paths are quoted for the SCM");
}

void TestUiPalettes() {
    const auto dark = hardwarescope::PaletteFor(hardwarescope::Theme::dark, 0xFF5252U);
    const auto light = hardwarescope::PaletteFor(hardwarescope::Theme::light, 0xFFD93DU);
    const auto midnight = hardwarescope::PaletteFor(hardwarescope::Theme::midnight, 0x52E0D4U);
    Expect(dark.background == 0x071016U && dark.text == 0xF3F8FAU, "dark palette keeps the entire app dark with readable text");
    Expect(dark.accent == 0xFF5252U, "dark palette honors the configured accent color");
    Expect(dark.selection != 0xFFFFFFU && dark.hover != 0xFFFFFFU, "dark palette never falls back to white selection or hover surfaces");
    Expect(light.background == 0xF4F7F9U && light.text == 0x17242DU, "light palette uses a readable light application surface");
    Expect(light.accent == 0xFFD93DU, "light palette honors the configured accent color");
    Expect(midnight.background == 0x000000U && midnight.header == 0x030303U && midnight.text == 0xF7F9FAU,
        "midnight palette uses pitch-black surfaces with readable text");
    Expect(hardwarescope::PaletteFor(hardwarescope::Theme::dark, 0xFF52E0D4U).accent == 0x52E0D4U, "palette strips non-RGB flag bits");
}

void TestSensorViewModel() {
    hardwarescope::SensorSnapshot snapshot{};
    snapshot.count = 5U;
    const auto sensor = [](const std::uint64_t id, const hardwarescope::SensorKind kind, const wchar_t* const name, const wchar_t* const hardware) {
        hardwarescope::SensorValue value{};
        value.id = id;
        value.kind = kind;
        value.available = true;
        static_cast<void>(wcscpy_s(value.name.data(), value.name.size(), name));
        static_cast<void>(wcscpy_s(value.hardware.data(), value.hardware.size(), hardware));
        return value;
    };
    snapshot.sensors[0] = sensor(0x0100'0000'0000'0001ULL, hardwarescope::SensorKind::temperature, L"Core (Tctl/Tdie)", L"Ryzen CPU");
    snapshot.sensors[1] = sensor(0x0100'0000'0000'1000ULL, hardwarescope::SensorKind::utilization, L"CPU Total", L"Ryzen CPU");
    snapshot.sensors[2] = sensor(0x0200'0000'0000'0001ULL, hardwarescope::SensorKind::temperature, L"GPU Core temperature", L"GeForce GPU");
    snapshot.sensors[3] = sensor(0x0300'0000'0000'0001ULL, hardwarescope::SensorKind::temperature, L"Drive composite temperature", L"NVMe SSD");
    snapshot.sensors[4] = sensor(0x0400'0000'0000'1000ULL, hardwarescope::SensorKind::utilization, L"Physical Memory Usage", L"System memory");

    const auto expanded = hardwarescope::BuildSensorView(snapshot, 0U, L"");
    Expect(expanded.count == 10U, "organized sensor view adds one collapsible header for each populated section");
    const auto cpu_temperature_bit = 1U << static_cast<std::uint32_t>(hardwarescope::SensorSection::cpu_temperatures);
    const auto collapsed = hardwarescope::BuildSensorView(snapshot, cpu_temperature_bit, L"");
    Expect(collapsed.count == 9U && collapsed.rows[0].is_section, "collapsed section hides only its sensors");
    const auto filtered = hardwarescope::BuildSensorView(snapshot, 0xFFFFFFFFU, L"geforce");
    Expect(filtered.count == 2U && filtered.rows[0].is_section && !filtered.rows[1].is_section && filtered.rows[1].sensor_index == 2U,
        "search filters names and hardware case-insensitively and temporarily expands matches");

    hardwarescope::SensorSnapshot missing_cpu_temperature{};
    missing_cpu_temperature.count = 1U;
    missing_cpu_temperature.sensors[0] = sensor(0x0100'0000'0000'1000ULL, hardwarescope::SensorKind::utilization, L"CPU Total", L"Ryzen CPU");
    const auto with_cpu_status = hardwarescope::BuildSensorView(missing_cpu_temperature, 0U, L"", true);
    Expect(with_cpu_status.count == 4U
            && with_cpu_status.rows[0].is_section
            && with_cpu_status.rows[1].is_placeholder
            && with_cpu_status.rows[2].is_section,
        "main sensor view keeps CPU temperatures first and explains a missing privileged reading");
}

void TestUpdateManifestAndVerification() {
    constexpr std::string_view valid_manifest = R"({
        "version": "2.0.1",
        "channel": "stable",
        "installer": "https://github.com/Aydren1/HardwareScope/releases/download/v2.0.1/HardwareScope-Setup-2.0.1-x64.exe",
        "sha256": "BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD",
        "size": 3
    })";
    const auto manifest = hardwarescope::ParseUpdateManifest(valid_manifest);
    Expect(manifest.has_value() && manifest->version == hardwarescope::SemanticVersion{2U, 0U, 1U}, "stable updater manifest parses an exact semantic version");
    Expect(manifest.has_value() && manifest->installer_size == 3U, "updater manifest retains the verified installer size");
    Expect(hardwarescope::ParseSemanticVersion("v2.1.0") > hardwarescope::ParseSemanticVersion("2.0.9"), "semantic version comparison is numeric");
    Expect(!hardwarescope::ParseUpdateManifest(std::string{valid_manifest}.replace(valid_manifest.find("stable"), 6U, "beta  ")), "non-stable updater channels are rejected");
    Expect(!hardwarescope::IsTrustedInstallerUrl(L"https://example.com/HardwareScope.exe"), "updater rejects installers outside the official GitHub release path");

    const auto path = std::filesystem::temp_directory_path() / (L"HardwareScopeNativeHash-" + std::to_wstring(GetCurrentProcessId()) + L".bin");
    {
        std::ofstream stream{path, std::ios::binary | std::ios::trunc};
        stream << "abc";
    }
    Expect(hardwarescope::VerifyFileSha256(path, 3U, "BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD"), "download verifier accepts the exact byte size and SHA-256");
    Expect(!hardwarescope::VerifyFileSha256(path, 4U, "BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD"), "download verifier rejects a size mismatch before installation");
    Expect(!hardwarescope::VerifyFileSha256(path, 3U, "AA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD"), "download verifier rejects a checksum mismatch");
    std::error_code error;
    static_cast<void>(std::filesystem::remove(path, error));
}

void TestLegacySettingsMigration() {
    constexpr std::string_view legacy = R"({
      "OsdCorner": "Top right",
      "Theme": "Light",
      "TextColor": "Orange",
      "RefreshSeconds": 0.25,
      "OsdOpacity": 0.75,
      "OsdScale": 1.5,
      "OsdLayout": "Horizontal",
      "OsdSpacing": "Tight",
      "FpsCounterEnabled": true,
      "FpsGameOnly": true,
      "FpsColor": "Yellow",
      "FpsOsdScale": 2,
      "FpsRefreshMilliseconds": 50,
      "FpsSmoothingMilliseconds": 750,
      "StartWithWindows": true,
      "StartMinimized": true,
      "MinimizeToTray": true,
      "ShowOsd": true,
      "AutomaticUpdates": false,
      "01|CPU temperatures": true,
      "02|CPU usage": false,
      "05|Graphics": true
    })";
    hardwarescope::AppSettings migrated{};
    Expect(hardwarescope::MigrateLegacySettingsJson(legacy, migrated), "1.x JSON settings are recognized for one-time migration");
    Expect(migrated.theme == hardwarescope::Theme::light && migrated.text_color_rgb == 0xFF9F43U, "legacy theme and accent migrate");
    Expect(migrated.refresh_interval_ms == 250U && migrated.osd_position == hardwarescope::OsdPosition::top_right, "legacy refresh and OSD corner migrate");
    Expect(migrated.osd_layout == hardwarescope::OsdLayout::horizontal && migrated.osd_opacity_percent == 75U && migrated.osd_scale_percent == 150U && migrated.osd_spacing_px == 2U, "legacy OSD layout controls migrate");
    Expect(migrated.fps_color_rgb == 0xFFD93DU && migrated.fps_scale_percent == 200U && migrated.fps_refresh_interval_ms == 50U && migrated.fps_smoothing_interval_ms == 750U, "legacy independent FPS controls migrate");
    const auto cpu_usage_bit = 1U << static_cast<std::uint32_t>(hardwarescope::SensorSection::cpu_usage);
    Expect((migrated.collapsed_sections & cpu_usage_bit) != 0U, "legacy collapsed sensor sections migrate");
    Expect(migrated.start_with_windows && migrated.start_minimized && !migrated.automatic_updates, "legacy startup and update preferences migrate");
    Expect(!hardwarescope::MigrateLegacySettingsJson("{}", migrated), "unrecognized JSON is never mistaken for HardwareScope settings");
}

void TestSensorExplanations() {
    Expect(std::wstring_view{hardwarescope::ColumnExplanation(hardwarescope::TableColumn::osd)}.find(L"On-Screen Display") != std::wstring_view::npos,
        "OSD column tooltip expands the abbreviation");
    hardwarescope::SensorValue cpu{};
    cpu.kind = hardwarescope::SensorKind::temperature;
    static_cast<void>(wcscpy_s(cpu.name.data(), cpu.name.size(), L"Core (Tctl/Tdie)"));
    Expect(hardwarescope::SensorExplanation(cpu).find(L"thermal management") != std::wstring::npos, "CPU Tctl/Tdie receives a specific plain-language explanation");
    hardwarescope::SensorValue memory{};
    memory.kind = hardwarescope::SensorKind::temperature;
    static_cast<void>(wcscpy_s(memory.name.data(), memory.name.size(), L"GPU Memory Junction temperature"));
    Expect(hardwarescope::SensorExplanation(memory).find(L"memory chips") != std::wstring::npos, "GPU memory junction receives a specific explanation");
    hardwarescope::SensorValue usage{};
    usage.kind = hardwarescope::SensorKind::utilization;
    static_cast<void>(wcscpy_s(usage.name.data(), usage.name.size(), L"CPU Core #1"));
    Expect(hardwarescope::SensorExplanation(usage).find(L"percentage") != std::wstring::npos, "unrecognized utilization sensors receive a useful generic explanation");
}

void BenchmarkSnapshotStore() {
    hardwarescope::SnapshotStore store;
    hardwarescope::SensorSnapshot snapshot{};
    snapshot.count = 7;
    constexpr std::uint64_t iterations = 2'000;
    const auto started = std::chrono::steady_clock::now();
    for (std::uint64_t sequence = 1; sequence <= iterations; ++sequence) {
        snapshot.sequence = sequence;
        store.Publish(snapshot);
        const auto copy = store.ReadLatest();
        if (copy.sequence != sequence) ++failures;
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - started);
    std::cout << "Snapshot publish+read: " << elapsed.count() << " us for " << iterations << " iterations\n";
    Expect(elapsed < std::chrono::seconds{2}, "snapshot handoff benchmark stays bounded");
}

} // namespace

int main() {
    TestWindowRegions();
    TestSnapshotStore();
    TestSensorWorker();
    TestSensorPublishCadence();
    TestSettingsStore();
    TestOsdModel();
    TestProcessorUsageMath();
    TestStorageTemperatureValidation();
    TestGpuTemperatureValidation();
    TestAmdPmLogDecoding();
    TestGpuAndBoardIdentityMappings();
    TestDdr5TemperatureConversion();
    TestFrameRateTracker();
    TestGameClassification();
    TestStartupCommand();
    TestServiceBinaryPath();
    TestUiPalettes();
    TestSensorViewModel();
    TestUpdateManifestAndVerification();
    TestLegacySettingsMigration();
    TestSensorExplanations();
    BenchmarkSnapshotStore();
    if (failures != 0) {
        std::cerr << failures << " native foundation test(s) failed\n";
        return 1;
    }
    std::cout << "OK: HardwareScope 2.0 native foundation tests passed\n";
    return 0;
}
