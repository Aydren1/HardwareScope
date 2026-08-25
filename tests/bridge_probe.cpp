#include "hardwarescope/sensor_bridge.hpp"

#include <windows.h>

#include <iomanip>
#include <iostream>
#include <string_view>

int main(const int argument_count, char** arguments) {
    hardwarescope::SensorBridgeClient bridge;
    const auto fps_target = argument_count == 3 && std::string_view{arguments[1]} == "--fps-target"
        ? strtoul(arguments[2], nullptr, 10) : 0U;
    if (fps_target != 0U && !bridge.SetFpsTarget(fps_target, 500U, 750U)) {
        std::cerr << "FAIL: FPS control bridge error " << GetLastError() << "\n";
        return 1;
    }
    for (int attempt = 0; attempt < (fps_target == 0U ? 30 : 150); ++attempt) {
        if (fps_target != 0U) static_cast<void>(bridge.SetFpsTarget(fps_target, 500U, 750U));
        hardwarescope::SensorSnapshot snapshot{};
        if (bridge.Collect(snapshot) && snapshot.count > 0U) {
            if (fps_target != 0U) {
                hardwarescope::SensorValue frame{};
                if (!bridge.CollectFrameRate(frame)) {
                    Sleep(100U);
                    continue;
                }
                std::wcout << L"FPS: " << std::fixed << std::setprecision(0) << frame.current
                           << L" | " << frame.hardware.data() << L"\n";
                static_cast<void>(bridge.SetFpsTarget(0U, 500U, 750U));
                return frame.current >= 10.0 ? 0 : 1;
            }
            std::wcout << L"Bridge sensors: " << snapshot.count << L"\n";
            for (std::uint32_t index = 0U; index < snapshot.count; ++index) {
                const auto& sensor = snapshot.sensors[index];
                std::wcout << sensor.hardware.data() << L" | " << sensor.name.data() << L": "
                           << std::fixed << std::setprecision(1) << sensor.current << L"\n";
            }
            return 0;
        }
        if (attempt == 0) std::cerr << "Bridge first error: " << GetLastError() << "\n";
        Sleep(100U);
    }
    if (fps_target != 0U) static_cast<void>(bridge.SetFpsTarget(0U, 500U, 750U));
    std::cerr << "FAIL: privileged sensor bridge did not publish a non-empty snapshot\n";
    return 1;
}
