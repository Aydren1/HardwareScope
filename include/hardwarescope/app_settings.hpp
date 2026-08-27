#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>

namespace hardwarescope {

enum class Theme : std::uint8_t {
    dark,
    light,
    midnight,
};

enum class OsdPosition : std::uint8_t {
    top_left,
    top_right,
    bottom_left,
    bottom_right,
};

enum class OsdLayout : std::uint8_t {
    vertical,
    horizontal,
};

enum EasyTemperature : std::uint32_t {
    easy_cpu_package = 1U << 0U,
    easy_gpu_core = 1U << 1U,
    easy_gpu_memory_junction = 1U << 2U,
};

struct AppSettings final {
    static constexpr std::uint32_t kSchemaVersion = 5U;
    static constexpr std::size_t kMaximumPinnedSensors = 64U;
    static constexpr std::size_t kMaximumFavoriteSensors = 64U;
    static constexpr std::uint32_t kMatchAccentColor = 0x01000000U;

    std::uint32_t refresh_interval_ms{750U};
    Theme theme{Theme::dark};
    std::uint32_t text_color_rgb{0x52E0D4U};
    std::uint32_t cpu_temperature_color_rgb{kMatchAccentColor};
    std::uint32_t cpu_usage_color_rgb{kMatchAccentColor};
    std::uint32_t cpu_clock_color_rgb{kMatchAccentColor};
    std::uint32_t cpu_power_color_rgb{kMatchAccentColor};
    std::uint32_t graphics_color_rgb{kMatchAccentColor};
    std::uint32_t storage_color_rgb{kMatchAccentColor};
    std::uint32_t memory_color_rgb{kMatchAccentColor};
    std::uint32_t system_color_rgb{kMatchAccentColor};

    std::uint32_t interface_text_scale_percent{100U};
    bool high_contrast{};
    bool onboarding_completed{};

    bool start_with_windows{};
    bool start_minimized{};

    bool show_osd{true};
    OsdPosition osd_position{OsdPosition::top_left};
    OsdLayout osd_layout{OsdLayout::vertical};
    std::uint32_t osd_opacity_percent{100U};
    std::uint32_t osd_scale_percent{100U};
    std::uint32_t osd_spacing_px{8U};
    bool osd_group_separators{true};
    bool osd_background{};

    bool easy_temperature_enabled{true};
    std::uint32_t easy_temperature_mask{
        easy_cpu_package | easy_gpu_core | easy_gpu_memory_junction};

    bool fps_enabled{true};
    bool fps_game_only{true};
    bool fps_separate_position{};
    OsdPosition fps_osd_position{OsdPosition::top_right};
    std::uint32_t fps_refresh_interval_ms{100U};
    std::uint32_t fps_smoothing_interval_ms{500U};
    std::uint32_t fps_color_rgb{0x52E0D4U};
    std::uint32_t fps_scale_percent{100U};
    bool fps_one_percent_low_enabled{true};

    bool osd_graph_enabled{};
    std::uint64_t osd_graph_sensor_id{3U};
    std::uint32_t osd_graph_history_seconds{30U};
    std::uint32_t osd_graph_refresh_interval_ms{100U};
    std::uint32_t osd_graph_width_px{240U};
    std::uint32_t osd_graph_height_px{64U};

    bool automatic_updates{true};
    std::uint64_t update_snooze_until_unix_seconds{};
    std::uint32_t skipped_update_major{};
    std::uint32_t skipped_update_minor{};
    std::uint32_t skipped_update_patch{};
    std::uint32_t collapsed_sections{};

    bool favorites_only{true};
    bool favorites_initialized{};
    std::array<std::uint64_t, kMaximumFavoriteSensors> favorite_sensor_ids{};
    std::uint32_t favorite_sensor_count{};

    std::array<std::uint64_t, kMaximumPinnedSensors> pinned_sensor_ids{};
    std::uint32_t pinned_sensor_count{};

    void Normalize() noexcept;
    [[nodiscard]] bool PinSensor(std::uint64_t sensor_id) noexcept;
    [[nodiscard]] bool UnpinSensor(std::uint64_t sensor_id) noexcept;
    [[nodiscard]] bool IsSensorPinned(std::uint64_t sensor_id) const noexcept;
    [[nodiscard]] bool AddFavorite(std::uint64_t sensor_id) noexcept;
    [[nodiscard]] bool RemoveFavorite(std::uint64_t sensor_id) noexcept;
    [[nodiscard]] bool IsFavorite(std::uint64_t sensor_id) const noexcept;
};

class SettingsStore final {
public:
    explicit SettingsStore(std::filesystem::path path) noexcept;

    [[nodiscard]] static std::filesystem::path DefaultPath() noexcept;
    [[nodiscard]] bool Load(AppSettings& destination) const noexcept;
    [[nodiscard]] bool Save(const AppSettings& settings) const noexcept;
    [[nodiscard]] const std::filesystem::path& Path() const noexcept { return path_; }

private:
    std::filesystem::path path_;
};

} // namespace hardwarescope
