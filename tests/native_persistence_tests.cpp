#include "aoe/format_versions.hpp"
#include "aoe/game_command.hpp"
#include "aoe/save_game.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

void require(bool condition) {
    if (!condition) throw std::runtime_error("native persistence test failed");
}

void write(const std::filesystem::path& path, const std::string& bytes) {
    std::ofstream output(path);
    output << bytes;
}

bool save_rejected(
    const std::filesystem::path& path,
    const std::string& bytes
) {
    write(path, bytes);
    try {
        (void)aoe::load_game(path);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

bool replay_rejected(
    const std::filesystem::path& path,
    const std::string& bytes
) {
    write(path, bytes);
    try {
        (void)aoe::load_replay(path);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

}  // namespace

int main() {
    const auto slot = [](std::size_t index) {
        return *aoe::PlayerSlotId::from_index(index);
    };
    const auto roster = aoe::MatchRoster::create({
        {
            slot(0), true, *aoe::TeamId::numbered(1), false,
            {{"host", aoe::RosterControllerKind::human}},
        },
        {
            slot(1), true, *aoe::TeamId::numbered(2), false,
            {{"red-ai", aoe::RosterControllerKind::computer}},
        },
        {
            slot(2), true, *aoe::TeamId::numbered(1), false,
            {{"green-ai", aoe::RosterControllerKind::computer}},
        },
    });
    require(roster.has_value());
    auto diplomacy = aoe::RosterDiplomacy::create(*roster);
    require(diplomacy.has_value());
    require(diplomacy->set_stance(
        slot(2), slot(1), aoe::Diplomacy::neutral
    ));

    aoe::Simulation simulation(aoe::GameMap(10, 8));
    simulation.replace_roster(*roster, *diplomacy);
    auto green = simulation.player_state(slot(2));
    green.economy = {401, 502, 603, 704};
    green.age = aoe::Age::castle;
    green.civilization = aoe::Civilization::mongols;
    green.formation = aoe::FormationKind::flank;
    green.farm_reseed_queue = 3;
    green.mayan_resource_remainder = 14;
    green.aztec_relic_gold_remainder = 33;
    green.victory_countdown = 77;
    green.countdown_kind = aoe::VictoryCountdownKind::wonder;
    green.countdown_last_tick = 9;
    green.technologies[static_cast<std::size_t>(
        aoe::Technology::husbandry
    )] = true;
    green.explored[23] = true;
    simulation.replace_player_state(slot(2), green);
    const aoe::EntityId green_unit =
        simulation.add_unit(aoe::UnitKind::militia, slot(2), {5, 4});
    const aoe::EntityId green_building = simulation.add_building(
        aoe::BuildingKind::house, slot(2), {7, 4}
    );
    auto units = simulation.units();
    units.front().facing = 7;
    auto buildings = simulation.buildings();
    buildings.front().facing = 3;
    simulation.replace_state(
        std::move(units), std::move(buildings),
        simulation.economy(slot(0)), simulation.economy(slot(1)),
        simulation.tick_number()
    );
    aoe::Projectile projectile;
    projectile.owner = *aoe::EntityOwner::from_stable_id(2);
    projectile.origin = {5, 4};
    projectile.destination = {6, 4};
    projectile.ticks_remaining = 2;
    projectile.total_ticks = 2;
    projectile.source_kind = aoe::UnitKind::militia;
    projectile.source_entity_id = green_unit;
    projectile.effect_id = 101;
    simulation.replace_projectiles({projectile});
    aoe::ImpactEffect impact;
    impact.position = {6, 4};
    impact.ticks_remaining = 2;
    impact.total_ticks = 2;
    impact.source_kind = aoe::UnitKind::militia;
    impact.source_entity_id = green_unit;
    impact.effect_id = 102;
    impact.commercial_projectile_identity =
        aoe::CommercialObjectIdentity{7, 42};
    simulation.replace_impact_effects({impact});
    simulation.replace_death_effects({
        {
            {5, 4}, aoe::UnitKind::militia,
            *aoe::EntityOwner::from_stable_id(2), 4, 4,
            green_unit, {4, 4}, 6, 103,
        },
    });
    simulation.replace_rubble_effects({
        {
            {7, 4}, aoe::BuildingKind::house,
            *aoe::EntityOwner::from_stable_id(2), 4, 4,
            green_building, 104,
        },
    });
    aoe::MatchStatistics statistics = simulation.match_statistics();
    statistics.players[2].units_created = 12;
    aoe::StatisticsTimelineSample sample;
    sample.tick = 4;
    sample.score[2] = 222;
    sample.gathered[2].gold = 603;
    statistics.timeline.push_back(sample);
    simulation.replace_match_statistics(statistics);

    const auto directory = std::filesystem::temp_directory_path();
    const auto save_path = directory / "aoe-native-v109.save";
    aoe::save_game(simulation, save_path);
    std::ifstream saved_input(save_path);
    const std::string saved{
        std::istreambuf_iterator<char>{saved_input},
        std::istreambuf_iterator<char>{},
    };
    require(saved.starts_with(
        "AOE-ARCHAEOLOGY-SAVE " +
        std::to_string(aoe::reconstruction_save_version) + "\n"
    ));
    const aoe::Simulation restored = aoe::load_game(save_path);
    require(restored.roster().slot(slot(2)).controllers.front().id ==
        "green-ai");
    require(restored.roster().slot(slot(2)).team.number() == 1);
    require(restored.economy(slot(2)).gold == 603);
    require(restored.age(slot(2)) == aoe::Age::castle);
    require(restored.civilization(slot(2)) ==
        aoe::Civilization::mongols);
    require(restored.player_state(slot(2)).technologies[
        static_cast<std::size_t>(aoe::Technology::husbandry)
    ]);
    require(restored.player_state(slot(2)).explored[23]);
    require(restored.victory_countdown(slot(2)) == 77);
    require(restored.diplomacy(slot(2), slot(1)) ==
        aoe::Diplomacy::neutral);
    require(restored.units().front().owner.stable_id() == 2);
    require(restored.units().front().facing == 7);
    require(restored.buildings().front().owner.stable_id() == 2);
    require(restored.buildings().front().facing == 3);
    require(
        restored.projectiles().front().source_entity_id == green_unit
    );
    require(restored.projectiles().front().effect_id == 101);
    require(
        restored.impact_effects().front().source_entity_id == green_unit
    );
    require(restored.impact_effects().front().effect_id == 102);
    require(
        restored.impact_effects().front().commercial_projectile_identity ==
        aoe::CommercialObjectIdentity{7, 42}
    );
    require(restored.death_effects().front().entity_id == green_unit);
    require(restored.death_effects().front().effect_id == 103);
    require(restored.death_effects().front().facing == 6);
    require(
        restored.death_effects().front().previous_position ==
        aoe::TilePosition{4, 4}
    );
    require(
        restored.rubble_effects().front().entity_id == green_building
    );
    require(restored.rubble_effects().front().effect_id == 104);
    require(restored.next_transient_effect_id() == 105);
    require(restored.player_statistics(slot(2)).units_created == 12);
    require(restored.match_statistics().timeline.front().score[2] == 222);

    std::istringstream current_lines(saved);
    std::ostringstream legacy_v130;
    std::string line;
    while (std::getline(current_lines, line)) {
        if (line.starts_with("AOE-ARCHAEOLOGY-SAVE ")) {
            legacy_v130 << "AOE-ARCHAEOLOGY-SAVE 130\n";
            continue;
        }
        if (line.starts_with("transient-effect-sequence ")) continue;
        int fields_to_remove = line.starts_with("impact ") ? 3
            : line.starts_with("projectile ") ||
              line.starts_with("death ") || line.starts_with("rubble ")
            ? 1 : 0;
        while (fields_to_remove-- > 0) {
            line.erase(line.find_last_of(' '));
        }
        legacy_v130 << line << '\n';
    }
    write(save_path, legacy_v130.str());
    const aoe::Simulation restored_v130 = aoe::load_game(save_path);
    require(restored_v130.projectiles().front().effect_id == 1);
    require(restored_v130.impact_effects().front().effect_id == 2);
    require(restored_v130.death_effects().front().effect_id == 3);
    require(restored_v130.rubble_effects().front().effect_id == 4);
    require(restored_v130.next_transient_effect_id() == 5);

    std::string duplicate_effect_id = saved;
    const auto impact_line = duplicate_effect_id.find("impact ");
    const auto impact_end = duplicate_effect_id.find('\n', impact_line);
    const auto impact_id = duplicate_effect_id.rfind(' ', impact_end);
    duplicate_effect_id.replace(
        impact_id + 1, impact_end - impact_id - 1, "101"
    );
    require(save_rejected(save_path, duplicate_effect_id));

    const auto diplomacy_line = saved.find("roster-diplomacy 2 1 ");
    require(diplomacy_line != std::string::npos);
    const auto diplomacy_end = saved.find('\n', diplomacy_line) + 1;
    require(save_rejected(
        save_path,
        saved.substr(0, diplomacy_line) + saved.substr(diplomacy_end)
    ));
    const auto slot_line = saved.find("roster-slot 0 ");
    const auto slot_end = saved.find('\n', slot_line) + 1;
    require(save_rejected(
        save_path,
        saved.substr(0, slot_end) +
            saved.substr(slot_line, slot_end - slot_line) +
            saved.substr(slot_end)
    ));
    std::string bad_owner = saved;
    const auto owner = bad_owner.find("unit 1 4 2 ");
    require(owner != std::string::npos);
    bad_owner.replace(owner, std::string("unit 1 4 2 ").size(),
                      "unit 1 4 7 ");
    require(save_rejected(save_path, bad_owner));

    const std::string legacy108 =
        "AOE-ARCHAEOLOGY-SAVE 108\n"
        "map 4 3\n"
        "blue 321 654 7 8\n"
        "red 123 456 9 10\n";
    write(save_path, legacy108);
    const aoe::Simulation migrated = aoe::load_game(save_path);
    require(migrated.roster().slot(slot(0)).occupied);
    require(migrated.roster().slot(slot(1)).occupied);
    require(!migrated.roster().slot(slot(2)).occupied);
    require(migrated.economy(slot(0)).wood == 321);
    require(migrated.economy(slot(1)).food == 456);

    aoe::Replay replay;
    replay.record(3, slot(2), aoe::StopUnitCommand{1});
    const auto replay_path = directory / "aoe-native-v63.replay";
    aoe::save_replay(replay, replay_path);
    const aoe::Replay restored_replay = aoe::load_replay(replay_path);
    require(restored_replay.commands().front().source == slot(2));
    require(replay_rejected(
        replay_path,
        "AOE-ARCHAEOLOGY-REPLAY 64\nstop 3 1\n"
    ));
    require(replay_rejected(
        replay_path,
        "AOE-ARCHAEOLOGY-REPLAY 64\nsource 9\nstop 3 1\n"
    ));
    require(replay_rejected(
        replay_path,
        "AOE-ARCHAEOLOGY-REPLAY 64\n"
        "source 2\nsource 2\nstop 3 1\n"
    ));
    write(
        replay_path,
        "AOE-ARCHAEOLOGY-REPLAY 62\n"
        "resign 3 1\n"
        "stop 4 99\n"
    );
    const aoe::Replay legacy_replay = aoe::load_replay(replay_path);
    require(legacy_replay.commands()[0].source == slot(1));
    require(!legacy_replay.commands()[1].source);

    std::filesystem::remove(save_path);
    std::filesystem::remove(replay_path);
    std::cout << "native persistence tests passed\n";
}
