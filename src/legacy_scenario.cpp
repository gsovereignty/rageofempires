#include "aoe/legacy_scenario.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <span>
#include <vector>

#include <zlib.h>

namespace aoe {
namespace {

constexpr std::size_t max_file_size = 128U * 1024U * 1024U;
constexpr std::size_t max_header_size = 1U * 1024U * 1024U;
constexpr std::size_t max_description_size = 256U * 1024U;
constexpr std::size_t max_inflated_size = 256U * 1024U * 1024U;

std::uint32_t read_u32(
    std::span<const std::byte> bytes,
    std::size_t offset
) {
    if (offset > bytes.size() || bytes.size() - offset < 4) {
        throw std::runtime_error("truncated 32-bit field");
    }
    const auto* data = reinterpret_cast<const unsigned char*>(
        bytes.data() + offset
    );
    return static_cast<std::uint32_t>(data[0]) |
        (static_cast<std::uint32_t>(data[1]) << 8U) |
        (static_cast<std::uint32_t>(data[2]) << 16U) |
        (static_cast<std::uint32_t>(data[3]) << 24U);
}

std::vector<std::byte> read_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) throw std::runtime_error("cannot open scenario file");
    const auto end = input.tellg();
    if (end < 0 || static_cast<std::uint64_t>(end) > max_file_size) {
        throw std::runtime_error("scenario file exceeds 128 MiB limit");
    }
    std::vector<std::byte> bytes(static_cast<std::size_t>(end));
    input.seekg(0);
    if (!bytes.empty()) {
        input.read(
            reinterpret_cast<char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size())
        );
    }
    if (!input) throw std::runtime_error("cannot read complete scenario file");
    return bytes;
}

std::vector<std::byte> inflate_raw(std::span<const std::byte> compressed) {
    if (compressed.empty() ||
        compressed.size() > std::numeric_limits<uInt>::max()) {
        throw std::runtime_error("missing or oversized DEFLATE body");
    }
    z_stream stream{};
    stream.next_in = reinterpret_cast<Bytef*>(
        const_cast<std::byte*>(compressed.data())
    );
    stream.avail_in = static_cast<uInt>(compressed.size());
    if (inflateInit2(&stream, -MAX_WBITS) != Z_OK) {
        throw std::runtime_error("cannot initialize DEFLATE decoder");
    }
    struct End {
        z_stream* stream;
        ~End() { inflateEnd(stream); }
    } end{&stream};

    std::vector<std::byte> output;
    std::array<std::byte, 64U * 1024U> chunk{};
    int status = Z_OK;
    while (status == Z_OK) {
        stream.next_out = reinterpret_cast<Bytef*>(chunk.data());
        stream.avail_out = static_cast<uInt>(chunk.size());
        status = inflate(&stream, Z_NO_FLUSH);
        const auto produced = chunk.size() - stream.avail_out;
        if (output.size() > max_inflated_size - produced) {
            throw std::runtime_error("inflated scenario exceeds 256 MiB limit");
        }
        output.insert(output.end(), chunk.begin(), chunk.begin() + produced);
    }
    if (status != Z_STREAM_END) {
        throw std::runtime_error("truncated or invalid raw-DEFLATE body");
    }
    if (stream.avail_in != 0) {
        throw std::runtime_error("trailing bytes after raw-DEFLATE body");
    }
    return output;
}

bool supported_classic_version(const std::string& version) {
    return version == "1.07" || version == "1.09" ||
        version == "1.10" || version == "1.11" ||
        version == "1.12" || version == "1.13" ||
        version == "1.14" || version == "1.15" ||
        version == "1.16" || version == "1.18" ||
        version == "1.19" || version == "1.20" ||
        version == "1.21";
}

class Cursor {
public:
    explicit Cursor(std::span<const std::byte> bytes) : bytes_(bytes) {}

    std::size_t offset() const { return offset_; }
    std::uint8_t u8() {
        require(1);
        return std::to_integer<std::uint8_t>(bytes_[offset_++]);
    }
    std::uint16_t u16() {
        const auto low = u8();
        return static_cast<std::uint16_t>(
            low | (static_cast<std::uint16_t>(u8()) << 8U)
        );
    }
    std::int16_t i16() { return static_cast<std::int16_t>(u16()); }
    std::uint32_t u32() {
        require(4);
        const auto value = read_u32(bytes_, offset_);
        offset_ += 4;
        return value;
    }
    std::int32_t i32() { return static_cast<std::int32_t>(u32()); }
    float f32() {
        const auto bits = u32();
        float value{};
        std::memcpy(&value, &bits, sizeof(value));
        if (!std::isfinite(value)) {
            throw std::runtime_error("non-finite scenario float");
        }
        return value;
    }
    double f64() {
        const std::uint64_t low = u32();
        const std::uint64_t high = u32();
        const std::uint64_t bits = low | (high << 32U);
        double value{};
        std::memcpy(&value, &bits, sizeof(value));
        if (!std::isfinite(value)) {
            throw std::runtime_error("non-finite scenario double");
        }
        return value;
    }
    void skip(std::size_t count) {
        require(count);
        offset_ += count;
    }
    void string16() {
        const auto size = u16();
        skip(size);
    }
    std::string string32() {
        const auto size = u32();
        if (size == 0) return {};
        if (size > max_description_size) {
            throw std::runtime_error("oversized trigger string");
        }
        require(size);
        const auto* data = reinterpret_cast<const char*>(
            bytes_.data() + offset_
        );
        if (data[size - 1] != '\0') {
            throw std::runtime_error("trigger string lacks terminator");
        }
        std::string value(data, data + size - 1);
        if (value.find('\0') != std::string::npos) {
            throw std::runtime_error("trigger string has embedded NUL");
        }
        offset_ += size;
        return value;
    }

private:
    void require(std::size_t count) const {
        if (offset_ > bytes_.size() || count > bytes_.size() - offset_) {
            throw std::runtime_error("truncated decompressed scenario section");
        }
    }

    std::span<const std::byte> bytes_;
    std::size_t offset_{};
};

void skip_bitmap(Cursor& cursor) {
    cursor.u32();
    const auto width = cursor.u32();
    const auto height = cursor.u32();
    cursor.u16();
    if (width == 0 || height == 0) return;
    if (width > 4096 || height > 4096) {
        throw std::runtime_error("oversized embedded scenario bitmap");
    }
    cursor.skip(40 + 256 * 4);
    const std::uint64_t pixels =
        static_cast<std::uint64_t>(height) * ((width + 3U) & ~3U);
    if (pixels > max_inflated_size) {
        throw std::runtime_error("oversized embedded scenario bitmap pixels");
    }
    cursor.skip(static_cast<std::size_t>(pixels));
}

float navigate_tribe_scen(
    Cursor& cursor,
    LegacyScenarioMetadata& metadata
) {
    const float version = cursor.f32();
    if (version < 1.0F || version > 1.22F) {
        throw std::runtime_error("unsupported decompressed data version");
    }
    metadata.players.resize(16);
    if (version > 1.13F) cursor.skip(16 * 256);
    if (version > 1.16F) cursor.skip(16 * 4);
    if (version > 1.13F) {
        for (auto& player : metadata.players) {
            player.active = cursor.i32();
            player.player_type = cursor.i32();
            player.civilization_id = cursor.i32();
            player.posture = cursor.i32();
        }
    }
    if (version >= 1.07F) cursor.u8();
    cursor.skip(8);  // RGE timeline
    cursor.string16();
    if (version >= 1.16F) cursor.skip(5 * 4);
    if (version >= 1.22F) cursor.skip(4);
    cursor.string16();
    if (version >= 1.11F) {
        for (int index = 0; index < 4; ++index) cursor.string16();
    }
    if (version >= 1.22F) cursor.string16();
    for (int index = 0; index < 3; ++index) cursor.string16();
    if (version >= 1.09F) cursor.string16();
    if (version >= 1.10F) skip_bitmap(cursor);
    for (int index = 0; index < 32; ++index) cursor.string16();
    if (version >= 1.08F) {
        for (int index = 0; index < 16; ++index) cursor.string16();
    }
    for (int player = 0; player < 16; ++player) {
        const auto build = cursor.u32();
        const auto city = cursor.u32();
        const auto ai = version >= 1.08F ? cursor.u32() : 0;
        const std::uint64_t total = static_cast<std::uint64_t>(build) +
            city + ai;
        if (total > max_inflated_size) {
            throw std::runtime_error("oversized embedded player files");
        }
        cursor.skip(static_cast<std::size_t>(total));
    }
    if (version >= 1.20F) cursor.skip(16);
    if (version >= 1.02F && cursor.i32() != -99) {
        throw std::runtime_error("missing RGE scenario separator");
    }

    if (version <= 1.13F) {
        cursor.skip(16 * 256);
        for (auto& player : metadata.players) {
            player.active = cursor.i32();
            player.gold = cursor.i32();
            player.wood = cursor.i32();
            player.food = cursor.i32();
            player.stone = cursor.i32();
            player.player_type = cursor.i32();
            player.civilization_id = cursor.i32();
            player.posture = cursor.i32();
        }
    } else {
        for (auto& player : metadata.players) {
            player.gold = cursor.i32();
            player.wood = cursor.i32();
            player.food = cursor.i32();
            player.stone = cursor.i32();
            if (version >= 1.17F) {
                player.ore = cursor.i32();
                player.goods = cursor.i32();
            }
        }
    }
    if (version >= 1.02F && cursor.i32() != -99) {
        throw std::runtime_error("missing Tribe scenario separator");
    }
    cursor.skip(6 * 4);  // victory info
    cursor.skip(4);  // victory-all flag
    if (version >= 1.13F) cursor.skip(3 * 4);
    for (auto& player : metadata.players) {
        player.diplomacy.reserve(16);
        for (int target = 0; target < 16; ++target) {
            const auto stance = cursor.i32();
            if (stance != 0 && stance != 1 && stance != 3) {
                throw std::runtime_error("invalid classic diplomacy stance");
            }
            player.diplomacy.push_back(stance);
        }
    }
    cursor.skip(16 * 12 * 60);  // legacy victory records
    if (version >= 1.02F && cursor.i32() != -99) {
        throw std::runtime_error("missing victory separator");
    }
    cursor.skip(16 * 4);  // allied-victory flags

    if (version >= 1.18F) {
        cursor.skip(16 * 4 + 16 * 30 * 4);  // tech counts + IDs
        cursor.skip(16 * 4 + 16 * 30 * 4);  // unit counts + IDs
        cursor.skip(16 * 4 + 16 * 20 * 4);  // building counts + IDs
    } else if (version > 1.03F) {
        cursor.skip(16 * 20 * 4);
    }
    if (version > 1.04F) cursor.skip(4);
    if (version >= 1.12F) cursor.skip(8);
    if (version > 1.05F) {
        for (auto& player : metadata.players) {
            player.starting_age_id = cursor.i32();
        }
    }
    if (version >= 1.02F && cursor.i32() != -99) {
        throw std::runtime_error("missing options separator");
    }
    if (version >= 1.19F) cursor.skip(8);
    if (version >= 1.21F) cursor.skip(4);
    metadata.player_settings_decoded = true;
    return version;
}

void decode_map(Cursor& cursor, LegacyScenarioMetadata& metadata) {
    const auto width = cursor.u32();
    const auto height = cursor.u32();
    if (width == 0 || height == 0 || width > 500 || height > 500) {
        throw std::runtime_error("invalid classic scenario map dimensions");
    }
    const std::uint64_t count =
        static_cast<std::uint64_t>(width) * height;
    metadata.map_width = width;
    metadata.map_height = height;
    metadata.map_tiles.reserve(static_cast<std::size_t>(count));
    for (std::uint64_t index = 0; index < count; ++index) {
        LegacyScenarioMetadata::Tile tile;
        tile.terrain = cursor.u8();
        tile.elevation = static_cast<std::int8_t>(cursor.u8());
        tile.zone = static_cast<std::int8_t>(cursor.u8());
        metadata.map_tiles.push_back(tile);
    }
    metadata.map_decoded = true;
}

float player_data_version(const std::string& format) {
    if (format == "1.07") return 1.07F;
    if (format == "1.09" || format == "1.10" || format == "1.11") {
        return 1.11F;
    }
    if (format >= "1.12" && format <= "1.16") return 1.12F;
    if (format == "1.18" || format == "1.19") return 1.13F;
    return 1.14F;
}

void skip_victory_conditions(Cursor& cursor, bool has_version) {
    const float version = has_version ? cursor.f32() : 0.0F;
    const auto count = cursor.i32();
    if (count < 0 || count > 4096) {
        throw std::runtime_error("invalid victory-condition count");
    }
    if (cursor.u8() > 2) throw std::runtime_error("invalid victory state");
    cursor.skip(static_cast<std::size_t>(count) * 44);
    if (version >= 1.0F) {
        cursor.skip(4);
        const auto points = cursor.i32();
        if (points < 0 || points > 4096) {
            throw std::runtime_error("invalid victory-point count");
        }
        if (version >= 2.0F) cursor.skip(8);
        cursor.skip(
            static_cast<std::size_t>(points) *
            (version >= 2.0F ? 32 : 24)
        );
    }
}

void skip_scenario_player(Cursor& cursor, float version) {
    cursor.string16();
    cursor.skip(12);
    if (version > 1.0F) cursor.u8();
    const auto diplomacy_count = cursor.i16();
    if (diplomacy_count < 0 || diplomacy_count > 64) {
        throw std::runtime_error("invalid scenario-player diplomacy count");
    }
    cursor.skip(static_cast<std::size_t>(diplomacy_count));
    if (version >= 1.08F) cursor.skip(9 * 4);
    if (version >= 1.13F) cursor.skip(4);
    skip_victory_conditions(cursor, version >= 1.09F);
}

void decode_objects_and_seek_triggers(
    Cursor& cursor,
    const std::string& format,
    LegacyScenarioMetadata& metadata
) {
    const auto players = cursor.u32();
    if (players == 0 || players > 16) {
        throw std::runtime_error("invalid body player-table count");
    }
    const float player_version = player_data_version(format);
    std::size_t world_size = player_version > 1.06F ? 16 : 0;
    if (player_version > 1.12F) world_size += 8;
    if (player_version >= 1.14F) world_size += 4;
    cursor.skip(static_cast<std::size_t>(players - 1) * world_size);

    std::size_t total_objects = 0;
    for (std::uint32_t owner = 0; owner < players; ++owner) {
        const auto count = cursor.u32();
        if (count > 100000 || total_objects > 100000 - count) {
            throw std::runtime_error("object table exceeds 100000 entries");
        }
        total_objects += count;
        for (std::uint32_t index = 0; index < count; ++index) {
            LegacyScenarioMetadata::Object object;
            object.owner_slot = owner;
            object.x = cursor.f32();
            object.y = cursor.f32();
            object.z = cursor.f32();
            object.object_id = cursor.i32();
            object.unit_type_id = cursor.u16();
            object.state = cursor.u8();
            object.angle = cursor.f32();
            if (format >= "1.15") object.animation_frame = cursor.i16();
            if (format >= "1.13") {
                const auto garrison = cursor.i32();
                if (garrison != -1 && garrison != 0) {
                    object.garrisoned_in = garrison;
                }
            }
            metadata.objects.push_back(object);
        }
    }
    metadata.objects_decoded = true;

    const auto scenario_players = cursor.u32();
    if (scenario_players == 0 || scenario_players > 16) {
        throw std::runtime_error("invalid scenario-player table count");
    }
    for (std::uint32_t index = 1; index < scenario_players; ++index) {
        skip_scenario_player(cursor, player_version);
    }
}

std::vector<std::int32_t> read_properties(
    Cursor& cursor,
    std::int32_t count,
    const char* kind
) {
    if (count < 0 || count > 256) {
        throw std::runtime_error(
            std::string("invalid trigger ") + kind + " property count"
        );
    }
    std::vector<std::int32_t> values;
    values.reserve(static_cast<std::size_t>(count));
    for (std::int32_t index = 0; index < count; ++index) {
        values.push_back(cursor.i32());
    }
    return values;
}

void decode_triggers(
    Cursor& cursor,
    LegacyScenarioMetadata& metadata
) {
    const double version = cursor.f64();
    if (version < 1.0 || version > 3.0) {
        throw std::runtime_error("unsupported trigger-system version");
    }
    metadata.trigger_version = version;
    if (version >= 1.5) cursor.u8();
    const auto trigger_count = cursor.i32();
    if (trigger_count < 0 || trigger_count > 10000) {
        throw std::runtime_error("invalid trigger count");
    }
    metadata.triggers.reserve(static_cast<std::size_t>(trigger_count));
    for (std::int32_t trigger_index = 0;
         trigger_index < trigger_count;
         ++trigger_index) {
        LegacyScenarioMetadata::Trigger trigger;
        trigger.enabled = cursor.i32() != 0;
        trigger.looping = cursor.u8() != 0;
        trigger.name_id = cursor.i32();
        trigger.objective = cursor.u8() != 0;
        trigger.objective_order = cursor.i32();
        if (version >= 1.8) {
            cursor.skip(1 + 4 + 1 + 1);
            trigger.start_time = cursor.u32();
            cursor.u8();
        } else {
            trigger.start_time = cursor.u32();
        }
        trigger.description = cursor.string32();
        trigger.name = cursor.string32();
        if (version >= 1.8) cursor.string32();

        const auto effect_count = cursor.i32();
        if (effect_count < 0 || effect_count > 10000) {
            throw std::runtime_error("invalid trigger effect count");
        }
        for (std::int32_t effect_index = 0;
             effect_index < effect_count;
             ++effect_index) {
            LegacyScenarioMetadata::TriggerEffect effect;
            effect.type = cursor.i32();
            effect.properties = read_properties(
                cursor,
                version > 1.0 ? cursor.i32() : 16,
                "effect"
            );
            effect.decoded_property_count = effect.properties.size();
            while (effect.properties.size() < 24) {
                effect.properties.push_back(-1);
            }
            effect.chat_text = cursor.string32();
            effect.audio_file = cursor.string32();
            const auto raw_object_count =
                version > 1.1 ? effect.properties[4] : 1;
            if (raw_object_count > 100000) {
                throw std::runtime_error("invalid trigger object count");
            }
            const auto object_count = std::max(0, raw_object_count);
            for (std::int32_t index = 0; index < object_count; ++index) {
                effect.object_ids.push_back(cursor.i32());
            }
            trigger.effects.push_back(std::move(effect));
        }
        for (std::int32_t index = 0; index < effect_count; ++index) {
            trigger.effect_order.push_back(cursor.i32());
        }

        const auto condition_count = cursor.i32();
        if (condition_count < 0 || condition_count > 10000) {
            throw std::runtime_error("invalid trigger condition count");
        }
        for (std::int32_t condition_index = 0;
             condition_index < condition_count;
             ++condition_index) {
            LegacyScenarioMetadata::TriggerCondition condition;
            condition.type = cursor.i32();
            condition.properties = read_properties(
                cursor,
                version > 1.0 ? cursor.i32() : 13,
                "condition"
            );
            condition.decoded_property_count =
                condition.properties.size();
            while (condition.properties.size() < 18) {
                condition.properties.push_back(-1);
            }
            trigger.conditions.push_back(std::move(condition));
        }
        for (std::int32_t index = 0; index < condition_count; ++index) {
            trigger.condition_order.push_back(cursor.i32());
        }
        metadata.triggers.push_back(std::move(trigger));
    }
    if (version >= 1.4) {
        metadata.trigger_order.reserve(
            static_cast<std::size_t>(trigger_count)
        );
        for (std::int32_t index = 0; index < trigger_count; ++index) {
            metadata.trigger_order.push_back(cursor.i32());
        }
    } else {
        for (std::int32_t index = 0; index < trigger_count; ++index) {
            metadata.trigger_order.push_back(index);
        }
    }
    if (version >= 2.2) {
        cursor.skip(256 * 4);
        const auto technologies = cursor.u32();
        if (technologies > 100000) {
            throw std::runtime_error("invalid enabled-technology count");
        }
        cursor.skip(static_cast<std::size_t>(technologies) * 4);
        const auto names = cursor.u32();
        if (names > 256) {
            throw std::runtime_error("invalid trigger-variable name count");
        }
        for (std::uint32_t index = 0; index < names; ++index) {
            if (cursor.u32() >= 256) {
                throw std::runtime_error("invalid trigger-variable ID");
            }
            cursor.string32();
        }
    }
    metadata.triggers_decoded = true;
}

}  // namespace

LegacyScenarioImportResult inspect_legacy_scenario_bytes(
    std::span<const std::byte> source_bytes
) {
    LegacyScenarioImportResult result;
    const std::vector<std::byte> bytes(
        source_bytes.begin(),
        source_bytes.end()
    );

    try {
        if (bytes.size() < 12) {
            throw std::runtime_error("truncated Genie scenario preamble");
        }
        std::string version(4, '\0');
        std::memcpy(version.data(), bytes.data(), 4);
        if (!supported_classic_version(version)) {
            result.status = LegacyScenarioImportStatus::unsupported_version;
            result.diagnostic =
                "unsupported Genie scenario format version '" + version + "'";
            return result;
        }

        const std::uint32_t header_size = read_u32(bytes, 4);
        if (header_size < 16 || header_size > max_header_size ||
            static_cast<std::size_t>(header_size) > bytes.size() - 8) {
            throw std::runtime_error("invalid Genie scenario header size");
        }
        const std::size_t header_begin = 8;
        const std::size_t header_end = header_begin + header_size;
        const std::uint32_t header_version = read_u32(bytes, header_begin);
        if (header_version != 1 && header_version != 2) {
            result.status = LegacyScenarioImportStatus::unsupported_version;
            result.diagnostic =
                "recognized classic format, but header version " +
                std::to_string(header_version) + " is not supported";
            return result;
        }

        std::size_t cursor = header_begin + 4;
        std::optional<std::uint32_t> timestamp;
        if (header_version >= 2) {
            timestamp = read_u32(bytes, cursor);
            cursor += 4;
        }
        const std::uint32_t description_size = read_u32(bytes, cursor);
        cursor += 4;
        if (description_size > max_description_size ||
            cursor > header_end ||
            description_size > header_end - cursor) {
            throw std::runtime_error("invalid scenario description length");
        }
        std::string description;
        if (description_size != 0) {
            const auto* description_data = reinterpret_cast<const char*>(
                bytes.data() + cursor
            );
            if (description_data[description_size - 1] != '\0') {
                throw std::runtime_error(
                    "scenario description lacks terminator"
                );
            }
            description.assign(
                description_data,
                description_data + description_size - 1
            );
            if (description.find('\0') != std::string::npos) {
                throw std::runtime_error(
                    "scenario description has embedded NUL"
                );
            }
        }
        cursor += description_size;
        const std::uint32_t victory = read_u32(bytes, cursor);
        cursor += 4;
        const std::uint32_t players = read_u32(bytes, cursor);
        cursor += 4;
        if (victory > 1) {
            throw std::runtime_error("invalid single-player victory flag");
        }
        if (players > 16) {
            throw std::runtime_error("implausible active player count");
        }
        if (cursor != header_end) {
            throw std::runtime_error("unexpected fields in classic header");
        }

        const auto inflated = inflate_raw(
            std::span<const std::byte>(bytes).subspan(header_end)
        );
        if (inflated.size() < 4) {
            throw std::runtime_error("scenario body lacks next object ID");
        }
        LegacyScenarioMetadata metadata;
        metadata.format_version = version;
        metadata.header_version = header_version;
        metadata.creation_timestamp = timestamp;
        metadata.description = std::move(description);
        metadata.has_single_player_victory = victory != 0;
        metadata.active_player_count = players;
        metadata.compressed_body_size = bytes.size() - header_end;
        metadata.uncompressed_body_size = inflated.size();
        metadata.next_object_id =
            static_cast<std::int32_t>(read_u32(inflated, 0));
        Cursor body_cursor(inflated);
        body_cursor.i32();
        metadata.data_version = navigate_tribe_scen(body_cursor, metadata);
        metadata.tribe_section_end = body_cursor.offset();
        decode_map(body_cursor, metadata);
        decode_objects_and_seek_triggers(
            body_cursor,
            metadata.format_version,
            metadata
        );
        if (metadata.format_version >= "1.14") {
            decode_triggers(body_cursor, metadata);
        }

        result.status = LegacyScenarioImportStatus::metadata_only;
        result.metadata = std::move(metadata);
        result.diagnostic =
            "classic Genie header, player/options section boundaries, and "
            "terrain/elevation map, raw object tables, and supported trigger "
            "metadata decoded; commercial ID translation is not imported";
        return result;
    } catch (const std::exception& error) {
        result.status = LegacyScenarioImportStatus::malformed;
        result.diagnostic = error.what();
        return result;
    }
}

LegacyScenarioImportResult inspect_legacy_scenario(
    const std::filesystem::path& path
) {
    try {
        const auto bytes = read_file(path);
        return inspect_legacy_scenario_bytes(bytes);
    } catch (const std::exception& error) {
        LegacyScenarioImportResult result;
        result.status = LegacyScenarioImportStatus::io_error;
        result.diagnostic = error.what();
        return result;
    }
}

const std::vector<CommercialObjectMapping>& commercial_object_mappings() {
    static const std::vector<CommercialObjectMapping> mappings{
        {4, UnitKind::archer, std::nullopt},
        {5, UnitKind::hand_cannoneer, std::nullopt},
        {6, UnitKind::elite_skirmisher, std::nullopt},
        {7, UnitKind::skirmisher, std::nullopt},
        {8, UnitKind::longbowman, std::nullopt},
        {11, UnitKind::mangudai, std::nullopt},
        {13, UnitKind::fishing_ship, std::nullopt},
        {17, UnitKind::trade_cog, std::nullopt},
        {21, UnitKind::war_galley, std::nullopt},
        {24, UnitKind::crossbowman, std::nullopt},
        {25, UnitKind::teutonic_knight, std::nullopt},
        {35, UnitKind::battering_ram, std::nullopt},
        {36, UnitKind::bombard_cannon, std::nullopt},
        {38, UnitKind::knight, std::nullopt},
        {39, UnitKind::cavalry_archer, std::nullopt},
        {40, UnitKind::cataphract, std::nullopt},
        {41, UnitKind::huskarl, std::nullopt},
        {42, UnitKind::trebuchet, std::nullopt},
        {46, UnitKind::janissary, std::nullopt},
        {48, UnitKind::boar, std::nullopt},
        {65, UnitKind::deer, std::nullopt},
        {73, UnitKind::chu_ko_nu, std::nullopt},
        {74, UnitKind::militia, std::nullopt},
        {75, UnitKind::man_at_arms, std::nullopt},
        {77, UnitKind::long_swordsman, std::nullopt},
        {83, UnitKind::villager, std::nullopt},
        {93, UnitKind::spearman, std::nullopt},
        {125, UnitKind::monk, std::nullopt},
        {128, UnitKind::trade_cart, std::nullopt},
        {199, std::nullopt, BuildingKind::fish_trap},
        {250, UnitKind::longboat, std::nullopt},
        {239, UnitKind::war_elephant, std::nullopt},
        {232, UnitKind::woad_raider, std::nullopt},
        {534, UnitKind::elite_woad_raider, std::nullopt},
        {279, UnitKind::scorpion, std::nullopt},
        {280, UnitKind::mangonel, std::nullopt},
        {281, UnitKind::throwing_axeman, std::nullopt},
        {282, UnitKind::mameluke, std::nullopt},
        {283, UnitKind::cavalier, std::nullopt},
        {285, UnitKind::relic, std::nullopt},
        {291, UnitKind::samurai, std::nullopt},
        {329, UnitKind::camel_rider, std::nullopt},
        {330, UnitKind::heavy_camel, std::nullopt},
        {331, UnitKind::packed_trebuchet, std::nullopt},
        {358, UnitKind::pikeman, std::nullopt},
        {359, UnitKind::halberdier, std::nullopt},
        {420, UnitKind::cannon_galleon, std::nullopt},
        {422, UnitKind::capped_ram, std::nullopt},
        {441, UnitKind::hussar, std::nullopt},
        {440, UnitKind::petard, std::nullopt},
        {442, UnitKind::galleon, std::nullopt},
        {448, UnitKind::scout_cavalry, std::nullopt},
        {473, UnitKind::two_handed_swordsman, std::nullopt},
        {474, UnitKind::heavy_cavalry_archer, std::nullopt},
        {492, UnitKind::arbalester, std::nullopt},
        {527, UnitKind::demolition_ship, std::nullopt},
        {528, UnitKind::heavy_demolition_ship, std::nullopt},
        {529, UnitKind::fire_ship, std::nullopt},
        {530, UnitKind::elite_longbowman, std::nullopt},
        {531, UnitKind::elite_throwing_axeman, std::nullopt},
        {532, UnitKind::fast_fire_ship, std::nullopt},
        {533, UnitKind::elite_longboat, std::nullopt},
        {539, UnitKind::galley, std::nullopt},
        {542, UnitKind::heavy_scorpion, std::nullopt},
        {545, UnitKind::transport_ship, std::nullopt},
        {546, UnitKind::light_cavalry, std::nullopt},
        {548, UnitKind::siege_ram, std::nullopt},
        {550, UnitKind::onager, std::nullopt},
        {553, UnitKind::elite_cataphract, std::nullopt},
        {554, UnitKind::elite_teutonic_knight, std::nullopt},
        {555, UnitKind::elite_huskarl, std::nullopt},
        {556, UnitKind::elite_mameluke, std::nullopt},
        {557, UnitKind::elite_janissary, std::nullopt},
        {558, UnitKind::elite_war_elephant, std::nullopt},
        {559, UnitKind::elite_chu_ko_nu, std::nullopt},
        {560, UnitKind::elite_samurai, std::nullopt},
        {561, UnitKind::elite_mangudai, std::nullopt},
        {567, UnitKind::champion, std::nullopt},
        {569, UnitKind::paladin, std::nullopt},
        {588, UnitKind::siege_onager, std::nullopt},
        {594, UnitKind::sheep, std::nullopt},
        {692, UnitKind::berserk, std::nullopt},
        {694, UnitKind::elite_berserk, std::nullopt},
        {691, UnitKind::elite_cannon_galleon, std::nullopt},
        {725, UnitKind::jaguar_warrior, std::nullopt},
        {726, UnitKind::elite_jaguar_warrior, std::nullopt},
        {751, UnitKind::eagle_warrior, std::nullopt},
        {752, UnitKind::elite_eagle_warrior, std::nullopt},
        {755, UnitKind::tarkan, std::nullopt},
        {757, UnitKind::elite_tarkan, std::nullopt},
        {763, UnitKind::plumed_archer, std::nullopt},
        {765, UnitKind::elite_plumed_archer, std::nullopt},
        {771, UnitKind::conquistador, std::nullopt},
        {773, UnitKind::elite_conquistador, std::nullopt},
        {775, UnitKind::missionary, std::nullopt},
        {831, UnitKind::turtle_ship, std::nullopt},
        {832, UnitKind::elite_turtle_ship, std::nullopt},
        {12, std::nullopt, BuildingKind::barracks},
        {45, std::nullopt, BuildingKind::dock},
        {49, std::nullopt, BuildingKind::siege_workshop},
        {50, std::nullopt, BuildingKind::farm},
        {68, std::nullopt, BuildingKind::mill},
        {70, std::nullopt, BuildingKind::house},
        {71, std::nullopt, BuildingKind::town_center},
        {72, std::nullopt, BuildingKind::palisade_wall},
        {79, std::nullopt, BuildingKind::watch_tower},
        {82, std::nullopt, BuildingKind::castle},
        {84, std::nullopt, BuildingKind::market},
        {87, std::nullopt, BuildingKind::archery_range},
        {101, std::nullopt, BuildingKind::stable},
        {103, std::nullopt, BuildingKind::blacksmith},
        {104, std::nullopt, BuildingKind::monastery},
        {109, std::nullopt, BuildingKind::town_center},
        {117, std::nullopt, BuildingKind::stone_wall},
        {141, std::nullopt, BuildingKind::town_center},
        {142, std::nullopt, BuildingKind::town_center},
        {155, std::nullopt, BuildingKind::stone_wall},
        {209, std::nullopt, BuildingKind::university},
        {236, std::nullopt, BuildingKind::bombard_tower},
        {276, std::nullopt, BuildingKind::wonder},
        {463, std::nullopt, BuildingKind::house},
        {464, std::nullopt, BuildingKind::house},
        {465, std::nullopt, BuildingKind::house},
        {562, std::nullopt, BuildingKind::lumber_camp},
        {584, std::nullopt, BuildingKind::mining_camp},
        {598, std::nullopt, BuildingKind::outpost},
    };
    return mappings;
}

LegacyScenarioConversionReport::TriggerAudit audit_legacy_scenario_triggers(
    const LegacyScenarioMetadata& source
) {
    LegacyScenarioConversionReport::TriggerAudit audit;
    audit.trigger_count = source.triggers.size();
    std::map<std::int32_t, std::size_t> object_id_counts;
    std::map<std::int32_t, const LegacyScenarioMetadata::Object*> objects;
    for (const auto& object : source.objects) {
        ++object_id_counts[object.object_id];
        objects.try_emplace(object.object_id, &object);
        const auto mapping = std::ranges::find(
            commercial_object_mappings(),
            object.unit_type_id,
            &CommercialObjectMapping::commercial_id
        );
        if (mapping == commercial_object_mappings().end()) {
            ++audit.unmapped_commercial_object_ids[object.unit_type_id];
        } else {
            ++audit.mapped_commercial_object_ids[object.unit_type_id];
        }
    }

    const auto audit_reference = [&](std::int32_t object_id) {
        if (object_id <= 0) {
            ++audit.object_reference_blockers["invalid_object_id"];
            return;
        }
        if (object_id_counts[object_id] > 1) {
            ++audit.object_reference_blockers["duplicate_object_id"];
            return;
        }
        const auto found = objects.find(object_id);
        if (found == objects.end()) {
            ++audit.object_reference_blockers["missing_object"];
            return;
        }
        const auto& object = *found->second;
        const auto mapping = std::ranges::find(
            commercial_object_mappings(),
            object.unit_type_id,
            &CommercialObjectMapping::commercial_id
        );
        if (mapping == commercial_object_mappings().end()) {
            ++audit.object_reference_blockers["unmapped_commercial_id"];
        } else if (object.owner_slot != 1 && object.owner_slot != 2) {
            ++audit.object_reference_blockers["unmapped_owner"];
        } else if (object.x != std::floor(object.x) ||
                   object.y != std::floor(object.y)) {
            ++audit.object_reference_blockers["fractional_position"];
        } else if (!source.map_decoded ||
                   object.x < 0 || object.y < 0 ||
                   object.x >= static_cast<float>(source.map_width) ||
                   object.y >= static_cast<float>(source.map_height)) {
            ++audit.object_reference_blockers["position_outside_map"];
        } else {
            ++audit.object_reference_blockers["resolved"];
        }
    };

    for (const auto& trigger : source.triggers) {
        for (const auto& condition : trigger.conditions) {
            ++audit.condition_count;
            ++audit.condition_types[condition.type];
            if (condition.type != 5 && condition.type != 6 &&
                condition.type != 8 &&
                condition.type != 10) {
                ++audit.unsupported_condition_types[condition.type];
            }
            const std::size_t decoded_count =
                condition.decoded_property_count != 0
                ? condition.decoded_property_count
                : condition.properties.size();
            for (std::size_t index = decoded_count;
                 index < 18; ++index) {
                ++audit.missing_property_indices[
                    "condition." + std::to_string(condition.type) + "." +
                    std::to_string(index)
                ];
            }
            if ((condition.type == 6 || condition.type == 23) &&
                condition.properties.size() > 2) {
                ++audit.direct_object_references;
                audit_reference(condition.properties[2]);
            }
            std::string shape =
                "condition." + std::to_string(condition.type);
            if (condition.properties.size() > 2 &&
                condition.properties[2] != -1) shape += ".direct";
            if (condition.properties.size() > 12 &&
                std::ranges::any_of(
                    condition.properties.begin() + 9,
                    condition.properties.begin() + 13,
                    [](std::int32_t value) { return value != -1; }
                )) shape += ".area";
            if (condition.properties.size() > 4 &&
                condition.properties[4] != -1) shape += ".unit_type";
            if (condition.properties.size() > 13 &&
                condition.properties[13] != -1) shape += ".group";
            if (condition.properties.size() > 14 &&
                condition.properties[14] != -1) shape += ".object_type";
            if (condition.properties.size() > 16 &&
                condition.properties[16] == 1) shape += ".inverted";
            ++audit.selector_shapes[shape];
        }
        for (const auto& effect : trigger.effects) {
            ++audit.effect_count;
            ++audit.effect_types[effect.type];
            if (effect.type == 2 && effect.properties.size() > 9) {
                ++audit.research_technology_ids[effect.properties[9]];
            }
            if (effect.type != 1 && effect.type != 2 &&
                effect.type != 3 && effect.type != 5 &&
                effect.type != 8 && effect.type != 9 &&
                effect.type != 11 && effect.type != 13 &&
                effect.type != 15) {
                ++audit.unsupported_effect_types[effect.type];
            }
            const std::size_t decoded_count =
                effect.decoded_property_count != 0
                ? effect.decoded_property_count
                : effect.properties.size();
            for (std::size_t index = decoded_count;
                 index < 24; ++index) {
                ++audit.missing_property_indices[
                    "effect." + std::to_string(effect.type) + "." +
                    std::to_string(index)
                ];
            }
            if ((effect.type == 14 || effect.type == 15) &&
                effect.properties.size() > 5 &&
                effect.properties[5] > 0) {
                ++audit.direct_object_references;
                audit_reference(effect.properties[5]);
            }
            audit.listed_object_references += effect.object_ids.size();
            for (const auto object_id : effect.object_ids) {
                audit_reference(object_id);
            }
            std::string shape = "effect." + std::to_string(effect.type);
            if (effect.properties.size() > 5 &&
                effect.properties[5] != -1) shape += ".direct";
            if (!effect.object_ids.empty()) shape += ".list";
            if (effect.properties.size() > 19 &&
                std::ranges::any_of(
                    effect.properties.begin() + 16,
                    effect.properties.begin() + 20,
                    [](std::int32_t value) { return value != -1; }
                )) shape += ".area";
            if (effect.properties.size() > 20 &&
                effect.properties[20] != -1) shape += ".group";
            if (effect.properties.size() > 21 &&
                effect.properties[21] != -1) shape += ".object_type";
            ++audit.selector_shapes[shape];
        }
    }
    return audit;
}

namespace {

std::string json_string(std::string_view value) {
    std::ostringstream output;
    output << '"';
    for (const unsigned char character : value) {
        switch (character) {
            case '"': output << "\\\""; break;
            case '\\': output << "\\\\"; break;
            case '\b': output << "\\b"; break;
            case '\f': output << "\\f"; break;
            case '\n': output << "\\n"; break;
            case '\r': output << "\\r"; break;
            case '\t': output << "\\t"; break;
            default:
                if (character < 0x20) {
                    output << "\\u00" << std::hex << std::setw(2)
                           << std::setfill('0')
                           << static_cast<int>(character) << std::dec;
                } else {
                    output << static_cast<char>(character);
                }
        }
    }
    output << '"';
    return output.str();
}

template <typename Key>
void write_count_map(
    std::ostringstream& output,
    const std::map<Key, std::size_t>& values
) {
    output << '{';
    bool first = true;
    for (const auto& [key, count] : values) {
        if (!first) output << ',';
        first = false;
        output << json_string(std::to_string(key)) << ':' << count;
    }
    output << '}';
}

void write_count_map(
    std::ostringstream& output,
    const std::map<std::string, std::size_t>& values
) {
    output << '{';
    bool first = true;
    for (const auto& [key, count] : values) {
        if (!first) output << ',';
        first = false;
        output << json_string(key) << ':' << count;
    }
    output << '}';
}

void write_trigger_audit_fields(
    std::ostringstream& output,
    const LegacyScenarioConversionReport::TriggerAudit& audit
) {
    output << "\"trigger_count\":" << audit.trigger_count
           << ",\"condition_count\":" << audit.condition_count
           << ",\"effect_count\":" << audit.effect_count
           << ",\"direct_object_references\":"
           << audit.direct_object_references
           << ",\"listed_object_references\":"
           << audit.listed_object_references
           << ",\"condition_types\":";
    write_count_map(output, audit.condition_types);
    output << ",\"effect_types\":";
    write_count_map(output, audit.effect_types);
    output << ",\"unsupported_condition_types\":";
    write_count_map(output, audit.unsupported_condition_types);
    output << ",\"unsupported_effect_types\":";
    write_count_map(output, audit.unsupported_effect_types);
    output << ",\"missing_property_indices\":";
    write_count_map(output, audit.missing_property_indices);
    output << ",\"object_reference_blockers\":";
    write_count_map(output, audit.object_reference_blockers);
    output << ",\"selector_shapes\":";
    write_count_map(output, audit.selector_shapes);
    output << ",\"mapped_commercial_object_ids\":";
    write_count_map(output, audit.mapped_commercial_object_ids);
    output << ",\"unmapped_commercial_object_ids\":";
    write_count_map(output, audit.unmapped_commercial_object_ids);
    output << ",\"research_technology_ids\":";
    write_count_map(output, audit.research_technology_ids);
}

}  // namespace

std::string legacy_scenario_trigger_audit_json(
    const LegacyScenarioConversionReport::TriggerAudit& audit
) {
    std::ostringstream output;
    output << '{';
    write_trigger_audit_fields(output, audit);
    output << "}\n";
    return output.str();
}

std::string legacy_scenario_conversion_report_json(
    const LegacyScenarioConversionReport& report
) {
    std::ostringstream output;
    output << "{\"playable\":" << (report.scenario ? "true" : "false")
           << ",\"translated_tiles\":" << report.translated_tiles
           << ",\"unsupported_tiles\":" << report.unsupported_tiles
           << ",\"translated_objects\":" << report.translated_objects
           << ",\"unsupported_objects\":" << report.unsupported_objects
           << ",\"object_hit_points_available\":"
           << (report.object_hit_points_available ? "true" : "false")
           << ",\"objects_using_default_hit_points\":"
           << report.objects_using_default_hit_points
           << ",\"unsupported_commercial_object_ids\":";
    write_count_map(output, report.unsupported_commercial_object_ids);
    output
           << ",\"translated_triggers\":" << report.translated_triggers
           << ",\"unsupported_triggers\":" << report.unsupported_triggers
           << ",\"translated_conditions\":" << report.translated_conditions
           << ",\"translated_effects\":" << report.translated_effects
           << ",\"trigger_audit\":{";
    write_trigger_audit_fields(output, report.trigger_audit);
    output << "},\"object_id_remap\":[";
    for (std::size_t index = 0; index < report.object_id_remap.size();
         ++index) {
        if (index != 0) output << ',';
        const auto& remap = report.object_id_remap[index];
        output << "{\"source_object_index\":"
               << remap.source_object_index
               << ",\"commercial_object_id\":"
               << remap.commercial_object_id
               << ",\"native_entity_id\":" << remap.native_entity_id
               << ",\"building\":"
               << (remap.building ? "true" : "false")
               << ",\"lossless\":"
               << (remap.lossless ? "true" : "false")
               << ",\"blocker\":" << json_string(remap.blocker) << '}';
    }
    output << "],\"diagnostics\":[";
    for (std::size_t index = 0; index < report.diagnostics.size(); ++index) {
        if (index != 0) output << ',';
        output << json_string(report.diagnostics[index]);
    }
    output << "]}\n";
    return output.str();
}

std::optional<std::string> trigger_player(std::int32_t slot) {
    if (slot == 1) return "blue";
    if (slot == 2) return "red";
    return std::nullopt;
}

std::optional<std::string> trigger_resource(std::int32_t id) {
    switch (id) {
        case 0: return "food";
        case 1: return "wood";
        case 2: return "stone";
        case 3: return "gold";
        default: return std::nullopt;
    }
}

std::optional<std::string> common_object_name(std::int32_t id) {
    switch (id) {
        case 4: return "archer";
        case 7: return "skirmisher";
        case 12: return "barracks";
        case 21: return "war_galley";
        case 45: return "dock";
        case 49: return "siege_workshop";
        case 50: return "farm";
        case 68: return "mill";
        case 70: return "house";
        case 71:
        case 109:
        case 141:
        case 142: return "town_center";
        case 74: return "militia";
        case 75: return "man_at_arms";
        case 79: return "watch_tower";
        case 82: return "castle";
        case 83: return "villager";
        case 84: return "market";
        case 87: return "archery_range";
        case 93: return "spearman";
        case 101: return "stable";
        case 103: return "blacksmith";
        case 104: return "monastery";
        case 117: return "stone_wall";
        case 125: return "monk";
        case 128: return "trade_cart";
        case 279: return "scorpion";
        case 280: return "mangonel";
        case 329: return "camel_rider";
        case 448: return "scout_cavalry";
        case 545: return "transport_ship";
        case 548: return "battering_ram";
        case 562: return "lumber_camp";
        case 584: return "mining_camp";
        default: return std::nullopt;
    }
}

std::optional<std::string> common_technology_name(std::int32_t id) {
    switch (id) {
        case 8: return "town_watch";
        case 19: return "cartography";
        case 67: return "forging";
        case 68: return "iron_casting";
        case 74: return "scale_mail_armor";
        case 76: return "chain_mail_armor";
        case 77: return "plate_mail_armor";
        case 93: return "ballistics";
        case 100: return "crossbowman";
        case 199: return "fletching";
        case 201: return "bracer";
        case 207: return "long_swordsman";
        case 211: return "padded_archer_armor";
        case 212: return "leather_archer_armor";
        case 215: return "squires";
        case 217: return "two_handed_swordsman";
        case 219: return "ring_archer_armor";
        case 322: return "murder_holes";
        default: return std::nullopt;
    }
}

bool common_object_is_building(std::int32_t id) {
    return std::ranges::any_of(
        commercial_object_mappings(),
        [id](const CommercialObjectMapping& mapping) {
            return mapping.commercial_id == id &&
                mapping.building.has_value();
        }
    );
}

struct ConvertedObjectReference {
    EntityId native_id{};
    bool building{};
};

std::optional<std::string> translate_trigger_condition(
    const LegacyScenarioMetadata::TriggerCondition& condition,
    std::string& reason,
    const std::map<std::int32_t, ConvertedObjectReference>& references
) {
    const auto& property = condition.properties;
    if (property.size() < 18) {
        reason = "condition lacks the pinned 18-property schema";
        return std::nullopt;
    }
    const bool inverted = property[16] == 1;
    if (property[16] != -1 && property[16] != 0 && !inverted) {
        reason = "condition has an invalid inversion flag";
        return std::nullopt;
    }
    if (inverted) {
        reason = "reconstruction trigger grammar cannot preserve inversion";
        return std::nullopt;
    }
    std::ostringstream output;
    if (condition.type == 6) {
        if (property[2] <= 0) {
            reason = "destroy-object condition has a nonpositive object ID";
            return std::nullopt;
        }
        const auto reference = references.find(property[2]);
        if (reference == references.end()) {
            reason =
                "destroy-object condition references an object without a "
                "unique lossless native remap";
            return std::nullopt;
        }
        output << (reference->second.building
            ? "building_destroyed " : "unit_destroyed ");
        output << reference->second.native_id;
    } else if (condition.type == 23) {
        if (property[2] <= 0) {
            reason = "object-HP condition has a nonpositive object ID";
            return std::nullopt;
        }
        if (!references.contains(property[2])) {
            reason =
                "object-HP condition references an object without a unique "
                "lossless native remap";
            return std::nullopt;
        }
        reason =
            "object-HP comparison is stored in unpinned property index 17; "
            "native >= syntax cannot be selected losslessly";
        return std::nullopt;
    } else if (condition.type == 1) {
        reason =
            "bring-object-to-area requires identity-aware area presence; "
            "native area_presence counts a player population";
        return std::nullopt;
    } else if (condition.type == 15 || condition.type == 16) {
        reason =
            "commercial object visibility is not native object existence";
        return std::nullopt;
    } else if (condition.type == 10 && property[7] >= 0 &&
        property[7] <= std::numeric_limits<int>::max() / 5) {
        output << "elapsed_ticks >= " << property[7] * 5;
    } else if (condition.type == 8 && property[0] >= 0) {
        const auto player = trigger_player(property[5]);
        const auto resource = trigger_resource(property[1]);
        if (!player || !resource) {
            reason = "resource condition has an unmapped player/resource";
            return std::nullopt;
        }
        output << "resource " << *player << ' ' << *resource
               << " >= " << property[0];
    } else if (condition.type == 5 && property[0] >= 0 &&
               property[4] == -1 && property[13] == -1 &&
               property[14] == -1) {
        const auto player = trigger_player(property[5]);
        if (!player || property[9] < 0 || property[10] < 0 ||
            property[11] < property[9] || property[12] < property[10]) {
            reason = "area condition has unmapped selectors or bounds";
            return std::nullopt;
        }
        output << "area_presence " << *player << ' ' << property[9] << ' '
               << property[10] << ' ' << property[11] << ' '
               << property[12] << " >= " << property[0];
    } else {
        reason = "condition type or selectors lack a lossless native mapping";
        return std::nullopt;
    }
    return output.str();
}

std::optional<std::string> translate_trigger_effect(
    const LegacyScenarioMetadata::TriggerEffect& effect,
    std::string& reason,
    const std::map<std::int32_t, ConvertedObjectReference>& references,
    std::size_t trigger_count
) {
    const auto& property = effect.properties;
    if (property.size() < 24) {
        reason = "effect lacks the pinned 24-property schema";
        return std::nullopt;
    }
    std::ostringstream output;
    if (effect.type == 2) {
        const auto player = trigger_player(property[7]);
        const auto technology = common_technology_name(property[9]);
        const bool canonical_properties = std::ranges::all_of(
            std::views::iota(std::size_t{}, property.size()),
            [&property](std::size_t index) {
                return index == 7 || index == 9 ||
                    property[index] == -1;
            }
        );
        if (!player || !technology || !canonical_properties ||
            !effect.object_ids.empty() || !effect.chat_text.empty() ||
            !effect.audio_file.empty()) {
            reason =
                "research effect has unmapped player/technology or "
                "noncanonical extra fields";
            return std::nullopt;
        }
        output << "research " << *player << ' ' << *technology;
    } else if (effect.type == 15) {
        std::vector<std::int32_t> selected = effect.object_ids;
        if (property[5] > 0) selected.push_back(property[5]);
        std::ranges::sort(selected);
        selected.erase(
            std::unique(selected.begin(), selected.end()),
            selected.end()
        );
        const bool count_matches =
            (property[4] == -1 && effect.object_ids.empty()) ||
            property[4] ==
                static_cast<std::int32_t>(effect.object_ids.size());
        if (selected.size() != 1 || selected[0] <= 0 || !count_matches ||
            property[20] != -1 || property[21] != -1 ||
            std::ranges::any_of(
                property.begin() + 16,
                property.begin() + 20,
                [](std::int32_t value) { return value != -1; }
            )) {
            reason =
                "remove-object effect is not one direct canonical selector";
            return std::nullopt;
        }
        const auto reference = references.find(selected[0]);
        if (reference == references.end()) {
            reason =
                "remove-object effect references an object without a unique "
                "lossless native remap";
            return std::nullopt;
        }
        output << "remove_object " << reference->second.native_id;
    } else if (effect.type == 3) {
        const auto player = trigger_player(property[7]);
        if (!player || effect.chat_text.empty()) {
            reason = "chat effect has unmapped player or empty text";
            return std::nullopt;
        }
        if (property[12] > std::numeric_limits<int>::max() / 5) {
            reason = "chat duration exceeds reconstruction tick range";
            return std::nullopt;
        }
        const int ticks = property[12] > 0 ? property[12] * 5 : 150;
        output << "message player=" << *player << " ticks=" << ticks
               << " text=" << std::quoted(effect.chat_text);
    } else if (effect.type == 11) {
        const auto player = trigger_player(property[7]);
        const auto object = common_object_name(property[6]);
        if (!player || !object || property[14] < 0 || property[15] < 0) {
            reason = "create-object effect has unmapped object/player/location";
            return std::nullopt;
        }
        output << (common_object_is_building(property[6])
            ? "create_building " : "create_unit ");
        output << *object << ' ' << *player << ' ' << property[14] << ' '
               << property[15];
    } else if (effect.type == 1) {
        const auto source = trigger_player(property[7]);
        const auto target = trigger_player(property[8]);
        if (!source || !target || *source == *target) {
            reason = "diplomacy effect is not the represented blue/red pair";
            return std::nullopt;
        }
        if (property[3] == 0) output << "diplomacy ally";
        else if (property[3] == 1) output << "diplomacy neutral";
        else if (property[3] == 3) output << "diplomacy enemy";
        else {
            reason = "diplomacy effect has an unmapped relation";
            return std::nullopt;
        }
    } else if (effect.type == 5) {
        const auto source = trigger_player(property[7]);
        const auto target = trigger_player(property[8]);
        const auto resource = trigger_resource(property[2]);
        if (!source || !target || !resource || *source == *target ||
            property[1] < 0) {
            reason = "tribute effect has unmapped players/resource/amount";
            return std::nullopt;
        }
        output << "tribute " << *source << ' ' << *target << ' '
               << *resource << ' ' << property[1];
    } else if (effect.type == 8 || effect.type == 9) {
        if (property[13] < 0 ||
            static_cast<std::size_t>(property[13]) >= trigger_count) {
            reason = "trigger-state effect has an invalid trigger ID";
            return std::nullopt;
        }
        output << (effect.type == 8
            ? "activate_trigger " : "deactivate_trigger ");
        output << property[13] + 1;
    } else if (effect.type == 13) {
        const auto player = trigger_player(property[7]);
        if (!player) {
            reason = "victory effect has an unmapped player";
            return std::nullopt;
        }
        output << "victory " << *player;
    } else if (effect.type == 27) {
        reason =
            "change-object-HP operation, clamp, and death semantics are "
            "unproved by the pinned classic physical schema";
        return std::nullopt;
    } else if (effect.type == 30) {
        reason =
            "attack-move classic properties do not prove destination versus "
            "selector-area packing";
        return std::nullopt;
    } else {
        reason = "effect type lacks a lossless native mapping";
        return std::nullopt;
    }
    return output.str();
}

LegacyScenarioConversionReport convert_legacy_scenario(
    const LegacyScenarioMetadata& source,
    const LegacyDatFile& dat
) {
    LegacyScenarioConversionReport report;
    report.trigger_audit = audit_legacy_scenario_triggers(source);
    if (dat.version() != "VER 5.7") {
        report.diagnostics.push_back("conversion requires live VER 5.7 DAT");
        return report;
    }
    if (!source.map_decoded || !source.player_settings_decoded ||
        !source.objects_decoded) {
        report.diagnostics.push_back(
            "conversion requires complete map, player, and object sections"
        );
        return report;
    }
    if (source.map_width == 0 || source.map_height == 0 ||
        source.map_tiles.size() !=
            static_cast<std::size_t>(source.map_width) * source.map_height) {
        report.diagnostics.push_back("decoded map dimensions are inconsistent");
        return report;
    }

    Scenario converted(
        static_cast<int>(source.map_width),
        static_cast<int>(source.map_height)
    );
    const auto terrain = [&dat](std::uint8_t id)
        -> std::optional<Terrain> {
        if (id >= dat.terrain_count()) return std::nullopt;
        switch (id) {
            case 0: return Terrain::grass;
            case 1: return Terrain::water;
            case 2: return Terrain::beach;
            case 4: return Terrain::shallows;
            default: return std::nullopt;
        }
    };
    for (std::size_t index = 0; index < source.map_tiles.size(); ++index) {
        const TilePosition position{
            static_cast<int>(index % source.map_width),
            static_cast<int>(index / source.map_width),
        };
        converted.map.set_elevation(
            position,
            std::clamp(
                static_cast<int>(
                    source.map_tiles[index].elevation
                ),
                0,
                7
            )
        );
        const auto mapped = terrain(source.map_tiles[index].terrain);
        if (!mapped) {
            ++report.unsupported_tiles;
            report.unsupported_tile_indices.push_back(index);
            continue;
        }
        ++report.translated_tiles;
        converted.map.set_terrain(
            position,
            *mapped
        );
    }

    const auto player = [](std::uint32_t slot) -> std::optional<Player> {
        if (slot == 1) return Player::blue;
        if (slot == 2) return Player::red;
        return std::nullopt;
    };
    std::map<std::int32_t, std::size_t> commercial_object_id_counts;
    for (const auto& object : source.objects) {
        ++commercial_object_id_counts[object.object_id];
    }
    struct PendingObjectRemap {
        std::size_t report_index{};
        std::size_t category_index{};
        bool building{};
    };
    std::vector<PendingObjectRemap> pending_remaps;
    report.object_id_remap.reserve(source.objects.size());
    for (std::size_t source_index = 0;
         source_index < source.objects.size(); ++source_index) {
        const auto& object = source.objects[source_index];
        LegacyScenarioConversionReport::ObjectIdRemap remap;
        remap.source_object_index = source_index;
        remap.commercial_object_id = object.object_id;
        if (object.object_id <= 0) {
            remap.blocker = "nonpositive_commercial_object_id";
        } else if (commercial_object_id_counts[object.object_id] != 1) {
            remap.blocker = "duplicate_commercial_object_id";
        }
        const auto owner = player(object.owner_slot);
        const bool integral =
            object.x == std::floor(object.x) &&
            object.y == std::floor(object.y);
        const TilePosition position{
            static_cast<int>(object.x),
            static_cast<int>(object.y),
        };
        if (!owner || !integral || !converted.map.contains(position)) {
            if (remap.blocker.empty()) {
                remap.blocker =
                    !owner ? "unmapped_owner" :
                    !integral ? "fractional_position" :
                    "position_outside_map";
            }
            report.object_id_remap.push_back(std::move(remap));
            ++report.unsupported_objects;
            report.unsupported_object_records.push_back(object);
            continue;
        }
        const auto mapping = std::ranges::find(
            commercial_object_mappings(),
            object.unit_type_id,
            &CommercialObjectMapping::commercial_id
        );
        if (mapping == commercial_object_mappings().end()) {
            if (remap.blocker.empty()) {
                remap.blocker = "unmapped_commercial_object_id";
            }
            report.object_id_remap.push_back(std::move(remap));
            ++report.unsupported_objects;
            ++report.unsupported_commercial_object_ids[
                object.unit_type_id
            ];
            report.unsupported_object_records.push_back(object);
            continue;
        }
        LegacyScenarioConversionReport::Loss loss;
        loss.object_id = object.object_id;
        if (object.z != 0.0F) loss.fields.push_back("z");
        if (object.state != 0) loss.fields.push_back("state");
        if (object.angle != 0.0F) loss.fields.push_back("angle");
        if (object.animation_frame != -1) {
            loss.fields.push_back("animation_frame");
        }
        if (object.garrisoned_in) loss.fields.push_back("garrisoned_in");
        const bool lossless = loss.fields.empty();
        if (!lossless) report.lossy_objects.push_back(std::move(loss));
        remap.building = mapping->building.has_value();
        remap.lossless = lossless;
        if (!remap.lossless && remap.blocker.empty()) {
            remap.blocker = "translated_object_has_unapplied_fields";
        }
        const std::size_t remap_index = report.object_id_remap.size();
        report.object_id_remap.push_back(std::move(remap));

        if (mapping->unit) {
            UnitPlacement placement{};
            placement.kind = *mapping->unit;
            placement.owner = *owner;
            placement.position = position;
            const std::size_t category_index = converted.units.size();
            converted.units.push_back(std::move(placement));
            ++report.objects_using_default_hit_points;
            pending_remaps.push_back({
                remap_index, category_index, false
            });
            ++report.translated_objects;
            continue;
        }
        if (mapping->building) {
            BuildingPlacement placement{};
            placement.kind = *mapping->building;
            placement.owner = *owner;
            placement.position = position;
            const std::size_t category_index = converted.buildings.size();
            converted.buildings.push_back(std::move(placement));
            ++report.objects_using_default_hit_points;
            pending_remaps.push_back({
                remap_index, category_index, true
            });
            ++report.translated_objects;
            continue;
        }
        report.object_id_remap[remap_index].lossless = false;
        report.object_id_remap[remap_index].blocker =
            "commercial_mapping_has_no_native_kind";
        ++report.unsupported_objects;
        report.unsupported_object_records.push_back(object);
    }
    std::map<std::int32_t, ConvertedObjectReference> object_references;
    const std::size_t unit_count = converted.units.size();
    for (const PendingObjectRemap& pending : pending_remaps) {
        auto& remap = report.object_id_remap[pending.report_index];
        const std::size_t raw_id = pending.building
            ? 1 + unit_count + pending.category_index
            : 1 + pending.category_index;
        if (raw_id > std::numeric_limits<EntityId>::max()) {
            remap.blocker = "native_entity_id_overflow";
            remap.lossless = false;
            continue;
        }
        remap.native_entity_id = static_cast<EntityId>(raw_id);
        if (remap.lossless && remap.blocker.empty()) {
            object_references.emplace(
                remap.commercial_object_id,
                ConvertedObjectReference{
                    remap.native_entity_id, remap.building
                }
            );
        }
    }

    const auto apply_player = [&source](
        std::size_t slot,
        Economy& economy
    ) {
        if (slot >= source.players.size()) return;
        const auto& raw = source.players[slot];
        economy.wood = raw.wood;
        economy.food = raw.food;
        economy.gold = raw.gold;
        economy.stone = raw.stone;
    };
    apply_player(1, converted.blue_economy);
    apply_player(2, converted.red_economy);
    if (source.players.size() > 2 &&
        source.players[1].diplomacy.size() > 2) {
        switch (source.players[1].diplomacy[2]) {
            case 0:
                converted.blue_red_diplomacy = Diplomacy::ally;
                break;
            case 1:
                converted.blue_red_diplomacy = Diplomacy::neutral;
                break;
            case 3:
                converted.blue_red_diplomacy = Diplomacy::enemy;
                break;
        }
    }

    if (source.triggers_decoded) {
        std::vector<std::int32_t> order = source.trigger_order;
        if (order.empty() && source.triggers.empty()) {
            // Valid empty trigger section.
        } else if (order.size() != source.triggers.size()) {
            report.unsupported_triggers = source.triggers.size();
            report.unsupported_trigger_records = source.triggers;
            report.diagnostics.push_back(
                "trigger order table size does not match trigger count"
            );
        } else {
            std::vector<bool> seen(source.triggers.size());
            bool valid_order = true;
            for (const auto index : order) {
                if (index < 0 ||
                    static_cast<std::size_t>(index) >= source.triggers.size() ||
                    seen[static_cast<std::size_t>(index)]) {
                    valid_order = false;
                    break;
                }
                seen[static_cast<std::size_t>(index)] = true;
            }
            if (!valid_order) {
                report.unsupported_triggers = source.triggers.size();
                report.unsupported_trigger_records = source.triggers;
                report.diagnostics.push_back(
                    "trigger order table is not a permutation"
                );
            } else {
                for (std::size_t rank = 0; rank < order.size(); ++rank) {
                    const std::size_t trigger_index =
                        static_cast<std::size_t>(order[rank]);
                    const auto& raw = source.triggers[trigger_index];
                    bool supported = true;
                    std::vector<std::string> condition_texts;
                    std::vector<std::string> effect_texts;
                    if (raw.objective) {
                        supported = false;
                    }
                    if (raw.start_time != 0 || raw.conditions.empty()) {
                        if (raw.start_time >
                            static_cast<std::uint32_t>(
                                std::numeric_limits<int>::max() / 5
                            )) {
                            supported = false;
                        } else {
                            condition_texts.push_back(
                                "elapsed_ticks >= " +
                                std::to_string(raw.start_time * 5)
                            );
                        }
                    }
                    const auto valid_entry_order = [](const auto& entries,
                                                      const auto& values) {
                        if (entries.size() != values.size()) return false;
                        std::vector<bool> used(entries.size());
                        for (const auto value : values) {
                            if (value < 0 ||
                                static_cast<std::size_t>(value) >=
                                    entries.size() ||
                                used[static_cast<std::size_t>(value)]) {
                                return false;
                            }
                            used[static_cast<std::size_t>(value)] = true;
                        }
                        return true;
                    };
                    if (valid_entry_order(
                            raw.conditions, raw.condition_order
                        )) {
                        for (const auto ordered_index : raw.condition_order) {
                            const std::size_t index =
                                static_cast<std::size_t>(ordered_index);
                        std::string reason;
                        const auto translated = translate_trigger_condition(
                            raw.conditions[index],
                            reason,
                            object_references
                        );
                        if (translated) {
                                condition_texts.push_back(*translated);
                        } else {
                            supported = false;
                            report.unsupported_conditions.push_back({
                                    trigger_index, index,
                                    raw.conditions[index], reason
                            });
                        }
                        }
                    } else {
                        supported = false;
                        for (std::size_t index = 0;
                             index < raw.conditions.size(); ++index) {
                            report.unsupported_conditions.push_back({
                                trigger_index,
                                index,
                                raw.conditions[index],
                                "condition order is not a permutation",
                            });
                        }
                    }
                    if (!raw.effects.empty() && valid_entry_order(
                            raw.effects, raw.effect_order
                        )) {
                        for (const auto ordered_index : raw.effect_order) {
                            const std::size_t index =
                                static_cast<std::size_t>(ordered_index);
                        std::string reason;
                        const auto translated = translate_trigger_effect(
                            raw.effects[index],
                            reason,
                            object_references,
                            source.triggers.size()
                        );
                        if (translated) {
                                effect_texts.push_back(*translated);
                        } else {
                            supported = false;
                            report.unsupported_effects.push_back({
                                    trigger_index, index,
                                    raw.effects[index], reason
                            });
                        }
                        }
                    } else {
                        supported = false;
                        for (std::size_t index = 0;
                             index < raw.effects.size(); ++index) {
                            report.unsupported_effects.push_back({
                                trigger_index,
                                index,
                                raw.effects[index],
                                "effect order is not a nonempty permutation",
                            });
                        }
                    }
                    if (!supported) {
                        ++report.unsupported_triggers;
                        report.unsupported_trigger_records.push_back(raw);
                        continue;
                    }
                    ScenarioTrigger trigger;
                    trigger.id = static_cast<int>(trigger_index + 1);
                    trigger.priority = static_cast<int>(
                        order.size() - rank
                    );
                    trigger.enabled = raw.enabled;
                    trigger.looping = raw.looping;
                    trigger.conditions = std::move(condition_texts);
                    trigger.effects = std::move(effect_texts);
                    converted.triggers.push_back(std::move(trigger));
                    ++report.translated_triggers;
                    report.translated_conditions += raw.conditions.size();
                    report.translated_effects += raw.effects.size();
                }
            }
        }
    }

    if (report.unsupported_tiles != 0) {
        report.diagnostics.push_back(
            std::to_string(report.unsupported_tiles) +
            " terrain tiles use IDs without a proved reconstruction mapping; "
            "no playable Scenario was returned"
        );
        return report;
    }
    if (report.unsupported_triggers != 0) {
        report.diagnostics.push_back(
            std::to_string(report.unsupported_triggers) +
            " triggers contain semantics without a lossless reconstruction "
            "mapping; raw records are preserved and no playable Scenario "
            "was returned"
        );
        return report;
    }
    if (report.unsupported_objects != 0) {
        report.diagnostics.push_back(
            std::to_string(report.unsupported_objects) +
            " object records remain unsupported and are preserved in report"
        );
    }
    if (!report.lossy_objects.empty()) {
        report.diagnostics.push_back(
            std::to_string(report.lossy_objects.size()) +
            " translated objects contain preserved-but-unapplied raw fields"
        );
    }
    report.diagnostics.push_back(
        "commercial object records expose no decoded hit-point field; "
        "reconstruction defaults are used"
    );
    report.scenario = std::move(converted);
    return report;
}

}  // namespace aoe
