# Second-PC native sensor validation

This gate is required before HardwareScope 2.0 can replace public 1.x. Perform
it on the AMD Radeon reference PC while HWiNFO is open in Sensors-only mode.

1. Extract `HardwareScope-2.0-sensor-validation-x64.zip` on the second PC.
2. Right-click it and choose **Run as administrator**. The exporter is read-only
   and embeds the same PawnIO modules used by the native sensor service.
3. Open an administrator Terminal in the extracted folder. Keep the PC idle,
   start an HWiNFO sensor log, and run this 60-second capture:

   `HardwareScopeSensorExport.exe --duration-seconds 60 --interval-ms 1000 --output HardwareScope-Sensors-idle.csv`

4. Stop the HWiNFO log. Start a repeatable GPU load, wait two minutes, start a
   new HWiNFO log, and run the matching 60-second capture:

   `HardwareScopeSensorExport.exe --duration-seconds 60 --interval-ms 1000 --output HardwareScope-Sensors-load.csv`

5. Stop and save the HWiNFO load log. HardwareScope timestamps every sample in
   UTC so the two series can be aligned even when their polling instants differ.
6. Validate both HardwareScope series and create their min/average/max summary:

   `powershell -ExecutionPolicy Bypass -File .\Analyze-HardwareScopeSeries.ps1 -IdlePath .\HardwareScope-Sensors-idle.csv -LoadPath .\HardwareScope-Sensors-load.csv`

7. Compare source, unit, and scaling for CPU Tctl/Tdie, CCD temperatures, GPU
   core temperature, GPU hotspot, GPU memory junction when supported, CPU/GPU
   utilization, effective clocks, fan RPM, board power, drive composite
   temperatures, DIMM temperatures, and supported motherboard sensors.

## Acceptance rules

- Temperatures should normally be within 2 C at the same timestamp; brief
  differences caused by polling order are acceptable.
- Utilization and clocks are compared over several samples, not one instant.
- Each HardwareScope capture must contain 60 sample indexes and must not contain
  `NaN`, infinity, empty units, duplicate sensor IDs within one sample, or
  timestamps that move backward.
- A missing hardware capability is omitted, never replaced by zero or a
  threshold/critical-limit value.
- Implausible, duplicated, mis-scaled, or mislabeled readings fail the gate.
- Preserve both HardwareScope CSV files, both HWiNFO logs, Windows version,
  motherboard model/BIOS, CPU, GPU, driver version, and storage model list with
  the validation record.
