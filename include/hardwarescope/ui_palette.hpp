#pragma once

#include "hardwarescope/app_settings.hpp"

#include <cstdint>

namespace hardwarescope {

struct UiPalette final {
    std::uint32_t background{};
    std::uint32_t header{};
    std::uint32_t surface{};
    std::uint32_t surface_alternate{};
    std::uint32_t text{};
    std::uint32_t muted{};
    std::uint32_t accent{};
    std::uint32_t line{};
    std::uint32_t hover{};
    std::uint32_t selection{};
    std::uint32_t selection_text{};
    std::uint32_t disabled{};
};

[[nodiscard]] UiPalette PaletteFor(Theme theme, std::uint32_t accent_rgb, bool high_contrast = false) noexcept;

} // namespace hardwarescope
