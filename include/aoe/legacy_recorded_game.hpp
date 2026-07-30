#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <vector>

#include "aoe/game_command.hpp"

namespace aoe {

enum class LegacyRecordedGameStatus {
    inspected,
    malformed,
    io_error,
};

enum class LegacyRecordedRecordKind {
    action,
    synchronization,
    game_start,
    chat,
};

enum class LegacyRecordedActionKind {
    primary_action,
    move,
    stop,
    train,
    research,
    tribute,
    diplomacy,
    resign,
    unsupported,
};

// Action tag 00 does not encode this distinction. Callers may supply context
// from independently decoded object state, keyed by the preserved record
// offset. Values without an exact native GameCommand remain explicit blockers.
enum class LegacyPrimaryActionContext {
    attack,
    gather_herdable,
    repair,
    convert,
    heal,
    collect_relic,
    embark,
};

struct LegacyRecordedAction {
    LegacyRecordedActionKind kind{LegacyRecordedActionKind::unsupported};
    std::uint8_t tag{};
    std::vector<std::byte> raw;
    bool schema_valid{};
    std::string diagnostic;
    std::optional<std::uint8_t> player_number;
    std::optional<std::uint8_t> player_id;
    std::optional<std::int32_t> target_id;
    std::vector<std::int32_t> entity_ids;
    std::optional<float> x;
    std::optional<float> y;
    bool reuses_previous_selection{};
    std::optional<std::int32_t> building_id;
    std::optional<std::int16_t> commercial_unit_id;
    std::optional<std::int16_t> train_count;
    std::optional<std::int16_t> commercial_technology_id;
    std::optional<std::uint8_t> target_player_number;
    std::optional<std::uint8_t> resource_type;
    std::optional<float> amount;
    std::optional<float> transaction_fee;
    std::optional<std::uint8_t> action_type;
    std::optional<std::uint8_t> option;
    std::optional<float> option2;
    std::optional<std::uint8_t> diplomatic_stance;
    std::optional<std::int32_t> disconnect;
};

struct LegacyRecordedTimelineEntry {
    std::uint64_t file_offset{};
    std::int32_t execute_at{};
    LegacyRecordedAction action;
};

struct LegacyRecordedRecord {
    LegacyRecordedRecordKind kind{LegacyRecordedRecordKind::action};
    std::uint64_t file_offset{};
    std::vector<std::byte> raw;
    std::optional<std::uint8_t> action_tag;
    std::optional<std::int32_t> execute_at;
    std::optional<std::int32_t> synchronization_interval;
    std::optional<std::int32_t> message_tag;
    std::string chat;
    std::optional<LegacyRecordedAction> action;
};

struct LegacyRecordedGameMetadata {
    std::int32_t compressed_header_length{};
    std::int32_t next_header{};
    std::vector<std::byte> compressed_header;
    std::vector<std::byte> decompressed_header;
    std::string game_version;
    std::vector<LegacyRecordedRecord> records;
    std::vector<std::byte> unsupported_tail;
    std::uint64_t unsupported_tail_offset{};
};

struct LegacyRecordedGameImportResult {
    LegacyRecordedGameStatus status{LegacyRecordedGameStatus::malformed};
    std::optional<LegacyRecordedGameMetadata> metadata;
    std::string diagnostic;
};

struct LegacyRecordedReplayMappings {
    // MGX execute_at units are not proved to equal Replay ticks. The caller
    // must provide every observed value explicitly.
    std::map<std::int32_t, std::uint64_t> ticks;
    std::map<std::int32_t, EntityId> entities;
    std::map<std::uint8_t, Player> players;
    std::map<std::int16_t, UnitKind> units;
    std::map<std::int16_t, Technology> technologies;
    std::map<std::uint64_t, LegacyPrimaryActionContext>
        primary_action_contexts;
};

struct LegacyRecordedMappingEnvelope {
    std::set<std::int32_t> execute_at_values;
    std::set<std::uint8_t> player_numbers;
    std::set<std::int32_t> entity_ids;
    std::set<std::int16_t> commercial_unit_ids;
    std::set<std::int16_t> commercial_technology_ids;
    std::set<std::uint64_t> primary_action_offsets;
};

struct LegacyRecordedReplayReport {
    std::vector<LegacyRecordedAction> actions;
    std::vector<LegacyRecordedTimelineEntry> timeline;
    std::size_t decoded_action_count{};
    std::size_t unsupported_action_count{};
    std::map<std::uint8_t, std::size_t> unsupported_tags;
    LegacyRecordedMappingEnvelope required_mappings;
    std::vector<std::string> blockers;
    std::optional<Replay> replay;
};

LegacyRecordedGameImportResult inspect_legacy_recorded_game(
    const std::filesystem::path& path
);
LegacyRecordedGameImportResult inspect_legacy_recorded_game_bytes(
    std::span<const std::byte> bytes
);
LegacyRecordedReplayReport convert_legacy_recorded_game_to_replay(
    const LegacyRecordedGameMetadata& metadata,
    const LegacyRecordedReplayMappings& mappings = {}
);

}  // namespace aoe
