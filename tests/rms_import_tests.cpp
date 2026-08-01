#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>

#include "aoe/computer_player.hpp"
#include "aoe/format_versions.hpp"
#include "aoe/rms_import.hpp"

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

constexpr std::string_view common_script = R"rms(
#include_drs random_map.def 54000
<PLAYER_SETUP>
random_placement
override_map_size 48
<LAND_GENERATION>
base_terrain WATER
create_player_lands
{
 terrain_type GRASS
 land_percent 35
 base_size 10
 border_fuzziness 10
}
<ELEVATION_GENERATION>
create_elevation 2 {
 number_of_clumps 8
 number_of_tiles 120
 set_scale_by_size
}
<CLIFF_GENERATION>
create_cliffs {
 min_number_of_cliffs 2
 max_number_of_cliffs 4
 min_length_of_cliff 3
 max_length_of_cliff 7
}
<TERRAIN_GENERATION>
start_random
percent_chance 60
create_terrain PINE_FOREST {
 number_of_clumps 8
 land_percent 5
}
percent_chance 40
create_terrain SHALLOWS {
 number_of_clumps 4
 land_percent 3
}
end_random
<CONNECTION_GENERATION>
create_connect_all_lands {
 terrain_type SHALLOWS
}
<OBJECTS_GENERATION>
create_object TOWN_CENTER {
 set_place_for_every_player
 min_distance_to_players 0
}
create_object VILLAGER {
 set_place_for_every_player
 number_of_objects 3
}
create_object SCOUT {
 set_place_for_every_player
 number_of_objects 1
}
create_object GOLD {
 set_place_for_every_player
 number_of_objects 7
}
)rms";

std::size_t count_terrain(
    const aoe::Scenario& scenario, aoe::Terrain terrain
);

void common_subset_parses_and_evaluates() {
    const aoe::RmsDocument document = aoe::parse_rms(common_script);
    require(document.syntactically_valid, document.error);
    require(document.playable(), "common subset unexpectedly refused");
    require(document.unsupported.size() == 1, "include not preserved");
    require(
        !document.unsupported.front().affects_map &&
        document.unsupported.front().exact_text ==
            "#include_drs random_map.def 54000",
        "include preservation mismatch"
    );
    const auto first = aoe::evaluate_rms(
        document, 77, aoe::Civilization::chinese,
        aoe::Civilization::mayans
    );
    const auto second = aoe::evaluate_rms(
        document, 77, aoe::Civilization::chinese,
        aoe::Civilization::mayans
    );
    require(first && second, "playable RMS did not evaluate");
    require(
        aoe::random_map_hash(*first) == aoe::random_map_hash(*second),
        "same RMS seed changed output"
    );
    require(
        first->map.width() == 120 &&
        first->blue_civilization == aoe::Civilization::chinese &&
        first->red_civilization == aoe::Civilization::mayans,
        "RMS setup did not reach generated scenario"
    );
    (void)aoe::create_simulation(*first);
}

void frontend_bridge_uses_rms_not_native_recipes() {
    std::set<std::string> rms_hashes;
    for (const aoe::RandomMapKind kind : {
             aoe::RandomMapKind::arabia,
             aoe::RandomMapKind::black_forest,
             aoe::RandomMapKind::islands,
             aoe::RandomMapKind::rivers,
         }) {
        const aoe::RandomMapSettings settings{
            kind,
            aoe::RandomMapSize::tiny,
            445,
            aoe::Civilization::britons,
            aoe::Civilization::franks,
        };
        const aoe::RmsMapResult first = aoe::generate_rms_map(settings);
        const aoe::RmsMapResult repeat = aoe::generate_rms_map(settings);
        require(
            first.scenario && repeat.scenario,
            "frontend RMS bridge failed: " + first.error
        );
        const std::string rms_hash = aoe::random_map_hash(*first.scenario);
        require(
            rms_hash == aoe::random_map_hash(*repeat.scenario),
            "frontend RMS bridge changed same-seed output"
        );
        require(
            rms_hash != aoe::random_map_hash(
                aoe::generate_random_map(settings)
            ),
            "frontend RMS bridge returned old native recipe"
        );
        rms_hashes.insert(rms_hash);
        aoe::Simulation simulation =
            aoe::create_simulation(*first.scenario);
        require(
            simulation.age(
                aoe::EntityOwner{aoe::Player::neutral}
            ) == aoe::Age::dark,
            "Gaia entity age baseline unavailable"
        );
        aoe::ComputerPlayer computer(aoe::Player::red);
        for (int tick = 0; tick < 20; ++tick) {
            simulation.update();
            computer.update(simulation);
        }
    }
    require(
        rms_hashes.size() == 4,
        "classic RMS map choices collapsed to same gameplay map"
    );

    constexpr std::string_view custom = R"rms(
<LAND_GENERATION>
base_terrain WATER
create_player_lands {
 terrain_type GRASS
 number_of_tiles 500
}
<OBJECTS_GENERATION>
create_object TOWN_CENTER {
 set_place_for_every_player
}
)rms";
    aoe::RandomMapSettings custom_settings;
    custom_settings.size = aoe::RandomMapSize::tiny;
    custom_settings.seed = 446;
    const aoe::RmsMapResult custom_map =
        aoe::generate_rms_map(custom_settings, custom);
    require(custom_map.scenario.has_value(), "custom frontend RMS refused");
    require(
        count_terrain(*custom_map.scenario, aoe::Terrain::water) > 0,
        "custom frontend RMS did not drive gameplay terrain"
    );
}

void unsupported_semantics_preserve_span_and_refuse() {
    constexpr std::string_view source = R"rms(<LAND_GENERATION>
base_terrain GRASS
unknown_land_command 7
{
 exact_child 19
}
<OBJECTS_GENERATION>
create_object TOWN_CENTER {
 set_place_for_every_player
}
)rms";
    const aoe::RmsDocument document = aoe::parse_rms(source);
    require(document.syntactically_valid, document.error);
    require(!document.playable(), "unknown map semantics accepted");
    require(document.unsupported.size() == 1, "unsupported block lost");
    require(
        document.unsupported[0].span.first_line == 3 &&
        document.unsupported[0].span.last_line == 6 &&
        document.unsupported[0].exact_text ==
            "unknown_land_command 7\n{\n exact_child 19\n}",
        "unsupported exact line span changed"
    );
    require(
        !aoe::evaluate_rms(document, 1),
        "unsupported semantics produced playable output"
    );
}

void malformed_and_oversized_inputs_fail_closed() {
    const aoe::RmsDocument malformed = aoe::parse_rms(
        "<LAND_GENERATION>\ncreate_land {\n"
    );
    require(
        !malformed.syntactically_valid &&
        !malformed.playable(),
        "unterminated block accepted"
    );
    aoe::RmsImportLimits limits;
    limits.maximum_bytes = 8;
    const aoe::RmsDocument oversized =
        aoe::parse_rms("<PLAYER_SETUP>", limits);
    require(!oversized.syntactically_valid, "byte limit ignored");
}

void evaluation_writes_current_scenario() {
    const auto scenario = aoe::evaluate_rms(
        aoe::parse_rms(common_script), 991
    );
    require(scenario.has_value(), "fixture did not evaluate");
    const auto path = std::filesystem::temp_directory_path() /
        "aoe-rms-import.scenario";
    aoe::save_scenario(*scenario, path);
    std::ifstream input(path);
    std::string magic;
    int version{};
    input >> magic >> version;
    std::filesystem::remove(path);
    require(
        magic == "AOE-ARCHAEOLOGY-SCENARIO" &&
        version == aoe::reconstruction_scenario_version,
        "RMS evaluation did not emit current scenario version"
    );
}

std::size_t count_units(
    const aoe::Scenario& scenario,
    aoe::UnitKind kind,
    aoe::Player player
) {
    return static_cast<std::size_t>(std::ranges::count_if(
        scenario.units, [&](const aoe::UnitPlacement& unit) {
            return unit.kind == kind &&
                unit.owner.legacy_player() == player;
        }
    ));
}

std::size_t count_terrain(
    const aoe::Scenario& scenario, aoe::Terrain terrain
) {
    std::size_t count{};
    for (int y = 0; y < scenario.map.height(); ++y) {
        for (int x = 0; x < scenario.map.width(); ++x) {
            count += scenario.map.terrain_at({x, y}) == terrain;
        }
    }
    return count;
}

void rms_resource_terrain_retains_gatherable_amounts() {
    aoe::RandomMapSettings settings;
    settings.kind = aoe::RandomMapKind::arabia;
    settings.size = aoe::RandomMapSize::maximum;
    settings.seed = 1;
    settings.blue_civilization = aoe::Civilization::britons;
    const aoe::RmsMapResult generated = aoe::generate_rms_map(settings);
    require(generated.scenario.has_value(), generated.error);

    const aoe::TilePosition live_regression_tree{51, 125};
    require(
        generated.scenario->map.terrain_at(live_regression_tree) ==
            aoe::Terrain::forest,
        "seed-1 live regression tile stopped being forest"
    );
    require(
        generated.scenario->map.resource_amount_at(live_regression_tree) ==
            100,
        "RMS forest terrain must retain default gatherable wood"
    );

    for (int y = 0; y < generated.scenario->map.height(); ++y) {
        for (int x = 0; x < generated.scenario->map.width(); ++x) {
            const aoe::TilePosition tile{x, y};
            const aoe::Terrain terrain =
                generated.scenario->map.terrain_at(tile);
            if (terrain == aoe::Terrain::forest ||
                terrain == aoe::Terrain::berry_bush ||
                terrain == aoe::Terrain::gold_mine ||
                terrain == aoe::Terrain::stone_mine ||
                terrain == aoe::Terrain::fish) {
                require(
                    generated.scenario->map.resource_amount_at(tile) > 0,
                    "RMS resource terrain contains no gatherable amount"
                );
            }
        }
    }

    aoe::Simulation simulation =
        aoe::create_simulation(*generated.scenario);
    const aoe::EntityId worker = simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {63, 126}
    );
    const int starting_wood =
        simulation.economy(aoe::Player::blue).wood;
    require(
        simulation.command_unit(worker, live_regression_tree),
        "live RMS forest gathering command was rejected"
    );
    for (int tick = 0; tick < 500; ++tick) simulation.update();
    const auto worker_state = std::ranges::find_if(
        simulation.units(),
        [worker](const aoe::Unit& unit) { return unit.id == worker; }
    );
    require(worker_state != simulation.units().end(), "worker disappeared");
    require(
        worker_state->position != aoe::TilePosition(63, 126) &&
        (worker_state->carried_amount > 0 ||
         simulation.economy(aoe::Player::blue).wood > starting_wood),
        "worker did not move and gather from live RMS forest"
    );
}

std::size_t count_elevated(const aoe::Scenario& scenario) {
    std::size_t count{};
    for (int y = 0; y < scenario.map.height(); ++y) {
        for (int x = 0; x < scenario.map.width(); ++x) {
            count += scenario.map.elevation_at({x, y}) > 0;
        }
    }
    return count;
}

void sections_drive_map_instead_of_recipe_selection() {
    constexpr std::string_view source = R"rms(
<PLAYER_SETUP>
override_map_size 48
<LAND_GENERATION>
base_terrain WATER
create_player_lands {
 terrain_type GRASS
 land_percent 18
 base_size 8
}
<TERRAIN_GENERATION>
create_terrain PINE_FOREST {
 number_of_clumps 3
 number_of_tiles 90
 set_avoid_player_start_areas
}
<ELEVATION_GENERATION>
create_elevation 3 {
 number_of_clumps 2
 number_of_tiles 50
}
<CONNECTION_GENERATION>
create_connect_all_lands {
 terrain_type SHALLOWS
}
<OBJECTS_GENERATION>
create_object VILLAGER {
 set_place_for_every_player
 number_of_objects 2
}
)rms";
    const auto scenario = aoe::evaluate_rms(aoe::parse_rms(source), 812);
    require(scenario.has_value(), "section-driven fixture refused");
    require(
        count_terrain(*scenario, aoe::Terrain::water) > 0 &&
        count_terrain(*scenario, aoe::Terrain::grass) > 0,
        "land generation did not paint player lands over base terrain"
    );
    require(
        count_terrain(*scenario, aoe::Terrain::forest) > 0,
        "terrain generation did not paint requested terrain"
    );
    require(
        count_elevated(*scenario) > 0,
        "elevation generation did not write elevation"
    );
    require(
        scenario->map.terrain_at({
            scenario->map.width() / 2, scenario->map.height() / 2
        }) == aoe::Terrain::shallows,
        "connection generation did not paint path between player lands"
    );
    require(
        count_units(*scenario, aoe::UnitKind::villager,
                    aoe::Player::blue) == 2 &&
        count_units(*scenario, aoe::UnitKind::villager,
                    aoe::Player::red) == 2,
        "object generation did not place requested per-player objects"
    );

    const std::string no_features =
        "<PLAYER_SETUP>\noverride_map_size 48\n"
        "<LAND_GENERATION>\nbase_terrain WATER\n"
        "create_player_lands {\n terrain_type GRASS\n"
        " land_percent 18\n base_size 8\n}\n";
    const auto plain = aoe::evaluate_rms(
        aoe::parse_rms(no_features), 812
    );
    require(plain.has_value(), "plain land fixture refused");
    require(
        count_terrain(*plain, aoe::Terrain::forest) == 0 &&
        count_elevated(*plain) == 0 &&
        aoe::random_map_hash(*plain) != aoe::random_map_hash(*scenario),
        "section changes still collapse to same hard-coded recipe"
    );
}

void block_base_terrain_and_land_origin_connections_are_scoped() {
    constexpr std::string_view scoped = R"rms(
<PLAYER_SETUP>
override_map_size 48
<LAND_GENERATION>
base_terrain GRASS
<TERRAIN_GENERATION>
create_terrain PINE_FOREST {
 base_terrain WATER
 number_of_clumps 2
 number_of_tiles 80
}
)rms";
    const auto scoped_map = aoe::evaluate_rms(
        aoe::parse_rms(scoped), 88
    );
    require(scoped_map.has_value(), "scoped base-terrain fixture refused");
    require(
        count_terrain(*scoped_map, aoe::Terrain::water) == 0 &&
        count_terrain(*scoped_map, aoe::Terrain::forest) == 0,
        "create_terrain base_terrain leaked into map base or ignored filter"
    );

    constexpr std::string_view connected = R"rms(
<PLAYER_SETUP>
override_map_size 48
<LAND_GENERATION>
base_terrain WATER
create_player_lands {
 terrain_type GRASS
 number_of_tiles 300
}
create_land {
 terrain_type FOREST
 number_of_tiles 80
 land_position 50 40
 zone 7
}
<CONNECTION_GENERATION>
create_connect_all_lands {
 default_terrain_replacement GRASS
 replace_terrain WATER SHALLOWS
 terrain_size 3
}
)rms";
    const auto connected_map = aoe::evaluate_rms(
        aoe::parse_rms(connected), 89
    );
    require(
        connected_map.has_value(), "land-origin connection fixture refused"
    );
    require(
        count_terrain(*connected_map, aoe::Terrain::shallows) > 0,
        "connection did not apply WATER-to-SHALLOWS replacement rule"
    );
    require(
        connected_map->map.terrain_at({60, 48}) ==
            aoe::Terrain::grass,
        "connection fallback did not replace neutral origin terrain"
    );
}

void generation_uses_classic_section_order() {
    constexpr std::string_view land = R"rms(
<LAND_GENERATION>
base_terrain GRASS
create_player_lands {
 terrain_type GRASS
 number_of_tiles 250
}
)rms";
    constexpr std::string_view elevation = R"rms(
<ELEVATION_GENERATION>
create_elevation 3 {
 number_of_clumps 2
 number_of_tiles 60
}
)rms";
    constexpr std::string_view terrain = R"rms(
<TERRAIN_GENERATION>
create_terrain FOREST {
 base_terrain GRASS
 number_of_clumps 3
 number_of_tiles 90
}
)rms";
    const std::string canonical =
        "<PLAYER_SETUP>\noverride_map_size 48\n" +
        std::string(land) + std::string(elevation) + std::string(terrain);
    const std::string reversed =
        "<PLAYER_SETUP>\noverride_map_size 48\n" +
        std::string(terrain) + std::string(elevation) + std::string(land);
    const auto first = aoe::evaluate_rms(
        aoe::parse_rms(canonical), 9911
    );
    const auto second = aoe::evaluate_rms(
        aoe::parse_rms(reversed), 9911
    );
    require(first && second, "section-order fixtures refused");
    require(
        aoe::random_map_hash(*first) == aoe::random_map_hash(*second),
        "source section order changed classic generation order"
    );
}

void terrain_spacing_respects_existing_generation_order() {
    constexpr std::string_view source = R"rms(
<PLAYER_SETUP>
override_map_size 48
<LAND_GENERATION>
base_terrain GRASS
<TERRAIN_GENERATION>
create_terrain WATER {
 base_terrain GRASS
 number_of_clumps 3
 number_of_tiles 300
 clumping_factor 20
}
create_terrain FOREST {
 base_terrain GRASS
 number_of_clumps 20
 number_of_tiles 600
 clumping_factor 2
 spacing_to_other_terrain_types 6
}
)rms";
    const auto scenario = aoe::evaluate_rms(
        aoe::parse_rms(source), 411
    );
    require(scenario.has_value(), "terrain-spacing fixture refused");
    require(
        count_terrain(*scenario, aoe::Terrain::water) > 0 &&
        count_terrain(*scenario, aoe::Terrain::forest) > 0,
        "terrain-spacing fixture did not generate both terrains"
    );
    for (int y = 0; y < scenario->map.height(); ++y) {
        for (int x = 0; x < scenario->map.width(); ++x) {
            if (scenario->map.terrain_at({x, y}) !=
                aoe::Terrain::forest) continue;
            for (int dy = -6; dy <= 6; ++dy) {
                for (int dx = -6; dx <= 6; ++dx) {
                    const aoe::TilePosition nearby{x + dx, y + dy};
                    if (!scenario->map.contains(nearby)) continue;
                    require(
                        scenario->map.terrain_at(nearby) !=
                            aoe::Terrain::water,
                        "later terrain violated spacing from existing terrain"
                    );
                }
            }
        }
    }
}

void elevation_filters_scales_and_avoids_player_origins() {
    const auto script = [](bool scale) {
        return std::string{
            "<PLAYER_SETUP>\noverride_map_size 48\n"
            "<LAND_GENERATION>\nbase_terrain GRASS\n"
            "create_player_lands {\n terrain_type GRASS\n"
            " number_of_tiles 200\n}\n"
            "create_land {\n terrain_type WATER\n number_of_tiles 500\n"
            " land_position 50 50\n}\n"
            "<ELEVATION_GENERATION>\ncreate_elevation 4 {\n"
            " base_terrain GRASS\n number_of_clumps 2\n"
            " number_of_tiles 100\n spacing 2\n"
        } + (scale ? " set_scale_by_size\n" : "") + "}\n";
    };
    const auto plain = aoe::evaluate_rms(
        aoe::parse_rms(script(false)), 902
    );
    const auto scaled = aoe::evaluate_rms(
        aoe::parse_rms(script(true)), 902
    );
    require(plain && scaled, "elevation scale fixtures refused");
    require(
        count_elevated(*scaled) > count_elevated(*plain),
        "set_scale_by_size did not increase elevation tile coverage"
    );
    for (int y = 0; y < scaled->map.height(); ++y) {
        for (int x = 0; x < scaled->map.width(); ++x) {
            const aoe::TilePosition tile{x, y};
            if (scaled->map.elevation_at(tile) == 0) continue;
            require(
                scaled->map.terrain_at(tile) == aoe::Terrain::grass,
                "base_terrain elevation filter elevated different terrain"
            );
        }
    }
    for (const aoe::TilePosition start :
         std::array<aoe::TilePosition, 2>{{
             {scaled->map.width() / 4, scaled->map.height() / 2},
             {scaled->map.width() - 1 - scaled->map.width() / 4,
              scaled->map.height() - 1 - scaled->map.height() / 2},
         }}) {
        for (int dy = -9; dy <= 9; ++dy) {
            for (int dx = -9; dx <= 9; ++dx) {
                if (std::abs(dx) + std::abs(dy) > 9) continue;
                const aoe::TilePosition tile{
                    start.x + dx, start.y + dy
                };
                if (!scaled->map.contains(tile)) continue;
                require(
                    scaled->map.elevation_at(tile) == 0,
                    "elevation entered protected player-origin area"
                );
            }
        }
    }
}

void object_blocks_own_attributes_and_execute_counts() {
    constexpr std::string_view source = R"rms(
<PLAYER_SETUP>
override_map_size 48
<LAND_GENERATION>
base_terrain GRASS
<OBJECTS_GENERATION>
create_object VILLAGER {
 number_of_objects 2
 number_of_groups 2
 set_place_for_every_player
 min_distance_to_players 3
 max_distance_to_players 8
 min_distance_group_placement 2
 set_tight_grouping
}
create_object SCOUT
{
 number_of_objects 1
 set_place_for_every_player
}
)rms";
    const aoe::RmsDocument document = aoe::parse_rms(source);
    require(document.syntactically_valid, document.error);
    require(document.playable(), "supported object AST refused");
    std::size_t villager_block{};
    std::size_t scout_block{};
    for (const aoe::RmsDirective& directive : document.directives) {
        if (directive.name == "create_object" &&
            directive.arguments.front() == "VILLAGER") {
            villager_block = directive.object_block;
        } else if (directive.name == "create_object" &&
                   directive.arguments.front() == "SCOUT") {
            scout_block = directive.object_block;
        } else if (directive.name == "number_of_groups") {
            require(
                directive.object_block == villager_block,
                "number_of_groups escaped villager block"
            );
        }
    }
    require(
        villager_block != 0 && scout_block != 0 &&
        villager_block != scout_block,
        "create_object block IDs not distinct"
    );
    const auto scenario = aoe::evaluate_rms(document, 91);
    require(scenario.has_value(), "object AST did not evaluate");
    require(
        count_units(*scenario, aoe::UnitKind::villager,
                    aoe::Player::blue) == 4 &&
        count_units(*scenario, aoe::UnitKind::villager,
                    aoe::Player::red) == 4,
        "number_of_objects/groups/every-player count mismatch"
    );
    require(
        count_units(*scenario, aoe::UnitKind::scout_cavalry,
                    aoe::Player::blue) == 1 &&
        count_units(*scenario, aoe::UnitKind::scout_cavalry,
                    aoe::Player::red) == 1,
        "second create_object inherited first block attributes"
    );
}

void object_validation_fails_closed() {
    const std::array<std::string_view, 5> invalid{{
        "<OBJECTS_GENERATION>\nnumber_of_objects 3\n",
        "<OBJECTS_GENERATION>\ncreate_object VILLAGER {\n"
        "number_of_objects nope\n}\n",
        "<OBJECTS_GENERATION>\ncreate_object VILLAGER extra {\n}\n",
        "<LAND_GENERATION>\ncreate_object VILLAGER {\n}\n",
        "<OBJECTS_GENERATION>\ncreate_object VILLAGER\n"
        "number_of_objects 3\n",
    }};
    for (const std::string_view source : invalid) {
        const aoe::RmsDocument document = aoe::parse_rms(source);
        require(
            !document.syntactically_valid && !document.playable(),
            "invalid object grammar accepted"
        );
    }
    const aoe::RmsDocument unsupported = aoe::parse_rms(
        "<OBJECTS_GENERATION>\ncreate_object WOLF {\n"
        " number_of_objects 2\n}\n"
    );
    require(
        unsupported.syntactically_valid && !unsupported.playable() &&
        !unsupported.unsupported.empty(),
        "unsupported object type silently accepted"
    );
    const aoe::RmsDocument unproved = aoe::parse_rms(
        "<OBJECTS_GENERATION>\ncreate_object VILLAGER {\n"
        " max_distance_to_other_zones 2\n}\n"
    );
    require(
        unproved.syntactically_valid && !unproved.playable(),
        "unimplemented object attribute silently accepted"
    );
}

void object_count_change_is_metamorphic() {
    const auto script = [](int count) {
        return std::string{
            "<PLAYER_SETUP>\noverride_map_size 48\n"
            "<LAND_GENERATION>\nbase_terrain GRASS\n"
            "<OBJECTS_GENERATION>\ncreate_object VILLAGER {\n"
            " set_place_for_every_player\n number_of_objects "
        } + std::to_string(count) + "\n}\n";
    };
    const auto two = aoe::evaluate_rms(aoe::parse_rms(script(2)), 314);
    const auto five = aoe::evaluate_rms(aoe::parse_rms(script(5)), 314);
    require(two && five, "metamorphic object fixtures refused");
    require(
        count_units(*two, aoe::UnitKind::villager,
                    aoe::Player::blue) == 2 &&
        count_units(*five, aoe::UnitKind::villager,
                    aoe::Player::blue) == 5,
        "number_of_objects change did not change exact multiplicity"
    );
    require(
        aoe::random_map_hash(*two) != aoe::random_map_hash(*five),
        "object-count semantic change left scenario hash unchanged"
    );
}

void object_variance_and_second_object_are_seeded() {
    constexpr std::string_view source = R"rms(
<PLAYER_SETUP>
override_map_size 48
<LAND_GENERATION>
base_terrain GRASS
<OBJECTS_GENERATION>
create_object RELIC {
 number_of_objects 5
 number_of_groups 8
 group_variance 3
 group_placement_radius 4
 set_gaia_object_only
 second_object SHEEP
}
)rms";
    const aoe::RmsDocument document = aoe::parse_rms(source);
    require(document.playable(), "variance/second-object fixture refused");
    const auto first = aoe::evaluate_rms(document, 701);
    const auto repeat = aoe::evaluate_rms(document, 701);
    const auto other = aoe::evaluate_rms(document, 702);
    require(first && repeat && other, "seeded object fixtures failed");
    require(
        aoe::random_map_hash(*first) == aoe::random_map_hash(*repeat),
        "same seed changed object variance or grouping"
    );
    require(
        aoe::random_map_hash(*first) != aoe::random_map_hash(*other),
        "different seed did not change object variance/group placement"
    );
    const std::size_t relics = count_units(
        *first, aoe::UnitKind::relic, aoe::Player::neutral
    );
    const std::size_t sheep = count_units(
        *first, aoe::UnitKind::sheep, aoe::Player::neutral
    );
    require(
        relics >= 16 && relics <= 56 && relics == sheep,
        "group variance bounds or second-object multiplicity mismatch"
    );
    for (const aoe::UnitPlacement& relic : first->units) {
        if (relic.kind != aoe::UnitKind::relic) continue;
        require(
            std::ranges::any_of(
                first->units, [&](const aoe::UnitPlacement& unit) {
                    return unit.kind == aoe::UnitKind::sheep &&
                        unit.position == relic.position;
                }
            ),
            "second object was not overlaid on primary object"
        );
    }
}

void neutral_objects_honor_player_and_group_distances() {
    constexpr std::string_view source = R"rms(
<PLAYER_SETUP>
override_map_size 48
<LAND_GENERATION>
base_terrain GRASS
<OBJECTS_GENERATION>
create_object RELIC {
 number_of_objects 1
 number_of_groups 6
 group_placement_radius 0
 min_distance_to_players 10
 max_distance_to_players 25
 min_distance_group_placement 5
 set_gaia_object_only
}
)rms";
    const auto scenario = aoe::evaluate_rms(
        aoe::parse_rms(source), 1701
    );
    require(scenario.has_value(), "neutral-distance fixture refused");
    std::vector<aoe::TilePosition> positions;
    for (const aoe::UnitPlacement& unit : scenario->units) {
        if (unit.kind == aoe::UnitKind::relic) {
            positions.push_back(unit.position);
        }
    }
    require(positions.size() == 6, "neutral group count mismatch");
    const std::array<aoe::TilePosition, 2> starts{{
        {scenario->map.width() / 4, scenario->map.height() / 2},
        {scenario->map.width() - 1 - scenario->map.width() / 4,
         scenario->map.height() - 1 - scenario->map.height() / 2},
    }};
    for (std::size_t index = 0; index < positions.size(); ++index) {
        const auto distance = [&](aoe::TilePosition target) {
            return std::abs(positions[index].x - target.x) +
                std::abs(positions[index].y - target.y);
        };
        const int nearest = std::min(
            distance(starts[0]), distance(starts[1])
        );
        require(
            nearest >= 10 && nearest <= 25,
            "neutral object violated player-distance bounds"
        );
        for (std::size_t other = index + 1;
             other < positions.size(); ++other) {
            require(
                std::abs(positions[index].x - positions[other].x) +
                    std::abs(positions[index].y - positions[other].y) >= 5,
                "neutral groups violated minimum separation"
            );
        }
    }
}

void supplied_temporary_group_distance_is_bounded_alias() {
    constexpr std::string_view temporary = R"rms(
<PLAYER_SETUP>
random_placement
override_map_size 48
<LAND_GENERATION>
base_terrain GRASS
create_player_lands
<OBJECTS_GENERATION>
create_object TOWN_CENTER {
 set_place_for_every_player
 min_distance_to_players 0
}
create_object RELIC {
 number_of_objects 5
 temp_min_distance_group_placement 20
}
)rms";
    const aoe::RmsDocument document = aoe::parse_rms(temporary);
    require(document.syntactically_valid, document.error);
    require(document.playable(), "supplied temporary distance refused");
    const auto first = aoe::evaluate_rms(
        document, 991, aoe::Civilization::britons,
        aoe::Civilization::franks
    );
    const auto second = aoe::evaluate_rms(
        document, 991, aoe::Civilization::britons,
        aoe::Civilization::franks
    );
    require(first && second, "temporary distance did not evaluate");
    require(
        first->units.size() == second->units.size() &&
        first->buildings.size() == second->buildings.size(),
        "temporary distance changed same-seed object counts"
    );
    for (std::size_t index = 0; index < first->units.size(); ++index) {
        require(
            first->units[index].kind == second->units[index].kind &&
            first->units[index].owner == second->units[index].owner &&
            first->units[index].position == second->units[index].position,
            "temporary distance changed same-seed placement"
        );
    }
}

}  // namespace

int main() {
    try {
        const auto run = [](std::string_view name, auto test) {
            try {
                test();
            } catch (const std::exception& error) {
                throw std::runtime_error(
                    std::string(name) + ": " + error.what()
                );
            }
        };
        run("common subset", common_subset_parses_and_evaluates);
        run("frontend RMS bridge",
            frontend_bridge_uses_rms_not_native_recipes);
        run("unsupported semantics",
            unsupported_semantics_preserve_span_and_refuse);
        run("malformed inputs", malformed_and_oversized_inputs_fail_closed);
        run("scenario round trip", evaluation_writes_current_scenario);
        run("section-driven generation",
            sections_drive_map_instead_of_recipe_selection);
        run("resource terrain amounts",
            rms_resource_terrain_retains_gatherable_amounts);
        run("scoped terrain and land origins",
            block_base_terrain_and_land_origin_connections_are_scoped);
        run("classic section order", generation_uses_classic_section_order);
        run("terrain spacing",
            terrain_spacing_respects_existing_generation_order);
        run("elevation filters and scaling",
            elevation_filters_scales_and_avoids_player_origins);
        run("object blocks", object_blocks_own_attributes_and_execute_counts);
        run("object validation", object_validation_fails_closed);
        run("object metamorphism", object_count_change_is_metamorphic);
        run("object variance and second object",
            object_variance_and_second_object_are_seeded);
        run("neutral object distances",
            neutral_objects_honor_player_and_group_distances);
        run("temporary group distance",
            supplied_temporary_group_distance_is_bounded_alias);
        std::cout << "All RMS import tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
