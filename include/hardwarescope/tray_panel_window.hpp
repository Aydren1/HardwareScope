#pragma once

#include "hardwarescope/app_settings.hpp"
#include "hardwarescope/sensor_snapshot.hpp"

#include <windows.h>

#include <array>
#include <cstdint>

namespace hardwarescope {

class TrayPanelWindow final {
public:
    explicit TrayPanelWindow(HINSTANCE instance) noexcept;
    ~TrayPanelWindow();

    TrayPanelWindow(const TrayPanelWindow&) = delete;
    TrayPanelWindow& operator=(const TrayPanelWindow&) = delete;

    [[nodiscard]] bool Initialize(HWND owner, const AppSettings& settings) noexcept;
    void ApplySettings(const AppSettings& settings) noexcept;
    void Update(const SensorSnapshot& snapshot, const AppSettings& settings) noexcept;
    void Toggle(POINT anchor, const SensorSnapshot& snapshot, const AppSettings& settings) noexcept;
    void Hide() noexcept;
    void DisplayChanged() noexcept;
    [[nodiscard]] bool Visible() const noexcept;
    [[nodiscard]] HWND Handle() const noexcept { return window_; }

    static constexpr wchar_t kWindowClass[] = L"HardwareScope.Native.TrayPanelWindow";

private:
    static LRESULT CALLBACK StaticWindowProcedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam) noexcept;
    LRESULT WindowProcedure(UINT message, WPARAM wparam, LPARAM lparam) noexcept;
    bool RegisterWindowClass() noexcept;
    void ShowAt(POINT anchor) noexcept;
    void Render(HDC destination) noexcept;
    void RefreshRows() noexcept;

    struct Row final {
        std::uint64_t sensor_id{};
        std::array<wchar_t, 64U> name{};
        std::array<wchar_t, 48U> value{};
        std::uint32_t color_rgb{};
    };

    HINSTANCE instance_{};
    HWND owner_{};
    HWND window_{};
    UINT dpi_{96U};
    AppSettings settings_{};
    SensorSnapshot snapshot_{};
    std::array<Row, 6U> rows_{};
    std::uint32_t row_count_{};
    bool open_hovered_{};
};

} // namespace hardwarescope
