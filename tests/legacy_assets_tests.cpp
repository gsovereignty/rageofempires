#include "aoe/legacy_assets.hpp"
#include "aoe/frontend_audio.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

int failures{};

void check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

template <typename Callback>
void check_throws(Callback callback, const char* message) {
    try {
        callback();
        check(false, message);
    } catch (const aoe::LegacyAssetError&) {
    }
}

void put_u16(
    std::vector<std::byte>& bytes,
    std::size_t offset,
    std::uint16_t value
) {
    bytes[offset] = static_cast<std::byte>(value & 0xFF);
    bytes[offset + 1] = static_cast<std::byte>((value >> 8) & 0xFF);
}

void put_u32(
    std::vector<std::byte>& bytes,
    std::size_t offset,
    std::uint32_t value
) {
    for (unsigned index = 0; index < 4; ++index) {
        bytes[offset + index] =
            static_cast<std::byte>((value >> (index * 8)) & 0xFF);
    }
}

std::vector<std::byte> synthetic_slp() {
    std::vector<std::byte> bytes(88);
    bytes[0] = std::byte{'2'};
    bytes[1] = std::byte{'.'};
    bytes[2] = std::byte{'0'};
    bytes[3] = std::byte{'N'};
    put_u32(bytes, 4, 1);

    put_u32(bytes, 32, 72);
    put_u32(bytes, 36, 64);
    put_u32(bytes, 48, 4);
    put_u32(bytes, 52, 2);
    put_u32(bytes, 56, 2);
    put_u32(bytes, 60, static_cast<std::uint32_t>(-3));
    put_u16(bytes, 64, 0);
    put_u16(bytes, 66, 0);
    put_u16(bytes, 68, 0x8000);
    put_u16(bytes, 70, 0x8000);
    put_u32(bytes, 72, 80);
    put_u32(bytes, 76, 88);

    bytes[80] = std::byte{0x08};  // two ordinary colors
    bytes[81] = std::byte{1};
    bytes[82] = std::byte{2};
    bytes[83] = std::byte{0x16};  // one player color
    bytes[84] = std::byte{3};
    bytes[85] = std::byte{0x17};  // one repeated ordinary color
    bytes[86] = std::byte{4};
    bytes[87] = std::byte{0x0F};
    return bytes;
}

aoe::LegacyPalette test_palette() {
    aoe::LegacyPalette palette;
    for (int index = 0; index < 256; ++index) {
        palette.colors.push_back({
            static_cast<std::uint8_t>(index),
            static_cast<std::uint8_t>(255 - index),
            static_cast<std::uint8_t>(index / 2)
        });
    }
    return palette;
}

std::vector<std::byte> read_file(
    const std::filesystem::path& path
) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    check(static_cast<bool>(input), "installed loose asset opens");
    if (!input) return {};
    const auto size = input.tellg();
    check(size >= 0, "installed loose asset size is readable");
    if (size < 0) return {};
    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    input.seekg(0);
    input.read(
        reinterpret_cast<char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size())
    );
    check(
        static_cast<bool>(input),
        "installed loose asset bytes are readable"
    );
    return bytes;
}

std::filesystem::path write_drs(
    const std::vector<std::byte>& payload,
    std::string extension = "slp",
    std::int32_t resource_id = 1234,
    std::string filename = "aoe-legacy-assets-test.drs"
) {
    while (extension.size() < 4) {
        extension.push_back(' ');
    }
    std::vector<std::byte> archive(88 + payload.size());
    const std::string copyright = "Copyright (c) 1997 Ensemble Studios";
    for (std::size_t index = 0; index < copyright.size(); ++index) {
        archive[index] = static_cast<std::byte>(copyright[index]);
    }
    archive[40] = std::byte{'1'};
    archive[41] = std::byte{'.'};
    archive[42] = std::byte{'0'};
    archive[43] = std::byte{'0'};
    put_u32(archive, 56, 1);
    put_u32(archive, 60, 88);
    for (std::size_t index = 0; index < 4; ++index) {
        archive[64 + index] =
            static_cast<std::byte>(extension[3 - index]);
    }
    put_u32(archive, 68, 76);
    put_u32(archive, 72, 1);
    put_u32(
        archive, 76, static_cast<std::uint32_t>(resource_id)
    );
    put_u32(archive, 80, 88);
    put_u32(archive, 84, static_cast<std::uint32_t>(payload.size()));
    std::copy(payload.begin(), payload.end(), archive.begin() + 88);

    const auto path = std::filesystem::temp_directory_path() /
        filename;
    std::ofstream output{path, std::ios::binary | std::ios::trunc};
    output.write(
        reinterpret_cast<const char*>(archive.data()),
        static_cast<std::streamsize>(archive.size())
    );
    return path;
}

std::filesystem::path write_wav_drs(
    const std::vector<std::pair<std::int32_t, std::vector<std::byte>>>&
        resources,
    std::string filename
) {
    const std::size_t index_end = 76 + resources.size() * 12;
    std::size_t payload_size = 0;
    for (const auto& [id, payload] : resources) {
        static_cast<void>(id);
        payload_size += payload.size();
    }
    std::vector<std::byte> archive(index_end + payload_size);
    const std::string copyright = "Copyright (c) 1997 Ensemble Studios";
    for (std::size_t index = 0; index < copyright.size(); ++index) {
        archive[index] = static_cast<std::byte>(copyright[index]);
    }
    archive[40] = std::byte{'1'};
    archive[41] = std::byte{'.'};
    archive[42] = std::byte{'0'};
    archive[43] = std::byte{'0'};
    put_u32(archive, 56, 1);
    put_u32(
        archive, 60, static_cast<std::uint32_t>(index_end)
    );
    const std::string extension = "wav ";
    for (std::size_t index = 0; index < 4; ++index) {
        archive[64 + index] =
            static_cast<std::byte>(extension[3 - index]);
    }
    put_u32(archive, 68, 76);
    put_u32(
        archive, 72, static_cast<std::uint32_t>(resources.size())
    );
    std::size_t payload_offset = index_end;
    for (std::size_t index = 0; index < resources.size(); ++index) {
        const auto& [id, payload] = resources[index];
        const std::size_t entry = 76 + index * 12;
        put_u32(archive, entry, static_cast<std::uint32_t>(id));
        put_u32(
            archive, entry + 4,
            static_cast<std::uint32_t>(payload_offset)
        );
        put_u32(
            archive, entry + 8,
            static_cast<std::uint32_t>(payload.size())
        );
        std::copy(
            payload.begin(), payload.end(),
            archive.begin() + static_cast<std::ptrdiff_t>(payload_offset)
        );
        payload_offset += payload.size();
    }
    const auto path =
        std::filesystem::temp_directory_path() / filename;
    std::ofstream output{path, std::ios::binary | std::ios::trunc};
    output.write(
        reinterpret_cast<const char*>(archive.data()),
        static_cast<std::streamsize>(archive.size())
    );
    return path;
}

std::vector<std::byte> synthetic_wav(std::byte marker) {
    std::vector<std::byte> bytes(16);
    const std::string header = "RIFF";
    const std::string wave = "WAVE";
    for (std::size_t index = 0; index < 4; ++index) {
        bytes[index] = static_cast<std::byte>(header[index]);
        bytes[8 + index] = static_cast<std::byte>(wave[index]);
    }
    put_u32(bytes, 4, 8);
    bytes[12] = marker;
    return bytes;
}

void test_palette_parser() {
    const std::string text =
        "JASC-PAL\r\n0100\r\n3\r\n1 2 3\r\n4 5 6\r\n7 8 9\r\n";
    const auto bytes = std::as_bytes(std::span{text.data(), text.size()});
    const auto palette = aoe::LegacyPalette::from_jasc(bytes);
    check(palette.colors.size() == 3, "palette reads color count");
    check(
        palette.colors[1] == std::array<std::uint8_t, 3>{4, 5, 6},
        "palette reads RGB values"
    );
    const std::string invalid = "JASC-PAL\n0100\n1\n999 0 0\n";
    check_throws(
        [&] {
            aoe::LegacyPalette::from_jasc(
                std::as_bytes(
                    std::span{invalid.data(), invalid.size()}
                )
            );
        },
        "palette rejects invalid channels"
    );
}

void test_slp_decoder() {
    const auto slp = synthetic_slp();
    check(aoe::slp_frame_count(slp) == 1, "SLP reads frame count");
    const auto frame =
        aoe::decode_slp_frame(slp, test_palette(), 0, 2);
    check(frame.width == 4 && frame.height == 2, "SLP reads dimensions");
    check(
        frame.hotspot_x == 2 && frame.hotspot_y == -3,
        "SLP reads hotspot"
    );
    check(
        frame.rgba[0] == 1 && frame.rgba[3] == 255,
        "SLP maps ordinary palette color"
    );
    check(
        frame.rgba[8] == 35,
        "SLP applies player palette bank"
    );
    constexpr std::array<std::uint8_t, 8> player_bases{
        16, 32, 48, 64, 96, 112, 128, 80,
    };
    for (std::size_t index = 0; index < player_bases.size(); ++index) {
        const auto slot = aoe::PlayerSlotId::from_index(index);
        const auto colored = aoe::decode_slp_frame(
            slp, test_palette(), 0, *slot
        );
        check(
            colored.rgba[8] ==
                static_cast<std::uint8_t>(player_bases[index] + 3),
            "SLP fixture decodes exact eight-player color"
        );
    }
    const auto neutral = aoe::decode_slp_frame(
        slp, test_palette(), 0, aoe::PlayerSlotId::neutral()
    );
    check(neutral.rgba[8] == 3, "neutral player color stays unchanged");
    for (std::size_t index = 0; index < player_bases.size(); ++index) {
        const auto slot = aoe::PlayerSlotId::from_index(index);
        const auto colored = aoe::decode_slp_frame(
            slp, test_palette(), 0, *slot
        );
        bool alpha_footprint_matches = true;
        bool non_player_pixels_match = true;
        for (std::size_t byte = 0; byte < colored.rgba.size(); ++byte) {
            if (byte % 4 == 3 &&
                colored.rgba[byte] != neutral.rgba[byte]) {
                alpha_footprint_matches = false;
            }
            const bool player_color_channel = byte >= 8 && byte < 12;
            if (!player_color_channel &&
                colored.rgba[byte] != neutral.rgba[byte]) {
                non_player_pixels_match = false;
            }
        }
        check(
            alpha_footprint_matches,
            "player remap cannot expand decoded opacity footprint"
        );
        check(
            non_player_pixels_match,
            "player remap changes only player-color pixel RGB"
        );
    }
    check(
        frame.rgba[12] == 4,
        "SLP decodes fill"
    );
    check(
        frame.rgba[4 * 4 + 3] == 0,
        "SLP leaves transparent row alpha zero"
    );

    auto truncated = slp;
    truncated.pop_back();
    check_throws(
        [&] {
            static_cast<void>(
                aoe::decode_slp_frame(truncated, test_palette(), 0)
            );
        },
        "SLP rejects truncated command stream"
    );
}

void test_drs_archive() {
    const auto slp = synthetic_slp();
    const auto path = write_drs(slp);
    const aoe::DrsArchive archive{path};
    check(archive.entry_count() == 1, "DRS indexes entries");
    check(archive.contains("SLP", 1234), "DRS normalizes extension");
    check(
        archive.resource_ids("slp") == std::vector<std::int32_t>{1234},
        "DRS lists typed resource identifiers"
    );
    check(archive.read("slp", 1234) == slp, "DRS reads exact payload");
    check_throws(
        [&] { static_cast<void>(archive.read("slp", 9999)); },
        "DRS rejects missing resource"
    );

    std::vector<std::byte> broken(64);
    broken[40] = std::byte{'1'};
    broken[41] = std::byte{'.'};
    put_u32(broken, 56, 1);
    const auto broken_path =
        std::filesystem::temp_directory_path() /
        "aoe-legacy-assets-broken-test.drs";
    std::ofstream output{
        broken_path,
        std::ios::binary | std::ios::trunc
    };
    output.write(
        reinterpret_cast<const char*>(broken.data()),
        static_cast<std::streamsize>(broken.size())
    );
    output.close();
    check_throws(
        [&] { aoe::DrsArchive invalid{broken_path}; },
        "DRS rejects truncated table"
    );
}

void test_wav_resources() {
    const auto root = std::filesystem::temp_directory_path() /
        "aoe-legacy-wav-resources-test";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "Data");
    const auto training = synthetic_wav(std::byte{0x37});
    const auto trebuchet = synthetic_wav(std::byte{0x29});
    const auto override = synthetic_wav(std::byte{0x42});
    std::filesystem::rename(
        write_wav_drs(
            {{5423, training}, {5366, synthetic_wav(std::byte{0x11})}},
            "aoe-sound-base-test.drs"
        ),
        root / "Data" / "sounds.drs"
    );
    std::filesystem::rename(
        write_wav_drs(
            {{5366, trebuchet}, {6450, override}},
            "aoe-sound-x1-test.drs"
        ),
        root / "Data" / "sounds_x1.drs"
    );

    const aoe::LegacyWavResources resources{root};
    check(resources.archive_count() == 2, "WAV indexes sound archives");
    check(resources.contains(5366), "WAV finds Trebuchet resource");
    check(resources.contains(5423), "WAV finds training resource");
    check(resources.contains(6450), "WAV finds expansion-only resource");
    check(
        resources.read(5366) == trebuchet,
        "sounds_x1 WAV overrides repeated base resource ID"
    );
    check(
        resources.read(5423) == training,
        "sounds_x1 lookup falls back to base WAV"
    );
    check(
        resources.read(6450) == override,
        "WAV reads expansion-only RIFF bytes"
    );
    check(
        resources.resource_ids() ==
            std::vector<std::int32_t>{5366, 5423, 6450},
        "WAV lists sorted resource IDs"
    );
    check_throws(
        [&] { static_cast<void>(resources.read(9999)); },
        "WAV rejects missing resource"
    );

    std::filesystem::rename(
        write_drs(
            override, "wav", 5366, "aoe-sound-x2-test.drs"
        ),
        root / "Data" / "sounds_x2.drs"
    );
    const aoe::LegacyWavResources expanded{root};
    check(
        expanded.read(5366) == override,
        "later expansion WAV overrides repeated resource ID"
    );

    std::filesystem::remove(root / "Data" / "sounds_x1.drs");
    std::filesystem::remove(root / "Data" / "sounds_x2.drs");
    const aoe::LegacyWavResources base_only{root};
    check(base_only.archive_count() == 1, "WAV preserves base-only root");
    check(
        base_only.read(5423) == training,
        "base-only root reads original WAV"
    );
    std::ofstream{
        root / "Data" / "sounds_x1.drs",
        std::ios::binary | std::ios::trunc
    }.write("bad", 3);
    const aoe::LegacyWavResources corrupt_expansion{root};
    check(
        corrupt_expansion.archive_count() == 1 &&
            corrupt_expansion.read(5423) == training,
        "malformed expansion archive preserves valid base fallback"
    );
    std::filesystem::remove(root / "Data" / "sounds_x1.drs");

    auto invalid = synthetic_wav(std::byte{0});
    invalid[0] = std::byte{'N'};
    std::filesystem::remove(root / "Data" / "sounds_x2.drs");
    std::filesystem::rename(
        write_drs(
            invalid, "wav", 6000, "aoe-sound-invalid-test.drs"
        ),
        root / "Data" / "sounds_x2.drs"
    );
    const aoe::LegacyWavResources invalid_resources{root};
    check_throws(
        [&] { static_cast<void>(invalid_resources.read(6000)); },
        "WAV rejects invalid RIFF resource"
    );
    std::filesystem::remove_all(root);
}

void test_music_discovery() {
    const auto root = std::filesystem::temp_directory_path() /
        "aoe-legacy-music-discovery-test";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(
        root / "app" / "Sound" / "stream"
    );
    for (const std::string name :
         {"Zeta.MP3", "town.mp3", "Alpha.wav", "notes.txt"}) {
        std::ofstream{root / "app" / "Sound" / "stream" / name}.put('x');
    }
    std::filesystem::create_directories(
        root / "app" / "Sound" / "stream" / "nested"
    );
    std::ofstream{
        root / "app" / "Sound" / "stream" / "nested" / "ignored.mp3"
    }.put('x');

    const auto tracks = aoe::discover_legacy_music_tracks(root);
    check(tracks.size() == 3, "music discovers supported direct files");
    check(
        tracks[0].filename() == "Alpha.wav" &&
            tracks[1].filename() == "town.mp3" &&
            tracks[2].filename() == "Zeta.MP3",
        "music discovery uses deterministic case-insensitive order"
    );

    std::filesystem::remove_all(root);
}

void test_command_acknowledgement_mapping() {
    check(
        aoe::accepted_command_sound(aoe::UnitKind::archer) == 422 &&
            aoe::accepted_command_sound(
                aoe::UnitKind::bombard_cannon
            ) == 422,
        "DAT command acknowledgement maps represented 422 units"
    );
    check(
        aoe::accepted_command_sound(
            aoe::UnitKind::packed_trebuchet
        ) == 484,
        "packed Trebuchet retains distinct command sound"
    );
    check(
        aoe::accepted_command_sound(aoe::UnitKind::villager) == 301 &&
            aoe::accepted_command_sound(
                aoe::UnitKind::missionary
            ) == 424,
        "civilian and religious command sounds retain DAT identities"
    );
    check(
        aoe::accepted_command_sound(aoe::UnitKind::knight) == 326 &&
            aoe::accepted_command_sound(
                aoe::UnitKind::scout_cavalry
            ) == 474 &&
            aoe::accepted_command_sound(
                aoe::UnitKind::cavalry_archer
            ) == 415 &&
            aoe::accepted_command_sound(
                aoe::UnitKind::war_elephant
            ) == 483,
        "cavalry acknowledgement families follow live unit records"
    );
}

void test_live_unit_event_sound_mapping() {
    check(
        aoe::selected_sound(aoe::UnitKind::villager) == 303 &&
            aoe::movement_sound(aoe::UnitKind::villager) == 301 &&
            aoe::trained_sound(aoe::UnitKind::villager) == 317,
        "Villager event fields follow live DAT"
    );
    check(
        aoe::selected_sound(aoe::UnitKind::scorpion) == 490 &&
            aoe::movement_sound(aoe::UnitKind::scorpion) == 476 &&
            aoe::selected_sound(aoe::UnitKind::onager) == 489,
        "siege selection and movement fields follow live DAT"
    );
    check(
        aoe::selected_sound(aoe::UnitKind::trade_cog) == 339 &&
            aoe::movement_sound(aoe::UnitKind::trade_cog) == 340 &&
            aoe::trained_sound(aoe::UnitKind::trade_cog) == 338,
        "naval event fields follow live DAT"
    );
    check(
        aoe::selected_sound(aoe::UnitKind::trade_cart) == -1 &&
            aoe::trained_sound(aoe::UnitKind::trade_cart) == -1 &&
            aoe::movement_sound(aoe::UnitKind::trade_cart) == 306,
        "civilization-varying Trade Cart fields stay unsupported"
    );

    int selected{};
    int commanded{};
    int moved{};
    int trained{};
    for (int value = 0;
         value <= static_cast<int>(aoe::UnitKind::elite_woad_raider);
         ++value) {
        const auto kind = static_cast<aoe::UnitKind>(value);
        selected += aoe::selected_sound(kind) >= 0;
        commanded += aoe::accepted_command_sound(kind) >= 0;
        moved += aoe::movement_sound(kind) >= 0;
        trained += aoe::trained_sound(kind) >= 0;
    }
    check(
        selected == 94 && commanded == 93 &&
            moved == 93 && trained == 91,
        "all 96 represented UnitKind event mappings stay accounted"
    );
    int buildings{};
    for (int value = 0;
         value <= static_cast<int>(aoe::BuildingKind::wonder);
         ++value) {
        buildings += aoe::selected_sound(
            static_cast<aoe::BuildingKind>(value)
        ) >= 0;
    }
    check(
        buildings == 27 &&
            aoe::selected_sound(aoe::BuildingKind::town_center) == 17 &&
            aoe::selected_sound(aoe::BuildingKind::market) == 16 &&
            aoe::selected_sound(aoe::BuildingKind::wonder) == 383,
        "all 27 represented BuildingKind selection sounds match DAT"
    );
}

void test_installed_assets_if_requested() {
    const char* root = std::getenv("AOE_ASSET_ROOT");
    if (root == nullptr || *root == '\0') {
        return;
    }
    std::filesystem::path installation{root};
    if (std::filesystem::is_directory(installation / "app" / "Data")) {
        installation /= "app";
    }
    const aoe::DrsArchive interface_archive{
        installation / "Data" / "interfac.drs"
    };
    const aoe::DrsArchive graphics_archive{
        installation / "Data" / "graphics.drs"
    };
    const aoe::LegacyWavResources sounds{installation};
    const auto palette_bytes = interface_archive.read("bina", 50500);
    const auto palette = aoe::LegacyPalette::from_jasc(palette_bytes);
    check(
        palette.colors.size() >= 256,
        "installed interface archive provides main palette"
    );
    const auto slp_ids = graphics_archive.resource_ids("slp");
    check(!slp_ids.empty(), "installed graphics archive contains SLP files");
    bool decoded{};
    for (std::int32_t id : slp_ids) {
        try {
            const auto slp = graphics_archive.read("slp", id);
            if (aoe::slp_frame_count(slp) == 0) {
                continue;
            }
            const auto frame =
                aoe::decode_slp_frame(slp, palette, 0, 1);
            if (frame.width > 0 && frame.height > 0) {
                decoded = true;
                break;
            }
        } catch (const aoe::LegacyAssetError&) {
            // Some archive entries use newer SLP variants. Find and prove one
            // classic 2.0 entry without weakening strict format checks.
        }
    }
    check(decoded, "installed graphics archive decodes classic SLP frame");

    const auto game_palette = aoe::LegacyPalette::from_jasc(
        read_file(installation / "Data" / "pal_2.pal")
    );
    using Metadata = std::array<int, 4>;
    constexpr std::array<Metadata, 18> frame1{{
        {325,175,0,0},{325,175,0,0},{322,175,0,0},
        {324,175,0,0},{325,175,0,0},{325,175,0,0},
        {325,175,0,0},{325,175,0,0},{325,175,0,0},
        {325,175,0,0},{325,175,0,0},{325,175,0,0},
        {325,175,0,0},{325,175,0,0},{325,175,0,0},
        {325,175,0,0},{325,175,0,0},{325,175,0,0},
    }};
    constexpr std::array<Metadata, 18> frame4{{
        {391,175,0,0},{384,175,-7,0},{391,175,0,0},
        {389,175,-2,0},{391,175,0,0},{386,175,-5,0},
        {390,175,-1,0},{391,175,0,0},{391,175,0,0},
        {391,175,0,0},{391,175,0,0},{391,175,0,0},
        {391,175,0,0},{391,175,0,0},{391,175,0,0},
        {391,175,0,0},{391,175,0,0},{391,175,0,0},
    }};
    constexpr std::array<Metadata, 18> frame5{{
        {239,113,0,-17},{226,107,-5,-7},{181,130,-54,0},
        {161,116,-56,-7},{239,117,0,-13},{239,130,0,0},
        {217,126,-4,-2},{239,130,0,0},{239,130,0,0},
        {239,130,0,0},{238,129,-1,-1},{238,129,-1,-1},
        {133,128,-46,-2},{239,130,0,0},{153,121,-53,-3},
        {120,120,-68,-9},{118,126,-53,-4},{239,130,0,0},
    }};
    constexpr std::array<Metadata, 18> frame6{{
        {392,25,-2,-1},{392,25,-2,-1},{392,25,-2,-1},
        {392,25,-2,-1},{392,25,-2,-1},{392,25,-2,-1},
        {392,25,-2,-1},{396,28,0,0},{392,25,-2,-1},
        {392,25,-2,-1},{392,25,-2,-1},{392,25,-2,-1},
        {392,25,-2,-1},{396,28,0,0},{392,25,-2,-1},
        {392,25,-2,-1},{392,25,-2,-1},{396,28,0,0},
    }};
    constexpr std::array<Metadata, 18> frame7{{
        {165,21,-2,-5},{165,21,-2,-5},{165,21,-2,-5},
        {165,21,-2,-5},{165,21,-2,-5},{165,21,-2,-5},
        {165,21,-2,-5},{169,32,0,0},{165,21,-2,-5},
        {165,21,-2,-5},{165,21,-2,-5},{165,21,-2,-5},
        {165,21,-2,-5},{169,32,0,0},{165,21,-2,-5},
        {165,21,-2,-5},{165,21,-2,-5},{169,32,0,0},
    }};
    const std::array expected_by_frame{
        frame1, frame4, frame5, frame6, frame7
    };
    for (int civilization = 1; civilization <= 18; ++civilization) {
        const auto bytes = read_file(
            installation / "Data" / "Slp" /
            ("game_b" + std::to_string(civilization) + ".slp")
        );
        check(
            aoe::slp_frame_count(bytes) == 8,
            "installed civilization HUD has exactly eight frames"
        );
        for (std::size_t frame_index = 0;
             frame_index < 8;
             ++frame_index) {
            aoe::RgbaFrame frame;
            try {
                frame = aoe::decode_slp_frame(
                    bytes, game_palette, frame_index
                );
            } catch (const aoe::LegacyAssetError& error) {
                std::cerr
                    << "FAIL: game_b" << civilization
                    << ".slp frame " << frame_index
                    << ": " << error.what() << '\n';
                ++failures;
                continue;
            }
            check(
                frame.width > 0 && frame.height > 0 &&
                    frame.rgba.size() ==
                        static_cast<std::size_t>(
                            frame.width * frame.height * 4
                        ),
                "installed civilization HUD frame decodes"
            );
            if (frame_index == 0) {
                check(
                    frame.width == 32 && frame.height == 32 &&
                        frame.hotspot_x == 0 &&
                        frame.hotspot_y == 0,
                    "civilization HUD frame 0 metadata is exact"
                );
            } else if (frame_index == 1) {
                const Metadata expected = frame1[
                    static_cast<std::size_t>(civilization - 1)
                ];
                check(
                    frame.width == expected[0] &&
                        frame.height == expected[1] &&
                        frame.hotspot_x == expected[2] &&
                        frame.hotspot_y == expected[3],
                    "civilization HUD frame 1 proves bottom split"
                );
            } else if (frame_index == 2 ||
                       frame_index == 3) {
                check(
                    frame.width == 34 && frame.height == 175 &&
                        frame.hotspot_x == 0 &&
                        frame.hotspot_y == 0,
                    "civilization HUD alternating tile metadata is exact"
                );
            } else {
                const std::size_t metadata_index =
                    frame_index == 1 ? 0 :
                    frame_index == 4 ? 1 :
                    frame_index == 5 ? 2 :
                    frame_index == 6 ? 3 : 4;
                const Metadata expected =
                    expected_by_frame[metadata_index][
                        static_cast<std::size_t>(civilization - 1)
                    ];
                check(
                    frame.width == expected[0] &&
                        frame.height == expected[1] &&
                        frame.hotspot_x == expected[2] &&
                        frame.hotspot_y == expected[3],
                    "civilization HUD variable frame metadata is exact"
                );
            }
        }
    }
    if (sounds.archive_count() != 0) {
        check(
            sounds.contains(5366) && sounds.contains(5423),
            "installed sounds contain Trebuchet and training WAVs"
        );
        const auto trebuchet = sounds.read(5366);
        const auto training = sounds.read(5423);
        check(
            trebuchet.size() >= 12 && training.size() >= 12,
            "installed known WAV resources materialize"
        );
    }
}

}  // namespace

int main() {
    test_palette_parser();
    test_slp_decoder();
    test_drs_archive();
    test_wav_resources();
    test_music_discovery();
    test_command_acknowledgement_mapping();
    test_live_unit_event_sound_mapping();
    test_installed_assets_if_requested();
    if (failures != 0) {
        std::cerr << failures << " legacy asset checks failed\n";
        return 1;
    }
    std::cout << "All legacy asset checks passed\n";
    return 0;
}
