#include "hardwarescope/amd_gpu_provider.hpp"
#include "hardwarescope/nvidia_gpu_provider.hpp"

#include <iomanip>
#include <iostream>
#include <chrono>
#include <thread>

namespace {

const wchar_t* Unit(const hardwarescope::SensorUnit unit) noexcept {
    switch (unit) {
    case hardwarescope::SensorUnit::celsius: return L"C";
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
    hardwarescope::NvidiaGpuProvider provider;
    hardwarescope::SensorSnapshot snapshot{};
    if (provider.Initialize()) {
        provider.Collect(snapshot);
        std::wcout << L"NVIDIA GPUs: " << provider.GpuCount() << L"\n";
    }
    hardwarescope::AmdGpuProvider amd_provider;
    if (amd_provider.Initialize()) {
        amd_provider.Collect(snapshot);
        std::wcout << L"AMD GPUs: " << amd_provider.GpuCount() << L"\n";
    }
    if (snapshot.count < 2U) {
        std::cerr << "FAIL: native GPU providers returned too few sensors\n";
        return 1;
    }
    std::wcout << L"Sensors: " << snapshot.count << L"\n";
    for (std::uint32_t index = 0U; index < snapshot.count; ++index) {
        const auto& sensor = snapshot.sensors[index];
        std::wcout << sensor.hardware.data() << L" | " << sensor.name.data() << L": "
                   << std::fixed << std::setprecision(1) << sensor.current << L' ' << Unit(sensor.unit) << L"\n";
    }
    constexpr std::size_t iterations = 500U;
    const auto started = std::chrono::steady_clock::now();
    for (std::size_t iteration = 0U; iteration < iterations; ++iteration) {
        hardwarescope::SensorSnapshot measured{};
        provider.Collect(measured);
        amd_provider.Collect(measured);
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - started);
    std::wcout << L"Average cached GPU collection: "
               << static_cast<double>(elapsed.count()) / static_cast<double>(iterations) << L" us\n";
    constexpr std::size_t refresh_iterations = 10U;
    std::chrono::microseconds refresh_elapsed{};
    for (std::size_t iteration = 0U; iteration < refresh_iterations; ++iteration) {
        std::this_thread::sleep_for(std::chrono::milliseconds{260});
        hardwarescope::SensorSnapshot measured{};
        const auto refresh_started = std::chrono::steady_clock::now();
        provider.Collect(measured);
        amd_provider.Collect(measured);
        refresh_elapsed += std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - refresh_started);
    }
    std::wcout << L"Average complete GPU refresh: "
               << static_cast<double>(refresh_elapsed.count()) / static_cast<double>(refresh_iterations) << L" us\n";
    return 0;
}
