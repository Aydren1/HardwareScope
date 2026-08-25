#include "hardwarescope/app_settings.hpp"

#include <windows.h>
#include <shlobj.h>

#include <algorithm>
#include <charconv>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>

namespace hardwarescope {
namespace {

using Values = std::unordered_map<std::string, std::string>;

std::string_view Trim(std::string_view value) noexcept {
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t' || value.front() == '\r')) value.remove_prefix(1U);
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r')) value.remove_suffix(1U);
    return value;
}

Values Parse(const std::string& text) {
    Values values;
    std::size_t start = 0;
    while (start <= text.size()) {
        const auto end = text.find('\n', start);
        const auto line = Trim(std::string_view{text}.substr(start, end == std::string::npos ? text.size() - start : end - start));
        if (!line.empty() && line.front() != '#' && line.front() != ';') {
            const auto separator = line.find('=');
            if (separator != std::string_view::npos) {
                const auto key = Trim(line.substr(0, separator));
                const auto value = Trim(line.substr(separator + 1U));
                if (!key.empty()) values.insert_or_assign(std::string{key}, std::string{value});
            }
        }
        if (end == std::string::npos) break;
        start = end + 1U;
    }
    return values;
}

template <typename T>
T IntegerValue(const Values& values, const char* key, const T fallback, const int base = 10) noexcept {
    const auto iterator = values.find(key);
    if (iterator == values.end()) return fallback;
    T result{};
    const auto& text = iterator->second;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), result, base);
    return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() ? result : fallback;
}

bool BooleanValue(const Values& values, const char* key, const bool fallback) noexcept {
    const auto iterator = values.find(key);
    if (iterator == values.end()) return fallback;
    if (iterator->second == "1" || iterator->second == "true") return true;
    if (iterator->second == "0" || iterator->second == "false") return false;
    return fallback;
}

template <typename Enum>
Enum EnumValue(const Values& values, const char* key, const Enum fallback, const std::uint32_t maximum) noexcept {
    const auto raw = IntegerValue<std::uint32_t>(values, key, static_cast<std::uint32_t>(fallback));
    return raw <= maximum ? static_cast<Enum>(raw) : fallback;
}

std::array<std::uint64_t, AppSettings::kMaximumPinnedSensors> ParsePinnedSensors(
    const Values& values,
    std::uint32_t& count) noexcept {
    std::array<std::uint64_t, AppSettings::kMaximumPinnedSensors> result{};
    count = 0;
    const auto iterator = values.find("pinned_sensor_ids");
    if (iterator == values.end()) return result;

    std::string_view text{iterator->second};
    while (!text.empty() && count < result.size()) {
        const auto separator = text.find(',');
        const auto token = Trim(text.substr(0, separator));
        std::uint64_t id{};
        const auto parsed = std::from_chars(token.data(), token.data() + token.size(), id, 10);
        if (id != 0U && parsed.ec == std::errc{} && parsed.ptr == token.data() + token.size()) {
            if (std::find(result.begin(), result.begin() + count, id) == result.begin() + count) {
                result[count++] = id;
            }
        }
        if (separator == std::string_view::npos) break;
        text.remove_prefix(separator + 1U);
    }
    return result;
}

void WriteBoolean(std::ostream& stream, const char* key, const bool value) {
    stream << key << '=' << (value ? 1 : 0) << '\n';
}

} // namespace

void AppSettings::Normalize() noexcept {
    refresh_interval_ms = std::clamp(refresh_interval_ms, 100U, 10'000U);
    text_color_rgb &= 0xFFFFFFU;
    osd_opacity_percent = std::clamp(osd_opacity_percent, 10U, 100U);
    osd_scale_percent = std::clamp(osd_scale_percent, 50U, 250U);
    osd_spacing_px = std::min(osd_spacing_px, 64U);
    easy_temperature_mask &= easy_cpu_package | easy_gpu_core | easy_gpu_memory_junction;
    fps_color_rgb &= 0xFFFFFFU;
    fps_scale_percent = std::clamp(fps_scale_percent, 50U, 300U);
    fps_refresh_interval_ms = std::clamp(fps_refresh_interval_ms, 50U, 500U);
    fps_smoothing_interval_ms = std::clamp(fps_smoothing_interval_ms, 250U, 1'250U);
    pinned_sensor_count = std::min<std::uint32_t>(pinned_sensor_count, static_cast<std::uint32_t>(pinned_sensor_ids.size()));
    collapsed_sections &= 0x01FFU;
}

bool AppSettings::PinSensor(const std::uint64_t sensor_id) noexcept {
    if (sensor_id == 0U || IsSensorPinned(sensor_id) || pinned_sensor_count >= pinned_sensor_ids.size()) return false;
    pinned_sensor_ids[pinned_sensor_count++] = sensor_id;
    return true;
}

bool AppSettings::UnpinSensor(const std::uint64_t sensor_id) noexcept {
    const auto end = pinned_sensor_ids.begin() + pinned_sensor_count;
    const auto found = std::find(pinned_sensor_ids.begin(), end, sensor_id);
    if (found == end) return false;
    std::move(found + 1, end, found);
    --pinned_sensor_count;
    pinned_sensor_ids[pinned_sensor_count] = 0U;
    return true;
}

bool AppSettings::IsSensorPinned(const std::uint64_t sensor_id) const noexcept {
    return std::find(pinned_sensor_ids.begin(), pinned_sensor_ids.begin() + pinned_sensor_count, sensor_id)
        != pinned_sensor_ids.begin() + pinned_sensor_count;
}

SettingsStore::SettingsStore(std::filesystem::path path) noexcept : path_(std::move(path)) {}

std::filesystem::path SettingsStore::DefaultPath() noexcept {
    PWSTR local_app_data = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT, nullptr, &local_app_data))) return {};
    std::filesystem::path result{local_app_data};
    CoTaskMemFree(local_app_data);
    result /= L"HardwareScope";
    result /= L"settings-v2.ini";
    return result;
}

bool SettingsStore::Load(AppSettings& destination) const noexcept {
    try {
        std::ifstream stream(path_, std::ios::binary);
        if (!stream) return false;
        const std::string text{std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
        const auto values = Parse(text);

        AppSettings loaded{};
        loaded.refresh_interval_ms = IntegerValue(values, "refresh_interval_ms", loaded.refresh_interval_ms);
        loaded.theme = EnumValue(values, "theme", loaded.theme, 1U);
        loaded.text_color_rgb = IntegerValue(values, "text_color_rgb", loaded.text_color_rgb, 16);
        loaded.start_with_windows = BooleanValue(values, "start_with_windows", loaded.start_with_windows);
        loaded.start_minimized = BooleanValue(values, "start_minimized", loaded.start_minimized);
        loaded.minimize_to_tray = BooleanValue(values, "minimize_to_tray", loaded.minimize_to_tray);
        loaded.hide_taskbar_when_minimized = BooleanValue(values, "hide_taskbar_when_minimized", loaded.hide_taskbar_when_minimized);
        loaded.show_osd = BooleanValue(values, "show_osd", loaded.show_osd);
        loaded.osd_position = EnumValue(values, "osd_position", loaded.osd_position, 3U);
        loaded.osd_layout = EnumValue(values, "osd_layout", loaded.osd_layout, 1U);
        loaded.osd_opacity_percent = IntegerValue(values, "osd_opacity_percent", loaded.osd_opacity_percent);
        loaded.osd_scale_percent = IntegerValue(values, "osd_scale_percent", loaded.osd_scale_percent);
        loaded.osd_spacing_px = IntegerValue(values, "osd_spacing_px", loaded.osd_spacing_px);
        loaded.osd_group_separators = BooleanValue(values, "osd_group_separators", loaded.osd_group_separators);
        loaded.osd_background = BooleanValue(values, "osd_background", loaded.osd_background);
        loaded.easy_temperature_enabled = BooleanValue(values, "easy_temperature_enabled", loaded.easy_temperature_enabled);
        loaded.easy_temperature_mask = IntegerValue(values, "easy_temperature_mask", loaded.easy_temperature_mask);
        loaded.fps_enabled = BooleanValue(values, "fps_enabled", loaded.fps_enabled);
        loaded.fps_game_only = BooleanValue(values, "fps_game_only", loaded.fps_game_only);
        loaded.fps_refresh_interval_ms = IntegerValue(values, "fps_refresh_interval_ms", loaded.fps_refresh_interval_ms);
        loaded.fps_smoothing_interval_ms = IntegerValue(values, "fps_smoothing_interval_ms", loaded.fps_smoothing_interval_ms);
        loaded.fps_color_rgb = IntegerValue(values, "fps_color_rgb", loaded.fps_color_rgb, 16);
        loaded.fps_scale_percent = IntegerValue(values, "fps_scale_percent", loaded.fps_scale_percent);
        loaded.automatic_updates = BooleanValue(values, "automatic_updates", loaded.automatic_updates);
        loaded.collapsed_sections = IntegerValue(values, "collapsed_sections", loaded.collapsed_sections);
        loaded.pinned_sensor_ids = ParsePinnedSensors(values, loaded.pinned_sensor_count);
        loaded.Normalize();
        destination = loaded;
        return true;
    } catch (...) {
        return false;
    }
}

bool SettingsStore::Save(const AppSettings& settings) const noexcept {
    try {
        auto normalized = settings;
        normalized.Normalize();
        const auto parent = path_.parent_path();
        if (!parent.empty()) std::filesystem::create_directories(parent);
        auto temporary = path_;
        temporary += L".tmp";

        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        if (!stream) return false;
        stream << "# HardwareScope 2.0 native settings\n";
        stream << "schema_version=" << AppSettings::kSchemaVersion << '\n';
        stream << "refresh_interval_ms=" << normalized.refresh_interval_ms << '\n';
        stream << "theme=" << static_cast<unsigned>(normalized.theme) << '\n';
        stream << "text_color_rgb=" << std::uppercase << std::hex << std::setw(6) << std::setfill('0') << normalized.text_color_rgb << std::dec << '\n';
        WriteBoolean(stream, "start_with_windows", normalized.start_with_windows);
        WriteBoolean(stream, "start_minimized", normalized.start_minimized);
        WriteBoolean(stream, "minimize_to_tray", normalized.minimize_to_tray);
        WriteBoolean(stream, "hide_taskbar_when_minimized", normalized.hide_taskbar_when_minimized);
        WriteBoolean(stream, "show_osd", normalized.show_osd);
        stream << "osd_position=" << static_cast<unsigned>(normalized.osd_position) << '\n';
        stream << "osd_layout=" << static_cast<unsigned>(normalized.osd_layout) << '\n';
        stream << "osd_opacity_percent=" << normalized.osd_opacity_percent << '\n';
        stream << "osd_scale_percent=" << normalized.osd_scale_percent << '\n';
        stream << "osd_spacing_px=" << normalized.osd_spacing_px << '\n';
        WriteBoolean(stream, "osd_group_separators", normalized.osd_group_separators);
        WriteBoolean(stream, "osd_background", normalized.osd_background);
        WriteBoolean(stream, "easy_temperature_enabled", normalized.easy_temperature_enabled);
        stream << "easy_temperature_mask=" << normalized.easy_temperature_mask << '\n';
        WriteBoolean(stream, "fps_enabled", normalized.fps_enabled);
        WriteBoolean(stream, "fps_game_only", normalized.fps_game_only);
        stream << "fps_refresh_interval_ms=" << normalized.fps_refresh_interval_ms << '\n';
        stream << "fps_smoothing_interval_ms=" << normalized.fps_smoothing_interval_ms << '\n';
        stream << "fps_color_rgb=" << std::uppercase << std::hex << std::setw(6) << std::setfill('0') << normalized.fps_color_rgb << std::dec << '\n';
        stream << "fps_scale_percent=" << normalized.fps_scale_percent << '\n';
        WriteBoolean(stream, "automatic_updates", normalized.automatic_updates);
        stream << "collapsed_sections=" << normalized.collapsed_sections << '\n';
        stream << "pinned_sensor_ids=";
        for (std::uint32_t index = 0; index < normalized.pinned_sensor_count; ++index) {
            if (index != 0U) stream << ',';
            stream << normalized.pinned_sensor_ids[index];
        }
        stream << '\n';
        stream.flush();
        if (!stream) return false;
        stream.close();

        if (!MoveFileExW(temporary.c_str(), path_.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            static_cast<void>(DeleteFileW(temporary.c_str()));
            return false;
        }
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace hardwarescope
