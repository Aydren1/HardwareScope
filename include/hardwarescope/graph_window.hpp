#pragma once

#include "hardwarescope/app_settings.hpp"
#include "hardwarescope/graph_model.hpp"
#include "hardwarescope/sensor_snapshot.hpp"

#include <windows.h>

namespace hardwarescope {

class GraphWindow final {
public:
    explicit GraphWindow(HINSTANCE instance) noexcept : instance_(instance) {}
    ~GraphWindow();

    GraphWindow(const GraphWindow&) = delete;
    GraphWindow& operator=(const GraphWindow&) = delete;

    [[nodiscard]] bool Initialize(HWND owner, const AppSettings& settings) noexcept;
    void ApplySettings(const AppSettings& settings) noexcept;
    void Update(const SensorSnapshot& snapshot) noexcept;
    void CapturePlacement(AppSettings& settings) const noexcept;
    void DisplayChanged() noexcept;
    [[nodiscard]] HWND Handle() const noexcept { return window_; }

    static constexpr wchar_t kWindowClass[] = L"HardwareScope.Native.GraphWindow";

private:
    static LRESULT CALLBACK StaticWindowProcedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam) noexcept;
    LRESULT WindowProcedure(UINT message, WPARAM wparam, LPARAM lparam) noexcept;
    [[nodiscard]] bool RegisterWindowClass() const noexcept;
    void Render(HDC destination, const RECT& client) noexcept;
    void HandleClick(POINT point) noexcept;
    void RecreateFont() noexcept;

    HINSTANCE instance_{};
    HWND owner_{};
    HWND window_{};
    UINT dpi_{96U};
    HFONT font_{};
    AppSettings settings_{};
    GraphHistory history_{};
    std::uint32_t view_history_seconds_{30U};
};

} // namespace hardwarescope
