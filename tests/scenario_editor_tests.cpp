#include "aoe/scenario_editor.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool value) {
    if (!value) throw std::runtime_error("scenario editor test failed");
}
}

int main() {
    aoe::ScenarioEditor editor{aoe::Scenario{12, 10}};
    require(editor.paint_terrain({3, 4}, aoe::Terrain::water));
    require(editor.paint_elevation({4, 4}, 3));
    require(!editor.paint_elevation({4, 4}, 8));
    require(editor.place_unit({
        aoe::UnitKind::villager, aoe::Player::blue, {2, 2},
        std::nullopt, std::nullopt, std::nullopt, std::nullopt,
        false, {}, aoe::UnitStance::aggressive, std::nullopt
    }));
    require(editor.place_building({
        aoe::BuildingKind::town_center, aoe::Player::blue, {5, 5},
        std::nullopt, std::nullopt, std::nullopt
    }));
    editor.set_economy(aoe::Player::blue, {500, 400, 300, 200});
    editor.set_age(aoe::Player::blue, aoe::Age::castle);
    editor.set_civilization(
        aoe::Player::blue, aoe::Civilization::britons
    );
    editor.set_diplomacy(aoe::Diplomacy::ally);
    require(editor.add_objective({
        1, aoe::Player::blue, true, false, "Hold the crossing."
    }));
    require(editor.add_trigger({
        1, 20, true, false,
        "elapsed_ticks >= 5", "victory blue"
    }));
    require(!editor.add_trigger({
        1, 10, true, false,
        "elapsed_ticks >= 6", "victory red"
    }));
    require(editor.validate().valid);

    const auto path = std::filesystem::temp_directory_path() /
        "aoe-scenario-editor-round-trip.scenario";
    std::string error;
    require(editor.save(path, error));
    aoe::ScenarioEditor loaded = aoe::ScenarioEditor::load(path);
    require(loaded.scenario().map.terrain_at({3, 4}) ==
            aoe::Terrain::water);
    require(loaded.scenario().map.elevation_at({4, 4}) == 3);
    require(loaded.scenario().units.size() == 1);
    require(loaded.scenario().buildings.size() == 1);
    require(loaded.scenario().objectives.size() == 1);
    require(loaded.scenario().triggers.size() == 1);
    require(loaded.scenario().blue_age == aoe::Age::castle);
    require(loaded.scenario().blue_civilization ==
            aoe::Civilization::britons);
    require(editor.remove_at({2, 2}));
    require(editor.undo());
    require(editor.scenario().units.size() == 1);
    require(editor.redo());
    require(editor.scenario().units.empty());
    std::filesystem::remove(path);
    std::cout << "scenario editor tests passed\n";
}
