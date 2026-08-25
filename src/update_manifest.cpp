#include "hardwarescope/update_manifest.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <limits>
#include <utility>

namespace hardwarescope {
namespace {

std::optional<std::string_view> JsonString(const std::string_view json, const std::string_view key) noexcept {
    std::string token{"\""};
    token.append(key);
    token.append("\"");
    const auto key_position = json.find(token);
    if (key_position == std::string_view::npos) return std::nullopt;
    const auto colon = json.find(':', key_position + token.size());
    if (colon == std::string_view::npos) return std::nullopt;
    const auto opening = json.find('"', colon + 1U);
    if (opening == std::string_view::npos) return std::nullopt;
    const auto closing = json.find('"', opening + 1U);
    if (closing == std::string_view::npos) return std::nullopt;
    const auto value = json.substr(opening + 1U, closing - opening - 1U);
    if (value.find('\\') != std::string_view::npos) return std::nullopt;
    return value;
}

std::optional<std::uint64_t> JsonInteger(const std::string_view json, const std::string_view key) noexcept {
    std::string token{"\""};
    token.append(key);
    token.append("\"");
    const auto key_position = json.find(token);
    if (key_position == std::string_view::npos) return std::nullopt;
    const auto colon = json.find(':', key_position + token.size());
    if (colon == std::string_view::npos) return std::nullopt;
    auto start = colon + 1U;
    while (start < json.size() && std::isspace(static_cast<unsigned char>(json[start])) != 0) ++start;
    auto end = start;
    while (end < json.size() && std::isdigit(static_cast<unsigned char>(json[end])) != 0) ++end;
    if (start == end) return std::nullopt;
    std::uint64_t value{};
    const auto parsed = std::from_chars(json.data() + start, json.data() + end, value);
    if (parsed.ec != std::errc{} || parsed.ptr != json.data() + end) return std::nullopt;
    return value;
}

bool ParseComponent(const std::string_view text, std::uint32_t& value) noexcept {
    if (text.empty() || text.size() > 10U) return false;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size();
}

std::wstring WidenAscii(const std::string_view text) {
    std::wstring result;
    result.reserve(text.size());
    for (const auto character : text) result.push_back(static_cast<wchar_t>(static_cast<unsigned char>(character)));
    return result;
}

} // namespace

std::optional<SemanticVersion> ParseSemanticVersion(std::string_view text) noexcept {
    if (!text.empty() && (text.front() == 'v' || text.front() == 'V')) text.remove_prefix(1U);
    const auto first = text.find('.');
    const auto second = first == std::string_view::npos ? std::string_view::npos : text.find('.', first + 1U);
    if (first == std::string_view::npos || second == std::string_view::npos || text.find('.', second + 1U) != std::string_view::npos) return std::nullopt;
    SemanticVersion version{};
    if (!ParseComponent(text.substr(0U, first), version.major)
        || !ParseComponent(text.substr(first + 1U, second - first - 1U), version.minor)
        || !ParseComponent(text.substr(second + 1U), version.patch)) return std::nullopt;
    return version;
}

bool IsTrustedInstallerUrl(const std::wstring_view url) noexcept {
    constexpr std::wstring_view current_prefix = L"https://github.com/Cero-SC/HardwareScope/releases/download/";
    // HardwareScope 2.0.4 and earlier only trust this exact pre-rename path.
    // Keep accepting it so the public manifest can bridge those installations
    // to versions that know the current repository owner.
    constexpr std::wstring_view legacy_prefix = L"https://github.com/Aydren1/HardwareScope/releases/download/";
    const bool trusted_repository = url.starts_with(current_prefix) || url.starts_with(legacy_prefix);
    if (!trusted_repository || url.find(L"..") != std::wstring_view::npos) return false;
    return url.ends_with(L".exe");
}

std::optional<UpdateManifest> ParseUpdateManifest(const std::string_view json) noexcept {
    if (json.size() > 64U * 1024U) return std::nullopt;
    const auto version_text = JsonString(json, "version");
    const auto channel = JsonString(json, "channel");
    const auto installer = JsonString(json, "installer");
    const auto sha256 = JsonString(json, "sha256");
    const auto size = JsonInteger(json, "size");
    if (!version_text || !channel || !installer || !sha256 || !size || *channel != "stable") return std::nullopt;
    const auto version = ParseSemanticVersion(*version_text);
    if (!version || *size == 0U || *size > 512ULL * 1024ULL * 1024ULL || sha256->size() != 64U) return std::nullopt;
    if (!std::all_of(sha256->begin(), sha256->end(), [](const char character) { return std::isxdigit(static_cast<unsigned char>(character)) != 0; })) return std::nullopt;
    auto url = WidenAscii(*installer);
    if (!IsTrustedInstallerUrl(url)) return std::nullopt;
    std::string normalized_hash{*sha256};
    std::transform(normalized_hash.begin(), normalized_hash.end(), normalized_hash.begin(), [](const char character) { return static_cast<char>(std::toupper(static_cast<unsigned char>(character))); });
    return UpdateManifest{*version, std::move(url), std::move(normalized_hash), *size};
}

} // namespace hardwarescope
