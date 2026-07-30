#pragma once

#include <cstdint>
#include <array>
#include <filesystem>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "aoe/types.hpp"
#include "aoe/projectile_catalog.hpp"

namespace aoe {

class Simulation;

enum class RenderObjectCategory {
    unit,
    building,
    projectile,
    impact,
    resource,
    unit_death,
    building_rubble,
};

enum class RenderAction {
    idle,
    moving,
    attacking,
    gathering,
    working,
    healing,
    converting,
    transforming,
    carrying_relic,
    dying,
    destroyed,
};

enum class RenderActionDetail {
    none,
    animal_resource,
    terrain_resource,
    farm,
    fish,
    fish_trap,
    repair,
    construction,
};

enum class ResourceRenderKind {
    forest,
    berry_bush,
    gold_mine,
    stone_mine,
    fish,
};

enum class RenderBuildingState {
    foundation,
    construction,
    completed,
    damaged,
    dying,
    destroyed,
};

enum class AssetCoverageStatus {
    renderable,
    intentional_procedural,
    missing_mapping,
    missing_archive_resource,
    invalid_dat_reference,
    decode_failure,
    missing_frame,
    missing_player_variant,
    missing_composite_part,
    missing_shadow,
    invalid_runtime_selection,
    unsupported_owner_state,
    renderer_failure,
};

struct RenderStateKey {
    RenderObjectCategory category{RenderObjectCategory::unit};
    std::string object_kind;
    RenderAction action{RenderAction::idle};
    RenderActionDetail action_detail{RenderActionDetail::none};
    RenderBuildingState building_state{RenderBuildingState::completed};
    std::uint8_t owner{};
    Civilization civilization{Civilization::generic};
    Age age{Age::dark};
    int architecture_family{};
    int damage_stage{};
    int construction_stage{};
    int upgrade_variant{};
    int direction{};
    int display_angle{};
    int animation_frame{};
    bool moving{};
    bool composite{};
    bool shadow{};

    [[nodiscard]] std::string stable_key() const;
    auto operator<=>(const RenderStateKey&) const = default;
};

struct AssetRequest {
    std::optional<std::int32_t> graphic_id;
    std::optional<std::int32_t> slp_id;
    std::vector<std::int32_t> composite_slp_ids;
    std::vector<std::int16_t> overlay_graphic_ids;
    std::optional<std::int32_t> shadow_slp_id;
    int required_frame_count{};
    int required_direction_count{};
    std::string source_mapping;
};

struct AssetResolution {
    RenderStateKey state;
    AssetRequest request;
    AssetCoverageStatus status{AssetCoverageStatus::missing_mapping};
    std::string reason;
    bool intentional_procedural{};
    std::vector<std::string> evidence_sources;
};

struct UnitAnimationSet {
    UnitKind kind{UnitKind::villager};
    std::int32_t idle_slp{-1};
    int idle_frames{};
    std::int32_t move_slp{-1};
    int move_frames{};
    std::int32_t attack_slp{-1};
    int attack_frames{};
    std::int32_t death_slp{-1};
    int death_frames{};
};

struct UnitDeathAnimationSet {
    UnitKind kind{UnitKind::villager};
    std::int32_t slp{-1};
    int frames{};
};

struct NavalCompositeSet {
    UnitKind kind{UnitKind::galley};
    RenderAction action{RenderAction::idle};
    std::array<std::int16_t, 4> graphic_roots{};
    bool expand_deltas{true};
};

struct BuildingCompositeSet {
    BuildingKind kind{BuildingKind::town_center};
    std::array<std::array<std::int16_t, 4>, 4> graphic_roots{};
    bool expand_deltas{true};
};

struct BuildingStateRoot {
    BuildingKind kind{BuildingKind::town_center};
    RenderBuildingState state{RenderBuildingState::completed};
    std::int16_t graphic_root{-1};
    bool intentional_procedural_body{};
};

struct BuildingDirectSlpSet {
    BuildingKind kind{BuildingKind::house};
    std::array<std::array<std::int32_t, 4>, 4> slps{};
    bool static_shadow{};
};

struct UnitRenderStateVariant {
    RenderAction action{RenderAction::idle};
    RenderActionDetail action_detail{RenderActionDetail::none};
    bool moving{};
    auto operator<=>(const UnitRenderStateVariant&) const = default;
};

struct ResourceAssetSet {
    ResourceRenderKind kind{ResourceRenderKind::forest};
    std::int32_t slp_id{-1};
    int frame_count{};
    int initial_amount{};
};

struct BuildingAnimatedSlpSet {
    BuildingKind kind{BuildingKind::fish_trap};
    RenderBuildingState state{RenderBuildingState::completed};
    std::int32_t slp_id{-1};
    int frame_count{};
};

struct WonderCompositeSet {
    Civilization civilization{Civilization::generic};
    std::int16_t graphic_root{-1};
};

struct BuildingTopologySlpSet {
    BuildingKind kind{BuildingKind::stone_wall};
    std::array<std::int32_t, 5> family_slps{};
    std::array<std::int16_t, 5> construction_graphic_roots{};
    std::optional<std::int32_t> explicit_shadow_slp_id;
    std::optional<std::int32_t> junction_overlay_slp_id;
    int junction_overlay_frame_count{};
    int asset_frame_count{};
    int reachable_frame_count{};
};

struct GateConstructionSet {
    BuildingKind kind{BuildingKind::palisade_gate_x};
    std::array<std::int16_t, 5> family_graphic_roots{};
};

// Canonical catalog consumed by renderer loading and coverage audit.
// A negative SLP means that action is not supplied by this record.
[[nodiscard]] std::span<const UnitAnimationSet>
canonical_unit_animation_sets();
[[nodiscard]] std::optional<UnitAnimationSet> unit_animation_set(
    UnitKind kind
);
[[nodiscard]] std::span<const UnitDeathAnimationSet>
canonical_unit_death_animation_sets();
[[nodiscard]] const UnitDeathAnimationSet* unit_death_animation_set(
    UnitKind kind
);
[[nodiscard]] std::span<const NavalCompositeSet>
canonical_naval_composite_sets();
[[nodiscard]] const NavalCompositeSet* naval_composite_set(
    UnitKind kind,
    RenderAction action
);
[[nodiscard]] std::span<const BuildingCompositeSet>
canonical_building_composite_sets();
[[nodiscard]] const BuildingCompositeSet* building_composite_set(
    BuildingKind kind
);
[[nodiscard]] std::span<const BuildingStateRoot>
canonical_building_state_roots();
[[nodiscard]] const BuildingStateRoot* building_state_root(
    BuildingKind kind,
    RenderBuildingState state
);
[[nodiscard]] std::span<const BuildingDirectSlpSet>
canonical_building_direct_slp_sets();
[[nodiscard]] const BuildingDirectSlpSet* building_direct_slp_set(
    BuildingKind kind
);
[[nodiscard]] std::span<const BuildingAnimatedSlpSet>
canonical_building_animated_slp_sets();
[[nodiscard]] const BuildingAnimatedSlpSet*
building_animated_slp_set(
    BuildingKind kind,
    RenderBuildingState state
);
[[nodiscard]] std::span<const WonderCompositeSet>
canonical_wonder_composite_sets();
[[nodiscard]] const WonderCompositeSet* wonder_composite_set(
    Civilization civilization
);
[[nodiscard]] std::span<const BuildingTopologySlpSet>
canonical_building_topology_slp_sets();
[[nodiscard]] const BuildingTopologySlpSet*
building_topology_slp_set(BuildingKind kind);
[[nodiscard]] std::span<const GateConstructionSet>
canonical_gate_construction_sets();
[[nodiscard]] const GateConstructionSet* gate_construction_set(
    BuildingKind kind
);
// Reachable action variants selected by render_action_for() and
// render_action_detail_for(). Returned order is deterministic.
[[nodiscard]] std::vector<UnitRenderStateVariant>
canonical_unit_render_states(UnitKind kind);
[[nodiscard]] std::span<const ResourceAssetSet>
canonical_resource_asset_sets();
[[nodiscard]] const ResourceAssetSet* resource_asset_set(
    ResourceRenderKind kind
);
[[nodiscard]] std::optional<ResourceRenderKind>
resource_render_kind_for(Terrain terrain);
[[nodiscard]] int render_resource_frame(
    ResourceRenderKind kind,
    int remaining
);
[[nodiscard]] std::string_view resource_render_kind_name(
    ResourceRenderKind kind
);
[[nodiscard]] AssetResolution resolve_building_asset(
    const RenderStateKey& state,
    BuildingKind kind
);
[[nodiscard]] AssetResolution resolve_unit_asset(
    const RenderStateKey& state,
    UnitKind kind
);
[[nodiscard]] AssetResolution resolve_projectile_asset(
    const RenderStateKey& state,
    ProjectileAssetKind kind
);
[[nodiscard]] AssetResolution resolve_resource_asset(
    const RenderStateKey& state,
    ResourceRenderKind kind
);

struct RuntimeFallbackEvent {
    EntityId entity_id{};
    RenderStateKey state;
    AssetRequest request;
    AssetCoverageStatus status{AssetCoverageStatus::missing_mapping};
    std::string reason;
    std::uint64_t simulation_tick{};
    std::string renderer_call_site;
};

[[nodiscard]] RenderAction render_action_for(const Unit& unit);
[[nodiscard]] bool render_unit_is_interpolating(
    const Simulation& simulation,
    const Unit& unit
);
struct RenderUnitElevationEndpoints {
    TilePosition previous{};
    TilePosition current{};
};
[[nodiscard]] RenderUnitElevationEndpoints
render_unit_elevation_endpoints(
    const Simulation& simulation,
    const Unit& unit
);
[[nodiscard]] RenderAction render_action_for(
    const Simulation& simulation,
    const Unit& unit
);
[[nodiscard]] RenderActionDetail render_action_detail_for(
    const Simulation& simulation,
    const Unit& unit
);
[[nodiscard]] RenderBuildingState render_state_for(
    const Building& building,
    int maximum_hit_points
);
[[nodiscard]] int render_construction_stage(
    const Building& building,
    int construction_ticks
);
[[nodiscard]] int render_damage_stage(int hit_points, int maximum_hit_points);
[[nodiscard]] int render_architecture_family(Civilization civilization);
[[nodiscard]] int render_building_architecture_family(
    BuildingKind kind,
    Civilization civilization
);
[[nodiscard]] int render_building_topology_frame(
    const Simulation& simulation,
    const Building& building
);
[[nodiscard]] Age render_building_visual_age(
    BuildingKind kind,
    Age current_age
);
[[nodiscard]] int render_building_upgrade_variant(
    const Simulation& simulation,
    const Building& building
);
[[nodiscard]] int render_building_composite_variant(
    BuildingKind kind,
    Age current_age,
    int upgrade_variant
);
[[nodiscard]] std::string render_unit_kind_name(UnitKind kind);
[[nodiscard]] std::string render_building_kind_name(BuildingKind kind);
[[nodiscard]] std::string asset_coverage_status_name(
    AssetCoverageStatus status
);
[[nodiscard]] std::string render_action_name(RenderAction action);
[[nodiscard]] std::string render_action_detail_name(
    RenderActionDetail detail
);

class RuntimeFallbackTelemetry {
public:
    explicit RuntimeFallbackTelemetry(
        std::optional<std::filesystem::path> output_path = std::nullopt
    );

    [[nodiscard]] bool enabled() const noexcept;
    [[nodiscard]] bool record(RuntimeFallbackEvent event);
    void write_report() const;
    [[nodiscard]] const std::map<std::string, RuntimeFallbackEvent>& events()
        const noexcept;

private:
    std::optional<std::filesystem::path> output_path_;
    std::map<std::string, RuntimeFallbackEvent> events_;
};

// Process-wide sink. Disabled unless AOE_RENDER_FALLBACK_REPORT names a file.
RuntimeFallbackTelemetry& runtime_fallback_telemetry();

}  // namespace aoe
