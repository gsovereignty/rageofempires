#include "aoe/gameplay_test_api.hpp"
#include "aoe/scenario.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

}  // namespace

int main() {
    aoe::Simulation simulation = aoe::Simulation::create_demo();
    const auto villager = simulation.units().front().id;
    const auto initial_tick = simulation.tick_number();

    const std::string listed = aoe::GameplayTestApi::execute(
        simulation, aoe::Player::blue, "list_units"
    );
    require(listed.find("\"units\":[") != std::string::npos,
            "list_units must include semantic unit state");
    require(listed.find("\"resources\":") != std::string::npos,
            "state must include economy resources");

    const std::string selected = aoe::GameplayTestApi::execute(
        simulation,
        aoe::Player::blue,
        "select " + std::to_string(villager)
    );
    require(selected.find("\"ok\":true") != std::string::npos,
            "owned unit should be selectable by id");
    require(simulation.selected_units() ==
                std::vector<aoe::EntityId>{villager},
            "selection should update live simulation");

    const std::string advanced = aoe::GameplayTestApi::execute(
        simulation, aoe::Player::blue, "advance 5"
    );
    require(advanced.find("\"ok\":true") != std::string::npos,
            "advance should return state");
    require(simulation.tick_number() == initial_tick + 5,
            "advance should run requested deterministic ticks");
    require(aoe::GameplayTestApi::execute(
                simulation, aoe::Player::blue, "advance 10001"
            ).find("\"ok\":false") != std::string::npos,
            "advance must enforce bounded work");

    {
        aoe::Scenario production_scenario(20, 12);
        production_scenario.blue_economy = {1000, 1000, 1000, 1000};
        production_scenario.red_economy = {1000, 1000, 1000, 1000};
        production_scenario.blue_age = aoe::Age::feudal;
        production_scenario.red_age = aoe::Age::feudal;
        production_scenario.units.push_back({
            aoe::UnitKind::villager, aoe::Player::blue, {8, 8}
        });
        production_scenario.units.push_back({
            aoe::UnitKind::villager, aoe::Player::red, {18, 10}
        });
        production_scenario.buildings.push_back({
            aoe::BuildingKind::town_center, aoe::Player::blue, {0, 0}
        });
        production_scenario.buildings.push_back({
            aoe::BuildingKind::archery_range, aoe::Player::blue, {5, 0}
        });
        production_scenario.buildings.push_back({
            aoe::BuildingKind::blacksmith, aoe::Player::blue, {10, 0}
        });
        production_scenario.buildings.push_back({
            aoe::BuildingKind::house, aoe::Player::blue, {10, 5}
        });
        production_scenario.buildings.push_back({
            aoe::BuildingKind::town_center, aoe::Player::red, {15, 0}
        });
        aoe::Simulation production =
            aoe::create_simulation(production_scenario);
        const aoe::EntityId blue_town_center =
            production.buildings()[0].id;
        const aoe::EntityId blue_archery_range =
            production.buildings()[1].id;
        const aoe::EntityId red_town_center =
            production.buildings()[4].id;

        const std::string buildings = aoe::GameplayTestApi::execute(
            production, aoe::Player::blue, "list_buildings"
        );
        require(buildings.find("\"kind\":\"town_center\"") !=
                    std::string::npos,
                "list_buildings must expose semantic building kinds");
        require(buildings.find("\"production_queue_size\":0") !=
                    std::string::npos,
                "list_buildings must expose production state");

        const std::string selected_by_id = aoe::GameplayTestApi::execute(
            production,
            aoe::Player::blue,
            "select_building " + std::to_string(blue_archery_range)
        );
        require(selected_by_id.find("\"selected_building\":" +
                    std::to_string(blue_archery_range)) != std::string::npos,
                "owned building should be selectable by id");

        const std::string selected_by_tile = aoe::GameplayTestApi::execute(
            production, aoe::Player::blue, "select_building_at 0 0"
        );
        require(selected_by_tile.find("\"selected_building\":" +
                    std::to_string(blue_town_center)) != std::string::npos,
                "owned building should be selectable by tile");

        const std::string selected_by_kind = aoe::GameplayTestApi::execute(
            production,
            aoe::Player::blue,
            "select_building_kind archery_range"
        );
        require(selected_by_kind.find("\"selected_building\":" +
                    std::to_string(blue_archery_range)) != std::string::npos,
                "owned building should be selectable by kind");

        const std::string trained = aoe::GameplayTestApi::execute(
            production,
            aoe::Player::blue,
            "train " + std::to_string(blue_archery_range) + " archer"
        );
        require(trained.find("\"ok\":true") != std::string::npos,
                "train must queue through normal production rules");
        require(production.buildings()[1].production_queue.size() == 1,
                "train must update live building production queue");

        require(aoe::GameplayTestApi::execute(
                    production,
                    aoe::Player::blue,
                    "select_building " + std::to_string(red_town_center)
                ).find("\"ok\":false") != std::string::npos,
                "building selection must reject enemy ownership");
        require(aoe::GameplayTestApi::execute(
                    production,
                    aoe::Player::blue,
                    "train " + std::to_string(red_town_center) + " villager"
                ).find("\"ok\":false") != std::string::npos,
                "training must reject enemy ownership");

        const std::string advanced_age = aoe::GameplayTestApi::execute(
            production,
            aoe::Player::blue,
            "advance_age " + std::to_string(blue_town_center)
        );
        require(advanced_age.find("\"ok\":true") != std::string::npos,
                "advance_age must start normal town-center research");
        require(production.buildings()[0].age_research_target ==
                    aoe::Age::castle,
                "advance_age must target next age");
        require(production.buildings()[0].age_research_ticks_remaining > 0,
                "advance_age must preserve normal research timing");
    }

    {
        aoe::GameMap map(8, 5);
        map.set_terrain({2, 1}, aoe::Terrain::gold_mine);
        map.set_resource_amount({2, 1}, 100);
        aoe::Simulation context_simulation(std::move(map));
        const aoe::EntityId worker = context_simulation.add_unit(
            aoe::UnitKind::villager,
            aoe::Player::blue,
            {1, 1}
        );
        context_simulation.add_building(
            aoe::BuildingKind::town_center,
            aoe::Player::blue,
            {4, 0}
        );
        context_simulation.add_unit(
            aoe::UnitKind::villager,
            aoe::Player::red,
            {7, 4}
        );

        const std::string moved = aoe::GameplayTestApi::execute(
            context_simulation,
            aoe::Player::blue,
            "move " + std::to_string(worker) + " 2 1"
        );
        require(moved.find("\"ok\":true") != std::string::npos,
                "semantic move should accept resource context command");
        require(context_simulation.units().front().has_resource_target,
                "semantic move to gold must enter gathering context");
        require(context_simulation.units().front().resource_target ==
                    aoe::TilePosition(2, 1),
                "semantic move must retain clicked resource tile");
        context_simulation.update();
        require(context_simulation.units().front().carried_resource ==
                    aoe::ResourceKind::gold,
                "semantic resource context must run normal gather behavior");
    }

    {
        aoe::Simulation context_simulation(aoe::GameMap(6, 4));
        const aoe::EntityId scout = context_simulation.add_unit(
            aoe::UnitKind::scout_cavalry,
            aoe::Player::blue,
            {1, 1}
        );
        const aoe::EntityId enemy = context_simulation.add_unit(
            aoe::UnitKind::villager,
            aoe::Player::red,
            {2, 1}
        );

        const std::string moved = aoe::GameplayTestApi::execute(
            context_simulation,
            aoe::Player::blue,
            "move " + std::to_string(scout) + " 2 1"
        );
        require(moved.find("\"ok\":true") != std::string::npos,
                "semantic move should accept enemy context command");
        require(context_simulation.units().front().attack_target_id == enemy,
                "semantic move to enemy must enter attack context");
    }

    const std::filesystem::path directory =
        std::filesystem::temp_directory_path() /
        "aoe-gameplay-test-api-tests";
    std::filesystem::remove_all(directory);
    aoe::GameplayTestApi api(directory);
    {
        std::ofstream commands(
            directory / "commands.jsonl", std::ios::app
        );
        commands << "request-1\tstate\n";
    }
    api.poll(simulation, aoe::Player::blue);
    std::ifstream responses(directory / "responses.jsonl");
    const std::string response{
        std::istreambuf_iterator<char>{responses},
        std::istreambuf_iterator<char>{}
    };
    require(response.find("\"id\":\"request-1\"") !=
                std::string::npos,
            "file boundary must correlate response id");
    std::filesystem::remove_all(directory);
    return 0;
}
