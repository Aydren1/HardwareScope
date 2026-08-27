#pragma once

#include "hardwarescope/app_settings.hpp"
#include "hardwarescope/sensor_snapshot.hpp"

#include <windows.h>

#include <array>

namespace hardwarescope {

enum class OsdWindowRole : std::uint8_t {
    primary,
    fps,
};

class OsdWindow final {
public:
    explicit OsdWindow(HINSTANCE instance, OsdWindowRole role = OsdWindowRole::primary) noexcept;
    ~OsdWindow();

    OsdWindow(const OsdWindow&) = delete;
    OsdWindow& operator=(const OsdWindow&) = delete;

    [[nodiscard]] bool Initialize(HWND monitor_anchor, const AppSettings& settings) noexcept;
    void ApplySettings(const AppSettings& settings) noexcept;
    void DisplayChanged() noexcept;
    void Update(const SensorSnapshot& snapshot) noexcept;
    void SetVisible(bool visible) noexcept;
    [[nodiscard]] bool Visible() const noexcept { return visible_; }
    [[nodiscard]] HWND Handle() const noexcept { return window_; }

    static constexpr wchar_t kWindowClass[] = L"HardwareScope.Native.OsdWindow";
    static constexpr wchar_t kFpsWindowClass[] = L"HardwareScope.Native.FpsOsdWindow";

private:
    static LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam) noexcept;
    [[nodiscard]] bool RegisterWindowClass() const noexcept;
    [[nodiscard]] const wchar_t* WindowClassName() const noexcept;
    [[nodiscard]] bool EnsureSurface(int width, int height) noexcept;
    void DestroySurface() noexcept;
    void RecreateFonts() noexcept;
    [[nodiscard]] bool RefreshDpi() noexcept;
    [[nodiscard]] int ScaleForDpi(int value) const noexcept;
    void Render() noexcept;
    void Position(int width, int height) noexcept;
    void UpdateGraphHistory(const SensorSnapshot& snapshot) noexcept;
    [[nodiscard]] const SensorValue* GraphSensor(const SensorSnapshot& snapshot) const noexcept;
    [[nodiscard]] bool GraphBelongsOnThisSurface() const noexcept;

    HINSTANCE instance_{};
    OsdWindowRole role_{OsdWindowRole::primary};
    HWND window_{};
    HWND monitor_anchor_{};
    UINT dpi_{96U};
    HDC memory_dc_{};
    HBITMAP bitmap_{};
    HGDIOBJ original_bitmap_{};
    void* pixels_{};
    int surface_width_{};
    int surface_height_{};
    HFONT sensor_font_{};
    HFONT fps_font_{};
    AppSettings settings_{};
    SensorSnapshot snapshot_{};
    static constexpr std::size_t kMaximumGraphSamples = 600U;
    std::array<double, kMaximumGraphSamples> graph_samples_{};
    std::size_t graph_first_{};
    std::size_t graph_count_{};
    std::uint64_t graph_sensor_id_{};
    std::uint64_t graph_snapshot_sequence_{};
    ULONGLONG last_graph_sample_tick_{};
    bool visible_{};
};

} // namespace hardwarescope
