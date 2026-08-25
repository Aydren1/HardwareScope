#include "hardwarescope/ui_palette.hpp"

namespace hardwarescope {
namespace {

constexpr std::uint32_t Blend(const std::uint32_t foreground, const std::uint32_t background, const std::uint32_t foreground_percent) noexcept {
    const auto channel = [&](const unsigned shift) {
        const auto front = (foreground >> shift) & 0xFFU;
        const auto back = (background >> shift) & 0xFFU;
        return ((front * foreground_percent + back * (100U - foreground_percent)) / 100U) << shift;
    };
    return channel(16U) | channel(8U) | channel(0U);
}

constexpr std::uint32_t ContrastText(const std::uint32_t color) noexcept {
    const auto red = (color >> 16U) & 0xFFU;
    const auto green = (color >> 8U) & 0xFFU;
    const auto blue = color & 0xFFU;
    return red * 299U + green * 587U + blue * 114U >= 150'000U ? 0x071016U : 0xF8FBFCU;
}

} // namespace

UiPalette PaletteFor(const Theme theme, const std::uint32_t accent_rgb) noexcept {
    const auto accent = accent_rgb & 0xFFFFFFU;
    if (theme == Theme::light) {
        return UiPalette{
            .background = 0xF4F7F9U,
            .header = 0xFFFFFFU,
            .surface = 0xFFFFFFU,
            .surface_alternate = 0xEEF3F6U,
            .text = 0x17242DU,
            .muted = 0x526876U,
            .accent = accent,
            .line = 0xCDD9DFU,
            .hover = 0xE4EDF2U,
            .selection = Blend(accent, 0xFFFFFFU, 24U),
            .selection_text = 0x17242DU,
            .disabled = 0x8A9AA4U,
        };
    }
    return UiPalette{
        .background = 0x071016U,
        .header = 0x0B151DU,
        .surface = 0x111B24U,
        .surface_alternate = 0x15222DU,
        .text = 0xF3F8FAU,
        .muted = 0x8EA5B3U,
        .accent = accent,
        .line = 0x233743U,
        .hover = 0x1B2C37U,
        .selection = Blend(accent, 0x111B24U, 34U),
        .selection_text = ContrastText(Blend(accent, 0x111B24U, 58U)),
        .disabled = 0x607784U,
    };
}

} // namespace hardwarescope
