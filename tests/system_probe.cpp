#include "hardwarescope/system_performance_provider.hpp"
#include "hardwarescope/storage_temperature_provider.hpp"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <thread>

namespace {

const wchar_t* Unit(const hardwarescope::SensorUnit unit) noexcept {
    switch (unit) {
    case hardwarescope::SensorUnit::celsius: return L"°C";
    case hardwarescope::SensorUnit::percent: return L"%";
    case hardwarescope::SensorUnit::megahertz: return L"MHz";
    case hardwarescope::SensorUnit::revolutions_per_minute: return L"RPM";
    case hardwarescope::SensorUnit::watts: return L"W";
    case hardwarescope::SensorUnit::volts: return L"V";
    case hardwarescope::SensorUnit::megabytes: return L"MB";
    case hardwarescope::SensorUnit::frames_per_second: return L"FPS";
    case hardwarescope::SensorUnit::milliseconds: return L"ms";
    }
    return L"";
}

} // namespace

int main() {
    hardwarescope::SystemPerformanceProvider provider;
    hardwarescope::SensorSnapshot warmup{};
    provider.Collect(warmup);
    std::this_thread::sleep_for(std::chrono::milliseconds{750});
    hardwarescope::SensorSnapshot snapshot{};
    provider.Collect(snapshot);
    hardwarescope::StorageTemperatureProvider storage_provider;
    storage_provider.Collect(snapshot);
    if (snapshot.count < 4U) {
        std::cerr << "FAIL: native system provider returned too few sensors\n";
        return 1;
    }
    std::wcout << L"Processor: " << provider.ProcessorName() << L"\nSensors: " << snapshot.count << L"\n";
    for (std::uint32_t index = 0; index < snapshot.count; ++index) {
        const auto& sensor = snapshot.sensors[index];
        if (std::wstring_view{sensor.name.data()} == L"CPU Total"
            || std::wstring_view{sensor.name.data()} == L"CPU Clock Average"
            || std::wstring_view{sensor.name.data()} == L"Physical Memory Usage"
            || sensor.kind == hardwarescope::SensorKind::temperature
            || index < 3U) {
            std::wcout << sensor.name.data() << L": " << std::fixed << std::setprecision(1)
                       << sensor.current << L' ' << Unit(sensor.unit) << L"\n";
        }
    }
    std::wcout << L"Physical drives opened: " << storage_provider.DriveCount() << L"\n";
    return 0;
}
