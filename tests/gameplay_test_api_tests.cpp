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
        production_scenario.buildings.push_back({
            aoe::BuildingKind::market, aoe::Player::blue, {0, 5}
        });
        production_scenario.buildings.push_back({
            aoe::BuildingKind::market, aoe::Player::red, {15, 5}
        });
        aoe::Simulation production =
            aoe::create_simulation(production_scenario);
        const aoe::EntityId blue_town_center =
            production.buildings()[0].id;
        const aoe::EntityId blue_archery_range =
            production.buildings()[1].id;
        const aoe::EntityId blue_blacksmith =
            production.buildings()[2].id;
        const aoe::EntityId red_town_center =
            production.buildings()[4].id;
        const aoe::EntityId blue_market = production.buildings()[5].id;
        const aoe::EntityId red_market = production.buildings()[6].id;

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

        const aoe::Economy before_buy =
            production.economy(aoe::Player::blue);
        const int food_buy_price = production.market_buy_price(
            aoe::Player::blue, aoe::MarketResource::food
        );
        const std::string bought = aoe::GameplayTestApi::execute(
            production,
            aoe::Player::blue,
            "market_buy " + std::to_string(blue_market) + " food"
        );
        require(bought.find("\"ok\":true") != std::string::npos,
                "market_buy must use an owned completed market");
        require(production.economy(aoe::Player::blue).food ==
                    before_buy.food + 100 &&
                production.economy(aoe::Player::blue).gold ==
                    before_buy.gold - food_buy_price,
                "market_buy must preserve normal quantity and price");

        const aoe::Economy before_sell =
            production.economy(aoe::Player::blue);
        const int stone_sell_price = production.market_sell_price(
            aoe::Player::blue, aoe::MarketResource::stone
        );
        const std::string sold = aoe::GameplayTestApi::execute(
            production,
            aoe::Player::blue,
            "market_sell " + std::to_string(blue_market) + " stone"
        );
        require(sold.find("\"ok\":true") != std::string::npos,
                "market_sell must use an owned completed market");
        require(production.economy(aoe::Player::blue).stone ==
                    before_sell.stone - 100 &&
                production.economy(aoe::Player::blue).gold ==
                    before_sell.gold + stone_sell_price,
                "market_sell must preserve normal quantity and price");
        require(aoe::GameplayTestApi::execute(
                    production,
                    aoe::Player::blue,
                    "market_buy " + std::to_string(blue_town_center) +
                        " food"
                ).find("market cannot be used") != std::string::npos,
                "market exchange must reject non-market buildings");
        require(aoe::GameplayTestApi::execute(
                    production,
                    aoe::Player::blue,
                    "market_sell " + std::to_string(red_market) + " wood"
                ).find("market cannot be used") != std::string::npos,
                "market exchange must reject enemy markets");
        require(aoe::GameplayTestApi::execute(
                    production,
                    aoe::Player::blue,
                    "market_buy " + std::to_string(blue_market) + " gold"
                ).find("unknown market resource") != std::string::npos,
                "market exchange must reject unsupported resources");

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

        const std::string researched = aoe::GameplayTestApi::execute(
            production,
            aoe::Player::blue,
            "research " + std::to_string(blue_blacksmith) + " fletching"
        );
        require(researched.find("\"ok\":true") != std::string::npos,
                "research must start through normal technology rules");
        require(production.buildings()[2].technology_research_target ==
                    aoe::Technology::fletching,
                "research must preserve requested technology");
        require(production.buildings()[2].
                    technology_research_ticks_remaining > 0,
                "research must preserve normal technology timing");
        const std::string researching_building =
            aoe::GameplayTestApi::execute(
                production, aoe::Player::blue, "list_buildings"
            );
        require(researching_building.find(
                    "\"technology_research_target\":\"fletching\""
                ) != std::string::npos,
                "list_buildings must expose technology research target");
        require(aoe::GameplayTestApi::execute(
                    production,
                    aoe::Player::blue,
                    "research " + std::to_string(red_town_center) +
                        " fletching"
                ).find("\"ok\":false") != std::string::npos,
                "research must reject enemy building ownership");
    }

    {
        aoe::Scenario construction_scenario(16, 12);
        construction_scenario.blue_economy = {1000, 1000, 1000, 1000};
        construction_scenario.red_economy = {1000, 1000, 1000, 1000};
        construction_scenario.units.push_back({
            aoe::UnitKind::villager, aoe::Player::blue, {5, 5}
        });
        construction_scenario.units.push_back({
            aoe::UnitKind::villager, aoe::Player::red, {14, 10}
        });
        aoe::Simulation construction =
            aoe::create_simulation(construction_scenario);
        const aoe::EntityId builder = construction.units()[0].id;
        const aoe::EntityId enemy_builder = construction.units()[1].id;
        const aoe::Economy before = construction.economy(aoe::Player::blue);

        const std::string constructed = aoe::GameplayTestApi::execute(
            construction,
            aoe::Player::blue,
            "construct " + std::to_string(builder) + " house 5 6"
        );
        require(constructed.find("\"ok\":true") != std::string::npos,
                "construct must accept valid normal building command");
        require(construction.buildings().size() == 1 &&
                    construction.buildings().front().kind ==
                        aoe::BuildingKind::house,
                "construct must create requested building foundation");
        require(construction.economy(aoe::Player::blue).wood < before.wood,
                "construct must charge normal building cost");
        require(aoe::GameplayTestApi::execute(
                    construction,
                    aoe::Player::blue,
                    "construct " + std::to_string(builder) + " house 5 6"
                ).find("\"ok\":false") != std::string::npos,
                "construct must reject occupied footprint");
        require(aoe::GameplayTestApi::execute(
                    construction,
                    aoe::Player::blue,
                    "construct " + std::to_string(enemy_builder) +
                        " house 13 10"
                ).find("\"ok\":false") != std::string::npos,
                "construct must reject enemy builder ownership");
        require(aoe::GameplayTestApi::execute(
                    construction,
                    aoe::Player::blue,
                    "construct " + std::to_string(builder) +
                        " castle 3 5"
                ).find("\"ok\":false") != std::string::npos,
                "construct must preserve age prerequisites");
        require(aoe::GameplayTestApi::execute(
                    construction,
                    aoe::Player::blue,
                    "construct " + std::to_string(builder) +
                        " unknown_building 3 5"
                ).find("unknown building kind") != std::string::npos,
                "construct must reject unknown building kind");
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

    {
        aoe::Simulation attack_move_simulation(aoe::GameMap(30, 10));
        const aoe::EntityId archer = attack_move_simulation.add_unit(
            aoe::UnitKind::archer,
            aoe::Player::blue,
            {1, 2}
        );
        const aoe::EntityId enemy = attack_move_simulation.add_unit(
            aoe::UnitKind::villager,
            aoe::Player::red,
            {6, 2}
        );

        const std::string ordered = aoe::GameplayTestApi::execute(
            attack_move_simulation,
            aoe::Player::blue,
            "attack_move " + std::to_string(archer) + " 14 2"
        );
        require(ordered.find("\"ok\":true") != std::string::npos,
                "attack_move must accept owned combat unit");
        require(attack_move_simulation.units().front().attack_moving,
                "attack_move must use normal attack-move state");
        require(attack_move_simulation.units().front().
                    attack_move_destination == aoe::TilePosition(14, 2),
                "attack_move must retain requested destination");
        attack_move_simulation.update();
        require(attack_move_simulation.units().front().attack_target_id ==
                    enemy,
                "attack_move must acquire visible hostile unit");
        attack_move_simulation.update();
        require(attack_move_simulation.units().front().attack_target_id ==
                    enemy &&
                attack_move_simulation.units().front().attack_moving,
                "attack_move must continue hostile target engagement");
        require(aoe::GameplayTestApi::execute(
                    attack_move_simulation,
                    aoe::Player::blue,
                    "attack_move " + std::to_string(enemy) + " 1 2"
                ).find("\"ok\":false") != std::string::npos,
                "attack_move must reject enemy unit ownership");
    }

    {
        aoe::Simulation fast_simulation(aoe::GameMap(30, 12));
        const aoe::EntityId first = fast_simulation.add_unit(
            aoe::UnitKind::archer, aoe::Player::blue, {1, 2}
        );
        const aoe::EntityId second = fast_simulation.add_unit(
            aoe::UnitKind::archer, aoe::Player::blue, {1, 4}
        );
        fast_simulation.add_unit(
            aoe::UnitKind::villager, aoe::Player::red, {28, 10}
        );
        fast_simulation.add_building(
            aoe::BuildingKind::house, aoe::Player::blue, {3, 7}
        );

        const std::string observed = aoe::GameplayTestApi::execute(
            fast_simulation, aoe::Player::blue, "observe"
        );
        require(observed.find("\"units\":[") != std::string::npos &&
                    observed.find("\"buildings\":[") != std::string::npos,
                "observe must combine state, units, and buildings");

        const std::string quiet = aoe::GameplayTestApi::execute(
            fast_simulation,
            aoe::Player::blue,
            "quiet move " + std::to_string(first) + " 8 2"
        );
        require(quiet == "{\"ok\":true}",
                "quiet command must omit repeated state serialization");

        const std::string grouped = aoe::GameplayTestApi::execute(
            fast_simulation,
            aoe::Player::blue,
            "attack_move_group 20 6 " + std::to_string(first) + " " +
                std::to_string(second)
        );
        require(grouped.find("\"ok\":true") != std::string::npos &&
                    fast_simulation.units()[0].attack_moving &&
                    fast_simulation.units()[1].attack_moving,
                "group attack move must order every owned unit");

        const std::string batched = aoe::GameplayTestApi::execute(
            fast_simulation,
            aoe::Player::blue,
            "batch move_group 10 2 " + std::to_string(first) + " " +
                std::to_string(second) + "; advance 2"
        );
        require(batched.find("\"completed_commands\":2") !=
                    std::string::npos,
                "batch must execute several commands with one final state");
    }

    {
        aoe::Scenario queued_scenario(20, 12);
        queued_scenario.blue_economy = {1000, 1000, 1000, 1000};
        queued_scenario.units.push_back({
            aoe::UnitKind::villager, aoe::Player::blue, {7, 7}
        });
        queued_scenario.units.push_back({
            aoe::UnitKind::villager, aoe::Player::red, {18, 10}
        });
        queued_scenario.buildings.push_back({
            aoe::BuildingKind::town_center, aoe::Player::blue, {0, 0}
        });
        queued_scenario.buildings.push_back({
            aoe::BuildingKind::house, aoe::Player::blue, {5, 0}
        });
        aoe::Simulation queued = aoe::create_simulation(queued_scenario);
        const aoe::EntityId town_center = queued.buildings().front().id;
        require(aoe::GameplayTestApi::execute(
                    queued,
                    aoe::Player::blue,
                    "train " + std::to_string(town_center) + " villager"
                ).find("\"ok\":true") != std::string::npos,
                "advance-until fixture must queue production");
        const std::string advanced_until = aoe::GameplayTestApi::execute(
            queued,
            aoe::Player::blue,
            "advance_until_idle " + std::to_string(town_center) + " 1000"
        );
        require(advanced_until.find("\"elapsed_ticks\":") !=
                    std::string::npos &&
                    queued.buildings().front().production_queue.empty(),
                "advance_until_idle must stop when production completes");
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
    api.poll(simulation, aoe::Player::blue, true);
    std::ifstream responses(directory / "responses.jsonl");
    const std::string response{
        std::istreambuf_iterator<char>{responses},
        std::istreambuf_iterator<char>{}
    };
    require(response.find("\"id\":\"request-1\"") !=
                std::string::npos,
            "file boundary must correlate response id");
    const auto gated_tick = simulation.tick_number();
    {
        std::ofstream commands(
            directory / "commands.jsonl", std::ios::app
        );
        commands << "request-2\tadvance 5\n";
    }
    api.poll(simulation, aoe::Player::blue, false);
    std::ifstream gated_responses(directory / "responses.jsonl");
    const std::string gated_response{
        std::istreambuf_iterator<char>{gated_responses},
        std::istreambuf_iterator<char>{}
    };
    require(gated_response.find("\"id\":\"request-2\"") !=
                std::string::npos &&
            gated_response.find("no visible active match") !=
                std::string::npos,
            "file boundary must reject commands without visible match");
    require(simulation.tick_number() == gated_tick,
            "rejected pre-match command must not mutate simulation");
    std::filesystem::remove_all(directory);
    return 0;
}
