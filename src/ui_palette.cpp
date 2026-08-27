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

UiPalette PaletteFor(const Theme theme, const std::uint32_t accent_rgb, const bool high_contrast) noexcept {
    const auto accent = accent_rgb & 0xFFFFFFU;
    if (theme == Theme::light) {
        if (high_contrast) {
            return UiPalette{
                .background = 0xFFFFFFU,
                .header = 0xFFFFFFU,
                .surface = 0xFFFFFFU,
                .surface_alternate = 0xE8EEF2U,
                .text = 0x000000U,
                .muted = 0x26343DU,
                .accent = accent,
                .line = 0x687B86U,
                .hover = 0xDCE7EDU,
                .selection = Blend(accent, 0xFFFFFFU, 38U),
                .selection_text = 0x000000U,
                .disabled = 0x52616AU,
            };
        }
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
    if (theme == Theme::midnight) {
        if (high_contrast) {
            return UiPalette{
                .background = 0x000000U,
                .header = 0x000000U,
                .surface = 0x050505U,
                .surface_alternate = 0x111111U,
                .text = 0xFFFFFFU,
                .muted = 0xD8DEE2U,
                .accent = accent,
                .line = 0x70787DU,
                .hover = 0x202020U,
                .selection = Blend(accent, 0x000000U, 58U),
                .selection_text = ContrastText(Blend(accent, 0x000000U, 72U)),
                .disabled = 0xA3AAAEU,
            };
        }
        return UiPalette{
            .background = 0x000000U,
            .header = 0x030303U,
            .surface = 0x080808U,
            .surface_alternate = 0x0D0D0DU,
            .text = 0xF7F9FAU,
            .muted = 0x9AA2A8U,
            .accent = accent,
            .line = 0x242424U,
            .hover = 0x161616U,
            .selection = Blend(accent, 0x080808U, 38U),
            .selection_text = ContrastText(Blend(accent, 0x080808U, 62U)),
            .disabled = 0x62686CU,
        };
    }
    if (high_contrast) {
        return UiPalette{
            .background = 0x02090DU,
            .header = 0x061017U,
            .surface = 0x0A151DU,
            .surface_alternate = 0x122531U,
            .text = 0xFFFFFFU,
            .muted = 0xD4E2E9U,
            .accent = accent,
            .line = 0x58717EU,
            .hover = 0x1A3542U,
            .selection = Blend(accent, 0x081219U, 58U),
            .selection_text = ContrastText(Blend(accent, 0x081219U, 72U)),
            .disabled = 0x9FB1BAU,
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
