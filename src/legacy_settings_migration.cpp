#include "hardwarescope/legacy_settings_migration.hpp"

#include "hardwarescope/sensor_view_model.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>

namespace hardwarescope {
namespace {

std::optional<std::string_view> ValueStart(const std::string_view json, const std::string_view key) noexcept {
    std::string token{"\""};
    token.append(key);
    token += '\"';
    const auto position = json.find(token);
    if (position == std::string_view::npos) return std::nullopt;
    const auto colon = json.find(':', position + token.size());
    if (colon == std::string_view::npos) return std::nullopt;
    auto start = colon + 1U;
    while (start < json.size() && std::isspace(static_cast<unsigned char>(json[start])) != 0) ++start;
    return json.substr(start);
}

std::optional<std::string_view> StringValue(const std::string_view json, const std::string_view key) noexcept {
    const auto tail = ValueStart(json, key);
    if (!tail || tail->empty() || tail->front() != '"') return std::nullopt;
    const auto end = tail->find('"', 1U);
    if (end == std::string_view::npos) return std::nullopt;
    return tail->substr(1U, end - 1U);
}

std::optional<bool> BooleanValue(const std::string_view json, const std::string_view key) noexcept {
    const auto tail = ValueStart(json, key);
    if (!tail) return std::nullopt;
    if (tail->starts_with("true")) return true;
    if (tail->starts_with("false")) return false;
    return std::nullopt;
}

std::optional<double> NumberValue(const std::string_view json, const std::string_view key) noexcept {
    const auto tail = ValueStart(json, key);
    if (!tail) return std::nullopt;
    std::size_t length{};
    while (length < tail->size() && (std::isdigit(static_cast<unsigned char>((*tail)[length])) != 0 || (*tail)[length] == '.' || (*tail)[length] == '-')) ++length;
    if (length == 0U) return std::nullopt;
    try {
        std::size_t consumed{};
        const auto value = std::stod(std::string{tail->substr(0U, length)}, &consumed);
        return consumed == length && std::isfinite(value) ? std::optional<double>{value} : std::nullopt;
    } catch (...) {
        return std::nullopt;
    }
}

std::uint32_t ColorValue(const std::string_view color, const std::uint32_t fallback) noexcept {
    if (color == "Teal") return 0x52E0D4U;
    if (color == "White") return 0xFFFFFFU;
    if (color == "Red") return 0xFF5252U;
    if (color == "Orange") return 0xFF9F43U;
    if (color == "Yellow") return 0xFFD93DU;
    return fallback;
}

void MigrateCollapsedSection(const std::string_view json, const std::string_view legacy_key, const SensorSection section, AppSettings& settings) noexcept {
    const auto expanded = BooleanValue(json, legacy_key);
    if (!expanded) return;
    const auto bit = 1U << static_cast<std::uint32_t>(section);
    if (*expanded) settings.collapsed_sections &= ~bit;
    else settings.collapsed_sections |= bit;
}

} // namespace

bool MigrateLegacySettingsJson(const std::string_view json, AppSettings& destination) noexcept {
    if (json.empty() || json.size() > 256U * 1024U || json.find("\"Theme\"") == std::string_view::npos) return false;
    AppSettings migrated{};
    if (const auto value = StringValue(json, "Theme")) migrated.theme = *value == "Light" ? Theme::light : Theme::dark;
    if (const auto value = StringValue(json, "TextColor")) migrated.text_color_rgb = ColorValue(*value, migrated.text_color_rgb);
    if (const auto value = NumberValue(json, "RefreshSeconds")) migrated.refresh_interval_ms = static_cast<std::uint32_t>(std::lround(*value * 1'000.0));
    if (const auto value = BooleanValue(json, "StartWithWindows")) migrated.start_with_windows = *value;
    if (const auto value = BooleanValue(json, "StartMinimized")) migrated.start_minimized = *value;
    if (const auto value = BooleanValue(json, "ShowOsd")) migrated.show_osd = *value;
    if (const auto value = BooleanValue(json, "AutomaticUpdates")) migrated.automatic_updates = *value;
    if (const auto value = StringValue(json, "OsdCorner")) {
        if (*value == "Top right") migrated.osd_position = OsdPosition::top_right;
        else if (*value == "Bottom left") migrated.osd_position = OsdPosition::bottom_left;
        else if (*value == "Bottom right") migrated.osd_position = OsdPosition::bottom_right;
    }
    if (const auto value = StringValue(json, "OsdLayout")) migrated.osd_layout = *value == "Horizontal" ? OsdLayout::horizontal : OsdLayout::vertical;
    if (const auto value = NumberValue(json, "OsdOpacity")) migrated.osd_opacity_percent = static_cast<std::uint32_t>(std::lround(*value * 100.0));
    if (const auto value = NumberValue(json, "OsdScale")) migrated.osd_scale_percent = static_cast<std::uint32_t>(std::lround(*value * 100.0));
    if (const auto value = StringValue(json, "OsdSpacing")) {
        if (*value == "Tight") migrated.osd_spacing_px = 2U;
        else if (*value == "Wide") migrated.osd_spacing_px = 20U;
        else migrated.osd_spacing_px = 8U;
    }
    if (const auto value = BooleanValue(json, "EzTemps")) migrated.easy_temperature_enabled = *value;
    if (const auto value = BooleanValue(json, "FpsCounterEnabled")) migrated.fps_enabled = *value;
    if (const auto value = BooleanValue(json, "FpsGameOnly")) migrated.fps_game_only = *value;
    if (const auto value = StringValue(json, "FpsColor")) migrated.fps_color_rgb = ColorValue(*value, migrated.fps_color_rgb);
    if (const auto value = NumberValue(json, "FpsOsdScale")) migrated.fps_scale_percent = static_cast<std::uint32_t>(std::lround(*value * 100.0));
    if (const auto value = NumberValue(json, "FpsRefreshMilliseconds")) migrated.fps_refresh_interval_ms = static_cast<std::uint32_t>(std::lround(*value));
    if (const auto value = NumberValue(json, "FpsSmoothingMilliseconds")) migrated.fps_smoothing_interval_ms = static_cast<std::uint32_t>(std::lround(*value));

    migrated.collapsed_sections = 0U;
    MigrateCollapsedSection(json, "01|CPU temperatures", SensorSection::cpu_temperatures, migrated);
    MigrateCollapsedSection(json, "02|CPU usage", SensorSection::cpu_usage, migrated);
    MigrateCollapsedSection(json, "03|CPU clock speeds", SensorSection::cpu_clock_speeds, migrated);
    MigrateCollapsedSection(json, "04|CPU power \\u0026 voltage", SensorSection::cpu_power_and_voltage, migrated);
    MigrateCollapsedSection(json, "05|Graphics", SensorSection::graphics, migrated);
    MigrateCollapsedSection(json, "06|Storage \\u0026 drives", SensorSection::storage, migrated);
    MigrateCollapsedSection(json, "07|Memory", SensorSection::memory, migrated);
    MigrateCollapsedSection(json, "08|System \\u0026 other", SensorSection::system_and_other, migrated);
    migrated.Normalize();
    destination = migrated;
    return true;
}

bool MigrateLegacySettingsFile(const std::filesystem::path& path, AppSettings& destination) noexcept {
    try {
        std::ifstream stream{path, std::ios::binary};
        if (!stream) return false;
        std::string json{std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
        return MigrateLegacySettingsJson(json, destination);
    } catch (...) {
        return false;
    }
}

} // namespace hardwarescope
