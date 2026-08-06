#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include "aoe/player_roster.hpp"

namespace aoe {

// Read-only clean-room support for Ensemble's classic AoE II asset formats.
//
// Layout assumptions are limited to the AoE/AoE II "1.00" DRS header with a
// 40-byte copyright field and SLP versions whose first four bytes begin
// "2.0". SWGB's 60-byte DRS header and later SLP/SMX/SLD variants are rejected.
// DRS extensions are stored reversed; resource IDs are signed 32-bit fields
// but this reader rejects negative IDs. SLP offsets are file-relative.
//
// Command behavior was cross-checked against:
//   openage/doc/media/drs-files.md
//   openage/doc/media/slp-files.md
//   openage/convert/value_object/read/media/{drs.py,slp.pyx}
// and against legally obtained HD graphics.drs/interfac.drs files. This code
// does not derive from or redistribute their GPL implementation or game data.
// Classic dither/alpha extended commands (0x8E, 0x9E, 0xAE) remain undefined
// by that reference and are rejected instead of approximated.
class LegacyAssetError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

struct LegacyPalette {
    std::vector<std::array<std::uint8_t, 3>> colors;

    static LegacyPalette from_jasc(std::span<const std::byte> bytes);
};

struct RgbaFrame {
    int width{};
    int height{};
    int hotspot_x{};
    int hotspot_y{};
    std::vector<std::uint8_t> rgba;
};

// Terrain elevation FilterMaps address bytes relative to the first command
// byte of a classic SLP frame. Keep that exact, unmodified byte span beside
// the ordinary RGBA rendering path.
struct IndexedSlpFrame {
    int width{};
    int height{};
    int hotspot_x{};
    int hotspot_y{};
    std::vector<std::uint8_t> source_bytes;
    std::vector<std::size_t> row_command_offsets;
    std::vector<std::uint16_t> outline_left;
    std::vector<std::uint16_t> outline_right;
};

class DrsArchive {
public:
    explicit DrsArchive(const std::filesystem::path& path);

    [[nodiscard]] bool contains(
        std::string extension,
        std::int32_t resource_id
    ) const;
    [[nodiscard]] std::vector<std::byte> read(
        std::string extension,
        std::int32_t resource_id
    ) const;
    [[nodiscard]] std::vector<std::int32_t> resource_ids(
        std::string extension
    ) const;
    [[nodiscard]] std::size_t entry_count() const noexcept;

private:
    struct Entry {
        std::uint64_t offset{};
        std::uint64_t size{};
    };

    std::filesystem::path path_;
    std::uint64_t file_size_{};
    std::map<std::pair<std::string, std::int32_t>, Entry> entries_;
};

// Resolves original WAV resources across an installation's classic sound
// archives. Later expansion archives take precedence when an exact resource
// ID is repeated. Returned bytes retain the complete RIFF/WAVE file and can
// be passed directly to an in-memory SDL audio loader.
class LegacyWavResources {
public:
    explicit LegacyWavResources(
        const std::filesystem::path& installation_root
    );

    [[nodiscard]] bool contains(std::int32_t resource_id) const noexcept;
    [[nodiscard]] std::vector<std::byte> read(
        std::int32_t resource_id
    ) const;
    [[nodiscard]] std::vector<std::int32_t> resource_ids() const;
    [[nodiscard]] std::size_t archive_count() const noexcept;

private:
    std::vector<DrsArchive> archives_;
    std::map<std::int32_t, std::size_t> resources_;
};

// Finds background music under Sound/music without mixing state-specific
// Sound/stream themes and stingers into a gameplay playlist.
[[nodiscard]] std::vector<std::filesystem::path>
discover_legacy_music_tracks(
    const std::filesystem::path& installation_root
);

[[nodiscard]] std::size_t slp_frame_count(
    std::span<const std::byte> slp
);

[[nodiscard]] RgbaFrame decode_slp_frame(
    std::span<const std::byte> slp,
    const LegacyPalette& palette,
    std::size_t frame_index,
    unsigned player = 1
);

[[nodiscard]] IndexedSlpFrame decode_indexed_slp_frame(
    std::span<const std::byte> slp,
    std::size_t frame_index
);

[[nodiscard]] RgbaFrame decode_slp_frame(
    std::span<const std::byte> slp,
    const LegacyPalette& palette,
    std::size_t frame_index,
    PlayerSlotId player
);

}  // namespace aoe
