#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <source_location>
#include <string>
#include <vector>

#include <zlib.h>

#include "aoe/legacy_scenario.hpp"
#include "aoe/legacy_campaign.hpp"

namespace {

void require(
    bool condition,
    const std::source_location location = std::source_location::current()
) {
    if (!condition) {
        std::cerr << "Requirement failed at " << location.file_name() << ':'
                  << location.line() << '\n';
        std::abort();
    }
}

void append_u32(std::vector<std::byte>& bytes, std::uint32_t value) {
    for (int shift : {0, 8, 16, 24}) {
        bytes.push_back(
            static_cast<std::byte>((value >> shift) & 0xffU)
        );
    }
}

void append_f32(std::vector<std::byte>& bytes, float value) {
    std::uint32_t bits{};
    std::memcpy(&bits, &value, sizeof(bits));
    append_u32(bytes, bits);
}

void append_f64(std::vector<std::byte>& bytes, double value) {
    std::uint64_t bits{};
    std::memcpy(&bits, &value, sizeof(bits));
    append_u32(bytes, static_cast<std::uint32_t>(bits));
    append_u32(bytes, static_cast<std::uint32_t>(bits >> 32U));
}

void append_string32(
    std::vector<std::byte>& bytes,
    const std::string& value
) {
    append_u32(bytes, value.size() + 1);
    for (char character : value) {
        bytes.push_back(static_cast<std::byte>(character));
    }
    bytes.push_back(std::byte{});
}

void append_zeroes(std::vector<std::byte>& bytes, std::size_t count) {
    bytes.insert(bytes.end(), count, std::byte{});
}

std::vector<std::byte> deflate_raw(
    const std::vector<std::byte>& plain
) {
    z_stream stream{};
    require(deflateInit2(
        &stream,
        Z_BEST_COMPRESSION,
        Z_DEFLATED,
        -MAX_WBITS,
        8,
        Z_DEFAULT_STRATEGY
    ) == Z_OK);
    stream.next_in = reinterpret_cast<Bytef*>(
        const_cast<std::byte*>(plain.data())
    );
    stream.avail_in = static_cast<uInt>(plain.size());
    std::vector<std::byte> compressed(deflateBound(&stream, plain.size()));
    stream.next_out = reinterpret_cast<Bytef*>(compressed.data());
    stream.avail_out = static_cast<uInt>(compressed.size());
    require(deflate(&stream, Z_FINISH) == Z_STREAM_END);
    compressed.resize(stream.total_out);
    deflateEnd(&stream);
    return compressed;
}

std::vector<std::byte> classic_fixture() {
    std::vector<std::byte> header;
    append_u32(header, 2);
    append_u32(header, 0x12345678U);
    const std::string description = "Fixture scenario";
    append_u32(header, description.size() + 1);
    for (char value : description) {
        header.push_back(static_cast<std::byte>(value));
    }
    header.push_back(std::byte{});
    append_u32(header, 1);
    append_u32(header, 8);

    std::vector<std::byte> body;
    append_u32(body, 4242);
    append_f32(body, 1.21F);
    append_zeroes(body, 16 * 256 + 16 * 4 + 16 * 16);
    append_zeroes(body, 1 + 8);
    append_zeroes(body, 2 + 5 * 4);
    append_zeroes(body, 5 * 2);
    append_zeroes(body, 4 * 2);
    append_zeroes(body, 14);
    append_zeroes(body, 32 * 2 + 16 * 2);
    append_zeroes(body, 16 * 3 * 4 + 16);
    append_u32(body, static_cast<std::uint32_t>(-99));
    append_zeroes(body, 16 * 6 * 4);
    append_u32(body, static_cast<std::uint32_t>(-99));
    append_zeroes(body, 6 * 4 + 4 + 3 * 4);
    append_zeroes(body, 16 * 16 * 4 + 16 * 12 * 60);
    append_u32(body, static_cast<std::uint32_t>(-99));
    append_zeroes(body, 16 * 4);
    append_zeroes(body, 16 * 4 + 16 * 30 * 4);
    append_zeroes(body, 16 * 4 + 16 * 30 * 4);
    append_zeroes(body, 16 * 4 + 16 * 20 * 4);
    append_zeroes(body, 4 + 8 + 16 * 4);
    append_u32(body, static_cast<std::uint32_t>(-99));
    append_zeroes(body, 8 + 4);
    append_u32(body, 2);
    append_u32(body, 1);
    body.insert(body.end(), {
        std::byte{0}, std::byte{1}, std::byte{2},
        std::byte{7}, std::byte{3}, std::byte{4},
    });
    append_u32(body, 1);
    append_u32(body, 1);
    append_f32(body, 2.5F);
    append_f32(body, 3.5F);
    append_f32(body, 0.0F);
    append_u32(body, 77);
    body.push_back(std::byte{42});
    body.push_back(std::byte{0});
    body.push_back(std::byte{5});
    append_f32(body, 1.5F);
    body.push_back(std::byte{3});
    body.push_back(std::byte{0});
    append_u32(body, static_cast<std::uint32_t>(-1));
    append_u32(body, 1);
    append_f64(body, 1.6);
    body.push_back(std::byte{0});
    append_u32(body, 1);
    append_u32(body, 1);
    body.push_back(std::byte{0});
    append_u32(body, 9);
    body.push_back(std::byte{1});
    append_u32(body, 2);
    append_u32(body, 30);
    append_string32(body, "Objective");
    append_string32(body, "Fixture trigger");
    append_u32(body, 1);
    append_u32(body, 7);
    append_u32(body, 24);
    for (int property = 0; property < 24; ++property) {
        append_u32(body, property == 4 ? 1 : 0);
    }
    append_string32(body, "Hello");
    append_string32(body, "");
    append_u32(body, 77);
    append_u32(body, 0);
    append_u32(body, 1);
    append_u32(body, 10);
    append_u32(body, 18);
    append_zeroes(body, 18 * 4);
    append_u32(body, 0);
    append_u32(body, 0);

    std::vector<std::byte> file{
        std::byte{'1'}, std::byte{'.'}, std::byte{'2'}, std::byte{'1'}
    };
    append_u32(file, header.size());
    file.insert(file.end(), header.begin(), header.end());
    const auto compressed = deflate_raw(body);
    file.insert(file.end(), compressed.begin(), compressed.end());
    return file;
}

std::filesystem::path write_fixture(
    const std::string& name,
    const std::vector<std::byte>& bytes
) {
    const auto path =
        std::filesystem::temp_directory_path() / ("aoe-" + name + ".scx");
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(
        reinterpret_cast<const char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size())
    );
    require(static_cast<bool>(output));
    return path;
}

void write_fixed(
    std::vector<std::byte>& bytes,
    std::size_t offset,
    std::size_t capacity,
    const std::string& value
) {
    require(value.size() <= capacity);
    for (std::size_t index = 0; index < value.size(); ++index) {
        bytes[offset + index] = static_cast<std::byte>(value[index]);
    }
}

std::vector<std::byte> classic_campaign_fixture() {
    constexpr std::size_t count = 2;
    constexpr std::size_t index_end = 264 + count * 520;
    std::vector<std::byte> bytes(index_end);
    write_fixed(bytes, 0, 4, "1.00");
    write_fixed(bytes, 4, 256, "Fixture campaign");
    bytes[260] = static_cast<std::byte>(count);

    const auto scenario = classic_fixture();
    const std::vector<std::byte> unsupported{
        std::byte{'N'}, std::byte{'O'}, std::byte{'P'}, std::byte{'E'},
    };
    const std::size_t first_offset = index_end + 3;
    const std::size_t second_offset = first_offset + scenario.size();
    auto write_entry = [&](std::size_t index,
                           std::size_t size,
                           std::size_t offset,
                           const std::string& name,
                           const std::string& filename) {
        const std::size_t cursor = 264 + index * 520;
        for (int shift : {0, 8, 16, 24}) {
            bytes[cursor + shift / 8] =
                static_cast<std::byte>((size >> shift) & 0xffU);
            bytes[cursor + 4 + shift / 8] =
                static_cast<std::byte>((offset >> shift) & 0xffU);
        }
        write_fixed(bytes, cursor + 8, 255, name);
        write_fixed(bytes, cursor + 263, 255, filename);
    };
    write_entry(0, scenario.size(), first_offset, "First", "first.scx");
    write_entry(
        1,
        unsupported.size(),
        second_offset,
        "Unsupported",
        "unknown.scx"
    );
    bytes.insert(bytes.end(), 3, std::byte{0x5a});
    bytes.insert(bytes.end(), scenario.begin(), scenario.end());
    bytes.insert(bytes.end(), unsupported.begin(), unsupported.end());
    return bytes;
}

std::vector<std::byte> cpx2_campaign_fixture() {
    const auto scenario = classic_fixture();
    const std::string scenario_name = "First mission";
    const std::string filename = "first.scx";
    const std::size_t index_end = 4 + 8 + 256 + 4 + 8 + 4 +
        scenario_name.size() + 4 + filename.size();
    std::vector<std::byte> bytes(index_end + scenario.size());
    write_fixed(bytes, 0, 4, "2.00");
    write_fixed(bytes, 12, 256, "CPX2 fixture");
    bytes[268] = std::byte{1};
    const auto write_u32_at = [&](std::size_t offset, std::uint32_t value) {
        for (int shift : {0, 8, 16, 24}) {
            bytes[offset + shift / 8] =
                static_cast<std::byte>((value >> shift) & 0xffU);
        }
    };
    std::size_t cursor = 272;
    write_u32_at(cursor, static_cast<std::uint32_t>(scenario.size()));
    write_u32_at(cursor + 4, static_cast<std::uint32_t>(index_end));
    cursor += 8;
    bytes[cursor] = static_cast<std::byte>(scenario_name.size());
    bytes[cursor + 2] = std::byte{0x60};
    bytes[cursor + 3] = std::byte{0x0a};
    cursor += 4;
    write_fixed(bytes, cursor, scenario_name.size(), scenario_name);
    cursor += scenario_name.size();
    bytes[cursor] = static_cast<std::byte>(filename.size());
    bytes[cursor + 2] = std::byte{0x60};
    bytes[cursor + 3] = std::byte{0x0a};
    cursor += 4;
    write_fixed(bytes, cursor, filename.size(), filename);
    std::copy(scenario.begin(), scenario.end(), bytes.begin() + index_end);
    return bytes;
}

void reads_proved_classic_header_and_body() {
    const auto path = write_fixture("legacy-scenario-valid", classic_fixture());
    const auto result = aoe::inspect_legacy_scenario(path);
    std::filesystem::remove(path);
    require(result.status == aoe::LegacyScenarioImportStatus::metadata_only);
    require(result.metadata.has_value());
    require(result.metadata->format_version == "1.21");
    require(result.metadata->header_version == 2);
    require(result.metadata->creation_timestamp == 0x12345678U);
    require(result.metadata->description == "Fixture scenario");
    require(result.metadata->has_single_player_victory);
    require(result.metadata->active_player_count == 8);
    require(result.metadata->next_object_id == 4242);
    require(result.metadata->data_version > 1.20F);
    require(result.metadata->player_settings_decoded);
    require(result.metadata->players.size() == 16);
    require(result.metadata->players[0].diplomacy.size() == 16);
    require(result.metadata->map_decoded);
    require(result.metadata->map_width == 2);
    require(result.metadata->map_height == 1);
    require(result.metadata->map_tiles.size() == 2);
    require(result.metadata->map_tiles[0].terrain == 0);
    require(result.metadata->map_tiles[0].elevation == 1);
    require(result.metadata->map_tiles[1].terrain == 7);
    require(result.metadata->map_tiles[1].elevation == 3);
    require(result.metadata->objects_decoded);
    require(result.metadata->objects.size() == 1);
    require(result.metadata->objects[0].object_id == 77);
    require(result.metadata->objects[0].unit_type_id == 42);
    require(result.metadata->triggers_decoded);
    require(result.metadata->triggers.size() == 1);
    require(result.metadata->triggers[0].name == "Fixture trigger");
    require(result.metadata->triggers[0].effects.size() == 1);
    require(result.metadata->triggers[0].effects[0].object_ids[0] == 77);
    require(
        result.metadata->triggers[0].effects[0].
            decoded_property_count == 24
    );
    require(result.metadata->triggers[0].conditions.size() == 1);
    require(
        result.metadata->triggers[0].conditions[0].
            decoded_property_count == 18
    );
    require(result.metadata->trigger_order == std::vector<std::int32_t>{0});
}

void rejects_unknown_versions_and_corrupt_payloads() {
    auto unknown = classic_fixture();
    unknown[2] = std::byte{'9'};
    unknown[3] = std::byte{'9'};
    auto path = write_fixture("legacy-scenario-unknown", unknown);
    auto result = aoe::inspect_legacy_scenario(path);
    std::filesystem::remove(path);
    require(
        result.status ==
        aoe::LegacyScenarioImportStatus::unsupported_version
    );
    require(!result.metadata);

    auto corrupt = classic_fixture();
    corrupt.pop_back();
    path = write_fixture("legacy-scenario-corrupt", corrupt);
    result = aoe::inspect_legacy_scenario(path);
    std::filesystem::remove(path);
    require(result.status == aoe::LegacyScenarioImportStatus::malformed);
    require(result.diagnostic.find("DEFLATE") != std::string::npos);
}

aoe::LegacyDatFile conversion_dat_fixture() {
    std::vector<std::byte> bytes(18);
    const std::string version = "VER 5.7";
    for (std::size_t index = 0; index < version.size(); ++index) {
        bytes[index] = static_cast<std::byte>(version[index]);
    }
    bytes[10] = std::byte{41};
    return aoe::LegacyDatFile::from_decompressed(bytes);
}

void conversion_reports_every_unsupported_record() {
    aoe::LegacyScenarioMetadata source;
    source.map_decoded = true;
    source.player_settings_decoded = true;
    source.objects_decoded = true;
    source.map_width = 2;
    source.map_height = 1;
    source.map_tiles = {{0, 1, 0}, {1, 3, 0}};
    source.players.resize(16);
    source.players[1].wood = 321;
    source.players[1].food = 654;
    source.players[1].gold = 87;
    source.players[1].stone = 45;
    source.players[1].diplomacy.assign(16, 3);
    source.players[2].diplomacy.assign(16, 3);

    aoe::LegacyScenarioMetadata::Object villager;
    villager.owner_slot = 1;
    villager.x = 0;
    villager.y = 0;
    villager.object_id = 10;
    villager.unit_type_id = 83;
    source.objects.push_back(villager);
    auto unsupported = villager;
    unsupported.x = 1;
    unsupported.object_id = 11;
    unsupported.unit_type_id = 999;
    source.objects.push_back(unsupported);

    const auto dat = conversion_dat_fixture();
    auto report = aoe::convert_legacy_scenario(source, dat);
    require(report.scenario.has_value());
    require(report.translated_tiles == 2);
    require(report.unsupported_tiles == 0);
    require(report.translated_objects == 1);
    require(report.unsupported_objects == 1);
    require(report.unsupported_commercial_object_ids.at(999) == 1);
    require(!report.object_hit_points_available);
    require(report.objects_using_default_hit_points == 1);
    require(report.unsupported_object_records.size() == 1);
    require(report.unsupported_object_records[0].object_id == 11);
    require(report.scenario->units.size() == 1);
    require(report.scenario->blue_economy.wood == 321);
    require(report.scenario->blue_economy.food == 654);
    require(report.scenario->map.elevation_at({0, 0}) == 1);
    require(report.scenario->map.elevation_at({1, 0}) == 3);
    require(
        report.scenario->blue_red_diplomacy == aoe::Diplomacy::enemy
    );

    source.map_tiles[1].terrain = 7;
    report = aoe::convert_legacy_scenario(source, dat);
    require(!report.scenario);
    require(report.translated_tiles == 1);
    require(report.unsupported_tiles == 1);
    require(report.unsupported_tile_indices.size() == 1);
    require(report.unsupported_tile_indices[0] == 1);
    require(report.translated_objects == 1);
    require(report.unsupported_objects == 1);
}

void every_catalog_mapping_converts_exhaustively() {
    const auto& mappings = aoe::commercial_object_mappings();
    require(!mappings.empty());
    std::vector<std::uint16_t> ids;
    aoe::LegacyScenarioMetadata source;
    source.map_decoded = true;
    source.player_settings_decoded = true;
    source.objects_decoded = true;
    source.map_width = static_cast<std::uint32_t>(mappings.size());
    source.map_height = 1;
    source.map_tiles.assign(mappings.size(), {0, 0, 0});
    source.players.resize(16);
    source.players[1].diplomacy.assign(16, 3);
    source.players[2].diplomacy.assign(16, 3);
    std::size_t unit_count = 0;
    std::size_t building_count = 0;
    for (std::size_t index = 0; index < mappings.size(); ++index) {
        const auto& mapping = mappings[index];
        require(mapping.unit.has_value() != mapping.building.has_value());
        require(
            std::find(ids.begin(), ids.end(), mapping.commercial_id) ==
            ids.end()
        );
        ids.push_back(mapping.commercial_id);
        unit_count += mapping.unit.has_value();
        building_count += mapping.building.has_value();
        aoe::LegacyScenarioMetadata::Object object;
        object.owner_slot = 1;
        object.x = static_cast<float>(index);
        object.object_id = static_cast<std::int32_t>(1000 + index);
        object.unit_type_id = mapping.commercial_id;
        source.objects.push_back(object);
    }
    source.objects.front().state = 2;
    source.objects.front().angle = 1.5F;
    source.objects.front().animation_frame = 3;
    source.objects.front().garrisoned_in = 77;

    const auto report =
        aoe::convert_legacy_scenario(source, conversion_dat_fixture());
    require(report.scenario.has_value());
    require(report.translated_objects == mappings.size());
    require(report.objects_using_default_hit_points == mappings.size());
    require(report.unsupported_objects == 0);
    require(report.unsupported_object_records.empty());
    require(report.scenario->units.size() == unit_count);
    require(report.scenario->buildings.size() == building_count);
    require(report.lossy_objects.size() == 1);
    require(report.lossy_objects[0].fields.size() == 4);
}

aoe::LegacyScenarioMetadata trigger_conversion_source() {
    aoe::LegacyScenarioMetadata source;
    source.map_decoded = true;
    source.player_settings_decoded = true;
    source.objects_decoded = true;
    source.triggers_decoded = true;
    source.map_width = 2;
    source.map_height = 1;
    source.map_tiles = {{0, 0, 0}, {0, 0, 0}};
    source.players.resize(16);
    source.players[1].diplomacy.assign(16, 3);
    source.players[2].diplomacy.assign(16, 3);

    aoe::LegacyScenarioMetadata::Trigger later;
    later.enabled = false;
    later.looping = true;
    aoe::LegacyScenarioMetadata::TriggerCondition timer;
    timer.type = 10;
    timer.properties.assign(18, -1);
    timer.properties[7] = 12;
    later.conditions.push_back(timer);
    later.condition_order = {0};
    aoe::LegacyScenarioMetadata::TriggerEffect message;
    message.type = 3;
    message.properties.assign(24, -1);
    message.properties[7] = 1;
    message.properties[12] = 6;
    message.chat_text = "Hold position";
    message.audio_file = "scenariovoice.mp3";
    later.effects.push_back(message);
    later.effect_order = {0};

    aoe::LegacyScenarioMetadata::Trigger first;
    first.enabled = true;
    aoe::LegacyScenarioMetadata::TriggerCondition resource;
    resource.type = 8;
    resource.properties.assign(18, -1);
    resource.properties[0] = 500;
    resource.properties[1] = 1;
    resource.properties[5] = 2;
    first.conditions.push_back(resource);
    first.condition_order = {0};
    aoe::LegacyScenarioMetadata::TriggerEffect create;
    create.type = 11;
    create.properties.assign(24, -1);
    create.properties[6] = 83;
    create.properties[7] = 2;
    create.properties[14] = 1;
    create.properties[15] = 0;
    first.effects.push_back(create);
    first.effect_order = {0};

    source.triggers = {later, first};
    source.trigger_order = {1, 0};
    return source;
}

void converts_lossless_trigger_subset_and_preserves_order_flags() {
    auto source = trigger_conversion_source();
    auto report =
        aoe::convert_legacy_scenario(source, conversion_dat_fixture());
    require(report.scenario.has_value());
    require(report.translated_triggers == 2);
    require(report.translated_conditions == 2);
    require(report.translated_effects == 2);
    require(report.unsupported_triggers == 0);
    require(!report.trigger_audit.unsupported_effect_types.contains(2));
    require(report.scenario->triggers.size() == 2);
    const auto& first = report.scenario->triggers[0];
    require(first.id == 2);
    require(first.priority == 2);
    require(first.enabled);
    require(!first.looping);
    require(first.conditions == std::vector<std::string>{
        "resource red wood >= 500"
    });
    require(first.effects == std::vector<std::string>{
        "create_unit villager red 1 0"
    });
    const auto& later = report.scenario->triggers[1];
    require(later.id == 1);
    require(later.priority == 1);
    require(!later.enabled);
    require(later.looping);
    require(later.conditions == std::vector<std::string>{
        "elapsed_ticks >= 60"
    });
    require(
        later.effects == std::vector<std::string>{
            "message player=blue ticks=30 "
            "audio=\"scenariovoice.mp3\" text=\"Hold position\""
        }
    );

    aoe::LegacyScenarioMetadata::TriggerEffect tribute;
    tribute.type = 5;
    tribute.properties.assign(24, -1);
    tribute.properties[1] = 25;
    tribute.properties[2] = 1;
    tribute.properties[7] = 1;
    tribute.properties[8] = 2;
    source.triggers[1].effects.push_back(tribute);
    source.triggers[1].effect_order = {1, 0};
    report = aoe::convert_legacy_scenario(
        source,
        conversion_dat_fixture()
    );
    require(report.scenario.has_value());
    require(report.translated_effects == 3);
    require(report.scenario->triggers.front().effects[0] ==
            "tribute blue red wood 25");
    require(report.scenario->triggers.front().effects[1] ==
            "create_unit villager red 1 0");

    source = trigger_conversion_source();
    auto& research = source.triggers[1].effects[0];
    research.type = 2;
    research.properties.assign(24, -1);
    research.properties[7] = 1;
    research.properties[9] = 8;
    report = aoe::convert_legacy_scenario(
        source,
        conversion_dat_fixture()
    );
    require(report.scenario.has_value());
    require(report.unsupported_triggers == 0);
    require(!report.trigger_audit.unsupported_effect_types.contains(2));
    require(
        report.scenario->triggers.front().effects ==
        std::vector<std::string>{"research blue town_watch"}
    );
    require(report.trigger_audit.research_technology_ids.at(8) == 1);

    research.properties[22] = 1;
    report = aoe::convert_legacy_scenario(
        source,
        conversion_dat_fixture()
    );
    require(!report.scenario);
    require(report.unsupported_effects.size() == 1);
    require(report.unsupported_effects[0].raw.properties[22] == 1);
    require(
        report.unsupported_effects[0].reason.find("noncanonical") !=
        std::string::npos
    );

    source = trigger_conversion_source();
    source.triggers[1].effects[0].type = 2;
    report = aoe::convert_legacy_scenario(
        source,
        conversion_dat_fixture()
    );
    require(!report.scenario);
    require(report.translated_triggers == 1);
    require(report.unsupported_triggers == 1);
    require(report.unsupported_trigger_records.size() == 1);
    require(report.unsupported_effects.size() == 1);
    require(report.unsupported_effects[0].raw.type == 2);
    require(report.unsupported_effects[0].trigger_index == 1);

    source = trigger_conversion_source();
    auto& hp = source.triggers[1].effects[0];
    hp.type = 27;
    hp.properties.assign(24, -1);
    hp.properties[1] = 25;
    hp.properties[7] = 1;
    hp.properties[16] = 0;
    hp.properties[17] = 0;
    hp.properties[18] = 1;
    hp.properties[19] = 0;
    report = aoe::convert_legacy_scenario(
        source,
        conversion_dat_fixture()
    );
    require(!report.scenario);
    require(report.unsupported_effects.size() == 1);
    require(report.unsupported_effects[0].raw.type == 27);
    require(report.unsupported_effects[0].raw.properties == hp.properties);
    require(
        report.unsupported_effects[0].reason.find(
            "operation, clamp, and death semantics"
        ) != std::string::npos
    );

    source = trigger_conversion_source();
    auto& attack_move = source.triggers[1].effects[0];
    attack_move.type = 30;
    attack_move.properties.assign(24, -1);
    attack_move.properties[7] = 1;
    attack_move.properties[16] = 5;
    attack_move.properties[17] = 6;
    attack_move.properties[18] = 9;
    attack_move.properties[19] = 10;
    report = aoe::convert_legacy_scenario(
        source,
        conversion_dat_fixture()
    );
    require(!report.scenario);
    require(report.unsupported_effects.size() == 1);
    require(report.unsupported_effects[0].raw.type == 30);
    require(
        report.unsupported_effects[0].reason.find(
            "destination versus selector-area"
        ) != std::string::npos
    );
}

void trigger_audit_is_deterministic_and_machine_readable() {
    aoe::LegacyScenarioMetadata source;
    source.map_decoded = true;
    source.map_width = 4;
    source.map_height = 4;
    aoe::LegacyScenarioMetadata::Object mapped;
    mapped.owner_slot = 1;
    mapped.x = 1;
    mapped.y = 1;
    mapped.object_id = 10;
    mapped.unit_type_id = 83;
    source.objects.push_back(mapped);
    auto unmapped = mapped;
    unmapped.object_id = 11;
    unmapped.unit_type_id = 999;
    source.objects.push_back(unmapped);

    aoe::LegacyScenarioMetadata::Trigger trigger;
    aoe::LegacyScenarioMetadata::TriggerCondition hit_points;
    hit_points.type = 23;
    hit_points.properties = {25, -1, 10};
    hit_points.decoded_property_count = 3;
    trigger.conditions.push_back(hit_points);
    aoe::LegacyScenarioMetadata::TriggerEffect remove;
    remove.type = 15;
    remove.properties = {-1, -1, -1, -1, 3, 10};
    remove.decoded_property_count = 6;
    remove.object_ids = {11, 99, -1};
    trigger.effects.push_back(remove);
    source.triggers.push_back(trigger);

    const auto audit = aoe::audit_legacy_scenario_triggers(source);
    const auto count = [](const auto& values, const auto& key) {
        const auto found = values.find(key);
        return found == values.end() ? std::size_t{} : found->second;
    };
    require(audit.trigger_count == 1);
    require(audit.condition_count == 1);
    require(audit.effect_count == 1);
    require(count(audit.condition_types, 23) == 1);
    require(count(audit.effect_types, 15) == 1);
    require(count(audit.unsupported_condition_types, 23) == 1);
    require(count(audit.unsupported_effect_types, 15) == 0);
    require(count(audit.missing_property_indices, "condition.23.3") == 1);
    require(count(audit.missing_property_indices, "effect.15.23") == 1);
    require(count(audit.mapped_commercial_object_ids, 83) == 1);
    require(count(audit.unmapped_commercial_object_ids, 999) == 1);
    require(audit.research_technology_ids.empty());
    require(audit.direct_object_references == 2);
    require(audit.listed_object_references == 3);
    require(count(audit.object_reference_blockers, "resolved") == 2);
    require(
        count(audit.object_reference_blockers, "unmapped_commercial_id") == 1
    );
    require(count(audit.object_reference_blockers, "missing_object") == 1);
    require(count(audit.object_reference_blockers, "invalid_object_id") == 1);
    require(count(audit.selector_shapes, "condition.23.direct") == 1);
    require(count(audit.selector_shapes, "effect.15.direct.list") == 1);

    const std::string first =
        aoe::legacy_scenario_trigger_audit_json(audit);
    const std::string second =
        aoe::legacy_scenario_trigger_audit_json(audit);
    require(first == second);
    require(first.starts_with("{\"trigger_count\":1"));
    require(first.ends_with("}\n"));
    require(
        first.find("\"unsupported_condition_types\":{\"23\":1}") !=
        std::string::npos
    );
    require(
        first.find("\"mapped_commercial_object_ids\":{\"83\":1}") !=
        std::string::npos
    );
    require(first.find("\"research_technology_ids\":{}") != std::string::npos);
    require(
        first.find(
            "\"selector_shapes\":{\"condition.23.direct\":1,"
            "\"effect.15.direct.list\":1}"
        ) != std::string::npos
    );

    aoe::LegacyScenarioConversionReport report;
    report.trigger_audit = audit;
    report.diagnostics = {"quoted \"diagnostic\"\nline"};
    const std::string report_json =
        aoe::legacy_scenario_conversion_report_json(report);
    require(
        report_json.find("\"object_hit_points_available\":false") !=
        std::string::npos
    );
    require(
        report_json.find("\"unsupported_commercial_object_ids\":{}") !=
        std::string::npos
    );
    require(
        report_json.find(
            "\"diagnostics\":[\"quoted \\\"diagnostic\\\"\\nline\"]"
        ) != std::string::npos
    );
}

aoe::LegacyScenarioMetadata object_reference_conversion_source() {
    aoe::LegacyScenarioMetadata source;
    source.map_decoded = true;
    source.player_settings_decoded = true;
    source.objects_decoded = true;
    source.triggers_decoded = true;
    source.map_width = 10;
    source.map_height = 4;
    source.map_tiles.assign(40, {0, 0, 0});
    source.players.resize(16);
    source.players[1].diplomacy.assign(16, 3);
    source.players[2].diplomacy.assign(16, 3);

    aoe::LegacyScenarioMetadata::Object house;
    house.owner_slot = 1;
    house.x = 0;
    house.y = 0;
    house.object_id = 100;
    house.unit_type_id = 70;
    source.objects.push_back(house);
    aoe::LegacyScenarioMetadata::Object villager;
    villager.owner_slot = 1;
    villager.x = 4;
    villager.y = 1;
    villager.object_id = 200;
    villager.unit_type_id = 83;
    source.objects.push_back(villager);
    auto barracks = house;
    barracks.x = 7;
    barracks.object_id = 300;
    barracks.unit_type_id = 12;
    source.objects.push_back(barracks);

    aoe::LegacyScenarioMetadata::Trigger trigger;
    trigger.enabled = true;
    aoe::LegacyScenarioMetadata::TriggerCondition destroyed;
    destroyed.type = 6;
    destroyed.properties.assign(18, -1);
    destroyed.decoded_property_count = 18;
    destroyed.properties[2] = 300;
    trigger.conditions.push_back(destroyed);
    trigger.condition_order = {0};
    aoe::LegacyScenarioMetadata::TriggerEffect remove;
    remove.type = 15;
    remove.properties.assign(24, -1);
    remove.decoded_property_count = 24;
    remove.properties[5] = 200;
    trigger.effects.push_back(remove);
    trigger.effect_order = {0};
    source.triggers.push_back(trigger);
    source.trigger_order = {0};
    return source;
}

void commercial_object_references_remap_stably_or_fail_atomically() {
    auto source = object_reference_conversion_source();
    auto report =
        aoe::convert_legacy_scenario(source, conversion_dat_fixture());
    require(report.scenario.has_value());
    require(report.object_id_remap.size() == 3);
    require(report.object_id_remap[0].commercial_object_id == 100);
    require(report.object_id_remap[0].native_entity_id == 2);
    require(report.object_id_remap[0].building);
    require(report.object_id_remap[0].lossless);
    require(report.object_id_remap[1].commercial_object_id == 200);
    require(report.object_id_remap[1].native_entity_id == 1);
    require(!report.object_id_remap[1].building);
    require(report.object_id_remap[2].commercial_object_id == 300);
    require(report.object_id_remap[2].native_entity_id == 3);
    require(report.object_id_remap[2].building);
    require(report.scenario->triggers.size() == 1);
    require(
        report.scenario->triggers[0].conditions ==
        std::vector<std::string>{"building_destroyed 3"}
    );
    require(
        report.scenario->triggers[0].effects ==
        std::vector<std::string>{"remove_object 1"}
    );
    const aoe::Simulation simulation =
        aoe::create_simulation(*report.scenario);
    require(simulation.units().size() == 1);
    require(simulation.units()[0].id == 1);
    require(simulation.units()[0].kind == aoe::UnitKind::villager);
    require(simulation.buildings().size() == 2);
    require(simulation.buildings()[0].id == 2);
    require(simulation.buildings()[0].kind == aoe::BuildingKind::house);
    require(simulation.buildings()[1].id == 3);
    require(
        simulation.buildings()[1].kind == aoe::BuildingKind::barracks
    );
    const std::string json =
        aoe::legacy_scenario_conversion_report_json(report);
    require(
        json.find(
            "\"commercial_object_id\":200,\"native_entity_id\":1"
        ) != std::string::npos
    );

    source = object_reference_conversion_source();
    auto second_destroyed = source.triggers[0].conditions[0];
    second_destroyed.properties[2] = 100;
    source.triggers[0].conditions.push_back(second_destroyed);
    source.triggers[0].condition_order = {1, 0};
    aoe::LegacyScenarioMetadata::TriggerEffect activate;
    activate.type = 8;
    activate.properties.assign(24, -1);
    activate.decoded_property_count = 24;
    activate.properties[13] = 1;
    auto deactivate = activate;
    deactivate.type = 9;
    source.triggers[0].effects.push_back(activate);
    source.triggers[0].effects.push_back(deactivate);
    source.triggers[0].effect_order = {1, 0, 2};
    aoe::LegacyScenarioMetadata::Trigger terminal;
    terminal.enabled = false;
    aoe::LegacyScenarioMetadata::TriggerEffect victory;
    victory.type = 13;
    victory.properties.assign(24, -1);
    victory.decoded_property_count = 24;
    victory.properties[7] = 1;
    terminal.effects.push_back(victory);
    terminal.effect_order = {0};
    source.triggers.push_back(terminal);
    source.trigger_order = {0, 1};
    report = aoe::convert_legacy_scenario(
        source, conversion_dat_fixture()
    );
    require(report.scenario.has_value());
    require(
        report.scenario->triggers[0].conditions ==
        std::vector<std::string>{
            "building_destroyed 2", "building_destroyed 3"
        }
    );
    require(
        report.scenario->triggers[0].effects ==
        std::vector<std::string>{
            "activate_trigger 2", "remove_object 1",
            "deactivate_trigger 2"
        }
    );
    source.triggers[0].effects[1].properties[13] = 2;
    report = aoe::convert_legacy_scenario(
        source, conversion_dat_fixture()
    );
    require(!report.scenario);
    require(report.unsupported_effects.size() == 1);
    require(
        report.unsupported_effects[0].reason.find("invalid trigger ID") !=
        std::string::npos
    );

    source = object_reference_conversion_source();
    source.triggers[0].effects[0].properties[4] = 1;
    source.triggers[0].effects[0].object_ids = {200};
    report = aoe::convert_legacy_scenario(
        source, conversion_dat_fixture()
    );
    require(report.scenario.has_value());
    require(
        report.scenario->triggers[0].effects ==
        std::vector<std::string>{"remove_object 1"}
    );
    source.triggers[0].effects[0].properties[4] = 2;
    report = aoe::convert_legacy_scenario(
        source, conversion_dat_fixture()
    );
    require(!report.scenario);
    require(report.unsupported_effects.size() == 1);

    source = object_reference_conversion_source();
    source.objects[2].object_id = 200;
    report = aoe::convert_legacy_scenario(
        source, conversion_dat_fixture()
    );
    require(!report.scenario);
    require(report.unsupported_triggers == 1);
    require(report.object_id_remap[1].blocker ==
            "duplicate_commercial_object_id");
    require(report.object_id_remap[2].blocker ==
            "duplicate_commercial_object_id");
    require(report.unsupported_effects.size() == 1);

    source = object_reference_conversion_source();
    source.objects[1].angle = 1.0F;
    report = aoe::convert_legacy_scenario(
        source, conversion_dat_fixture()
    );
    require(!report.scenario);
    require(report.object_id_remap[1].native_entity_id == 1);
    require(!report.object_id_remap[1].lossless);
    require(
        report.object_id_remap[1].blocker ==
        "translated_object_has_unapplied_fields"
    );

    source = object_reference_conversion_source();
    source.triggers[0].conditions[0].properties[2] = 999;
    report = aoe::convert_legacy_scenario(
        source, conversion_dat_fixture()
    );
    require(!report.scenario);
    require(report.unsupported_conditions.size() == 1);
    require(
        report.unsupported_conditions[0].reason.find(
            "unique lossless native remap"
        ) != std::string::npos
    );

    source = object_reference_conversion_source();
    source.triggers[0].conditions[0].properties[2] = -1;
    report = aoe::convert_legacy_scenario(
        source, conversion_dat_fixture()
    );
    require(!report.scenario);
    require(
        report.unsupported_conditions[0].reason.find("nonpositive") !=
        std::string::npos
    );

    source = object_reference_conversion_source();
    source.objects[1].object_id = -5;
    source.triggers[0].conditions[0].properties[2] = -5;
    source.triggers[0].effects[0].properties[5] = -5;
    report = aoe::convert_legacy_scenario(
        source, conversion_dat_fixture()
    );
    require(!report.scenario);
    require(
        report.object_id_remap[1].blocker ==
        "nonpositive_commercial_object_id"
    );

    source = object_reference_conversion_source();
    source.triggers[0].conditions[0].type = 23;
    source.triggers[0].conditions[0].properties[0] = 25;
    source.triggers[0].conditions[0].properties[2] = 200;
    source.triggers[0].conditions[0].properties[17] = 3;
    report = aoe::convert_legacy_scenario(
        source, conversion_dat_fixture()
    );
    require(!report.scenario);
    require(
        report.unsupported_conditions[0].reason.find(
            "unpinned property index 17"
        ) != std::string::npos
    );

    source = object_reference_conversion_source();
    auto& area = source.triggers[0].conditions[0];
    area.type = 5;
    area.properties.assign(18, -1);
    area.decoded_property_count = 18;
    area.properties[0] = 1;
    area.properties[5] = 1;
    area.properties[9] = 0;
    area.properties[10] = 0;
    area.properties[11] = 9;
    area.properties[12] = 3;
    report = aoe::convert_legacy_scenario(
        source, conversion_dat_fixture()
    );
    require(report.scenario.has_value());
    require(
        report.scenario->triggers[0].conditions ==
        std::vector<std::string>{"area_presence blue 0 0 9 3 >= 1"}
    );
    source.triggers[0].conditions[0].properties[4] = 83;
    report = aoe::convert_legacy_scenario(
        source, conversion_dat_fixture()
    );
    require(!report.scenario);

    source = object_reference_conversion_source();
    source.triggers[0].conditions[0].type = 1;
    source.triggers[0].conditions[0].properties[9] = 0;
    source.triggers[0].conditions[0].properties[10] = 0;
    source.triggers[0].conditions[0].properties[11] = 9;
    source.triggers[0].conditions[0].properties[12] = 3;
    report = aoe::convert_legacy_scenario(
        source, conversion_dat_fixture()
    );
    require(!report.scenario);
    require(
        report.unsupported_conditions[0].reason.find(
            "identity-aware area presence"
        ) != std::string::npos
    );

    source = object_reference_conversion_source();
    source.triggers[0].conditions[0].type = 15;
    report = aoe::convert_legacy_scenario(
        source, conversion_dat_fixture()
    );
    require(!report.scenario);
    require(
        report.unsupported_conditions[0].reason.find(
            "visibility is not native object existence"
        ) != std::string::npos
    );
}

void campaign_index_preserves_order_payloads_and_unknowns() {
    const auto bytes = classic_campaign_fixture();
    const auto result = aoe::inspect_legacy_campaign_bytes(bytes);
    require(result.status == aoe::LegacyCampaignImportStatus::inspected);
    require(result.version == "1.00");
    require(result.name == "Fixture campaign");
    require(result.entries.size() == 2);
    require(result.entries[0].name == "First");
    require(result.entries[0].filename == "first.scx");
    require(
        result.entries[0].scenario.status ==
        aoe::LegacyScenarioImportStatus::metadata_only
    );
    require(result.entries[1].name == "Unsupported");
    require(result.entries[1].raw_payload.size() == 4);
    require(
        result.entries[1].scenario.status ==
        aoe::LegacyScenarioImportStatus::malformed
    );
    require(result.decoded_scenarios == 1);
    require(result.unsupported_scenarios == 1);
    require(result.unindexed_payload.size() == 1);
    require(result.unindexed_payload[0].bytes.size() == 3);

    auto malformed = bytes;
    const std::size_t second = 264 + 520;
    for (int byte = 0; byte < 4; ++byte) {
        malformed[second + 4 + byte] = malformed[264 + 4 + byte];
    }
    require(
        aoe::inspect_legacy_campaign_bytes(malformed).status ==
        aoe::LegacyCampaignImportStatus::malformed
    );

    auto unsupported = bytes;
    write_fixed(unsupported, 0, 4, "9.99");
    require(
        aoe::inspect_legacy_campaign_bytes(unsupported).status ==
        aoe::LegacyCampaignImportStatus::unsupported_version
    );
}

void cpx2_and_serializer_round_trip_exactly() {
    const auto bytes = cpx2_campaign_fixture();
    const auto result = aoe::inspect_legacy_campaign_bytes(bytes);
    require(result.status == aoe::LegacyCampaignImportStatus::inspected);
    require(result.version == "2.00");
    require(result.name == "CPX2 fixture");
    require(result.entries.size() == 1);
    require(result.entries[0].name == "First mission");
    require(result.entries[0].filename == "first.scx");
    require(result.entries[0].raw_payload == classic_fixture());
    require(aoe::serialize_legacy_campaign(result) == bytes);
}

void cpx2_import_installs_and_persists_completion() {
    const auto root = std::filesystem::temp_directory_path() /
        "aoe-cpx2-campaign-import-tests";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    const auto campaign_path = root / "fixture.cpx2";
    {
        const auto bytes = cpx2_campaign_fixture();
        std::ofstream output(campaign_path, std::ios::binary);
        output.write(reinterpret_cast<const char*>(bytes.data()),
                     static_cast<std::streamsize>(bytes.size()));
    }
    std::vector<std::byte> dat_bytes(18);
    write_fixed(dat_bytes, 0, 7, "VER 5.7");
    dat_bytes[10] = std::byte{41};
    const auto compressed_dat = deflate_raw(dat_bytes);
    const auto dat_path = root / "empires2_x1_p1.dat";
    {
        std::ofstream output(dat_path, std::ios::binary);
        output.write(reinterpret_cast<const char*>(compressed_dat.data()),
                     static_cast<std::streamsize>(compressed_dat.size()));
    }
    const aoe::Campaign campaign = aoe::import_legacy_campaign(
        campaign_path, dat_path, root / "installed"
    );
    require(campaign.name == "CPX2 fixture");
    require(campaign.scenarios.size() == 1);
    require(std::filesystem::is_regular_file(campaign.scenarios[0].path));
    aoe::CampaignProgress progress = aoe::fresh_campaign_progress(campaign);
    const auto progress_path = root / "progress.txt";
    require(aoe::commit_campaign_outcome(
        campaign, 1, aoe::MatchOutcome::blue_victory,
        progress, progress_path
    ));
    const auto loaded = aoe::load_campaign_progress(campaign, progress_path);
    require(loaded.status == aoe::CampaignProgressStatus::current);
    require(loaded.progress.completed == std::vector<int>{1});
    std::filesystem::remove_all(root);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc == 3 && std::string_view{argv[1]} == "--write-cpx2") {
        const auto bytes = cpx2_campaign_fixture();
        std::ofstream output(argv[2], std::ios::binary | std::ios::trunc);
        output.write(reinterpret_cast<const char*>(bytes.data()),
                     static_cast<std::streamsize>(bytes.size()));
        return output ? 0 : 1;
    }
    reads_proved_classic_header_and_body();
    rejects_unknown_versions_and_corrupt_payloads();
    conversion_reports_every_unsupported_record();
    every_catalog_mapping_converts_exhaustively();
    converts_lossless_trigger_subset_and_preserves_order_flags();
    trigger_audit_is_deterministic_and_machine_readable();
    commercial_object_references_remap_stably_or_fail_atomically();
    campaign_index_preserves_order_payloads_and_unknowns();
    cpx2_and_serializer_round_trip_exactly();
    cpx2_import_installs_and_persists_completion();
    for (int index = 1; index < argc; ++index) {
        const std::filesystem::path input_path = argv[index];
        const auto extension = input_path.extension().string();
        if (extension == ".cpn" || extension == ".cpx" ||
            extension == ".cpx2") {
            const auto campaign = aoe::inspect_legacy_campaign(input_path);
            require(
                campaign.status ==
                aoe::LegacyCampaignImportStatus::inspected
            );
            std::cout << argv[index] << ": campaign "
                      << campaign.version << ", "
                      << campaign.entries.size() << " entries, "
                      << campaign.decoded_scenarios
                      << " decoded scenarios\n";
            continue;
        }
        const auto result = aoe::inspect_legacy_scenario(argv[index]);
        if (result.status !=
            aoe::LegacyScenarioImportStatus::metadata_only) {
            std::cerr << argv[index] << ": " << result.diagnostic << '\n';
        }
        require(result.status == aoe::LegacyScenarioImportStatus::metadata_only);
        require(result.metadata.has_value());
        require(result.metadata->objects_decoded);
        if (result.metadata->format_version >= "1.14") {
            require(result.metadata->triggers_decoded);
        }
        std::cout << argv[index] << ": "
                  << result.metadata->format_version << ", header "
                  << result.metadata->header_version << ", "
                  << result.metadata->active_player_count << " players, "
                  << result.metadata->map_width << 'x'
                  << result.metadata->map_height << " map, "
                  << result.metadata->objects.size() << " objects, "
                  << result.metadata->triggers.size() << " triggers\n";
        std::cout << aoe::legacy_scenario_trigger_audit_json(
            aoe::audit_legacy_scenario_triggers(*result.metadata)
        );
    }
    std::cout << "All legacy scenario tests passed\n";
}
