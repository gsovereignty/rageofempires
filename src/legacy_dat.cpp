#include "aoe/legacy_dat.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <limits>
#include <zlib.h>

namespace aoe {
namespace {

constexpr std::size_t maximum_count = 100000;

class Reader {
public:
    explicit Reader(std::span<const std::byte> bytes) : bytes_{bytes} {}

    void need(std::size_t length) const {
        if (position_ > bytes_.size() || length > bytes_.size() - position_) {
            throw LegacyDatError{"VER 5.7 DAT record is truncated"};
        }
    }
    void skip(std::size_t length) {
        need(length);
        position_ += length;
    }
    std::uint8_t u8() {
        need(1);
        return std::to_integer<std::uint8_t>(bytes_[position_++]);
    }
    std::uint16_t u16() {
        const auto lo = u8();
        return static_cast<std::uint16_t>(lo | (u8() << 8));
    }
    std::int16_t i16() { return static_cast<std::int16_t>(u16()); }
    std::uint32_t u32() {
        const auto lo = u16();
        return static_cast<std::uint32_t>(lo) |
            (static_cast<std::uint32_t>(u16()) << 16);
    }
    std::int32_t i32() { return static_cast<std::int32_t>(u32()); }
    std::string fixed_string(std::size_t length) {
        need(length);
        const char* start =
            reinterpret_cast<const char*>(bytes_.data() + position_);
        const auto end = std::find(start, start + length, '\0');
        std::string result{start, end};
        position_ += length;
        return result;
    }
    [[nodiscard]] std::size_t position() const noexcept { return position_; }
    [[nodiscard]] std::size_t remaining() const noexcept {
        return bytes_.size() - position_;
    }

private:
    std::span<const std::byte> bytes_;
    std::size_t position_{};
};

std::vector<std::byte> read_file(const std::filesystem::path& path) {
    std::ifstream input{path, std::ios::binary | std::ios::ate};
    if (!input) {
        throw LegacyDatError{"cannot open legacy DAT file"};
    }
    const auto length = input.tellg();
    if (length < 0) {
        throw LegacyDatError{"cannot determine legacy DAT size"};
    }
    std::vector<std::byte> bytes(static_cast<std::size_t>(length));
    input.seekg(0);
    if (!bytes.empty()) {
        input.read(
            reinterpret_cast<char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size())
        );
    }
    if (!input.good() && !input.eof()) {
        throw LegacyDatError{"cannot read legacy DAT file"};
    }
    return bytes;
}

std::vector<std::byte> inflate_raw(std::span<const std::byte> compressed) {
    z_stream stream{};
    if (inflateInit2(&stream, -MAX_WBITS) != Z_OK) {
        throw LegacyDatError{"cannot initialize raw DAT decompressor"};
    }
    struct End {
        z_stream* stream;
        ~End() { inflateEnd(stream); }
    } end{&stream};

    std::vector<std::byte> output;
    std::array<std::byte, 65536> chunk{};
    std::size_t consumed{};
    int status = Z_OK;
    while (status == Z_OK) {
        const std::size_t available = std::min<std::size_t>(
            compressed.size() - consumed,
            std::numeric_limits<uInt>::max()
        );
        stream.next_in = reinterpret_cast<Bytef*>(
            const_cast<std::byte*>(compressed.data() + consumed)
        );
        stream.avail_in = static_cast<uInt>(available);
        do {
            stream.next_out = reinterpret_cast<Bytef*>(chunk.data());
            stream.avail_out = static_cast<uInt>(chunk.size());
            status = inflate(&stream, Z_NO_FLUSH);
            if (status != Z_OK && status != Z_STREAM_END) {
                throw LegacyDatError{"invalid raw-DEFLATE DAT payload"};
            }
            output.insert(
                output.end(),
                chunk.begin(),
                chunk.begin() + static_cast<std::ptrdiff_t>(
                    chunk.size() - stream.avail_out
                )
            );
        } while (stream.avail_out == 0);
        consumed += available - stream.avail_in;
        if (available == 0 && status != Z_STREAM_END) {
            throw LegacyDatError{"truncated raw-DEFLATE DAT payload"};
        }
    }
    return output;
}

void checked_count(std::size_t count, const char* field) {
    if (count > maximum_count) {
        throw LegacyDatError{std::string{"invalid DAT "} + field};
    }
}

}  // namespace

LegacyDatFile LegacyDatFile::load(const std::filesystem::path& path) {
    const auto compressed = read_file(path);
    const auto bytes = inflate_raw(compressed);
    return from_decompressed(bytes);
}

LegacyDatFile LegacyDatFile::from_decompressed(
    std::span<const std::byte> bytes
) {
    Reader reader{bytes};
    LegacyDatFile result;
    result.version_ = reader.fixed_string(8);
    if (result.version_ != "VER 5.7") {
        throw LegacyDatError{"unsupported legacy DAT version"};
    }

    result.terrain_restriction_count_ = reader.u16();
    result.terrain_count_ = reader.u16();
    checked_count(result.terrain_restriction_count_, "restriction count");
    checked_count(result.terrain_count_, "terrain count");
    reader.skip(
        static_cast<std::size_t>(result.terrain_restriction_count_) * 8
    );
    reader.skip(
        static_cast<std::size_t>(result.terrain_restriction_count_) *
        result.terrain_count_ * 20
    );

    result.player_color_count_ = reader.u16();
    checked_count(result.player_color_count_, "player color count");
    reader.skip(static_cast<std::size_t>(result.player_color_count_) * 36);

    result.sound_count_ = reader.u16();
    checked_count(result.sound_count_, "sound count");
    result.sounds_.reserve(result.sound_count_);
    for (std::size_t sound = 0; sound < result.sound_count_; ++sound) {
        LegacySound value;
        value.id = reader.u16();
        value.play_delay = reader.i16();
        const auto item_count = reader.u16();
        checked_count(item_count, "sound item count");
        value.cache_time = reader.i32();
        value.items.reserve(item_count);
        for (std::size_t item = 0; item < item_count; ++item) {
            LegacySoundItem entry;
            entry.filename = reader.fixed_string(13);
            entry.resource_id = reader.i32();
            entry.probability = reader.i16();
            entry.civilization = reader.i16();
            entry.icon_set = reader.i16();
            value.items.push_back(std::move(entry));
        }
        result.sounds_.push_back(std::move(value));
    }

    const auto graphic_count = reader.u16();
    checked_count(graphic_count, "graphic count");
    std::vector<std::int32_t> pointers(graphic_count);
    for (auto& pointer : pointers) {
        pointer = reader.i32();
    }
    result.graphics_.resize(graphic_count);
    for (std::size_t index = 0; index < graphic_count; ++index) {
        if (pointers[index] == 0) {
            result.graphics_[index].graphic_id = -1;
            continue;
        }
        auto& graphic = result.graphics_[index];
        graphic.name = reader.fixed_string(21);
        graphic.filename = reader.fixed_string(13);
        graphic.slp_id = reader.i32();
        reader.skip(2);
        graphic.layer = reader.u8();
        graphic.player_color = reader.i16();
        reader.skip(1);
        reader.skip(8);
        const auto delta_count = reader.u16();
        checked_count(delta_count, "graphic delta count");
        reader.skip(2);
        const auto angle_sounds_used = reader.u8();
        graphic.frame_count = reader.i16();
        graphic.angle_count = reader.i16();
        reader.skip(12);
        reader.skip(1);
        graphic.graphic_id = reader.i16();
        graphic.mirroring_mode = reader.u8();
        reader.skip(1);
        graphic.deltas.reserve(delta_count);
        for (std::size_t delta = 0; delta < delta_count; ++delta) {
            LegacyGraphicDelta value;
            value.graphic_id = reader.i16();
            reader.skip(6);
            value.offset_x = reader.i16();
            value.offset_y = reader.i16();
            value.display_angle = reader.i16();
            reader.skip(2);
            graphic.deltas.push_back(value);
        }
        if (angle_sounds_used != 0) {
            reader.skip(static_cast<std::size_t>(graphic.angle_count) * 12);
        }
        if (graphic.graphic_id != static_cast<std::int16_t>(index)) {
            throw LegacyDatError{"graphic ID breaks sequential DAT alignment"};
        }
    }
    result.terrain_block_offset_ = reader.position();
    if (reader.remaining() != 0) {
        // This HD-distributed VER 5.7 file retains the AoC 11.97 layout:
        // 41 used terrains in the header, 42 stored records, 16 terrain
        // borders, and an opaque legacy map tail. The complete terrain/map
        // prefix ends at the independently validated random-map header.
        constexpr std::size_t stored_terrains = 42;
        constexpr std::size_t random_map_offset = 960744;
        if (
            result.terrain_block_offset_ > random_map_offset ||
            random_map_offset > bytes.size()
        ) {
            throw LegacyDatError{"invalid VER 5.7 terrain section"};
        }
        reader.skip(random_map_offset - result.terrain_block_offset_);
        result.stored_terrain_count_ = stored_terrains;
        result.random_map_offset_ = reader.position();
        result.random_map_count_ = reader.u32();
        const auto random_map_pointer = reader.u32();
        if (
            result.random_map_offset_ != random_map_offset ||
            result.random_map_count_ != 3 ||
            random_map_pointer != 4
        ) {
            throw LegacyDatError{
                "VER 5.7 terrain boundary does not reach random maps"
            };
        }
    }
    return result;
}

const std::string& LegacyDatFile::version() const noexcept { return version_; }
std::uint16_t LegacyDatFile::terrain_restriction_count() const noexcept {
    return terrain_restriction_count_;
}
std::uint16_t LegacyDatFile::terrain_count() const noexcept {
    return terrain_count_;
}
std::uint16_t LegacyDatFile::player_color_count() const noexcept {
    return player_color_count_;
}
std::uint16_t LegacyDatFile::sound_count() const noexcept { return sound_count_; }
const std::vector<LegacySound>& LegacyDatFile::sounds() const noexcept {
    return sounds_;
}
const LegacySound* LegacyDatFile::sound(std::size_t id) const noexcept {
    const auto found = std::find_if(
        sounds_.begin(),
        sounds_.end(),
        [id](const LegacySound& value) {
            return value.id == id;
        }
    );
    return found == sounds_.end() ? nullptr : &*found;
}
const LegacySoundItem* select_legacy_sound_item(
    const LegacySound& sound,
    std::int16_t civilization
) noexcept {
    const auto select = [&sound](std::int16_t candidate_civilization) {
        const LegacySoundItem* selected = nullptr;
        for (const LegacySoundItem& item : sound.items) {
            if (item.civilization != candidate_civilization) {
                continue;
            }
            if (selected == nullptr ||
                item.probability > selected->probability) {
                selected = &item;
            }
        }
        return selected;
    };
    if (const LegacySoundItem* exact = select(civilization)) {
        return exact;
    }
    return select(-1);
}
const std::vector<LegacyGraphic>& LegacyDatFile::graphics() const noexcept {
    return graphics_;
}
const LegacyGraphic* LegacyDatFile::graphic(std::size_t id) const noexcept {
    if (id >= graphics_.size() || graphics_[id].graphic_id < 0) {
        return nullptr;
    }
    return &graphics_[id];
}
std::size_t LegacyDatFile::terrain_block_offset() const noexcept {
    return terrain_block_offset_;
}
std::uint16_t LegacyDatFile::stored_terrain_count() const noexcept {
    return stored_terrain_count_;
}
std::size_t LegacyDatFile::random_map_offset() const noexcept {
    return random_map_offset_;
}
std::uint32_t LegacyDatFile::random_map_count() const noexcept {
    return random_map_count_;
}

}  // namespace aoe
