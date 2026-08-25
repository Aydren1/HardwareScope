#include "hardwarescope/amd_zen_provider.hpp"
#include "hardwarescope/ddr5_temperature_provider.hpp"
#include "hardwarescope/nct6687_provider.hpp"

#include <windows.h>

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace {

void SaveReport(const int argument_count, wchar_t** const arguments, const std::wstring& report) {
    if (argument_count >= 3 && std::wstring_view{arguments[1]} == L"--output") {
        std::wofstream output{arguments[2], std::ios::trunc};
        output << report;
    }
}

} // namespace

int wmain(const int argument_count, wchar_t** arguments) {
    std::wostringstream report;
    hardwarescope::AmdZenProvider provider;
    if (!provider.Initialize(GetModuleHandleW(nullptr))) {
        report << L"SKIP: AMD Zen/PawnIO provider is unavailable; HRESULT=0x"
               << std::hex << static_cast<unsigned long>(provider.LastError()) << L'\n';
        std::wcerr << report.str();
        SaveReport(argument_count, arguments, report.str());
        return 2;
    }
    hardwarescope::AmdZenTemperatures temperatures{};
    if (!provider.ReadTemperatures(temperatures)) {
        report << L"FAIL: provider initialized but could not read a valid package temperature; HRESULT=0x"
               << std::hex << static_cast<unsigned long>(provider.LastError()) << L'\n';
        std::wcerr << report.str();
        SaveReport(argument_count, arguments, report.str());
        return 1;
    }
    report << L"CPU: " << provider.ProcessorName() << L"\nFamily: 0x" << std::hex << provider.Family()
           << L"  Model: 0x" << provider.Model() << std::dec
           << L"\nCore (Tctl/Tdie): " << std::fixed << std::setprecision(1) << temperatures.package_celsius << L" C\n";
    for (std::size_t index = 0U; index < temperatures.ccd_count; ++index) {
        report << L"CCD" << index + 1U << L" (Tdie): " << temperatures.ccd_celsius[index] << L" C\n";
    }

    hardwarescope::Nct6687Provider motherboard;
    if (motherboard.Initialize(GetModuleHandleW(nullptr))) {
        hardwarescope::SensorSnapshot snapshot{};
        motherboard.Collect(snapshot);
        report << motherboard.ChipName() << L" base port: 0x" << std::hex << motherboard.BasePort() << std::dec
               << L"\nMotherboard sensors: " << snapshot.count << L"\n";
        for (std::uint32_t index = 0U; index < snapshot.count; ++index) {
            const auto& sensor = snapshot.sensors[index];
            report << sensor.name.data() << L": " << sensor.current
                   << (sensor.kind == hardwarescope::SensorKind::temperature ? L" C" : L" RPM") << L"\n";
        }
    } else {
        report << L"SKIP: supported Nuvoton provider unavailable; HRESULT=0x"
               << std::hex << static_cast<unsigned long>(motherboard.LastError())
               << L"; slots=";
        for (std::size_t slot = 0U; slot < motherboard.DetectedChipIds().size(); ++slot) {
            report << L" [" << slot << L":0x" << static_cast<unsigned>(motherboard.DetectedChipIds()[slot])
                   << L"/0x" << static_cast<unsigned>(motherboard.DetectedRevisions()[slot]) << L"]";
        }
        report << std::dec << L"\n";
    }

    hardwarescope::Ddr5TemperatureProvider memory;
    if (memory.Initialize(GetModuleHandleW(nullptr))) {
        hardwarescope::SensorSnapshot snapshot{};
        memory.Collect(snapshot);
        report << L"DDR5 temperature sensors: " << snapshot.count << L"\n";
        for (std::uint32_t index = 0U; index < snapshot.count; ++index) {
            const auto& sensor = snapshot.sensors[index];
            report << sensor.name.data() << L": " << sensor.current << L" C (" << sensor.hardware.data() << L")\n";
        }
    } else {
        report << L"SKIP: native DDR5/PawnIO provider unavailable; HRESULT=0x"
               << std::hex << static_cast<unsigned long>(memory.LastError()) << std::dec << L"\n";
    }

    std::wcout << report.str();
    SaveReport(argument_count, arguments, report.str());
    return 0;
}
