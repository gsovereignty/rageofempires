#include "aoe/render_asset_coverage.hpp"
#include "aoe/simulation.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

void require(bool value, const char* message) {
    if (!value) throw std::runtime_error{message};
}

std::string read_all(const std::filesystem::path& path) {
    std::ifstream input{path, std::ios::binary};
    std::ostringstream output;
    output << input.rdbuf();
    return output.str();
}

void state_derivation_is_deterministic() {
    aoe::Unit unit;
    require(
        aoe::render_action_for(unit) == aoe::RenderAction::idle,
        "default unit must be idle"
    );
    unit.moving = true;
    require(
        aoe::render_action_for(unit) == aoe::RenderAction::idle,
        "move-order intent alone must not claim physical movement"
    );
    unit.attack_target_id = 9;
    require(
        aoe::render_action_for(unit) == aoe::RenderAction::attacking,
        "attack must override moving"
    );
    unit.conversion_target_id = 10;
    require(
        aoe::render_action_for(unit) == aoe::RenderAction::converting,
        "conversion must override attack"
    );
    unit.trebuchet_transform_ticks_remaining = 1;
    require(
        aoe::render_action_for(unit) == aoe::RenderAction::transforming,
        "transformation must override other actions"
    );

    aoe::Building building;
    building.construction_ticks_remaining = 50;
    require(
        aoe::render_state_for(building, 500) ==
            aoe::RenderBuildingState::construction,
        "unfinished building must select construction"
    );
    require(
        aoe::render_construction_stage(building, 100) == 2,
        "half-built building must select stage two"
    );
    building.construction_ticks_remaining = 0;
    building.hit_points = 249;
    require(
        aoe::render_state_for(building, 500) ==
            aoe::RenderBuildingState::damaged,
        "damaged completed building must select damaged"
    );
    require(
        aoe::render_damage_stage(124, 500) == 3,
        "quarter-health building must select severe damage"
    );

    aoe::Simulation simulation{aoe::GameMap{4, 4}};
    aoe::Unit placed;
    require(
        !aoe::render_unit_is_interpolating(simulation, placed) &&
        aoe::render_action_for(simulation, placed) ==
            aoe::RenderAction::idle,
        "tick-zero placement must not masquerade as movement"
    );
    placed.render_subtile_initialized = true;
    placed.render_previous_subtile = {320, 320};
    placed.render_current_subtile = {480, 320};
    simulation.replace_state(
        {placed}, {}, {}, {}, 3
    );
    require(
        aoe::render_unit_is_interpolating(
            simulation, simulation.units().front()
        ) &&
        aoe::render_action_for(
            simulation, simulation.units().front()
        ) == aoe::RenderAction::moving,
        "current-tick movement must drive moving render selection"
    );
}

void simulation_command_reaches_sheep_attack_mapping() {
    aoe::Simulation simulation{aoe::GameMap{8, 6}};
    const aoe::EntityId sheep = simulation.add_unit(
        aoe::UnitKind::sheep, aoe::Player::blue, {2, 2}
    );
    simulation.add_unit(
        aoe::UnitKind::villager, aoe::Player::red, {3, 2}
    );
    require(
        simulation.command_unit(sheep, {3, 2}),
        "production command path must accept sheep attack target"
    );
    const auto found = std::ranges::find(
        simulation.units(), sheep, &aoe::Unit::id
    );
    require(
        found != simulation.units().end() &&
        aoe::render_action_for(simulation, *found) ==
            aoe::RenderAction::attacking,
        "sheep attack mapping must be reachable from production command path"
    );
    aoe::RenderStateKey state;
    state.object_kind = "sheep";
    state.action = aoe::render_action_for(simulation, *found);
    const auto resolution = aoe::resolve_unit_asset(
        state, found->kind
    );
    require(
        resolution.request.slp_id == 3623,
        "reachable sheep attack must request missing SLP 3623"
    );
}

void cavalry_accumulator_wait_does_not_animate_in_place() {
    aoe::GameMap map{8, 4};
    aoe::Simulation simulation{std::move(map)};
    const aoe::EntityId scout = simulation.add_unit(
        aoe::UnitKind::scout_cavalry,
        aoe::Player::blue,
        {1, 1}
    );
    simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {7, 3}
    );
    require(
        simulation.command_unit(scout, {6, 1}),
        "scout movement command must route"
    );

    simulation.update();
    const aoe::Unit& first_step = simulation.units().front();
    const auto first_endpoints =
        aoe::render_unit_elevation_endpoints(simulation, first_step);
    require(
        first_step.position == aoe::TilePosition(2, 1) &&
        aoe::render_unit_is_interpolating(simulation, first_step) &&
        first_endpoints.previous == aoe::TilePosition(1, 1) &&
        first_endpoints.current == aoe::TilePosition(2, 1),
        "scout must begin with a physical movement step"
    );

    simulation.update();
    const aoe::Unit& waiting = simulation.units().front();
    const auto waiting_endpoints =
        aoe::render_unit_elevation_endpoints(simulation, waiting);
    const auto projected_y = [&simulation](
        aoe::TilePosition subtile,
        aoe::TilePosition elevation_position
    ) {
        return static_cast<float>(subtile.x + subtile.y) *
                16.0F / 320.0F -
            static_cast<float>(
                simulation.map().elevation_at(elevation_position) * 8
            );
    };
    require(
        waiting.moving &&
        waiting.position == aoe::TilePosition(2, 1) &&
        waiting.movement_speed_remainder > 0 &&
        waiting.movement_speed_remainder < 320,
        "scout must retain move order during accumulator wait"
    );
    require(
        aoe::render_unit_is_interpolating(simulation, waiting) &&
        aoe::render_action_for(simulation, waiting) ==
            aoe::RenderAction::moving &&
        waiting.render_previous_subtile == aoe::TilePosition(640, 320) &&
        waiting.render_current_subtile.x > 640 &&
        waiting.render_current_subtile.x < 960,
        "accumulator interval must advance cavalry through sub-tile space"
    );
    require(
        waiting.previous_position == aoe::TilePosition(1, 1) &&
        waiting_endpoints.previous == aoe::TilePosition(2, 1) &&
        waiting_endpoints.current == aoe::TilePosition(2, 1) &&
        projected_y(
            waiting.render_previous_subtile,
            waiting_endpoints.previous
        ) == 48.0F &&
        projected_y(
            waiting.render_current_subtile,
            waiting_endpoints.current
        ) > 48.0F,
        "fractional endpoints must retain presentation elevation without "
        "mutating authoritative previous position"
    );

    simulation.update();
    const aoe::Unit& advanced = simulation.units().front();
    require(
        advanced.position == aoe::TilePosition(3, 1) &&
        aoe::render_unit_is_interpolating(simulation, advanced) &&
        aoe::render_action_for(simulation, advanced) ==
            aoe::RenderAction::moving,
        "physical cavalry step must select movement animation"
    );
}

void villager_paced_step_advances_through_subtile_space() {
    aoe::Simulation simulation{aoe::GameMap{8, 4}};
    const aoe::EntityId villager = simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {1, 1}
    );
    simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {7, 3}
    );
    require(
        simulation.command_unit(villager, {5, 1}),
        "villager movement command must route"
    );

    simulation.update();
    const aoe::Unit& half_step = simulation.units().front();
    require(
        half_step.position == aoe::TilePosition(2, 1) &&
        half_step.movement_cooldown == 1 &&
        half_step.render_previous_subtile == aoe::TilePosition(320, 320) &&
        half_step.render_current_subtile == aoe::TilePosition(480, 320) &&
        aoe::render_unit_is_interpolating(simulation, half_step) &&
        aoe::render_action_for(simulation, half_step) ==
            aoe::RenderAction::moving,
        "paced villager step must expose first half-tile displacement"
    );

    simulation.update();
    const aoe::Unit& completed_step = simulation.units().front();
    require(
        completed_step.position == aoe::TilePosition(2, 1) &&
        completed_step.movement_cooldown == 0 &&
        completed_step.render_previous_subtile ==
            aoe::TilePosition(480, 320) &&
        completed_step.render_current_subtile ==
            aoe::TilePosition(640, 320) &&
        aoe::render_unit_is_interpolating(simulation, completed_step) &&
        aoe::render_action_for(simulation, completed_step) ==
            aoe::RenderAction::moving,
        "villager cooldown tick must complete presentation movement"
    );

    simulation.update();
    const aoe::Unit& next_half_step = simulation.units().front();
    require(
        next_half_step.position == aoe::TilePosition(3, 1) &&
        next_half_step.render_previous_subtile ==
            aoe::TilePosition(640, 320) &&
        next_half_step.render_current_subtile ==
            aoe::TilePosition(800, 320),
        "next villager step must continue without tile-center hold"
    );
}

void telemetry_deduplicates_and_sorts() {
    const auto path = std::filesystem::temp_directory_path() /
        "aoe-render-fallback-telemetry-test.json";
    std::filesystem::remove(path);
    aoe::RuntimeFallbackTelemetry telemetry{path};

    aoe::RuntimeFallbackEvent second;
    second.entity_id = 8;
    second.state.object_kind = "sheep";
    second.state.action = aoe::RenderAction::attacking;
    second.status = aoe::AssetCoverageStatus::missing_archive_resource;
    second.reason = "SLP 3623 absent";
    second.simulation_tick = 22;
    second.renderer_call_site = "render_unit";
    second.request.slp_id = 3623;
    require(telemetry.record(second), "first event must insert");
    require(!telemetry.record(second), "duplicate state must deduplicate");

    aoe::RuntimeFallbackEvent first = second;
    first.entity_id = 2;
    first.state.object_kind = "farm";
    first.state.category = aoe::RenderObjectCategory::building;
    first.state.action = aoe::RenderAction::working;
    first.state.building_state = aoe::RenderBuildingState::construction;
    first.status = aoe::AssetCoverageStatus::intentional_procedural;
    first.reason = "reviewed procedural contract";
    first.request.slp_id.reset();
    require(telemetry.record(first), "distinct state must insert");

    const std::string once = read_all(path);
    telemetry.write_report();
    const std::string twice = read_all(path);
    require(once == twice, "telemetry JSON must be byte deterministic");
    require(
        once.find("\"schema\":\"aoe-runtime-render-fallback-v1\"") !=
            std::string::npos,
        "schema missing"
    );
    require(
        once.find("\"slp_id\":3623") != std::string::npos,
        "requested SLP missing"
    );
    require(
        once.find("\"missing_archive_resource\"") != std::string::npos,
        "failure status missing"
    );
    require(
        telemetry.events().size() == 2,
        "wrong deduplicated event count"
    );
    std::filesystem::remove(path);
}

void canonical_resolver_selects_exact_actions() {
    aoe::RenderStateKey state;
    state.object_kind = "sheep";
    state.action = aoe::RenderAction::attacking;
    const aoe::AssetResolution attack = aoe::resolve_unit_asset(
        state, aoe::UnitKind::sheep
    );
    require(
        attack.status == aoe::AssetCoverageStatus::renderable,
        "mapped sheep attack must resolve before archive validation"
    );
    require(
        attack.request.slp_id == 3623,
        "sheep attack must request exact mapped SLP 3623"
    );
    require(
        attack.request.required_frame_count == 15,
        "sheep attack frame layout must remain exact"
    );

    state.object_kind = "archer";
    state.action = aoe::RenderAction::idle;
    const aoe::AssetResolution idle = aoe::resolve_unit_asset(
        state, aoe::UnitKind::archer
    );
    require(
        idle.status == aoe::AssetCoverageStatus::renderable &&
        idle.request.slp_id == 8 &&
        idle.request.required_frame_count == 10,
        "archer idle must select nonuniform-layout SLP 8"
    );

    state.category = aoe::RenderObjectCategory::unit_death;
    state.object_kind = "heavy_demolition_ship";
    state.action = aoe::RenderAction::dying;
    const aoe::AssetResolution naval_death = aoe::resolve_unit_asset(
        state, aoe::UnitKind::heavy_demolition_ship
    );
    require(
        naval_death.status == aoe::AssetCoverageStatus::renderable,
        "dedicated naval death must resolve through canonical catalog"
    );
    require(
        naval_death.request.slp_id == 4338 &&
        naval_death.request.required_frame_count == 7,
        "heavy demolition ship death mapping must remain exact"
    );

    state.object_kind = "villager";
    const aoe::AssetResolution villager_death =
        aoe::resolve_unit_asset(state, aoe::UnitKind::villager);
    require(
        villager_death.status ==
            aoe::AssetCoverageStatus::renderable &&
        villager_death.request.slp_id == 1476 &&
        villager_death.request.required_frame_count == 15,
        "villager death must use exact live DAT/DRS animation"
    );
}

void canonical_unit_states_cover_runtime_special_actions() {
    const auto villager = aoe::canonical_unit_render_states(
        aoe::UnitKind::villager
    );
    const auto contains = [](
        const std::vector<aoe::UnitRenderStateVariant>& states,
        aoe::RenderAction action,
        aoe::RenderActionDetail detail,
        bool moving
    ) {
        for (const auto& state : states) {
            if (state.action == action &&
                state.action_detail == detail &&
                state.moving == moving) {
                return true;
            }
        }
        return false;
    };
    require(
        contains(
            villager,
            aoe::RenderAction::gathering,
            aoe::RenderActionDetail::animal_resource,
            false
        ) &&
        contains(
            villager,
            aoe::RenderAction::working,
            aoe::RenderActionDetail::construction,
            true
        ),
        "villager catalog must include gather and construction variants"
    );

    const auto monk = aoe::canonical_unit_render_states(
        aoe::UnitKind::monk
    );
    require(
        contains(
            monk,
            aoe::RenderAction::converting,
            aoe::RenderActionDetail::none,
            false
        ) &&
        contains(
            monk,
            aoe::RenderAction::carrying_relic,
            aoe::RenderActionDetail::none,
            true
        ),
        "monk catalog must include conversion and moving relic variants"
    );

    const auto trade_cart = aoe::canonical_unit_render_states(
        aoe::UnitKind::trade_cart
    );
    require(
        !contains(
            trade_cart,
            aoe::RenderAction::attacking,
            aoe::RenderActionDetail::none,
            false
        ) &&
        contains(
            trade_cart,
            aoe::RenderAction::dying,
            aoe::RenderActionDetail::none,
            false
        ),
        "noncombat trade cart must omit attack but retain reachable death"
    );

    const auto sheep = aoe::canonical_unit_render_states(
        aoe::UnitKind::sheep
    );
    require(
        contains(
            sheep,
            aoe::RenderAction::attacking,
            aoe::RenderActionDetail::none,
            false
        ),
        "production-reachable sheep attack must remain audited"
    );

    const auto relic_states = aoe::canonical_unit_render_states(
        aoe::UnitKind::relic
    );
    require(
        relic_states.size() == 1 &&
        relic_states.front().action == aoe::RenderAction::idle,
        "untargetable stationary relic must expose only reachable idle"
    );

    aoe::RenderStateKey state;
    state.object_kind = "villager";
    state.action = aoe::RenderAction::gathering;
    state.action_detail = aoe::RenderActionDetail::animal_resource;
    const auto gather = aoe::resolve_unit_asset(
        state, aoe::UnitKind::villager
    );
    require(
        gather.request.slp_id == 1528 &&
        gather.request.required_frame_count == 15,
        "animal gathering must select runtime SLP 1528"
    );

    state.action_detail = aoe::RenderActionDetail::terrain_resource;
    const auto terrain_gather = aoe::resolve_unit_asset(
        state, aoe::UnitKind::villager
    );
    require(
        terrain_gather.request.slp_id == 1528 &&
        terrain_gather.request.required_frame_count == 15,
        "stationary terrain gathering must select gather animation"
    );

    state.moving = true;
    const auto moving_gather = aoe::resolve_unit_asset(
        state, aoe::UnitKind::villager
    );
    require(
        moving_gather.request.slp_id == 1484,
        "moving terrain gatherer must select villager movement animation"
    );

    state.object_kind = "monk";
    state.action = aoe::RenderAction::carrying_relic;
    state.action_detail = aoe::RenderActionDetail::none;
    state.moving = true;
    const auto relic = aoe::resolve_unit_asset(
        state, aoe::UnitKind::monk
    );
    require(
        relic.request.slp_id == 3831,
        "moving relic monk must select runtime SLP 3831"
    );
}

void projectile_resolver_covers_body_shadow_and_impact() {
    aoe::RenderStateKey state;
    state.category = aoe::RenderObjectCategory::projectile;
    state.object_kind = "cannonball";
    const auto body = aoe::resolve_projectile_asset(
        state, aoe::ProjectileAssetKind::cannonball
    );
    require(
        body.status == aoe::AssetCoverageStatus::renderable &&
        body.request.graphic_id == 3382 &&
        body.request.slp_id == 3803 &&
        body.request.shadow_slp_id == 3804,
        "cannonball body and shadow dependency must remain exact"
    );

    state.shadow = true;
    const auto shadow = aoe::resolve_projectile_asset(
        state, aoe::ProjectileAssetKind::cannonball
    );
    require(
        shadow.request.graphic_id == 3383 &&
        shadow.request.slp_id == 3804,
        "cannonball linked shadow must remain exact"
    );

    state.category = aoe::RenderObjectCategory::impact;
    state.shadow = false;
    const auto impact = aoe::resolve_projectile_asset(
        state, aoe::ProjectileAssetKind::cannonball
    );
    require(
        impact.request.graphic_id == 1744 &&
        impact.request.slp_id == 416 &&
        impact.request.required_frame_count == 10,
        "cannonball impact must remain exact"
    );

    state.category = aoe::RenderObjectCategory::projectile;
    state.object_kind = "arrow";
    const auto arrow = aoe::resolve_projectile_asset(
        state, aoe::ProjectileAssetKind::arrow
    );
    require(
        arrow.status == aoe::AssetCoverageStatus::renderable &&
        arrow.request.graphic_id == 638 &&
        arrow.request.slp_id == 50 &&
        arrow.request.required_direction_count == 72,
        "static 72-direction arrow must resolve exactly"
    );
}

void resource_resolver_covers_all_depletion_frames() {
    const auto mappings = aoe::canonical_resource_asset_sets();
    require(mappings.size() == 5, "all terrain resource kinds required");
    for (const aoe::ResourceAssetSet& mapping : mappings) {
        for (int frame = 0; frame < mapping.frame_count; ++frame) {
            aoe::RenderStateKey state;
            state.category = aoe::RenderObjectCategory::resource;
            state.object_kind = std::string{
                aoe::resource_render_kind_name(mapping.kind)
            };
            state.animation_frame = frame;
            const auto resolution = aoe::resolve_resource_asset(
                state, mapping.kind
            );
            require(
                resolution.status ==
                    aoe::AssetCoverageStatus::renderable &&
                resolution.request.slp_id == mapping.slp_id,
                "resource frame must resolve through canonical mapping"
            );
        }
    }
    require(
        aoe::render_resource_frame(
            aoe::ResourceRenderKind::berry_bush, 125
        ) == 0 &&
        aoe::render_resource_frame(
            aoe::ResourceRenderKind::berry_bush, 0
        ) == 3 &&
        aoe::render_resource_frame(
            aoe::ResourceRenderKind::gold_mine, 0
        ) == 6,
        "resource depletion endpoints must match runtime frame selection"
    );
}

void building_resolver_selects_age_family_and_reviewed_farm() {
    aoe::RenderStateKey state;
    state.category = aoe::RenderObjectCategory::building;
    state.object_kind = "barracks";
    state.building_state = aoe::RenderBuildingState::completed;
    state.age = aoe::Age::castle;
    state.architecture_family = 2;
    const aoe::AssetResolution barracks = aoe::resolve_building_asset(
        state, aoe::BuildingKind::barracks
    );
    require(
        barracks.status == aoe::AssetCoverageStatus::renderable,
        "castle-age barracks must resolve"
    );
    require(
        barracks.request.graphic_id == 104,
        "barracks must select exact castle-age Mediterranean root"
    );

    state.building_state = aoe::RenderBuildingState::damaged;
    state.civilization = aoe::Civilization::britons;
    state.architecture_family = 0;
    state.damage_stage = 1;
    const aoe::AssetResolution damaged_barracks =
        aoe::resolve_building_asset(
            state, aoe::BuildingKind::barracks
        );
    require(
        damaged_barracks.status ==
            aoe::AssetCoverageStatus::renderable,
        "damaged barracks must preserve renderable completed body"
    );
    require(
        damaged_barracks.request.graphic_id == 105 &&
        damaged_barracks.request.overlay_graphic_ids ==
            std::vector<std::int16_t>{4429},
        "damaged barracks must select body and first exact overlay root"
    );
    require(
        aoe::render_damage_stage(100, 100) == 0 &&
        aoe::render_damage_stage(74, 100) == 1 &&
        aoe::render_damage_stage(49, 100) == 2 &&
        aoe::render_damage_stage(24, 100) == 3,
        "damage stages must match serialized 25/50/75 thresholds"
    );

    state.building_state = aoe::RenderBuildingState::foundation;
    state.construction_stage = 0;
    const aoe::AssetResolution foundation =
        aoe::resolve_building_asset(
            state, aoe::BuildingKind::barracks
        );
    require(
        foundation.status == aoe::AssetCoverageStatus::renderable &&
        foundation.request.graphic_id == 120,
        "foundation must share canonical construction selection"
    );

    state.category = aoe::RenderObjectCategory::building_rubble;
    state.building_state = aoe::RenderBuildingState::dying;
    state.action = aoe::RenderAction::dying;
    const aoe::AssetResolution dying = aoe::resolve_building_asset(
        state, aoe::BuildingKind::barracks
    );
    require(
        dying.status == aoe::AssetCoverageStatus::renderable &&
        dying.request.graphic_id == 39,
        "dying building must share canonical destruction selection"
    );

    state.object_kind = "farm";
    const aoe::AssetResolution farm = aoe::resolve_building_asset(
        state, aoe::BuildingKind::farm
    );
    require(
        farm.status == aoe::AssetCoverageStatus::renderable &&
            !farm.intentional_procedural,
        "farm must use HD terrain textures"
    );

    state.object_kind = "house";
    state.building_state = aoe::RenderBuildingState::destroyed;
    const aoe::AssetResolution rubble = aoe::resolve_building_asset(
        state, aoe::BuildingKind::house
    );
    require(
        rubble.request.graphic_id == 38,
        "house rubble must select canonical death root"
    );

    state.category = aoe::RenderObjectCategory::building;
    state.object_kind = "fish_trap";
    state.building_state = aoe::RenderBuildingState::construction;
    const auto fish_trap = aoe::resolve_building_asset(
        state, aoe::BuildingKind::fish_trap
    );
    require(
        fish_trap.request.slp_id == 4585 &&
        fish_trap.request.required_frame_count == 1,
        "fish trap construction must use loaded exact animation"
    );

    state.object_kind = "wonder";
    state.building_state = aoe::RenderBuildingState::completed;
    state.civilization = aoe::Civilization::aztecs;
    const auto wonder = aoe::resolve_building_asset(
        state, aoe::BuildingKind::wonder
    );
    require(
        wonder.request.graphic_id == 6631,
        "Aztec Wonder must use civilization-specific root"
    );

    state.object_kind = "stone_wall";
    state.civilization = aoe::Civilization::mayans;
    state.architecture_family =
        aoe::render_building_architecture_family(
            aoe::BuildingKind::stone_wall,
            state.civilization
        );
    state.animation_frame = 2;
    const auto wall = aoe::resolve_building_asset(
        state, aoe::BuildingKind::stone_wall
    );
    require(
        state.architecture_family == 4 &&
        wall.request.slp_id == 5124,
        "Mesoamerican Stone Wall must use fifth exact SLP family"
    );
    state.building_state = aoe::RenderBuildingState::construction;
    state.construction_stage = 2;
    const auto wall_construction = aoe::resolve_building_asset(
        state, aoe::BuildingKind::stone_wall
    );
    require(
        wall_construction.request.graphic_id == 7107,
        "Mesoamerican Stone Wall construction root must remain exact"
    );

    aoe::Simulation topology{aoe::GameMap{8, 6}};
    const auto first = topology.add_building(
        aoe::BuildingKind::stone_wall,
        aoe::Player::blue,
        {2, 2}
    );
    topology.add_building(
        aoe::BuildingKind::stone_wall,
        aoe::Player::blue,
        {3, 2}
    );
    const auto first_wall = std::ranges::find(
        topology.buildings(), first, &aoe::Building::id
    );
    require(
        first_wall != topology.buildings().end() &&
        aoe::render_building_topology_frame(
            topology, *first_wall
        ) == 0,
        "horizontal wall connection must select topology frame zero"
    );

    state.object_kind = "palisade_wall";
    state.building_state = aoe::RenderBuildingState::completed;
    state.civilization = aoe::Civilization::generic;
    state.architecture_family = 0;
    state.animation_frame = 2;
    const auto palisade = aoe::resolve_building_asset(
        state, aoe::BuildingKind::palisade_wall
    );
    require(
        palisade.request.slp_id == 1828 &&
        palisade.request.shadow_slp_id == 4682 &&
        palisade.request.composite_slp_ids ==
            std::vector<std::int32_t>{4534},
        "Palisade Wall junction must include body, shadow, and flags"
    );
    state.building_state = aoe::RenderBuildingState::damaged;
    const auto damaged_palisade = aoe::resolve_building_asset(
        state, aoe::BuildingKind::palisade_wall
    );
    require(
        damaged_palisade.status ==
            aoe::AssetCoverageStatus::intentional_procedural,
        "Palisade Wall damaged body must remain reviewed procedural"
    );

    state.object_kind = "castle";
    state.building_state = aoe::RenderBuildingState::completed;
    state.age = aoe::Age::dark;
    state.architecture_family = 0;
    state.animation_frame = 0;
    const auto early_castle = aoe::resolve_building_asset(
        state, aoe::BuildingKind::castle
    );
    require(
        aoe::render_building_visual_age(
            aoe::BuildingKind::castle, aoe::Age::dark
        ) == aoe::Age::castle &&
        early_castle.request.graphic_id == 174,
        "scenario-placed early Castle must clamp to minimum visual Age"
    );

    state.object_kind = "watch_tower";
    state.age = aoe::Age::imperial;
    state.upgrade_variant = 2;
    const auto keep = aoe::resolve_building_asset(
        state, aoe::BuildingKind::watch_tower
    );
    require(
        keep.request.graphic_id == 2407,
        "Keep technology variant must select exact upgraded root"
    );
    require(
        aoe::render_building_composite_variant(
            aoe::BuildingKind::stone_gate_x,
            aoe::Age::dark,
            0
        ) == 2,
        "early scenario Stone Gate must select first available Castle root"
    );
    for (const aoe::Age age : {
             aoe::Age::dark,
             aoe::Age::feudal,
             aoe::Age::castle,
             aoe::Age::imperial,
         }) {
        require(
            aoe::render_building_visual_age(
                aoe::BuildingKind::town_center, age
            ) == age,
            "Town Center renderer must isolate requested current Age"
        );
    }
    state.object_kind = "town_center";
    state.building_state = aoe::RenderBuildingState::completed;
    state.age = aoe::Age::feudal;
    state.civilization = aoe::Civilization::mayans;
    state.architecture_family =
        aoe::render_building_architecture_family(
            aoe::BuildingKind::town_center,
            state.civilization
        );
    const auto mayan_town_center = aoe::resolve_building_asset(
        state, aoe::BuildingKind::town_center
    );
    require(
        state.architecture_family == 4 &&
        mayan_town_center.request.graphic_id == 6986,
        "Mesoamerican Town Center must select exact X-family Feudal root"
    );
    const auto* town_center_mapping =
        aoe::building_composite_set(aoe::BuildingKind::town_center);
    require(
        town_center_mapping != nullptr &&
        town_center_mapping->composition_policy ==
            aoe::CompositePolicy::complete_root,
        "present Town Center root SLP must not expand its DAT deltas"
    );

    state.object_kind = "monastery";
    state.age = aoe::Age::castle;
    state.civilization = aoe::Civilization::mayans;
    state.architecture_family =
        aoe::render_building_architecture_family(
            aoe::BuildingKind::monastery,
            state.civilization
        );
    const auto missing_mayan_monastery = aoe::resolve_building_asset(
        state, aoe::BuildingKind::monastery
    );
    require(
        missing_mayan_monastery.status ==
            aoe::AssetCoverageStatus::missing_mapping &&
        !missing_mayan_monastery.request.graphic_id,
        "missing selected X-family mapping must not borrow another family"
    );

    const auto* dock_mapping =
        aoe::building_composite_set(aoe::BuildingKind::dock);
    require(
        dock_mapping != nullptr &&
        dock_mapping->composition_policy ==
            aoe::CompositePolicy::delta_graph,
        "SLP-less Dock root must retain selected-root delta composition"
    );
    require(
        aoe::render_component_animation_frame(1, 999, true) == 0 &&
        aoe::render_component_animation_frame(6, 14, true) == 1 &&
        aoe::render_component_animation_frame(6, 14, false) == 0 &&
        !aoe::render_component_animation_frame(0, 14, true),
        "each composite layer must bound elapsed phase to its own frames"
    );
    require(
        aoe::render_component_animation_frame_at_time(
            4, 0, 0.05F, 0.10F, true
        ) == 0 &&
        aoe::render_component_animation_frame_at_time(
            4, 51, 0.05F, 0.10F, true
        ) == 1 &&
        aoe::render_component_animation_frame_at_time(
            4, 225, 0.05F, 0.10F, true
        ) == 3 &&
        aoe::render_component_animation_frame_at_time(
            4, 301, 0.05F, 0.10F, true
        ) == 0,
        "graphic timing must use frame rate and hold final frame for replay delay"
    );

    for (const aoe::BuildingCompositeSet& mapping :
         aoe::canonical_building_composite_sets()) {
        for (int raw_age = 0; raw_age < 4; ++raw_age) {
            for (int family = 0; family < 5; ++family) {
                state.object_kind =
                    aoe::render_building_kind_name(mapping.kind);
                state.age = static_cast<aoe::Age>(raw_age);
                state.architecture_family = family;
                state.upgrade_variant =
                    mapping.kind == aoe::BuildingKind::watch_tower
                    ? std::min(raw_age, 2)
                    : 0;
                const int variant =
                    aoe::render_building_composite_variant(
                        mapping.kind,
                        state.age,
                        state.upgrade_variant
                    );
                const auto resolution = aoe::resolve_building_asset(
                    state, mapping.kind
                );
                const std::int16_t expected =
                    mapping.graphic_roots[
                        static_cast<std::size_t>(variant)
                    ][static_cast<std::size_t>(family)];
                require(
                    expected < 0
                        ? resolution.status ==
                            aoe::AssetCoverageStatus::missing_mapping
                        : resolution.request.graphic_id == expected,
                    "catalog/runtime composite selection parity failed"
                );
            }
        }
    }

    for (const aoe::BuildingDirectSlpSet& mapping :
         aoe::canonical_building_direct_slp_sets()) {
        for (int raw_age = 0; raw_age < 4; ++raw_age) {
            for (int family = 0; family < 5; ++family) {
                state.object_kind =
                    aoe::render_building_kind_name(mapping.kind);
                state.age = static_cast<aoe::Age>(raw_age);
                state.architecture_family = family;
                state.upgrade_variant = 0;
                const auto visual_age =
                    aoe::render_building_visual_age(
                        mapping.kind, state.age
                    );
                const std::int32_t expected = mapping.slps[
                    static_cast<std::size_t>(visual_age)
                ][static_cast<std::size_t>(family)];
                const auto resolution = aoe::resolve_building_asset(
                    state, mapping.kind
                );
                require(
                    expected < 0
                        ? resolution.status ==
                            aoe::AssetCoverageStatus::missing_mapping
                        : resolution.request.slp_id == expected,
                    "catalog/runtime direct-SLP selection parity failed"
                );
            }
        }
    }
    state.object_kind = "palisade_gate_x";
    state.building_state = aoe::RenderBuildingState::construction;
    state.civilization = aoe::Civilization::mayans;
    const auto gate_construction = aoe::resolve_building_asset(
        state, aoe::BuildingKind::palisade_gate_x
    );
    require(
        gate_construction.request.graphic_id == 6798,
        "Mesoamerican Palisade Gate X construction root must remain exact"
    );
    state.building_state = aoe::RenderBuildingState::completed;
    const auto gate_complete = aoe::resolve_building_asset(
        state, aoe::BuildingKind::palisade_gate_x
    );
    require(
        gate_complete.request.graphic_id == 6512,
        "Palisade Gate X completed composite root must remain exact"
    );
}

}  // namespace

int main() {
    try {
        state_derivation_is_deterministic();
        simulation_command_reaches_sheep_attack_mapping();
        cavalry_accumulator_wait_does_not_animate_in_place();
        villager_paced_step_advances_through_subtile_space();
        telemetry_deduplicates_and_sorts();
        canonical_resolver_selects_exact_actions();
        canonical_unit_states_cover_runtime_special_actions();
        projectile_resolver_covers_body_shadow_and_impact();
        resource_resolver_covers_all_depletion_frames();
        building_resolver_selects_age_family_and_reviewed_farm();
    } catch (const std::exception& error) {
        std::cerr << "render_asset_coverage_tests: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
