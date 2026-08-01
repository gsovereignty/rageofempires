#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

#include "aoe/pathfinding.hpp"
#include "aoe/save_game.hpp"
#include "aoe/scenario.hpp"

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

}  // namespace

int main() {
    aoe::GameMap map(3, 1);
    map.set_elevation({0, 0}, 0);
    map.set_elevation({1, 0}, 1);
    map.set_elevation({2, 0}, 2);
    require(map.elevation_at({2, 0}) == 2, "elevation lookup");
    require(map.traversable({0, 0}, {1, 0}), "slope traversal");
    require(!map.traversable({0, 0}, {2, 0}), "cliff traversal");
    require(
        aoe::find_path(map, {0, 0}, {2, 0}, [](aoe::TilePosition) {
            return false;
        }).size() == 2,
        "graded slope path"
    );
    map.set_elevation({1, 0}, 2);
    require(
        aoe::find_path(map, {0, 0}, {2, 0}, [](aoe::TilePosition) {
            return false;
        }).empty(),
        "cliff blocks path"
    );
    map.set_elevation({1, 0}, 1);
    for (const aoe::Terrain resource : {
             aoe::Terrain::forest,
             aoe::Terrain::berry_bush,
             aoe::Terrain::gold_mine,
             aoe::Terrain::stone_mine,
         }) {
        map.set_terrain({1, 0}, resource);
        require(!map.walkable({1, 0}), "land resource blocks traversal");
        require(
            aoe::find_path(map, {0, 0}, {2, 0}, [](aoe::TilePosition) {
                return false;
            }).empty(),
            "land resource blocks path"
        );
    }
    map.set_terrain({1, 0}, aoe::Terrain::grass);
    map.set_elevation({1, 0}, 2);
    require(
        aoe::apply_elevation_damage(map, {1, 0}, {0, 0}, 20) == 25,
        "downhill damage"
    );
    require(
        aoe::apply_elevation_damage(map, {0, 0}, {1, 0}, 20) == 15,
        "uphill damage"
    );
    require(
        aoe::apply_elevation_damage(map, {1, 0}, {2, 0}, 20) == 20,
        "level damage"
    );

    const std::filesystem::path scenario_path =
        std::filesystem::temp_directory_path() /
        "aoe-elevation-roundtrip.scenario";
    aoe::Scenario scenario(3, 1);
    scenario.map.set_elevation({1, 0}, 3);
    aoe::save_scenario(scenario, scenario_path);
    const aoe::Scenario loaded_scenario =
        aoe::load_scenario(scenario_path);
    require(
        loaded_scenario.map.elevation_at({1, 0}) == 3,
        "scenario elevation round trip"
    );
    std::filesystem::remove(scenario_path);

    const std::filesystem::path save_path =
        std::filesystem::temp_directory_path() /
        "aoe-elevation-roundtrip.save";
    aoe::GameMap saved_map(2, 1);
    saved_map.set_elevation({1, 0}, 7);
    aoe::save_game(aoe::Simulation(std::move(saved_map)), save_path);
    const aoe::Simulation loaded_game = aoe::load_game(save_path);
    require(
        loaded_game.map().elevation_at({1, 0}) == 7,
        "save elevation round trip"
    );
    std::filesystem::remove(save_path);

    const std::filesystem::path legacy_scenario_path =
        std::filesystem::temp_directory_path() /
        "aoe-elevation-v62.scenario";
    {
        std::ofstream legacy(legacy_scenario_path);
        legacy << "AOE-ARCHAEOLOGY-SCENARIO 62\n"
               << "map 1 1\n";
    }
    require(
        aoe::load_scenario(legacy_scenario_path)
                .map.elevation_at({0, 0}) == 0,
        "pre-elevation scenario migrates to level zero"
    );
    std::filesystem::remove(legacy_scenario_path);

    const std::filesystem::path legacy_save_path =
        std::filesystem::temp_directory_path() /
        "aoe-elevation-v102.save";
    {
        std::ofstream legacy(legacy_save_path);
        legacy << "AOE-ARCHAEOLOGY-SAVE 102\n"
               << "map 1 1\n"
               << "tile 0 0 0 0\n";
    }
    require(
        aoe::load_game(legacy_save_path)
                .map().elevation_at({0, 0}) == 0,
        "pre-elevation save migrates to level zero"
    );
    std::filesystem::remove(legacy_save_path);
    return 0;
}
