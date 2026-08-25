#pragma once

#include <cstdint>
#include <compare>
#include <optional>
#include <string>
#include <string_view>

namespace hardwarescope {

struct SemanticVersion final {
    std::uint32_t major{};
    std::uint32_t minor{};
    std::uint32_t patch{};

    auto operator<=>(const SemanticVersion&) const = default;
};

struct UpdateManifest final {
    SemanticVersion version{};
    std::wstring installer_url;
    std::string sha256;
    std::uint64_t installer_size{};
};

[[nodiscard]] std::optional<SemanticVersion> ParseSemanticVersion(std::string_view text) noexcept;
[[nodiscard]] std::optional<UpdateManifest> ParseUpdateManifest(std::string_view json) noexcept;
[[nodiscard]] bool IsTrustedInstallerUrl(std::wstring_view url) noexcept;

} // namespace hardwarescope
