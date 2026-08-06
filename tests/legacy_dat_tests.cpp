#include "aoe/legacy_dat.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

int failures{};

void check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void put_u16(
    std::vector<std::byte>& bytes,
    std::size_t offset,
    std::uint16_t value
) {
    bytes[offset] = static_cast<std::byte>(value & 0xFF);
    bytes[offset + 1] = static_cast<std::byte>(value >> 8);
}

void put_u32(
    std::vector<std::byte>& bytes,
    std::size_t offset,
    std::uint32_t value
) {
    put_u16(bytes, offset, static_cast<std::uint16_t>(value));
    put_u16(bytes, offset + 2, static_cast<std::uint16_t>(value >> 16));
}

void put_f32(
    std::vector<std::byte>& bytes,
    std::size_t offset,
    float value
) {
    put_u32(bytes, offset, std::bit_cast<std::uint32_t>(value));
}

void put_i16(
    std::vector<std::byte>& bytes,
    std::size_t offset,
    std::int16_t value
) {
    put_u16(bytes, offset, static_cast<std::uint16_t>(value));
}

void put_i32(
    std::vector<std::byte>& bytes,
    std::size_t offset,
    std::int32_t value
) {
    put_u32(bytes, offset, static_cast<std::uint32_t>(value));
}

void put_fixed(
    std::vector<std::byte>& bytes,
    std::size_t offset,
    const char* value,
    std::size_t capacity
) {
    for (std::size_t index = 0; value[index] != '\0' && index < capacity;
         ++index) {
        bytes[offset + index] = static_cast<std::byte>(value[index]);
    }
}

std::vector<std::byte> fixture() {
    std::vector<std::byte> bytes(168);
    const char version[] = "VER 5.7";
    for (std::size_t index = 0; index < 7; ++index) {
        bytes[index] = static_cast<std::byte>(version[index]);
    }
    put_u16(bytes, 14, 1);
    put_u16(bytes, 16, 291);
    put_i16(bytes, 18, 4);
    put_u16(bytes, 20, 2);
    put_i32(bytes, 22, 900);

    put_fixed(bytes, 26, "TREB.WAV", 13);
    put_i32(bytes, 39, 5366);
    put_i16(bytes, 43, 70);
    put_i16(bytes, 45, -1);
    put_i16(bytes, 47, -1);

    put_fixed(bytes, 49, "TREBALT.WAV", 13);
    put_i32(bytes, 62, 6424);
    put_i16(bytes, 66, 30);
    put_i16(bytes, 68, 15);
    put_i16(bytes, 70, 2);

    put_u16(bytes, 72, 1);
    put_u32(bytes, 74, 1);
    const std::size_t record = 78;
    bytes[record] = std::byte{'T'};
    bytes[record + 21] = std::byte{'F'};
    put_u32(bytes, record + 34, 57);
    bytes[record + 40] = std::byte{20};
    put_u16(bytes, record + 57, 1);
    put_u16(bytes, record + 59, 1);
    put_f32(bytes, record + 61, 1.25F);
    put_f32(bytes, record + 65, 0.05F);
    put_f32(bytes, record + 69, 0.10F);
    bytes[record + 73] = std::byte{11};
    put_u16(bytes, record + 74, 0);
    bytes[record + 56] = std::byte{1};
    put_i16(bytes, record + 78, 0);
    put_i16(bytes, record + 80, 291);
    put_i16(bytes, record + 82, 4);
    put_i16(bytes, record + 84, 420);
    put_i16(bytes, record + 86, -1);
    put_i16(bytes, record + 88, -1);
    return bytes;
}

}  // namespace

int main() {
    const auto dat = aoe::LegacyDatFile::from_decompressed(fixture());
    check(dat.version() == "VER 5.7", "version");
    check(dat.sound_count() == 1, "sound count");
    check(dat.sounds().size() == 1, "stored sound count");
    const auto* sound = dat.sound(291);
    check(sound != nullptr, "sound lookup");
    check(sound && sound->play_delay == 4, "sound play delay");
    check(sound && sound->cache_time == 900, "sound cache time");
    check(sound && sound->items.size() == 2, "sound item count");
    check(
        sound && sound->items[0].filename == "TREB.WAV" &&
            sound->items[0].resource_id == 5366 &&
            sound->items[0].probability == 70 &&
            sound->items[0].civilization == -1 &&
            sound->items[0].icon_set == -1,
        "first sound item"
    );
    check(
        sound && sound->items[1].filename == "TREBALT.WAV" &&
            sound->items[1].resource_id == 6424 &&
            sound->items[1].probability == 30 &&
            sound->items[1].civilization == 15 &&
            sound->items[1].icon_set == 2,
        "second sound item"
    );
    check(dat.sound(9999) == nullptr, "missing sound lookup");
    const aoe::LegacySound civilization_sound{
        420,
        0,
        0,
        {
            {"GENERIC.WAV", 5299, 100, -1, -1},
            {"SPANISH1.WAV", 6691, 33, 14, -1},
            {"SPANISH2.WAV", 6692, 34, 14, -1},
            {"HUN1.WAV", 6511, 33, 17, -1},
            {"HUN2.WAV", 6512, 33, 17, -1},
        }
    };
    check(
        aoe::select_legacy_sound_item(civilization_sound, 14, 0)->
            resource_id == 6691 &&
        aoe::select_legacy_sound_item(civilization_sound, 14, 33)->
            resource_id == 6692,
        "civilization sound uses exact probability ranges"
    );
    check(
        aoe::select_legacy_sound_item(civilization_sound, 17, 33)->
            resource_id == 6512,
        "civilization sound selects later weighted range"
    );
    check(
        aoe::select_legacy_sound_item(civilization_sound, 7, 999)->
            resource_id == 5299,
        "civilization sound falls back to generic record"
    );
    check(dat.graphics().size() == 1, "graphic count");
    const auto* graphic = dat.graphic(0);
    check(graphic != nullptr, "present graphic");
    check(graphic && graphic->slp_id == 57, "SLP ID");
    check(graphic && graphic->layer == 20, "layer");
    check(graphic && graphic->frame_count == 1, "frame count");
    check(graphic && graphic->angle_count == 1, "angle count");
    check(graphic && graphic->speed_adjust == 1.25F, "graphic speed adjust");
    check(graphic && graphic->frame_rate == 0.05F, "graphic frame rate");
    check(graphic && graphic->replay_delay == 0.10F, "graphic replay delay");
    check(graphic && graphic->sequence_type == 11, "graphic sequence type");
    check(
        graphic && graphic->angle_sounds.size() == 1 &&
            graphic->angle_sounds[0][0].frame == 0 &&
            graphic->angle_sounds[0][0].sound_id == 291 &&
            graphic->angle_sounds[0][1].frame == 4 &&
            graphic->angle_sounds[0][1].sound_id == 420,
        "graphic frame sound triggers"
    );
    check(dat.terrain_block_offset() == fixture().size(), "prefix end offset");

    if (const char* path = std::getenv("AOE_TEST_DAT")) {
        const auto live = aoe::LegacyDatFile::load(path);
        check(live.terrain_restriction_count() == 22, "live restrictions");
        check(live.terrain_count() == 41, "live used terrains");
        check(live.player_color_count() == 15, "live player colors");
        check(live.sound_count() == 506, "live sounds");
        check(live.sounds().size() == 506, "live stored sounds");
        const auto* trebuchet_sound = live.sound(291);
        check(
            trebuchet_sound && trebuchet_sound->play_delay == 0 &&
                trebuchet_sound->items.size() == 1 &&
                trebuchet_sound->items[0].resource_id == 5366 &&
                trebuchet_sound->items[0].probability == 100 &&
                trebuchet_sound->items[0].civilization == -1 &&
                trebuchet_sound->items[0].icon_set == -1,
            "live trebuchet sound"
        );
        const auto* training_sound = live.sound(337);
        check(
            training_sound && training_sound->items.size() == 1 &&
                training_sound->items[0].resource_id == 5423 &&
                training_sound->items[0].probability == 100,
            "live land training sound"
        );
        const auto* selection_sound = live.sound(420);
        check(
            selection_sound &&
                aoe::select_legacy_sound_item(*selection_sound, 14, 0)->
                    resource_id == 6692 &&
                aoe::select_legacy_sound_item(*selection_sound, 17, 0)->
                    resource_id == 6513 &&
                aoe::select_legacy_sound_item(*selection_sound, 99, 0)->
                    resource_id == 5299,
            "live civilization-aware selection sound"
        );
        check(live.graphics().size() == 7367, "live graphics");
        check(live.terrain_block_offset() == 917004, "live terrain start");
        check(live.stored_terrain_count() == 42, "live stored terrains");
        check(live.random_map_offset() == 960744, "live random-map start");
        check(live.random_map_count() == 3, "live random-map count");
        const auto* relic = live.graphic(647);
        check(
            relic && relic->name == "ARTCT_FN" && relic->slp_id == 53,
            "live artifact graphic mapping"
        );
        const auto* archery = live.graphic(9);
        check(
            archery && archery->name == "ARRG2NNE" &&
                archery->slp_id == 21 && archery->player_color == -1 &&
                archery->deltas.size() == 3,
            "live archery-range composite"
        );
    }

    return failures == 0 ? 0 : 1;
}
