#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

#include "aoe/ordinary_match_setup.hpp"
#include "aoe/rms_import.hpp"

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void eight_player_setup_reaches_runtime() {
    aoe::OrdinaryMatchSetup setup = aoe::OrdinaryMatchSetup::standard();
    for (std::size_t index = 2; index < setup.slots.size(); ++index) {
        setup.slots[index].kind = aoe::OrdinarySlotKind::computer;
        setup.slots[index].civilization = static_cast<aoe::Civilization>(
            1 + static_cast<int>(index)
        );
        setup.slots[index].team = *aoe::TeamId::numbered(
            static_cast<int>(index % 4) + 1
        );
    }
    setup.slots[0].team = *aoe::TeamId::numbered(1);
    setup.slots[1].team = *aoe::TeamId::numbered(2);

    aoe::RandomMapSettings settings;
    settings.seed = 90210;
    const aoe::RmsMapResult generated = aoe::generate_rms_map(settings);
    require(generated.scenario.has_value(), "RMS generation failed");
    aoe::Scenario scenario = aoe::configure_ordinary_random_map(
        *generated.scenario, setup
    );
    require(scenario.roster_entries.size() == 8, "eight slots not emitted");
    require(
        scenario.directed_diplomacy.size() == 56,
        "directed diplomacy matrix incomplete"
    );

    const std::filesystem::path persisted =
        std::filesystem::temp_directory_path() /
        "aoe-ordinary-match-setup-test.scenario";
    aoe::save_scenario(scenario, persisted);
    scenario = aoe::load_scenario(persisted);
    std::filesystem::remove(persisted);
    require(
        scenario.roster_entries.size() == 8 &&
            scenario.directed_diplomacy.size() == 56,
        "ordinary roster did not survive scenario persistence"
    );

    aoe::Simulation simulation = aoe::create_simulation(scenario);
    require(
        simulation.roster().slots()[7].occupied,
        "slot eight absent from runtime roster"
    );
    require(
        simulation.civilization(*aoe::PlayerSlotId::from_index(7)) ==
            setup.slots[7].civilization,
        "slot civilization lost"
    );
    require(
        simulation.roster_diplomacy().stance(
            *aoe::PlayerSlotId::from_index(0),
            *aoe::PlayerSlotId::from_index(4)
        ) == aoe::Diplomacy::ally,
        "same-team players did not start allied"
    );
    for (std::size_t index = 0; index < 8; ++index) {
        const aoe::EntityOwner owner = aoe::entity_owner_from_slot(
            *aoe::PlayerSlotId::from_index(index)
        );
        int town_centers{};
        int villagers{};
        for (const aoe::Building& building : simulation.buildings()) {
            town_centers += building.owner == owner &&
                building.kind == aoe::BuildingKind::town_center;
        }
        for (const aoe::Unit& unit : simulation.units()) {
            villagers += unit.owner == owner &&
                unit.kind == aoe::UnitKind::villager;
        }
        require(town_centers == 1, "player start lacks Town Center");
        require(villagers == 3, "player start lacks villagers");
    }
}

void closed_slots_stay_closed() {
    const aoe::OrdinaryMatchSetup setup = aoe::OrdinaryMatchSetup::standard();
    aoe::RandomMapSettings settings;
    const auto generated = aoe::generate_rms_map(settings);
    require(generated.scenario.has_value(), "RMS generation failed");
    const aoe::Scenario scenario = aoe::configure_ordinary_random_map(
        *generated.scenario, setup
    );
    const aoe::Simulation simulation = aoe::create_simulation(scenario);
    require(
        !simulation.roster().slots()[2].occupied,
        "closed slot became occupied"
    );
}

}  // namespace

int main() {
    try {
        eight_player_setup_reaches_runtime();
        closed_slots_stay_closed();
        std::cout << "All ordinary match setup tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
