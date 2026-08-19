#include "aoe/frontend_game_modes.hpp"

#include <algorithm>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <filesystem>
#include <ranges>

#include "aoe/game_command.hpp"
#include "aoe/save_game.hpp"

int main() {
    using namespace aoe;
    const auto unit_with_id = [](const Simulation& simulation, EntityId id) {
        return std::ranges::find_if(
            simulation.units(),
            [id](const Unit& unit) { return unit.id == id; }
        );
    };

    const RandomMapSettings settings{
        RandomMapKind::arabia, RandomMapSize::tiny, 19,
        Civilization::britons, Civilization::franks,
    };
    const Scenario base = generate_random_map(settings);

    const Scenario regicide = configure_frontend_game_mode(
        base, FrontendGameMode::regicide
    );
    assert(regicide.blue_age == Age::castle);
    assert(regicide.red_age == Age::castle);
    assert(regicide.match_rules.regicide_enabled);
    assert(!regicide.match_rules.conquest_enabled);
    assert(regicide.match_rules.blue_king != 0);
    assert(regicide.match_rules.red_king != 0);
    assert(std::ranges::count_if(
        regicide.units,
        [](const UnitPlacement& unit) {
            return unit.kind == UnitKind::king;
        }
    ) == 2);
    assert(std::ranges::count_if(
        regicide.units,
        [](const UnitPlacement& unit) {
            return unit.owner == Player::blue &&
                unit.kind == UnitKind::villager;
        }
    ) == 10);
    assert(std::ranges::count_if(
        regicide.units,
        [](const UnitPlacement& unit) {
            return unit.owner == Player::red &&
                unit.kind == UnitKind::villager;
        }
    ) == 10);
    assert(std::ranges::count_if(
        regicide.buildings,
        [](const BuildingPlacement& building) {
            return building.kind == BuildingKind::castle;
        }
    ) == 2);

    Simulation regicide_game = create_simulation(regicide);
    assert(unit_with_id(
        regicide_game, regicide.match_rules.blue_king
    )->kind ==
        UnitKind::king);
    assert(unit_with_id(
        regicide_game, regicide.match_rules.red_king
    )->kind ==
        UnitKind::king);
    assert(!regicide_game.command_attack_move(
        regicide.match_rules.blue_king,
        unit_with_id(
            regicide_game, regicide.match_rules.red_king
        )->position
    ));
    assert(execute(regicide_game, DeleteEntityCommand{
        regicide.match_rules.red_king, false
    }));
    regicide_game.update();
    assert(regicide_game.outcome() == MatchOutcome::blue_victory);

    const Scenario death_match = configure_frontend_game_mode(
        base, FrontendGameMode::death_match
    );
    assert(death_match.blue_age == Age::imperial);
    assert(death_match.red_age == Age::imperial);
    assert(death_match.blue_economy.wood == 20'000);
    assert(death_match.blue_economy.food == 20'000);
    assert(death_match.blue_economy.gold == 10'000);
    assert(death_match.blue_economy.stone == 5'000);
    assert(death_match.red_economy.wood == death_match.blue_economy.wood);
    assert(death_match.red_economy.food == death_match.blue_economy.food);
    assert(death_match.red_economy.gold == death_match.blue_economy.gold);
    assert(death_match.red_economy.stone == death_match.blue_economy.stone);
    assert(!death_match.blue_technologies.empty());
    assert(!death_match.red_technologies.empty());
    assert(death_match.match_rules.conquest_enabled);

    const Scenario tutorial = make_learn_to_play_scenario(7);
    assert(tutorial.objectives.size() == 3);
    assert(tutorial.triggers.size() == 3);
    Simulation tutorial_game = create_simulation(tutorial);
    tutorial_game.update();
    assert(!tutorial_game.scenario_messages().empty());

    const ZoneServiceContract& zone = zone_service_contract();
    assert(zone.service_name == "MSN Gaming Zone");
    assert(zone.original_url == "http://www.zone.com/");
    assert(!zone.available);
    assert(!zone.status.empty());
    assert(!zone.supported_alternative.empty());

    const auto root = std::filesystem::temp_directory_path();
    const auto scenario_path = root / "aoe-regicide-mode.scenario";
    const auto save_path = root / "aoe-regicide-mode.save";
    save_scenario(regicide, scenario_path);
    const Scenario restored_scenario = load_scenario(scenario_path);
    assert(restored_scenario.match_rules.regicide_enabled);
    assert(restored_scenario.match_rules.blue_king ==
        regicide.match_rules.blue_king);
    assert(restored_scenario.match_rules.red_king ==
        regicide.match_rules.red_king);
    save_game(create_simulation(regicide), save_path);
    const Simulation restored_game = load_game(save_path);
    assert(restored_game.match_rules().regicide_enabled);
    assert(unit_with_id(
        restored_game, restored_game.match_rules().blue_king
    )->kind == UnitKind::king);
    std::filesystem::remove(scenario_path);
    std::filesystem::remove(save_path);

    Replay replay;
    replay.record(0, SetStanceCommand{
        regicide.match_rules.blue_king,
        UnitStance::passive,
    });
    Simulation replayed = create_simulation(regicide);
    replay.apply_current_tick(replayed);
    assert(replayed.match_rules().regicide_enabled);
}
