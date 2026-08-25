#pragma once

#include "hardwarescope/app_settings.hpp"
#include "hardwarescope/sensor_snapshot.hpp"

#include <windows.h>

namespace hardwarescope {

class OsdWindow final {
public:
    explicit OsdWindow(HINSTANCE instance) noexcept;
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

private:
    static LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam) noexcept;
    [[nodiscard]] bool RegisterWindowClass() const noexcept;
    [[nodiscard]] bool EnsureSurface(int width, int height) noexcept;
    void DestroySurface() noexcept;
    void RecreateFonts() noexcept;
    [[nodiscard]] bool RefreshDpi() noexcept;
    [[nodiscard]] int ScaleForDpi(int value) const noexcept;
    void Render() noexcept;
    void Position(int width, int height) noexcept;

    HINSTANCE instance_{};
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
    bool visible_{};
};

} // namespace hardwarescope
