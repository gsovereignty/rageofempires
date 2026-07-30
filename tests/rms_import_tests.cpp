#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

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
        first->map.width() == 48 &&
        first->blue_civilization == aoe::Civilization::chinese &&
        first->red_civilization == aoe::Civilization::mayans,
        "RMS setup did not reach generated scenario"
    );
    (void)aoe::create_simulation(*first);
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
        " group_variance 2\n}\n"
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
        run("unsupported semantics",
            unsupported_semantics_preserve_span_and_refuse);
        run("malformed inputs", malformed_and_oversized_inputs_fail_closed);
        run("scenario round trip", evaluation_writes_current_scenario);
        run("object blocks", object_blocks_own_attributes_and_execute_counts);
        run("object validation", object_validation_fails_closed);
        run("object metamorphism", object_count_change_is_metamorphic);
        run("temporary group distance",
            supplied_temporary_group_distance_is_bounded_alias);
        std::cout << "All RMS import tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
