#pragma once

#include <compare>
#include <cstdint>
#include <optional>

namespace aoe::selection_visual {

enum class Shape : std::uint8_t {
    unknown,
    square,
    cube,
};

enum class RadiusSource : std::uint8_t {
    outline,
    collision,
};

struct Dispatch {
    Shape back{Shape::unknown};
    Shape front{Shape::unknown};
    bool hardware_path{};
    RadiusSource radii{RadiusSource::outline};

    auto operator<=>(const Dispatch&) const = default;
};

[[nodiscard]] constexpr Dispatch dispatch(
    std::int32_t renderer_mode_e8,
    std::uint8_t application_mode_78,
    bool hardware_available
) {
    if (
        hardware_available && renderer_mode_e8 != 1 &&
        (application_mode_78 == 2 || application_mode_78 == 3)
    ) {
        return {
            Shape::unknown,
            Shape::unknown,
            true,
            application_mode_78 == 3
                ? RadiusSource::collision
                : RadiusSource::outline,
        };
    }
    if (renderer_mode_e8 == 1 || application_mode_78 == 1) {
        return {Shape::cube, Shape::cube, false, RadiusSource::outline};
    }
    if (application_mode_78 == 2 || application_mode_78 == 3) {
        return {Shape::square, Shape::square, false, RadiusSource::outline};
    }
    return {};
}

[[nodiscard]] constexpr bool selected(std::uint8_t feedback_flags) {
    return (feedback_flags & 0x01U) != 0;
}

[[nodiscard]] constexpr bool selected_overlay(
    std::uint8_t feedback_flags
) {
    return selected(feedback_flags) && (feedback_flags & 0x08U) == 0;
}

[[nodiscard]] constexpr bool selected_health(
    std::uint8_t feedback_flags,
    std::uint8_t master_flags_b8
) {
    return selected_overlay(feedback_flags) &&
        (master_flags_b8 & 0x02U) == 0;
}

[[nodiscard]] constexpr std::uint8_t palette_index(
    std::uint8_t feedback_flags,
    bool alternate_mode,
    std::uint32_t milliseconds
) {
    if ((feedback_flags & 0x02U) != 0) {
        return (milliseconds & 0x100U) != 0 ? 241 : 36;
    }
    if ((feedback_flags & 0x04U) != 0) {
        return (milliseconds & 0x100U) != 0 ? 243 : 36;
    }
    if (selected(feedback_flags) && (feedback_flags & 0x08U) != 0) {
        return 243;
    }
    return alternate_mode ? 133 : 255;
}

[[nodiscard]] constexpr std::optional<std::uint8_t> group_number_frame(
    std::uint8_t group_number
) {
    if (group_number < 1 || group_number > 9) return std::nullopt;
    return static_cast<std::uint8_t>(group_number - 1);
}

[[nodiscard]] constexpr bool draw_group_numbers(
    std::uint8_t feedback_flags,
    std::uint16_t group_mask,
    std::int32_t object_player,
    std::int32_t local_player
) {
    return selected_overlay(feedback_flags) &&
        group_mask != 0 && object_player == local_player;
}

inline constexpr std::int32_t group_number_resource_id = 50403;
inline constexpr std::uint8_t group_number_frame_count = 9;
inline constexpr int group_number_advance_x = 8;

inline constexpr int square_back_segments = 2;
inline constexpr int square_front_segments = 2;
inline constexpr int cube_back_segments = 6;
inline constexpr int cube_front_segments = 18;
inline constexpr float cube_trim_near = 0.25F;
inline constexpr float cube_trim_far = 0.75F;
inline constexpr int cube_screen_y_offset = -16;

// No recovered path maps DAT selection_shape/subtype/class values to methods.
inline constexpr bool dat_shape_dispatch_proved = false;
inline constexpr bool hover_visibility_policy_proved = false;

}  // namespace aoe::selection_visual
