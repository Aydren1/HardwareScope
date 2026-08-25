#pragma once

#include <windows.h>

namespace hardwarescope {

[[nodiscard]] constexpr LRESULT HitTestWindowRegion(
    const int width,
    const int height,
    const int x,
    const int y,
    const int resize_border,
    const int header_height,
    const int interactive_width) noexcept {
    const bool left = x >= 0 && x < resize_border;
    const bool right = x < width && x >= width - resize_border;
    const bool top = y >= 0 && y < resize_border;
    const bool bottom = y < height && y >= height - resize_border;

    if (top && left) return HTTOPLEFT;
    if (top && right) return HTTOPRIGHT;
    if (bottom && left) return HTBOTTOMLEFT;
    if (bottom && right) return HTBOTTOMRIGHT;
    if (left) return HTLEFT;
    if (right) return HTRIGHT;
    if (top) return HTTOP;
    if (bottom) return HTBOTTOM;

    if (y < header_height) {
        if (x >= width - interactive_width) return HTCLIENT;
        return HTCAPTION;
    }
    return HTCLIENT;
}

} // namespace hardwarescope
