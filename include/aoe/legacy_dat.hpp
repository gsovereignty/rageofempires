#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace aoe {

class LegacyDatError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

struct LegacyGraphicDelta {
    std::int16_t graphic_id{};
    std::int16_t offset_x{};
    std::int16_t offset_y{};
    std::int16_t display_angle{};
};

struct LegacyGraphicSound {
    std::int16_t frame{};
    std::int16_t sound_id{};
};

struct LegacyGraphic {
    std::string name;
    std::string filename;
    std::int32_t slp_id{};
    std::uint8_t layer{};
    std::int16_t player_color{};
    std::int16_t frame_count{};
    std::int16_t angle_count{};
    float speed_adjust{};
    float frame_rate{};
    float replay_delay{};
    std::uint8_t sequence_type{};
    std::int16_t graphic_id{};
    std::uint8_t mirroring_mode{};
    std::vector<LegacyGraphicDelta> deltas;
    std::vector<std::array<LegacyGraphicSound, 3>> angle_sounds;
};

struct LegacySoundItem {
    std::string filename;
    std::int32_t resource_id{};
    std::int16_t probability{};
    std::int16_t civilization{};
    std::int16_t icon_set{};
};

struct LegacySound {
    std::uint16_t id{};
    std::int16_t play_delay{};
    std::int32_t cache_time{};
    std::vector<LegacySoundItem> items;
};

// Exact civilization records take precedence over generic (-1) records.
// roll is mapped across positive DAT probability weights.
[[nodiscard]] const LegacySoundItem* select_legacy_sound_item(
    const LegacySound& sound,
    std::int16_t civilization,
    std::uint32_t roll
) noexcept;

// Read-only parser for the stable prefix of HD Edition's raw-DEFLATE
// empires2_x1_p1.dat version "VER 5.7". Pointer-array values are presence
// flags; present graphic records are stored sequentially.
class LegacyDatFile {
public:
    static LegacyDatFile load(const std::filesystem::path& path);
    static LegacyDatFile from_decompressed(std::span<const std::byte> bytes);

    [[nodiscard]] const std::string& version() const noexcept;
    [[nodiscard]] std::uint16_t terrain_restriction_count() const noexcept;
    [[nodiscard]] std::uint16_t terrain_count() const noexcept;
    [[nodiscard]] std::uint16_t player_color_count() const noexcept;
    [[nodiscard]] std::uint16_t sound_count() const noexcept;
    [[nodiscard]] const std::vector<LegacySound>& sounds() const noexcept;
    [[nodiscard]] const LegacySound* sound(std::size_t id) const noexcept;
    [[nodiscard]] const std::vector<LegacyGraphic>& graphics() const noexcept;
    [[nodiscard]] const LegacyGraphic* graphic(std::size_t id) const noexcept;
    [[nodiscard]] std::size_t terrain_block_offset() const noexcept;
    [[nodiscard]] std::uint16_t stored_terrain_count() const noexcept;
    [[nodiscard]] std::size_t random_map_offset() const noexcept;
    [[nodiscard]] std::uint32_t random_map_count() const noexcept;

private:
    std::string version_;
    std::uint16_t terrain_restriction_count_{};
    std::uint16_t terrain_count_{};
    std::uint16_t player_color_count_{};
    std::uint16_t sound_count_{};
    std::vector<LegacySound> sounds_;
    std::vector<LegacyGraphic> graphics_;
    std::size_t terrain_block_offset_{};
    std::uint16_t stored_terrain_count_{};
    std::size_t random_map_offset_{};
    std::uint32_t random_map_count_{};
};

}  // namespace aoe
