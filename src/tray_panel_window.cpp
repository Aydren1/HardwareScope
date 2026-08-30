#include "hardwarescope/tray_panel_window.hpp"

#include "hardwarescope/app_commands.hpp"
#include "hardwarescope/sensor_view_model.hpp"
#include "hardwarescope/ui_palette.hpp"

#include <dwmapi.h>
#include <windowsx.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cwchar>
#include <string_view>

namespace hardwarescope {
namespace {

COLORREF WinColor(const std::uint32_t rgb) noexcept {
    return RGB((rgb >> 16U) & 0xFFU, (rgb >> 8U) & 0xFFU, rgb & 0xFFU);
}

int Scale(const UINT dpi, const int value) noexcept {
    return MulDiv(value, static_cast<int>(dpi == 0U ? 96U : dpi), 96);
}

bool Contains(const std::wstring_view text, const std::wstring_view part) noexcept {
    return text.find(part) != std::wstring_view::npos;
}

const wchar_t* UnitSuffix(const SensorUnit unit) noexcept {
    switch (unit) {
    case SensorUnit::celsius: return L" °C";
    case SensorUnit::percent: return L" %";
    case SensorUnit::megahertz: return L" MHz";
    case SensorUnit::revolutions_per_minute: return L" RPM";
    case SensorUnit::watts: return L" W";
    case SensorUnit::volts: return L" V";
    case SensorUnit::megabytes: return L" MB";
    case SensorUnit::frames_per_second: return L" FPS";
    case SensorUnit::milliseconds: return L" ms";
    }
    return L"";
}

void FormatValue(const SensorValue& sensor, std::array<wchar_t, 48U>& destination) noexcept {
    if (!sensor.available || !std::isfinite(sensor.current)) {
        static_cast<void>(wcscpy_s(destination.data(), destination.size(), L"—"));
        return;
    }
    if (sensor.unit == SensorUnit::volts) {
        static_cast<void>(swprintf_s(destination.data(), destination.size(), L"%.3f%s", sensor.current, UnitSuffix(sensor.unit)));
    } else if (sensor.unit == SensorUnit::megahertz || sensor.unit == SensorUnit::revolutions_per_minute
        || sensor.unit == SensorUnit::frames_per_second) {
        static_cast<void>(swprintf_s(destination.data(), destination.size(), L"%.0f%s", sensor.current, UnitSuffix(sensor.unit)));
    } else {
        static_cast<void>(swprintf_s(destination.data(), destination.size(), L"%.1f%s", sensor.current, UnitSuffix(sensor.unit)));
    }
}

bool IsRecommended(const SensorValue& sensor) noexcept {
    const std::wstring_view name{sensor.name.data()};
    if (sensor.id == kFpsSensorId || sensor.id == kFpsOnePercentLowSensorId) return true;
    if (sensor.kind == SensorKind::temperature
        && (Contains(name, L"Tctl/Tdie") || Contains(name, L"CPU Package")
            || Contains(name, L"GPU Core temperature") || Contains(name, L"Memory Junction"))) return true;
    return Contains(name, L"Total CPU Utility") || Contains(name, L"GPU Core load")
        || Contains(name, L"Physical Memory Usage");
}

} // namespace

TrayPanelWindow::TrayPanelWindow(const HINSTANCE instance) noexcept : instance_(instance) {}

TrayPanelWindow::~TrayPanelWindow() {
    if (window_ != nullptr) DestroyWindow(window_);
}

bool TrayPanelWindow::RegisterWindowClass() noexcept {
    WNDCLASSEXW type{};
    type.cbSize = sizeof(type);
    type.hInstance = instance_;
    type.lpszClassName = kWindowClass;
    type.lpfnWndProc = &TrayPanelWindow::StaticWindowProcedure;
    type.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    type.hIconSm = static_cast<HICON>(LoadImageW(instance_, MAKEINTRESOURCEW(101), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR | LR_SHARED));
    type.style = CS_HREDRAW | CS_VREDRAW;
    return RegisterClassExW(&type) != 0U || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

bool TrayPanelWindow::Initialize(const HWND owner, const AppSettings& settings) noexcept {
    owner_ = owner;
    settings_ = settings;
    if (!RegisterWindowClass()) return false;
    window_ = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
        kWindowClass,
        L"HardwareScope quick view",
        WS_POPUP,
        0, 0, 320, 260,
        owner_, nullptr, instance_, this);
    if (window_ == nullptr) return false;
    dpi_ = GetDpiForWindow(window_);
    constexpr DWORD dark_mode = 20U;
    const BOOL dark = settings_.theme == Theme::light ? FALSE : TRUE;
    static_cast<void>(DwmSetWindowAttribute(window_, dark_mode, &dark, sizeof(dark)));
    return true;
}

void TrayPanelWindow::ApplySettings(const AppSettings& settings) noexcept {
    settings_ = settings;
    constexpr DWORD dark_mode = 20U;
    const BOOL dark = settings_.theme == Theme::light ? FALSE : TRUE;
    if (window_ != nullptr) {
        static_cast<void>(DwmSetWindowAttribute(window_, dark_mode, &dark, sizeof(dark)));
        if (Visible()) {
            RefreshRows();
            InvalidateRect(window_, nullptr, FALSE);
        }
    }
}

void TrayPanelWindow::Update(const SensorSnapshot& snapshot, const AppSettings& settings) noexcept {
    settings_ = settings;
    if (!Visible()) return;
    CopySnapshot(snapshot, snapshot_);
    RefreshRows();
    InvalidateRect(window_, nullptr, FALSE);
}

void TrayPanelWindow::RefreshRows() noexcept {
    row_count_ = 0U;
    const auto add_sensor = [&](const SensorValue& sensor) {
        if (!sensor.available || row_count_ >= rows_.size()) return;
        for (std::uint32_t index{}; index < row_count_; ++index) {
            if (rows_[index].sensor_id == sensor.id) return;
        }
        auto& row = rows_[row_count_++];
        row.sensor_id = sensor.id;
        static_cast<void>(wcsncpy_s(row.name.data(), row.name.size(), sensor.name.data(), _TRUNCATE));
        FormatValue(sensor, row.value);
        row.color_rgb = SensorSectionColor(ClassifySensor(sensor), settings_);
    };

    for (std::uint32_t favorite{}; favorite < settings_.favorite_sensor_count && row_count_ < rows_.size(); ++favorite) {
        const auto id = settings_.favorite_sensor_ids[favorite];
        const auto end = snapshot_.sensors.begin() + snapshot_.count;
        const auto found = std::find_if(snapshot_.sensors.begin(), end, [id](const SensorValue& sensor) { return sensor.id == id; });
        if (found != end) add_sensor(*found);
    }
    for (std::uint32_t index{}; index < snapshot_.count && row_count_ < rows_.size(); ++index) {
        if (IsRecommended(snapshot_.sensors[index])) add_sensor(snapshot_.sensors[index]);
    }
}

void TrayPanelWindow::Toggle(const POINT anchor, const SensorSnapshot& snapshot, const AppSettings& settings) noexcept {
    if (Visible()) {
        Hide();
        return;
    }
    settings_ = settings;
    CopySnapshot(snapshot, snapshot_);
    RefreshRows();
    ShowAt(anchor);
}

void TrayPanelWindow::ShowAt(const POINT anchor) noexcept {
    if (window_ == nullptr) return;
    const auto monitor = MonitorFromPoint(anchor, MONITOR_DEFAULTTONEAREST);
    MONITORINFO information{sizeof(information)};
    static_cast<void>(GetMonitorInfoW(monitor, &information));
    dpi_ = GetDpiForWindow(window_);
    const auto width = Scale(dpi_, 330);
    const auto height = Scale(dpi_, 116 + static_cast<int>(std::max(1U, row_count_)) * 36);
    const auto x = std::clamp(anchor.x - width, information.rcWork.left + Scale(dpi_, 8), information.rcWork.right - width - Scale(dpi_, 8));
    const auto y = std::clamp(anchor.y - height - Scale(dpi_, 8), information.rcWork.top + Scale(dpi_, 8), information.rcWork.bottom - height - Scale(dpi_, 8));
    static_cast<void>(SetWindowPos(window_, HWND_TOPMOST, x, y, width, height, SWP_NOACTIVATE | SWP_SHOWWINDOW));
    InvalidateRect(window_, nullptr, FALSE);
}

void TrayPanelWindow::Hide() noexcept {
    if (window_ != nullptr) ShowWindow(window_, SW_HIDE);
}

void TrayPanelWindow::DisplayChanged() noexcept {
    Hide();
}

bool TrayPanelWindow::Visible() const noexcept {
    return window_ != nullptr && IsWindowVisible(window_) != FALSE;
}

LRESULT CALLBACK TrayPanelWindow::StaticWindowProcedure(
    const HWND window, const UINT message, const WPARAM wparam, const LPARAM lparam) noexcept {
    auto* self = reinterpret_cast<TrayPanelWindow*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lparam);
        self = static_cast<TrayPanelWindow*>(create->lpCreateParams);
        self->window_ = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    return self == nullptr ? DefWindowProcW(window, message, wparam, lparam) : self->WindowProcedure(message, wparam, lparam);
}

LRESULT TrayPanelWindow::WindowProcedure(const UINT message, const WPARAM wparam, const LPARAM lparam) noexcept {
    switch (message) {
    case WM_ERASEBKGND:
        return 1;
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    case WM_DPICHANGED: {
        dpi_ = HIWORD(wparam);
        const auto* suggested = reinterpret_cast<const RECT*>(lparam);
        static_cast<void>(SetWindowPos(window_, HWND_TOPMOST, suggested->left, suggested->top,
            suggested->right - suggested->left, suggested->bottom - suggested->top, SWP_NOACTIVATE));
        return 0;
    }
    case WM_MOUSEMOVE: {
        RECT client{};
        GetClientRect(window_, &client);
        const auto button_top = client.bottom - Scale(dpi_, 45);
        const auto hovered = GET_Y_LPARAM(lparam) >= button_top;
        if (hovered != open_hovered_) {
            open_hovered_ = hovered;
            InvalidateRect(window_, nullptr, FALSE);
        }
        TRACKMOUSEEVENT tracking{sizeof(tracking), TME_LEAVE, window_, 0U};
        static_cast<void>(TrackMouseEvent(&tracking));
        return 0;
    }
    case WM_MOUSELEAVE:
        open_hovered_ = false;
        InvalidateRect(window_, nullptr, FALSE);
        return 0;
    case WM_LBUTTONUP: {
        RECT client{};
        GetClientRect(window_, &client);
        if (GET_Y_LPARAM(lparam) >= client.bottom - Scale(dpi_, 45)) {
            Hide();
            static_cast<void>(PostMessageW(owner_, WM_COMMAND, kCommandOpen, 0U));
        }
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        const auto dc = BeginPaint(window_, &paint);
        Render(dc);
        EndPaint(window_, &paint);
        return 0;
    }
    case WM_NCDESTROY:
        SetWindowLongPtrW(window_, GWLP_USERDATA, 0);
        window_ = nullptr;
        return DefWindowProcW(window_, message, wparam, lparam);
    default:
        break;
    }
    return DefWindowProcW(window_, message, wparam, lparam);
}

void TrayPanelWindow::Render(const HDC destination) noexcept {
    RECT client{};
    GetClientRect(window_, &client);
    const auto width = client.right - client.left;
    const auto height = client.bottom - client.top;
    const auto memory = CreateCompatibleDC(destination);
    const auto bitmap = CreateCompatibleBitmap(destination, width, height);
    const auto previous_bitmap = SelectObject(memory, bitmap);
    const auto palette = PaletteFor(settings_.theme, settings_.text_color_rgb, settings_.high_contrast);
    const auto background = CreateSolidBrush(WinColor(palette.background));
    const auto surface = CreateSolidBrush(WinColor(palette.surface));
    const auto hover = CreateSolidBrush(WinColor(palette.hover));
    const auto line_pen = CreatePen(PS_SOLID, std::max(1, Scale(dpi_, 1)), WinColor(palette.line));
    FillRect(memory, &client, background);
    const auto old_pen = SelectObject(memory, line_pen);
    const auto old_brush = SelectObject(memory, GetStockObject(HOLLOW_BRUSH));
    RoundRect(memory, 0, 0, width, height, Scale(dpi_, 12), Scale(dpi_, 12));
    SelectObject(memory, old_brush);
    SelectObject(memory, old_pen);

    const auto title_font = CreateFontW(-Scale(dpi_, 17), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    const auto body_font = CreateFontW(-Scale(dpi_, 14), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    const auto value_font = CreateFontW(-Scale(dpi_, 14), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    SetBkMode(memory, TRANSPARENT);
    auto previous_font = SelectObject(memory, title_font);
    SetTextColor(memory, WinColor(palette.text));
    RECT title{Scale(dpi_, 18), Scale(dpi_, 13), width - Scale(dpi_, 16), Scale(dpi_, 42)};
    DrawTextW(memory, L"HardwareScope", -1, &title, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    SelectObject(memory, body_font);
    SetTextColor(memory, WinColor(palette.muted));
    RECT subtitle{Scale(dpi_, 18), Scale(dpi_, 38), width - Scale(dpi_, 16), Scale(dpi_, 60)};
    DrawTextW(memory, L"Quick view", -1, &subtitle, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

    auto y = Scale(dpi_, 64);
    if (row_count_ == 0U) {
        RECT empty{Scale(dpi_, 18), y, width - Scale(dpi_, 18), y + Scale(dpi_, 34)};
        SetTextColor(memory, WinColor(palette.muted));
        DrawTextW(memory, L"Waiting for live sensor data…", -1, &empty, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    }
    for (std::uint32_t index{}; index < row_count_; ++index) {
        const auto& row = rows_[index];
        RECT row_bounds{Scale(dpi_, 12), y, width - Scale(dpi_, 12), y + Scale(dpi_, 32)};
        if ((index % 2U) != 0U) FillRect(memory, &row_bounds, surface);
        SelectObject(memory, body_font);
        SetTextColor(memory, WinColor(palette.text));
        RECT name{Scale(dpi_, 20), y, width - Scale(dpi_, 112), y + Scale(dpi_, 32)};
        DrawTextW(memory, row.name.data(), -1, &name, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        SelectObject(memory, value_font);
        SetTextColor(memory, WinColor(row.color_rgb));
        RECT value{width - Scale(dpi_, 132), y, width - Scale(dpi_, 20), y + Scale(dpi_, 32)};
        DrawTextW(memory, row.value.data(), -1, &value, DT_RIGHT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        y += Scale(dpi_, 36);
    }

    RECT button{Scale(dpi_, 12), height - Scale(dpi_, 41), width - Scale(dpi_, 12), height - Scale(dpi_, 8)};
    FillRect(memory, &button, open_hovered_ ? hover : surface);
    SelectObject(memory, value_font);
    SetTextColor(memory, WinColor(palette.accent));
    DrawTextW(memory, L"Open HardwareScope", -1, &button, DT_CENTER | DT_SINGLELINE | DT_VCENTER);

    SelectObject(memory, previous_font);
    BitBlt(destination, 0, 0, width, height, memory, 0, 0, SRCCOPY);
    DeleteObject(title_font);
    DeleteObject(body_font);
    DeleteObject(value_font);
    DeleteObject(line_pen);
    DeleteObject(background);
    DeleteObject(surface);
    DeleteObject(hover);
    SelectObject(memory, previous_bitmap);
    DeleteObject(bitmap);
    DeleteDC(memory);
}

} // namespace hardwarescope
