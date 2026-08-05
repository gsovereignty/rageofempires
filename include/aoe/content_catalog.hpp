#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace aoe {

using CommercialObjectId = std::uint16_t;
using CommercialTechnologyId = std::uint16_t;
using CommercialEffectId = std::uint16_t;
using CommercialCivilizationId = std::uint8_t;

struct CommercialObjectIdentity {
    CommercialCivilizationId civilization_id{};
    CommercialObjectId object_id{};
    auto operator<=>(const CommercialObjectIdentity&) const = default;
};

enum class CommercialObjectBaseClass : std::uint8_t {
    static_object,
    animated,
    doppelganger,
    moving,
    action,
    base_combat,
    missile,
    combat,
    building,
    tree,
};

struct CommercialClassAmount {
    std::int16_t class_id{};
    std::int16_t amount{};
    auto operator<=>(const CommercialClassAmount&) const = default;
};

struct CommercialResourceCost {
    std::int16_t resource_id{};
    std::int16_t amount{};
    bool paid{};
    auto operator<=>(const CommercialResourceCost&) const = default;
};

struct CommercialTask {
    std::uint16_t id{};
    bool is_default{};
    std::uint16_t action_type{};
    std::int16_t object_class{-1};
    std::optional<CommercialObjectId> object_id;
    std::int16_t terrain_id{-1};
    std::array<std::int16_t, 4> attribute_types{};
    std::array<float, 2> work_values{};
    float work_range{};
    bool auto_search_targets{};
    float search_wait_time{};
    bool enable_targeting{};
    std::uint8_t combat_level{};
    std::array<std::uint16_t, 2> work_flags{};
    std::uint8_t owner_type{};
    std::uint8_t holding_attribute{};
    std::uint8_t state_building{};
    std::optional<std::uint16_t> move_graphic;
    std::optional<std::uint16_t> work_graphic;
    std::optional<std::uint16_t> secondary_work_graphic;
    std::optional<std::uint16_t> carry_graphic;
    std::optional<std::uint16_t> work_sound;
    std::optional<std::uint16_t> secondary_work_sound;
};

enum class CommercialTaskAbility : std::uint8_t {
    garrison, gather, graze, combat, bird, predator, transport, guard, make,
    build, convert, heal, repair, auto_convert, retreat, hunt, trade,
    wonder_victory, deselect, loot, unpack_attack, off_map_trade, pickup,
    kidnap, deposit,
};

[[nodiscard]] CommercialTaskAbility commercial_task_ability(
    std::uint16_t action_type
);

struct CommercialObjectRecord {
    CommercialObjectId id{};
    CommercialObjectId copy_id{};
    CommercialObjectId unit_group{};
    CommercialObjectBaseClass base_class{};
    std::int16_t unit_class{};
    bool enabled{};
    bool disabled{};
    int hit_points{};
    float line_of_sight{};
    float speed{};
    int garrison_capacity{};
    std::uint16_t terrain_restriction_id{};
    std::int16_t resource_group{};
    bool track_as_resource{};
    std::array<float, 3> radius{};
    std::uint8_t obstruction_type{};
    std::uint8_t selection_shape{};
    int attack{};
    int armor{};
    int displayed_pierce_armor{};
    float maximum_range{};
    float minimum_range{};
    float reload_time{};
    float area_effect_range{};
    int accuracy{};
    int frame_delay{};
    std::optional<CommercialObjectId> missile_object_id;
    std::vector<CommercialClassAmount> attacks;
    std::vector<CommercialClassAmount> armors;
    std::vector<CommercialResourceCost> costs;
    int creation_time{};
    std::optional<CommercialObjectId> creation_location_object_id;
    int creation_button{};
    std::optional<std::uint16_t> standing_graphic;
    std::optional<std::uint16_t> secondary_standing_graphic;
    std::optional<std::uint16_t> walking_graphic;
    std::optional<std::uint16_t> running_graphic;
    std::optional<std::uint16_t> dying_graphic;
    std::optional<std::uint16_t> attack_graphic;
    std::optional<std::uint16_t> construction_graphic;
    std::optional<std::uint16_t> button_icon;
    std::optional<std::uint16_t> portrait_icon;
    std::optional<std::uint16_t> train_sound;
    std::optional<std::uint16_t> selected_sound;
    std::optional<std::uint16_t> damage_sound;
    std::optional<std::uint16_t> death_sound;
    std::vector<CommercialTask> tasks;
};

struct CommercialTechnologyCost {
    std::uint16_t resource_id{};
    std::uint16_t amount{};
    bool enabled{};
};

struct CommercialTechnologyRecord {
    CommercialTechnologyId id{};
    std::string_view internal_name;
    std::vector<CommercialTechnologyId> prerequisites;
    std::vector<CommercialTechnologyCost> costs;
    std::optional<CommercialCivilizationId> civilization_id;
    bool full_technology_mode{};
    std::optional<CommercialObjectId> research_location_object_id;
    int research_time{};
    CommercialEffectId effect_id{};
    std::uint16_t type{};
    std::optional<std::uint16_t> icon_id;
    std::uint8_t button_id{};
};

struct CommercialEffectCommand {
    std::uint8_t type{};
    std::int16_t object_id{};
    std::int16_t unit_class{};
    std::int16_t attribute_id{};
    float amount{};
    std::optional<std::int16_t> packed_class;
    std::optional<std::int16_t> packed_amount;
};

struct CommercialEffectRecord {
    CommercialEffectId id{};
    std::string_view internal_name;
    std::vector<CommercialEffectCommand> commands;
};

class ContentCatalog {
public:
    [[nodiscard]] std::span<const CommercialCivilizationId>
    civilization_ids() const noexcept;
    [[nodiscard]] const CommercialObjectRecord* object(
        CommercialCivilizationId civilization,
        CommercialObjectId id
    ) const noexcept;
    [[nodiscard]] const CommercialTechnologyRecord* technology(
        CommercialTechnologyId id
    ) const noexcept;
    [[nodiscard]] const CommercialEffectRecord* effect(
        CommercialEffectId id
    ) const noexcept;
    [[nodiscard]] std::size_t object_variant_count() const noexcept;
    [[nodiscard]] std::size_t object_record_count() const noexcept;
    [[nodiscard]] std::span<const CommercialTechnologyRecord>
    technologies() const noexcept;
    [[nodiscard]] std::span<const CommercialObjectRecord>
    object_variants() const noexcept;
    [[nodiscard]] std::span<const CommercialEffectRecord>
    effects() const noexcept;

private:
    friend const ContentCatalog& commercial_content_catalog();
    std::vector<CommercialCivilizationId> civilization_ids_;
    std::vector<CommercialObjectRecord> object_variants_;
    std::vector<std::vector<std::uint16_t>> civilization_variant_ids_;
    std::vector<CommercialTechnologyRecord> technologies_;
    std::vector<CommercialEffectRecord> effects_;
};

[[nodiscard]] const ContentCatalog& commercial_content_catalog();

}  // namespace aoe
