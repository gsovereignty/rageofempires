#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <source_location>
#include <string>
#include <vector>

#include <zlib.h>

#include "aoe/legacy_recorded_game.hpp"

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

void put_u32(
    std::vector<std::byte>& bytes,
    std::size_t offset,
    std::uint32_t value
) {
    require(offset <= bytes.size() && bytes.size() - offset >= 4);
    for (int shift : {0, 8, 16, 24}) {
        bytes[offset + shift / 8] =
            static_cast<std::byte>((value >> shift) & 0xffU);
    }
}

void append_u32(std::vector<std::byte>& bytes, std::uint32_t value) {
    const auto offset = bytes.size();
    bytes.resize(offset + 4);
    put_u32(bytes, offset, value);
}

void append_u16(std::vector<std::byte>& bytes, std::uint16_t value) {
    bytes.push_back(static_cast<std::byte>(value & 0xffU));
    bytes.push_back(static_cast<std::byte>((value >> 8U) & 0xffU));
}

void append_action(
    std::vector<std::byte>& bytes,
    const std::vector<std::byte>& action,
    std::uint32_t execute_at
) {
    append_u32(bytes, 1);
    append_u32(bytes, static_cast<std::uint32_t>(action.size()));
    bytes.insert(bytes.end(), action.begin(), action.end());
    append_u32(bytes, execute_at);
}

void put_f32(
    std::vector<std::byte>& bytes,
    std::size_t offset,
    float value
) {
    std::uint32_t bits{};
    std::memcpy(&bits, &value, sizeof(bits));
    put_u32(bytes, offset, bits);
}

void append_f32(std::vector<std::byte>& bytes, float value) {
    const auto offset = bytes.size();
    bytes.resize(offset + 4);
    put_f32(bytes, offset, value);
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
    std::vector<std::byte> output(deflateBound(&stream, plain.size()));
    stream.next_out = reinterpret_cast<Bytef*>(output.data());
    stream.avail_out = static_cast<uInt>(output.size());
    require(deflate(&stream, Z_FINISH) == Z_STREAM_END);
    output.resize(stream.total_out);
    deflateEnd(&stream);
    return output;
}

std::vector<std::byte> fixture() {
    std::vector<std::byte> header(138);
    const std::string version = "VER 9.4";
    for (std::size_t index = 0; index < version.size(); ++index) {
        header[index] = static_cast<std::byte>(version[index]);
    }
    put_f32(header, 8, 12.3F);
    put_u32(header, 12, 1);
    put_u32(header, 20, 20);
    put_u32(header, 28, 20);
    header[63] = std::byte{2};
    put_u32(header, 126, 4);
    put_u32(header, 130, 4);
    const auto compressed = deflate_raw(header);

    std::vector<std::byte> bytes(8);
    put_u32(bytes, 0, compressed.size() + 8);
    put_u32(bytes, 4, 12345);
    bytes.insert(bytes.end(), compressed.begin(), compressed.end());

    append_u32(bytes, 4);
    append_u32(bytes, 500);
    for (int value = 0; value < 5; ++value) append_u32(bytes, value);

    append_u32(bytes, 2);
    append_u32(bytes, 150);
    append_u32(bytes, 3);
    append_f32(bytes, 1.5F);
    append_f32(bytes, 2.5F);
    append_u32(bytes, 1);

    append_u32(bytes, 1);
    append_u32(bytes, 3);
    bytes.push_back(std::byte{3});
    bytes.push_back(std::byte{0xaa});
    bytes.push_back(std::byte{0xbb});
    append_u32(bytes, 900);

    append_u32(bytes, 4);
    append_u32(bytes, static_cast<std::uint32_t>(-1));
    append_u32(bytes, 6);
    for (char value : std::string{"hello"}) {
        bytes.push_back(static_cast<std::byte>(value));
    }
    bytes.push_back(std::byte{});

    append_u32(bytes, 99);
    bytes.push_back(std::byte{0xde});
    bytes.push_back(std::byte{0xad});
    return bytes;
}

void inspects_proved_records_and_preserves_tail() {
    const auto bytes = fixture();
    const auto result = aoe::inspect_legacy_recorded_game_bytes(bytes);
    require(result.status == aoe::LegacyRecordedGameStatus::inspected);
    require(result.metadata.has_value());
    require(result.metadata->game_version == "VER 9.4");
    require(result.metadata->records.size() == 4);
    require(
        result.metadata->records[0].kind ==
        aoe::LegacyRecordedRecordKind::game_start
    );
    require(
        result.metadata->records[1].synchronization_interval == 150
    );
    require(result.metadata->records[2].action_tag == 3);
    require(result.metadata->records[2].execute_at == 900);
    require(result.metadata->records[3].chat == "hello");
    require(result.metadata->unsupported_tail.size() == 6);
    require(
        result.metadata->unsupported_tail_offset + 6 == bytes.size()
    );
}

void rejects_corrupt_header_and_action_length() {
    auto bytes = fixture();
    bytes[8] ^= std::byte{0xff};
    require(
        aoe::inspect_legacy_recorded_game_bytes(bytes).status ==
        aoe::LegacyRecordedGameStatus::malformed
    );

    bytes = fixture();
    const auto inspected = aoe::inspect_legacy_recorded_game_bytes(bytes);
    require(inspected.metadata.has_value());
    const auto action_offset =
        inspected.metadata->records[2].file_offset;
    put_u32(bytes, action_offset + 4, 0x7fffffffU);
    require(
        aoe::inspect_legacy_recorded_game_bytes(bytes).status ==
        aoe::LegacyRecordedGameStatus::malformed
    );
}

void decodes_validated_action_schemas_without_guessing() {
    auto bytes = fixture();
    const auto base = aoe::inspect_legacy_recorded_game_bytes(bytes);
    require(base.metadata.has_value());
    bytes.resize(base.metadata->records[2].file_offset);

    std::vector<std::byte> move{
        std::byte{0x03}, std::byte{0x01}, std::byte{}, std::byte{}
    };
    append_u32(move, static_cast<std::uint32_t>(-1));
    append_u32(move, 2);
    append_f32(move, 12.0F);
    append_f32(move, 34.0F);
    append_u32(move, 101);
    append_u32(move, 102);
    append_action(bytes, move, 10);

    std::vector<std::byte> stop{std::byte{0x01}, std::byte{0x02}};
    append_u32(stop, 101);
    append_u32(stop, 102);
    append_action(bytes, stop, 11);

    std::vector<std::byte> train{
        std::byte{0x77}, std::byte{}, std::byte{}, std::byte{}
    };
    append_u32(train, 201);
    append_u16(train, 83);
    append_u16(train, 3);
    append_action(bytes, train, 12);

    std::vector<std::byte> research{
        std::byte{0x65}, std::byte{}, std::byte{}, std::byte{}
    };
    append_u32(research, 201);
    research.push_back(std::byte{0x01});
    research.push_back(std::byte{});
    append_u16(research, 22);
    append_u32(research, static_cast<std::uint32_t>(-1));
    append_action(bytes, research, 13);

    std::vector<std::byte> tribute{
        std::byte{0x6c}, std::byte{0x01}, std::byte{0x02}, std::byte{0x03}
    };
    append_f32(tribute, 100.0F);
    append_f32(tribute, 30.0F);
    append_action(bytes, tribute, 14);

    std::vector<std::byte> resign{
        std::byte{0x0b}, std::byte{0x02}, std::byte{0x02}
    };
    append_u32(resign, 0);
    append_action(bytes, resign, 15);

    std::vector<std::byte> primary{
        std::byte{0x00}, std::byte{0x01}, std::byte{}, std::byte{}
    };
    append_u32(primary, 301);
    primary.push_back(std::byte{0x01});
    primary.push_back(std::byte{});
    primary.push_back(std::byte{});
    primary.push_back(std::byte{});
    append_f32(primary, 8.0F);
    append_f32(primary, 9.0F);
    append_u32(primary, 101);
    append_action(bytes, primary, 16);

    std::vector<std::byte> diplomacy{
        std::byte{0x67}, std::byte{}, std::byte{0x01}, std::byte{},
        std::byte{0x02}, std::byte{}, std::byte{}, std::byte{}
    };
    append_f32(diplomacy, 1.0F);
    diplomacy.push_back(std::byte{0x01});
    diplomacy.push_back(std::byte{});
    diplomacy.push_back(std::byte{});
    diplomacy.push_back(std::byte{});
    append_action(bytes, diplomacy, 17);

    append_action(bytes, {std::byte{0x67}, std::byte{}}, 18);
    const auto result = aoe::inspect_legacy_recorded_game_bytes(bytes);
    require(result.status == aoe::LegacyRecordedGameStatus::inspected);
    require(result.metadata.has_value());
    const auto& records = result.metadata->records;
    require(records.size() == 11);
    for (std::size_t index = 2; index < 10; ++index) {
        require(records[index].action.has_value());
        require(records[index].action->schema_valid);
        require(!records[index].action->raw.empty());
    }
    require(records[2].action->entity_ids.size() == 2);
    require(records[4].action->train_count == 3);
    require(records[5].action->commercial_technology_id == 22);
    require(records[6].action->transaction_fee == 30.0F);
    require(records[7].action->disconnect == 0);
    require(records[8].action->target_id == 301);
    require(
        records[8].action->kind ==
        aoe::LegacyRecordedActionKind::primary_action
    );
    require(
        records[9].action->kind ==
            aoe::LegacyRecordedActionKind::diplomacy &&
        records[9].action->target_player_number == 2 &&
        records[9].action->diplomatic_stance == 1
    );
    require(!records[10].action->schema_valid);
    require(records[10].action->raw == std::vector<std::byte>({
        std::byte{0x67}, std::byte{}
    }));

    const auto report =
        aoe::convert_legacy_recorded_game_to_replay(*result.metadata);
    require(report.actions.size() == 9);
    require(report.timeline.size() == 9);
    require(report.timeline[0].execute_at == 10);
    require(report.decoded_action_count == 8);
    require(report.unsupported_action_count == 1);
    require(report.unsupported_tags.at(0x67) == 1);
    require(report.required_mappings.execute_at_values.size() == 9);
    require(report.required_mappings.player_numbers.contains(2));
    require(report.required_mappings.entity_ids.contains(301));
    require(report.required_mappings.commercial_unit_ids.contains(83));
    require(
        report.required_mappings.commercial_technology_ids.contains(22)
    );
    require(!report.replay.has_value());
}

void converts_lossless_mapped_action_subset() {
    auto bytes = fixture();
    const auto base = aoe::inspect_legacy_recorded_game_bytes(bytes);
    require(base.metadata.has_value());
    bytes.resize(base.metadata->records[2].file_offset);

    std::vector<std::byte> move{
        std::byte{0x03}, std::byte{0x01}, std::byte{}, std::byte{}
    };
    append_u32(move, static_cast<std::uint32_t>(-1));
    append_u32(move, 2);
    append_f32(move, 6.0F);
    append_f32(move, 7.0F);
    append_u32(move, 101);
    append_u32(move, 102);
    append_action(bytes, move, 10);

    std::vector<std::byte> train{
        std::byte{0x77}, std::byte{}, std::byte{}, std::byte{}
    };
    append_u32(train, 201);
    append_u16(train, 83);
    append_u16(train, 2);
    append_action(bytes, train, 11);

    std::vector<std::byte> research{
        std::byte{0x65}, std::byte{}, std::byte{}, std::byte{}
    };
    append_u32(research, 201);
    research.push_back(std::byte{0x01});
    research.push_back(std::byte{});
    append_u16(research, 22);
    append_u32(research, static_cast<std::uint32_t>(-1));
    append_action(bytes, research, 12);

    std::vector<std::byte> diplomacy{
        std::byte{0x67}, std::byte{}, std::byte{0x01}, std::byte{},
        std::byte{0x02}, std::byte{}, std::byte{}, std::byte{}
    };
    append_f32(diplomacy, 3.0F);
    diplomacy.push_back(std::byte{0x03});
    diplomacy.push_back(std::byte{});
    diplomacy.push_back(std::byte{});
    diplomacy.push_back(std::byte{});
    append_action(bytes, diplomacy, 13);

    const auto result = aoe::inspect_legacy_recorded_game_bytes(bytes);
    require(result.metadata.has_value());
    aoe::LegacyRecordedReplayMappings mappings;
    mappings.ticks = {{10, 100}, {11, 101}, {12, 102}, {13, 103}};
    mappings.entities = {{101, 1001}, {102, 1002}, {201, 2001}};
    mappings.players = {
        {1, aoe::Player::blue}, {2, aoe::Player::red}
    };
    mappings.units.emplace(83, aoe::UnitKind::villager);
    mappings.technologies.emplace(22, aoe::Technology::loom);

    const auto report = aoe::convert_legacy_recorded_game_to_replay(
        *result.metadata, mappings
    );
    require(report.blockers.empty());
    require(report.replay.has_value());
    require(report.replay->commands().size() == 6);
    require(std::get<aoe::MoveUnitCommand>(
        report.replay->commands()[1].command
    ).destination == aoe::TilePosition(6, 7));
    require(std::get<aoe::QueueUnitCommand>(
        report.replay->commands()[3].command
    ).kind == aoe::UnitKind::villager);
    require(std::get<aoe::ResearchTechnologyCommand>(
        report.replay->commands()[4].command
    ).technology == aoe::Technology::loom);
    require(std::get<aoe::SetDiplomacyCommand>(
        report.replay->commands()[5].command
    ).relation == aoe::Diplomacy::enemy);
}

void emits_replay_only_with_complete_explicit_mappings() {
    auto bytes = fixture();
    const auto base = aoe::inspect_legacy_recorded_game_bytes(bytes);
    require(base.metadata.has_value());
    bytes.resize(base.metadata->records[2].file_offset);
    std::vector<std::byte> stop{std::byte{0x01}, std::byte{0x02}};
    append_u32(stop, 101);
    append_u32(stop, 102);
    append_action(bytes, stop, 77);

    const auto result = aoe::inspect_legacy_recorded_game_bytes(bytes);
    require(result.metadata.has_value());
    require(
        !aoe::convert_legacy_recorded_game_to_replay(*result.metadata)
             .replay.has_value()
    );
    aoe::LegacyRecordedReplayMappings mappings;
    mappings.ticks.emplace(77, 900);
    mappings.entities.emplace(101, 1001);
    mappings.entities.emplace(102, 1002);
    const auto report =
        aoe::convert_legacy_recorded_game_to_replay(
            *result.metadata,
            mappings
        );
    require(report.replay.has_value());
    require(report.replay->commands().size() == 2);
    require(report.replay->commands()[0].tick == 900);
    require(std::get<aoe::StopUnitCommand>(
        report.replay->commands()[1].command
    ).unit == 1002);
}

std::vector<std::byte> primary_action(
    std::uint8_t player,
    std::int32_t target,
    std::int32_t actor
) {
    std::vector<std::byte> action{
        std::byte{0x00}, static_cast<std::byte>(player),
        std::byte{}, std::byte{}
    };
    append_u32(action, static_cast<std::uint32_t>(target));
    action.push_back(std::byte{0x01});
    action.push_back(std::byte{});
    action.push_back(std::byte{});
    action.push_back(std::byte{});
    append_f32(action, 8.0F);
    append_f32(action, 9.0F);
    append_u32(action, static_cast<std::uint32_t>(actor));
    return action;
}

void converts_only_explicit_exact_primary_contexts() {
    auto bytes = fixture();
    const auto base = aoe::inspect_legacy_recorded_game_bytes(bytes);
    require(base.metadata.has_value());
    bytes.resize(base.metadata->records[2].file_offset);
    for (std::uint32_t index = 0; index < 5; ++index) {
        append_action(
            bytes,
            primary_action(1, 300 + index, 100 + index),
            20 + index
        );
    }
    const auto result = aoe::inspect_legacy_recorded_game_bytes(bytes);
    require(result.metadata.has_value());

    aoe::LegacyRecordedReplayMappings mappings;
    mappings.players.emplace(1, aoe::Player::blue);
    const std::array contexts{
        aoe::LegacyPrimaryActionContext::gather_herdable,
        aoe::LegacyPrimaryActionContext::convert,
        aoe::LegacyPrimaryActionContext::heal,
        aoe::LegacyPrimaryActionContext::collect_relic,
        aoe::LegacyPrimaryActionContext::embark,
    };
    for (std::size_t index = 0; index < contexts.size(); ++index) {
        mappings.ticks.emplace(
            20 + static_cast<std::int32_t>(index),
            200 + index
        );
        mappings.entities.emplace(
            100 + static_cast<std::int32_t>(index),
            1000 + index
        );
        mappings.entities.emplace(
            300 + static_cast<std::int32_t>(index),
            3000 + index
        );
        mappings.primary_action_contexts.emplace(
            result.metadata->records[index + 2].file_offset,
            contexts[index]
        );
    }
    const auto report = aoe::convert_legacy_recorded_game_to_replay(
        *result.metadata, mappings
    );
    require(report.blockers.empty());
    require(report.replay.has_value());
    require(report.replay->commands().size() == 5);
    require(report.required_mappings.primary_action_offsets.size() == 5);
    require(std::get<aoe::GatherUnitCommand>(
        report.replay->commands()[0].command
    ).herdable == 3000);
    require(std::get<aoe::ConvertUnitCommand>(
        report.replay->commands()[1].command
    ).target == 3001);
    require(std::get<aoe::HealUnitCommand>(
        report.replay->commands()[2].command
    ).target == 3002);
    require(std::get<aoe::CollectRelicCommand>(
        report.replay->commands()[3].command
    ).relic == 3003);
    require(std::get<aoe::EmbarkCommand>(
        report.replay->commands()[4].command
    ).transport == 3004);
}

void refuses_unrepresented_primary_context_atomically() {
    auto bytes = fixture();
    const auto base = aoe::inspect_legacy_recorded_game_bytes(bytes);
    require(base.metadata.has_value());
    bytes.resize(base.metadata->records[2].file_offset);
    const auto raw = primary_action(1, 301, 101);
    append_action(bytes, raw, 20);
    const auto result = aoe::inspect_legacy_recorded_game_bytes(bytes);
    require(result.metadata.has_value());

    aoe::LegacyRecordedReplayMappings mappings;
    mappings.ticks.emplace(20, 200);
    mappings.players.emplace(1, aoe::Player::blue);
    mappings.entities = {{101, 1001}, {301, 3001}};
    const auto offset = result.metadata->records[2].file_offset;
    mappings.primary_action_contexts.emplace(
        offset, aoe::LegacyPrimaryActionContext::attack
    );
    const auto attack = aoe::convert_legacy_recorded_game_to_replay(
        *result.metadata, mappings
    );
    require(!attack.replay.has_value());
    require(attack.timeline.size() == 1);
    require(attack.timeline[0].action.raw == raw);
    require(
        attack.blockers[0].find("no exact GameCommand") != std::string::npos
    );

    mappings.primary_action_contexts[offset] =
        aoe::LegacyPrimaryActionContext::repair;
    const auto repair = aoe::convert_legacy_recorded_game_to_replay(
        *result.metadata, mappings
    );
    require(!repair.replay.has_value());
    require(repair.timeline[0].action.raw == raw);

    mappings.primary_action_contexts.clear();
    const auto missing = aoe::convert_legacy_recorded_game_to_replay(
        *result.metadata, mappings
    );
    require(!missing.replay.has_value());
    require(
        missing.blockers[0].find("missing explicit target context") !=
        std::string::npos
    );
}

}  // namespace

int main(int argc, char** argv) {
    inspects_proved_records_and_preserves_tail();
    rejects_corrupt_header_and_action_length();
    decodes_validated_action_schemas_without_guessing();
    converts_lossless_mapped_action_subset();
    emits_replay_only_with_complete_explicit_mappings();
    converts_only_explicit_exact_primary_contexts();
    refuses_unrepresented_primary_context_atomically();
    for (int index = 1; index < argc; ++index) {
        const auto result = aoe::inspect_legacy_recorded_game(argv[index]);
        if (result.status != aoe::LegacyRecordedGameStatus::inspected) {
            std::cerr << argv[index] << ": " << result.diagnostic << '\n';
        }
        require(result.status == aoe::LegacyRecordedGameStatus::inspected);
        require(result.metadata.has_value());
        const auto report = aoe::convert_legacy_recorded_game_to_replay(
            *result.metadata
        );
        std::cout << argv[index] << ": "
                  << result.metadata->game_version << ", "
                  << result.metadata->records.size()
                  << " proved records, "
                  << report.decoded_action_count << " decoded actions, "
                  << report.unsupported_action_count
                  << " unsupported actions, "
                  << result.metadata->unsupported_tail.size()
                  << " preserved tail bytes\n";
    }
    std::cout << "All legacy recorded-game tests passed\n";
}
