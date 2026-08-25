#include "hardwarescope/osd_window.hpp"
#include "hardwarescope/osd_model.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace hardwarescope {
namespace {

SIZE TextSize(const HDC dc, const HFONT font, const std::wstring& text) noexcept {
    const auto previous = SelectObject(dc, font);
    SIZE size{};
    static_cast<void>(GetTextExtentPoint32W(dc, text.c_str(), static_cast<int>(text.size()), &size));
    static_cast<void>(SelectObject(dc, previous));
    return size;
}

} // namespace

OsdWindow::OsdWindow(const HINSTANCE instance) noexcept : instance_(instance) {}

OsdWindow::~OsdWindow() {
    if (window_ != nullptr) DestroyWindow(window_);
    DestroySurface();
    if (sensor_font_ != nullptr) DeleteObject(sensor_font_);
    if (fps_font_ != nullptr) DeleteObject(fps_font_);
}

bool OsdWindow::Initialize(const HWND monitor_anchor, const AppSettings& settings) noexcept {
    monitor_anchor_ = monitor_anchor;
    settings_ = settings;
    if (!RegisterWindowClass()) return false;
    window_ = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        kWindowClass,
        L"HardwareScope OSD",
        WS_POPUP,
        0,
        0,
        1,
        1,
        nullptr,
        nullptr,
        instance_,
        nullptr);
    if (window_ == nullptr) return false;
    static_cast<void>(RefreshDpi());
    RecreateFonts();
    visible_ = settings_.show_osd;
    return true;
}

bool OsdWindow::RefreshDpi() noexcept {
    const auto next_dpi = monitor_anchor_ != nullptr ? GetDpiForWindow(monitor_anchor_) : 96U;
    const auto normalized = next_dpi == 0U ? 96U : next_dpi;
    if (normalized == dpi_) return false;
    dpi_ = normalized;
    return true;
}

int OsdWindow::ScaleForDpi(const int value) const noexcept {
    return MulDiv(value, static_cast<int>(dpi_), 96);
}

void OsdWindow::DisplayChanged() noexcept {
    if (RefreshDpi()) RecreateFonts();
    Render();
}

bool OsdWindow::RegisterWindowClass() const noexcept {
    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = &OsdWindow::WindowProcedure;
    window_class.hInstance = instance_;
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.lpszClassName = kWindowClass;
    return RegisterClassExW(&window_class) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

LRESULT CALLBACK OsdWindow::WindowProcedure(const HWND window, const UINT message, const WPARAM wparam, const LPARAM lparam) noexcept {
    if (message == WM_NCHITTEST) return HTTRANSPARENT;
    if (message == WM_MOUSEACTIVATE) return MA_NOACTIVATE;
    return DefWindowProcW(window, message, wparam, lparam);
}

void OsdWindow::ApplySettings(const AppSettings& settings) noexcept {
    const bool fonts_changed = settings.osd_scale_percent != settings_.osd_scale_percent
        || settings.fps_scale_percent != settings_.fps_scale_percent;
    settings_ = settings;
    if (fonts_changed) RecreateFonts();
    visible_ = settings_.show_osd;
    Render();
}

void OsdWindow::Update(const SensorSnapshot& snapshot) noexcept {
    CopySnapshot(snapshot, snapshot_);
    Render();
}

void OsdWindow::SetVisible(const bool visible) noexcept {
    visible_ = visible;
    if (!visible_ && window_ != nullptr) ShowWindow(window_, SW_HIDE);
    else Render();
}

void OsdWindow::RecreateFonts() noexcept {
    if (sensor_font_ != nullptr) DeleteObject(sensor_font_);
    if (fps_font_ != nullptr) DeleteObject(fps_font_);
    const auto sensor_height = -std::max(
        ScaleForDpi(10),
        MulDiv(ScaleForDpi(16), static_cast<int>(settings_.osd_scale_percent), 100));
    const auto fps_height = -std::max(10, MulDiv(-sensor_height, static_cast<int>(settings_.fps_scale_percent), 100));
    sensor_font_ = CreateFontW(sensor_height, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    fps_font_ = CreateFontW(fps_height, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Segoe UI");
}

bool OsdWindow::EnsureSurface(const int width, const int height) noexcept {
    if (memory_dc_ != nullptr && width == surface_width_ && height == surface_height_) return true;
    DestroySurface();
    memory_dc_ = CreateCompatibleDC(nullptr);
    if (memory_dc_ == nullptr) return false;
    BITMAPINFO information{};
    information.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    information.bmiHeader.biWidth = width;
    information.bmiHeader.biHeight = -height;
    information.bmiHeader.biPlanes = 1;
    information.bmiHeader.biBitCount = 32;
    information.bmiHeader.biCompression = BI_RGB;
    bitmap_ = CreateDIBSection(memory_dc_, &information, DIB_RGB_COLORS, &pixels_, nullptr, 0);
    if (bitmap_ == nullptr || pixels_ == nullptr) {
        DestroySurface();
        return false;
    }
    original_bitmap_ = SelectObject(memory_dc_, bitmap_);
    surface_width_ = width;
    surface_height_ = height;
    return true;
}

void OsdWindow::DestroySurface() noexcept {
    if (memory_dc_ != nullptr && original_bitmap_ != nullptr) static_cast<void>(SelectObject(memory_dc_, original_bitmap_));
    if (bitmap_ != nullptr) DeleteObject(bitmap_);
    if (memory_dc_ != nullptr) DeleteDC(memory_dc_);
    memory_dc_ = nullptr;
    bitmap_ = nullptr;
    original_bitmap_ = nullptr;
    pixels_ = nullptr;
    surface_width_ = 0;
    surface_height_ = 0;
}

void OsdWindow::Render() noexcept {
    if (window_ == nullptr || !visible_) return;
    const auto items = BuildOsdDisplayItems(snapshot_, settings_);
    if (items.empty()) {
        ShowWindow(window_, SW_HIDE);
        return;
    }
    if (memory_dc_ == nullptr && !EnsureSurface(1, 1)) return;

    const auto padding = std::max(
        1,
        MulDiv(ScaleForDpi(settings_.osd_background ? 8 : 2), static_cast<int>(settings_.osd_scale_percent), 100));
    const auto spacing = std::max(
        0,
        MulDiv(ScaleForDpi(static_cast<int>(settings_.osd_spacing_px)), static_cast<int>(settings_.osd_scale_percent), 100));
    std::vector<SIZE> sizes;
    sizes.reserve(items.size());
    int width = padding * 2;
    int height = padding * 2;
    if (settings_.osd_layout == OsdLayout::vertical) {
        for (const auto& item : items) {
            const auto size = TextSize(memory_dc_, item.fps ? fps_font_ : sensor_font_, item.text);
            sizes.push_back(size);
            width = std::max(width, static_cast<int>(size.cx) + padding * 2);
            height += size.cy;
        }
        height += spacing * static_cast<int>(items.size() - 1U);
    } else {
        const auto separator_size = TextSize(memory_dc_, sensor_font_, L"|");
        OsdHardwareGroup previous_group = OsdHardwareGroup::fps;
        bool have_previous = false;
        for (const auto& item : items) {
            const auto size = TextSize(memory_dc_, item.fps ? fps_font_ : sensor_font_, item.text);
            sizes.push_back(size);
            if (have_previous) width += spacing;
            if (have_previous && settings_.osd_group_separators && item.group != OsdHardwareGroup::fps && previous_group != item.group) width += separator_size.cx + spacing;
            width += size.cx;
            height = std::max(height, static_cast<int>(size.cy) + padding * 2);
            previous_group = item.group;
            have_previous = true;
        }
    }
    width = std::max(width, 1);
    height = std::max(height, 1);
    if (!EnsureSurface(width, height)) return;
    std::memset(pixels_, 0, static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * sizeof(std::uint32_t));
    SetBkMode(memory_dc_, TRANSPARENT);
    SetTextColor(memory_dc_, RGB(255, 255, 255));

    auto draw_item = [&](const OsdDisplayItem& item, const SIZE size, const int x, const int y) {
        const auto previous_font = SelectObject(memory_dc_, item.fps ? fps_font_ : sensor_font_);
        static_cast<void>(TextOutW(memory_dc_, x, y, item.text.c_str(), static_cast<int>(item.text.size())));
        static_cast<void>(SelectObject(memory_dc_, previous_font));
        auto* pixels = static_cast<std::uint32_t*>(pixels_);
        const auto red = static_cast<std::uint8_t>((item.color_rgb >> 16U) & 0xFFU);
        const auto green = static_cast<std::uint8_t>((item.color_rgb >> 8U) & 0xFFU);
        const auto blue = static_cast<std::uint8_t>(item.color_rgb & 0xFFU);
        for (int row = std::max(0, y); row < std::min(height, y + static_cast<int>(size.cy) + 2); ++row) {
            for (int column = std::max(0, x); column < std::min(width, x + static_cast<int>(size.cx) + 2); ++column) {
                auto& pixel = pixels[static_cast<std::size_t>(row) * static_cast<std::size_t>(width) + static_cast<std::size_t>(column)];
                const auto coverage = static_cast<std::uint8_t>(std::max({pixel & 0xFFU, (pixel >> 8U) & 0xFFU, (pixel >> 16U) & 0xFFU}));
                if (coverage == 0U) continue;
                const auto alpha = static_cast<std::uint8_t>((static_cast<unsigned>(coverage) * settings_.osd_opacity_percent) / 100U);
                pixel = (static_cast<std::uint32_t>(alpha) << 24U)
                    | (static_cast<std::uint32_t>(red) * alpha / 255U << 16U)
                    | (static_cast<std::uint32_t>(green) * alpha / 255U << 8U)
                    | (static_cast<std::uint32_t>(blue) * alpha / 255U);
            }
        }
    };

    if (settings_.osd_layout == OsdLayout::vertical) {
        int y = padding;
        for (std::size_t index = 0; index < items.size(); ++index) {
            draw_item(items[index], sizes[index], padding, y);
            y += sizes[index].cy + spacing;
        }
    } else {
        int x = padding;
        OsdHardwareGroup previous_group = OsdHardwareGroup::fps;
        bool have_previous = false;
        for (std::size_t index = 0; index < items.size(); ++index) {
            if (have_previous) x += spacing;
            if (have_previous && settings_.osd_group_separators && items[index].group != OsdHardwareGroup::fps && previous_group != items[index].group) {
                OsdDisplayItem separator{0U, L"|", items[index].group, settings_.text_color_rgb, false};
                const auto separator_size = TextSize(memory_dc_, sensor_font_, separator.text);
                draw_item(separator, separator_size, x, padding);
                x += separator_size.cx + spacing;
            }
            draw_item(items[index], sizes[index], x, padding);
            x += sizes[index].cx;
            previous_group = items[index].group;
            have_previous = true;
        }
    }

    if (settings_.osd_background) {
        auto* pixels = static_cast<std::uint32_t*>(pixels_);
        const auto background_alpha = static_cast<std::uint8_t>((145U * settings_.osd_opacity_percent) / 100U);
        for (std::size_t index = 0; index < static_cast<std::size_t>(width) * static_cast<std::size_t>(height); ++index) {
            if ((pixels[index] >> 24U) == 0U) pixels[index] = static_cast<std::uint32_t>(background_alpha) << 24U;
        }
    }

    Position(width, height);
    POINT source{};
    SIZE size{width, height};
    POINT destination{};
    RECT bounds{};
    GetWindowRect(window_, &bounds);
    destination.x = bounds.left;
    destination.y = bounds.top;
    BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    static_cast<void>(UpdateLayeredWindow(window_, nullptr, &destination, &size, memory_dc_, &source, 0, &blend, ULW_ALPHA));
    ShowWindow(window_, SW_SHOWNOACTIVATE);
}

void OsdWindow::Position(const int width, const int height) noexcept {
    MONITORINFO information{};
    information.cbSize = sizeof(information);
    const auto monitor = MonitorFromWindow(monitor_anchor_, MONITOR_DEFAULTTOPRIMARY);
    if (!GetMonitorInfoW(monitor, &information)) return;
    const auto margin = ScaleForDpi(12);
    int x = information.rcWork.left + margin;
    int y = information.rcWork.top + margin;
    if (settings_.osd_position == OsdPosition::top_right || settings_.osd_position == OsdPosition::bottom_right) x = information.rcWork.right - width - margin;
    if (settings_.osd_position == OsdPosition::bottom_left || settings_.osd_position == OsdPosition::bottom_right) y = information.rcWork.bottom - height - margin;
    static_cast<void>(SetWindowPos(window_, HWND_TOPMOST, x, y, width, height, SWP_NOACTIVATE | SWP_NOOWNERZORDER));
}

} // namespace hardwarescope
