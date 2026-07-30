#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "aoe/legacy_dat.hpp"
#include "aoe/scenario.hpp"

namespace aoe {

enum class LegacyScenarioImportStatus {
    metadata_only,
    unsupported_version,
    malformed,
    io_error,
};

struct LegacyScenarioMetadata {
    struct PlayerSettings {
        std::int32_t active{};
        std::int32_t player_type{};
        std::int32_t civilization_id{};
        std::int32_t posture{};
        std::int32_t gold{};
        std::int32_t wood{};
        std::int32_t food{};
        std::int32_t stone{};
        std::int32_t ore{100};
        std::int32_t goods{};
        std::int32_t starting_age_id{};
        std::vector<std::int32_t> diplomacy;
    };
    std::string format_version;
    std::uint32_t header_version{};
    std::optional<std::uint32_t> creation_timestamp;
    std::string description;
    bool has_single_player_victory{};
    std::uint32_t active_player_count{};
    std::uint64_t compressed_body_size{};
    std::uint64_t uncompressed_body_size{};
    std::int32_t next_object_id{};
    float data_version{};
    std::uint64_t tribe_section_end{};
    bool player_settings_decoded{};
    std::vector<PlayerSettings> players;
    bool map_decoded{};
    std::uint32_t map_width{};
    std::uint32_t map_height{};
    struct Tile {
        std::uint8_t terrain{};
        std::int8_t elevation{};
        std::int8_t zone{};
    };
    std::vector<Tile> map_tiles;
    struct Object {
        std::uint32_t owner_slot{};
        float x{};
        float y{};
        float z{};
        std::int32_t object_id{};
        std::uint16_t unit_type_id{};
        std::uint8_t state{};
        float angle{};
        std::int16_t animation_frame{-1};
        std::optional<std::int32_t> garrisoned_in;
    };
    struct TriggerCondition {
        std::int32_t type{};
        std::vector<std::int32_t> properties;
        std::size_t decoded_property_count{};
    };
    struct TriggerEffect {
        std::int32_t type{};
        std::vector<std::int32_t> properties;
        std::size_t decoded_property_count{};
        std::string chat_text;
        std::string audio_file;
        std::vector<std::int32_t> object_ids;
    };
    struct Trigger {
        bool enabled{};
        bool looping{};
        std::int32_t name_id{};
        bool objective{};
        std::int32_t objective_order{};
        std::uint32_t start_time{};
        std::string description;
        std::string name;
        std::vector<TriggerEffect> effects;
        std::vector<std::int32_t> effect_order;
        std::vector<TriggerCondition> conditions;
        std::vector<std::int32_t> condition_order;
    };
    bool objects_decoded{};
    std::vector<Object> objects;
    bool triggers_decoded{};
    double trigger_version{};
    std::vector<Trigger> triggers;
    // Display/execution order stored after the trigger records. Values are
    // indices into triggers.
    std::vector<std::int32_t> trigger_order;
};

struct LegacyScenarioImportResult {
    LegacyScenarioImportStatus status{LegacyScenarioImportStatus::malformed};
    std::optional<LegacyScenarioMetadata> metadata;
    std::string diagnostic;
};

// Inspects classic Genie .scn/.scx data without pretending to convert its
// version-dependent map, object, player, or trigger sections.
LegacyScenarioImportResult inspect_legacy_scenario(
    const std::filesystem::path& path
);
LegacyScenarioImportResult inspect_legacy_scenario_bytes(
    std::span<const std::byte> bytes
);

struct LegacyScenarioConversionReport {
    struct Loss {
        std::int32_t object_id{};
        std::vector<std::string> fields;
    };
    struct UnsupportedCondition {
        std::size_t trigger_index{};
        std::size_t condition_index{};
        LegacyScenarioMetadata::TriggerCondition raw;
        std::string reason;
    };
    struct UnsupportedEffect {
        std::size_t trigger_index{};
        std::size_t effect_index{};
        LegacyScenarioMetadata::TriggerEffect raw;
        std::string reason;
    };
    struct ObjectIdRemap {
        std::size_t source_object_index{};
        std::int32_t commercial_object_id{};
        EntityId native_entity_id{};
        bool building{};
        bool lossless{};
        std::string blocker;
    };
    std::optional<Scenario> scenario;
    std::size_t translated_tiles{};
    std::size_t unsupported_tiles{};
    std::size_t translated_objects{};
    std::size_t unsupported_objects{};
    // Classic ScenarioObject schema has no current/max HP field.
    bool object_hit_points_available{};
    std::size_t objects_using_default_hit_points{};
    std::vector<std::size_t> unsupported_tile_indices;
    std::vector<LegacyScenarioMetadata::Object> unsupported_object_records;
    std::map<std::uint16_t, std::size_t>
        unsupported_commercial_object_ids;
    std::vector<Loss> lossy_objects;
    std::size_t translated_triggers{};
    std::size_t unsupported_triggers{};
    std::size_t translated_conditions{};
    std::size_t translated_effects{};
    std::vector<LegacyScenarioMetadata::Trigger> unsupported_trigger_records;
    std::vector<UnsupportedCondition> unsupported_conditions;
    std::vector<UnsupportedEffect> unsupported_effects;
    std::vector<ObjectIdRemap> object_id_remap;
    std::vector<std::string> diagnostics;
    struct TriggerAudit {
        std::map<std::int32_t, std::size_t> condition_types;
        std::map<std::int32_t, std::size_t> effect_types;
        std::map<std::int32_t, std::size_t> unsupported_condition_types;
        std::map<std::int32_t, std::size_t> unsupported_effect_types;
        std::map<std::string, std::size_t> missing_property_indices;
        std::map<std::string, std::size_t> object_reference_blockers;
        std::map<std::string, std::size_t> selector_shapes;
        std::map<std::uint16_t, std::size_t> mapped_commercial_object_ids;
        std::map<std::uint16_t, std::size_t> unmapped_commercial_object_ids;
        std::map<std::int32_t, std::size_t> research_technology_ids;
        std::size_t trigger_count{};
        std::size_t condition_count{};
        std::size_t effect_count{};
        std::size_t direct_object_references{};
        std::size_t listed_object_references{};
    };
    TriggerAudit trigger_audit;
};

struct CommercialObjectMapping {
    std::uint16_t commercial_id{};
    std::optional<UnitKind> unit;
    std::optional<BuildingKind> building;
};

[[nodiscard]] const std::vector<CommercialObjectMapping>&
commercial_object_mappings();

[[nodiscard]] LegacyScenarioConversionReport::TriggerAudit
audit_legacy_scenario_triggers(const LegacyScenarioMetadata& source);
[[nodiscard]] std::string legacy_scenario_trigger_audit_json(
    const LegacyScenarioConversionReport::TriggerAudit& audit
);
[[nodiscard]] std::string legacy_scenario_conversion_report_json(
    const LegacyScenarioConversionReport& report
);

// Converts only IDs independently evidenced by the live VER 5.7 DAT metadata
// work. Unsupported terrain or trigger semantics prevent a playable Scenario;
// unsupported objects remain explicit while supported content is converted.
LegacyScenarioConversionReport convert_legacy_scenario(
    const LegacyScenarioMetadata& source,
    const LegacyDatFile& dat
);

}  // namespace aoe
