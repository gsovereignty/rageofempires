#include "aoe/content_catalog.hpp"
#include "aoe/simulation.hpp"
#include "aoe/save_game.hpp"
#include "aoe/game_command.hpp"
#include "aoe/localization.hpp"

#include <cstdlib>
#include <iostream>
#include <filesystem>
#include <set>
#include <map>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

}  // namespace

int main() {
    const aoe::ContentCatalog& catalog = aoe::commercial_content_catalog();
    require(catalog.civilization_ids().size() == 19, "all civs represented");
    require(catalog.object_record_count() == 12939, "all object records");
    require(catalog.object_variant_count() == 1414, "deduplicated variants");
    require(catalog.technologies().size() == 460, "all technologies");
    require(catalog.effects().size() == 514, "all effects");
    require(catalog.civilization_effect(1) == 254,
            "British technology-tree effect bound");
    require(catalog.civilization_bonus_effect(1) == 399,
            "British civilization bonus effect bound");

    for (auto civilization : catalog.civilization_ids()) {
        std::set<aoe::CommercialObjectId> ids;
        for (std::uint16_t id = 0; id < 1000; ++id) {
            if (const auto* record = catalog.object(civilization, id)) {
                require(record->id == id, "lookup preserves DAT object ID");
                require(ids.insert(id).second, "object ID unique per civ");
            }
        }
        require(!ids.empty(), "civilization has catalog objects");
    }
    for (std::uint16_t id = 0; id < 460; ++id) {
        require(catalog.technology(id) != nullptr, "technology lookup total");
    }
    for (std::uint16_t id = 0; id < 514; ++id) {
        require(catalog.effect(id) != nullptr, "effect lookup total");
    }
    std::map<unsigned, std::size_t> effect_types;
    bool saw_packed_combat{};
    for (const auto& effect : catalog.effects()) {
        for (const auto& command : effect.commands) {
            ++effect_types[command.type];
            if (command.packed_class && command.packed_amount) {
                saw_packed_combat = true;
            }
        }
    }
    require(
        effect_types == std::map<unsigned, std::size_t>{{0,68},{1,62},{2,88},
            {3,170},{4,631},{5,346},{6,1},{101,43},{102,1351},
            {103,11},{255,71}},
        "every DAT effect opcode classified"
    );
    require(saw_packed_combat, "packed armor and attack amounts retained");

    const auto* archer = catalog.object(1, 4);
    require(archer != nullptr, "British Archer exists");
    require(archer->hit_points == 30, "Archer HP semantic metadata");
    require(archer->attack == 4, "Archer attack semantic metadata");
    require(archer->standing_graphic == 633, "Archer render binding");
    require(archer->creation_location_object_id == 87, "Archer producer");
    require(!archer->costs.empty(), "Archer costs represented");
    require(
        archer->language_name_id == 5083 &&
        archer->language_help_id == 105083,
        "object language IDs retained"
    );
    const aoe::StringTable translated{
        "fr", {{"fallback", "fallback"}},
        {{5083, "Archer traduit"}, {105083, "Aide archer"},
         {7022, "Métier à tisser"}, {107022, "Aide métier"}}
    };
    require(
        catalog.localized_object_name(translated, 1, 4, "Archer") ==
            "Archer traduit" &&
        catalog.localized_object_help(translated, 1, 4, "") ==
            "Aide archer",
        "object name and help consume numeric DLL IDs"
    );
    require(
        catalog.localized_technology_name(translated, 22) ==
            "Métier à tisser" &&
        catalog.localized_technology_help(translated, 22) == "Aide métier",
        "technology name and help consume numeric DLL IDs"
    );

    std::size_t tasks{};
    std::map<unsigned, std::size_t> task_types;
    for (const auto& object : catalog.object_variants()) {
        tasks += object.tasks.size();
        for (const auto& task : object.tasks) {
            ++task_types[task.action_type];
            (void)aoe::commercial_task_ability(task.action_type);
        }
    }
    require(tasks > 1000, "task catalog represented semantically");
    require(tasks == 1591, "every deduplicated DAT task retained");
    require(task_types.size() == 25, "every DAT task action type classified");

    aoe::Simulation simulation{aoe::GameMap{12, 12}};
    aoe::MatchRules match_rules;
    match_rules.conquest_enabled = false;
    simulation.set_match_rules(match_rules);
    const auto villager_id = simulation.add_commercial_object(
        {1, 83}, aoe::EntityOwner{aoe::Player::blue}, {2, 2}
    );
    const auto town_center_id = simulation.add_commercial_object(
        {1, 109}, aoe::EntityOwner{aoe::Player::blue}, {5, 5}
    );
    const auto berry_id = simulation.add_commercial_object(
        {0, 59}, aoe::EntityOwner{aoe::Player::neutral}, {2, 3}
    );
    require(simulation.command_gather_unit(villager_id, berry_id),
            "commercial gather targets DAT stored resource");
    for (int tick = 0; tick < 8; ++tick) simulation.update();
    const auto gathered_berry = std::ranges::find_if(
        simulation.units(), [berry_id](const auto& unit) {
            return unit.id == berry_id;
        }
    );
    require(gathered_berry != simulation.units().end() &&
                gathered_berry->food_remaining < 125,
            "commercial gather consumes DAT stored resource");
    const auto archer_id = simulation.add_commercial_object(
        {1, 4}, aoe::EntityOwner{aoe::Player::blue}, {3, 3}
    );
    const auto enemy_id = simulation.add_unit(
        aoe::UnitKind::militia, aoe::Player::red, {9, 3}
    );
    require(
        simulation.command_commercial_task(
            archer_id, 0, enemy_id, false, {9, 3}
        ),
        "commercial combat task dispatches"
    );
    const auto commanded_archer = std::ranges::find_if(
        simulation.units(), [archer_id](const auto& unit) {
            return unit.id == archer_id;
        }
    );
    require(commanded_archer != simulation.units().end() &&
                commanded_archer->attack_target_id == enemy_id,
            "commercial combat task enters attack state");
    const auto swordsman_id = simulation.add_commercial_object(
        {1, 74}, aoe::EntityOwner{aoe::Player::blue}, {2, 8}
    );
    const auto melee_target_id = simulation.add_unit(
        aoe::UnitKind::militia, aoe::Player::red, {3, 8}
    );
    require(simulation.command_commercial_task(
                swordsman_id, 0, melee_target_id, false, {3, 8}),
            "commercial melee task dispatches");
    bool saw_commercial_projectile{};
    for (int tick = 0; tick < 20; ++tick) {
        simulation.update();
        saw_commercial_projectile = saw_commercial_projectile ||
            std::ranges::any_of(
                simulation.projectiles(), [](const auto& projectile) {
                    return projectile.commercial_projectile_identity.has_value() &&
                        projectile.precomputed_damage;
                }
            );
    }
    const auto melee_target = std::ranges::find_if(
        simulation.units(), [melee_target_id](const auto& unit) {
            return unit.id == melee_target_id;
        }
    );
    require(melee_target != simulation.units().end() &&
                melee_target->hit_points < 40,
            "commercial class attack reaches combat runtime");
    require(saw_commercial_projectile,
            "commercial missile identity reaches projectile runtime");
    const auto commercial_villager = std::ranges::find_if(
        simulation.units(), [villager_id](const auto& unit) {
            return unit.id == villager_id;
        }
    );
    require(commercial_villager != simulation.units().end(), "commercial unit made");
    require(
        commercial_villager->commercial_identity ==
            aoe::CommercialObjectIdentity{1, 83},
        "commercial unit identity retained"
    );
    require(
        simulation.buildings().back().id == town_center_id,
        "commercial building made"
    );
    auto state = simulation.player_state(*aoe::PlayerSlotId::from_index(0));
    state.commercial_technologies[104] = true;
    state.commercial_resources[96] = 0.5f;
    state.commercial_disabled_technologies[8] = true;
    state.commercial_technology_cost_overrides[22] = {1, 2, 3, 4};
    state.commercial_technology_time_overrides[22] = 7;
    simulation.replace_player_state(
        *aoe::PlayerSlotId::from_index(0), std::move(state)
    );
    require(
        simulation.research_commercial_technology_at(town_center_id, 22),
        "commercial technology queues at DAT producer"
    );
    for (int tick = 0; tick < 25; ++tick) simulation.update();
    require(
        simulation.has_commercial_technology(
            aoe::EntityOwner{aoe::Player::blue}, 22
        ),
        "commercial technology completes"
    );
    const auto loom_villager = std::ranges::find_if(
        simulation.units(), [villager_id](const auto& unit) {
            return unit.id == villager_id;
        }
    );
    require(loom_villager != simulation.units().end() &&
                loom_villager->hit_points == 40,
            "Loom effect applied");
    const auto save_path = std::filesystem::temp_directory_path() /
        "aoe-content-catalog-roundtrip.save";
    aoe::save_game(simulation, save_path);
    aoe::Simulation restored = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(
        std::ranges::find_if(restored.units(), [villager_id](const auto& unit) {
            return unit.id == villager_id && unit.commercial_identity ==
                aoe::CommercialObjectIdentity{1, 83};
        }) != restored.units().end(),
        "commercial unit identity survives save"
    );
    require(
        restored.buildings().back().commercial_identity ==
            aoe::CommercialObjectIdentity{1, 109},
        "commercial building identity survives save"
    );
    require(
        restored.has_commercial_technology(
            aoe::EntityOwner{aoe::Player::blue}, 22
        ),
        "commercial technology survives save"
    );
    const auto& restored_state = restored.player_state(
        *aoe::PlayerSlotId::from_index(0)
    );
    require(restored_state.commercial_resources[96] == 0.5f,
            "commercial resources survive save");
    require(restored_state.commercial_disabled_technologies[8],
            "commercial disabled tech survives save");
    require(restored_state.commercial_technology_cost_overrides.at(22) ==
                std::array<int, 4>{1, 2, 3, 4},
            "commercial tech costs survive save");
    require(restored_state.commercial_technology_time_overrides.at(22) == 7,
            "commercial tech time survives save");
    require(restored_state.commercial_civilization == 1,
            "commercial civilization survives save");
    require(restored_state.commercial_civilization_initialized,
            "commercial civilization bootstrap survives save");
    aoe::Replay replay;
    replay.record(3, aoe::QueueCommercialObjectCommand{
        town_center_id, {1, 4}
    });
    replay.record(4, aoe::ResearchCommercialTechnologyCommand{
        town_center_id, 22
    });
    replay.record(5, aoe::CommercialTaskCommand{
        archer_id, 0, enemy_id, false, {9, 3}
    });
    const auto replay_path = std::filesystem::temp_directory_path() /
        "aoe-content-catalog-roundtrip.replay";
    aoe::save_replay(replay, replay_path);
    const aoe::Replay restored_replay = aoe::load_replay(replay_path);
    std::filesystem::remove(replay_path);
    require(restored_replay.commands().size() == 3,
            "commercial commands persist");
    require(
        std::get<aoe::QueueCommercialObjectCommand>(
            restored_replay.commands()[0].command
        ).identity == aoe::CommercialObjectIdentity{1, 4},
        "commercial queue identity survives replay"
    );
    require(
        std::get<aoe::CommercialTaskCommand>(
            restored_replay.commands()[2].command
        ).target == enemy_id,
        "commercial task survives replay"
    );

    aoe::Simulation autonomous{aoe::GameMap{20, 12}};
    aoe::MatchRules autonomous_rules;
    autonomous_rules.conquest_enabled = false;
    autonomous.set_match_rules(autonomous_rules);
    const auto bird_id = autonomous.add_commercial_object(
        {0, 96}, aoe::EntityOwner{aoe::Player::neutral}, {3, 3}
    );
    const auto predator_id = autonomous.add_commercial_object(
        {0, 89}, aoe::EntityOwner{aoe::Player::neutral}, {8, 3}
    );
    const auto prey_id = autonomous.add_commercial_object(
        {0, 65}, aoe::EntityOwner{aoe::Player::neutral}, {9, 3}
    );
    autonomous.add_unit(aoe::UnitKind::villager, aoe::Player::blue, {14, 3});
    const auto convertible_id = autonomous.add_commercial_object(
        {0, 159}, aoe::EntityOwner{aoe::Player::neutral}, {15, 3}
    );
    for (int tick = 0; tick < 30; ++tick) autonomous.update();
    const auto autonomous_unit = [&autonomous](aoe::EntityId id) {
        return std::ranges::find(
            autonomous.units(), id, &aoe::Unit::id
        );
    };
    require(autonomous_unit(bird_id) != autonomous.units().end() &&
                autonomous_unit(bird_id)->position != aoe::TilePosition{3, 3},
            "commercial bird runs autonomous flight state");
    require(autonomous_unit(predator_id) != autonomous.units().end() &&
                autonomous_unit(prey_id) != autonomous.units().end() &&
                autonomous_unit(prey_id)->hit_points < 5,
            "commercial predator hunts configured prey");
    require(autonomous_unit(convertible_id) != autonomous.units().end() &&
                autonomous_unit(convertible_id)->owner == aoe::Player::blue,
            "commercial auto-convert acquires nearby owner");

    aoe::Simulation wonder{aoe::GameMap{20, 12}};
    aoe::MatchRules wonder_rules;
    wonder_rules.conquest_enabled = false;
    wonder_rules.wonder_enabled = true;
    wonder_rules.wonder_countdown_ticks = 2;
    wonder.set_match_rules(wonder_rules);
    wonder.add_commercial_object(
        {1, 276}, aoe::EntityOwner{aoe::Player::blue}, {3, 3}
    );
    wonder.add_unit(aoe::UnitKind::militia, aoe::Player::red, {15, 8});
    wonder.update();
    wonder.update();
    require(wonder.outcome() == aoe::MatchOutcome::blue_victory,
            "commercial wonder runs victory countdown");
    std::cout << "content catalog tests passed\n";
}
