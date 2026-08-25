#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>

namespace hardwarescope {

enum class Theme : std::uint8_t {
    dark,
    light,
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
    static constexpr std::uint32_t kSchemaVersion = 2U;
    static constexpr std::size_t kMaximumPinnedSensors = 64U;

    std::uint32_t refresh_interval_ms{750U};
    Theme theme{Theme::dark};
    std::uint32_t text_color_rgb{0x52E0D4U};

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
    std::uint32_t fps_refresh_interval_ms{100U};
    std::uint32_t fps_smoothing_interval_ms{500U};
    std::uint32_t fps_color_rgb{0x52E0D4U};
    std::uint32_t fps_scale_percent{100U};

    bool automatic_updates{true};
    std::uint32_t collapsed_sections{};

    std::array<std::uint64_t, kMaximumPinnedSensors> pinned_sensor_ids{};
    std::uint32_t pinned_sensor_count{};

    void Normalize() noexcept;
    [[nodiscard]] bool PinSensor(std::uint64_t sensor_id) noexcept;
    [[nodiscard]] bool UnpinSensor(std::uint64_t sensor_id) noexcept;
    [[nodiscard]] bool IsSensorPinned(std::uint64_t sensor_id) const noexcept;
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
