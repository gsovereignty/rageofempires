#include "aoe/content_catalog.hpp"
#include "aoe/simulation.hpp"
#include "aoe/save_game.hpp"
#include "aoe/game_command.hpp"

#include <cstdlib>
#include <iostream>
#include <filesystem>
#include <set>

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

    const auto* archer = catalog.object(1, 4);
    require(archer != nullptr, "British Archer exists");
    require(archer->hit_points == 30, "Archer HP semantic metadata");
    require(archer->attack == 4, "Archer attack semantic metadata");
    require(archer->standing_graphic == 633, "Archer render binding");
    require(archer->creation_location_object_id == 87, "Archer producer");
    require(!archer->costs.empty(), "Archer costs represented");

    std::size_t tasks{};
    for (auto civilization : catalog.civilization_ids()) {
        for (std::uint16_t id = 0; id < 1000; ++id) {
            if (const auto* object = catalog.object(civilization, id)) {
                tasks += object->tasks.size();
            }
        }
    }
    require(tasks > 1000, "task catalog represented semantically");

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
    require(simulation.units().back().id == villager_id, "commercial unit made");
    require(
        simulation.units().back().commercial_identity ==
            aoe::CommercialObjectIdentity{1, 83},
        "commercial unit identity retained"
    );
    require(
        simulation.buildings().back().id == town_center_id,
        "commercial building made"
    );
    auto state = simulation.player_state(*aoe::PlayerSlotId::from_index(0));
    state.commercial_technologies[104] = true;
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
    require(simulation.units().back().hit_points == 40, "Loom effect applied");
    const auto save_path = std::filesystem::temp_directory_path() /
        "aoe-content-catalog-roundtrip.save";
    aoe::save_game(simulation, save_path);
    aoe::Simulation restored = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(
        restored.units().back().commercial_identity ==
            aoe::CommercialObjectIdentity{1, 83},
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
    aoe::Replay replay;
    replay.record(3, aoe::QueueCommercialObjectCommand{
        town_center_id, {1, 4}
    });
    replay.record(4, aoe::ResearchCommercialTechnologyCommand{
        town_center_id, 22
    });
    const auto replay_path = std::filesystem::temp_directory_path() /
        "aoe-content-catalog-roundtrip.replay";
    aoe::save_replay(replay, replay_path);
    const aoe::Replay restored_replay = aoe::load_replay(replay_path);
    std::filesystem::remove(replay_path);
    require(restored_replay.commands().size() == 2, "commercial commands persist");
    require(
        std::get<aoe::QueueCommercialObjectCommand>(
            restored_replay.commands()[0].command
        ).identity == aoe::CommercialObjectIdentity{1, 4},
        "commercial queue identity survives replay"
    );
    std::cout << "content catalog tests passed\n";
}
