#include "aoe/scenario_editor.hpp"

#include <algorithm>
#include <exception>
#include <set>

namespace aoe {

ScenarioEditor::ScenarioEditor(Scenario scenario)
    : scenario_(std::move(scenario)) {}

bool ScenarioEditor::paint_terrain(
    TilePosition position,
    Terrain terrain
) {
    return mutate([&](Scenario& value) {
        if (!value.map.contains(position)) return false;
        value.map.set_terrain(position, terrain);
        return true;
    });
}

bool ScenarioEditor::paint_elevation(
    TilePosition position,
    int elevation
) {
    return mutate([&](Scenario& value) {
        if (!value.map.contains(position) ||
            elevation < 0 || elevation > 7) return false;
        value.map.set_elevation(position, elevation);
        return true;
    });
}

bool ScenarioEditor::place_unit(UnitPlacement placement) {
    return mutate([&](Scenario& value) {
        if (!value.map.contains(placement.position)) return false;
        value.units.push_back(std::move(placement));
        return true;
    });
}

bool ScenarioEditor::place_building(BuildingPlacement placement) {
    return mutate([&](Scenario& value) {
        if (!value.map.contains(placement.position)) return false;
        value.buildings.push_back(std::move(placement));
        return true;
    });
}

bool ScenarioEditor::remove_at(TilePosition position) {
    return mutate([&](Scenario& value) {
        const auto old_units = value.units.size();
        const auto old_buildings = value.buildings.size();
        std::erase_if(value.units, [&](const UnitPlacement& placement) {
            return placement.position == position;
        });
        std::erase_if(
            value.buildings,
            [&](const BuildingPlacement& placement) {
                return placement.position == position;
            }
        );
        return old_units != value.units.size() ||
            old_buildings != value.buildings.size();
    });
}

void ScenarioEditor::set_economy(Player player, Economy economy) {
    (void)mutate([&](Scenario& value) {
        if (player != Player::blue && player != Player::red) return false;
        (player == Player::blue
             ? value.blue_economy : value.red_economy) = economy;
        return true;
    });
}

void ScenarioEditor::set_age(Player player, Age age) {
    (void)mutate([&](Scenario& value) {
        if (player != Player::blue && player != Player::red) return false;
        (player == Player::blue ? value.blue_age : value.red_age) = age;
        return true;
    });
}

void ScenarioEditor::set_civilization(
    Player player,
    Civilization civilization
) {
    (void)mutate([&](Scenario& value) {
        if (player != Player::blue && player != Player::red) return false;
        (player == Player::blue
             ? value.blue_civilization
             : value.red_civilization) = civilization;
        return true;
    });
}

void ScenarioEditor::set_diplomacy(Diplomacy diplomacy) {
    (void)mutate([&](Scenario& value) {
        value.blue_red_diplomacy = diplomacy;
        return true;
    });
}

void ScenarioEditor::set_match_rules(MatchRules rules) {
    (void)mutate([&](Scenario& value) {
        value.match_rules = rules;
        return true;
    });
}

bool ScenarioEditor::add_objective(ScenarioObjective objective) {
    return mutate([&](Scenario& value) {
        if (objective.id <= 0 || objective.description.empty() ||
            std::ranges::any_of(value.objectives, [&](const auto& prior) {
                return prior.id == objective.id;
            })) return false;
        value.objectives.push_back(std::move(objective));
        return true;
    });
}

bool ScenarioEditor::add_trigger(ScenarioTrigger trigger) {
    return mutate([&](Scenario& value) {
        if (trigger.id <= 0 || trigger.conditions.empty() ||
            trigger.effects.empty() ||
            std::ranges::any_of(value.triggers, [&](const auto& prior) {
                return prior.id == trigger.id;
            })) return false;
        value.triggers.push_back(std::move(trigger));
        return true;
    });
}

bool ScenarioEditor::undo() {
    if (undo_.empty()) return false;
    redo_.push_back(std::move(scenario_));
    scenario_ = std::move(undo_.back());
    undo_.pop_back();
    return true;
}

bool ScenarioEditor::redo() {
    if (redo_.empty()) return false;
    undo_.push_back(std::move(scenario_));
    scenario_ = std::move(redo_.back());
    redo_.pop_back();
    return true;
}

ScenarioEditorValidation ScenarioEditor::validate() const {
    ScenarioEditorValidation result{true, {}};
    std::set<int> objective_ids;
    for (const ScenarioObjective& objective : scenario_.objectives) {
        if (objective.id <= 0 || objective.description.empty() ||
            !objective_ids.insert(objective.id).second) {
            result.errors.emplace_back("invalid or duplicate objective");
        }
    }
    std::set<int> trigger_ids;
    for (const ScenarioTrigger& trigger : scenario_.triggers) {
        if (trigger.id <= 0 || trigger.conditions.empty() ||
            trigger.effects.empty() ||
            !trigger_ids.insert(trigger.id).second) {
            result.errors.emplace_back("invalid or duplicate trigger");
        }
    }
    const auto valid_position = [&](TilePosition position) {
        return scenario_.map.contains(position);
    };
    if (!std::ranges::all_of(
            scenario_.units, [&](const UnitPlacement& value) {
                return valid_position(value.position);
            }) ||
        !std::ranges::all_of(
            scenario_.buildings, [&](const BuildingPlacement& value) {
                return valid_position(value.position);
            })) {
        result.errors.emplace_back("placement outside map");
    }
    if (result.errors.empty()) {
        try {
            (void)create_simulation(scenario_);
        } catch (const std::exception& exception) {
            result.errors.push_back(exception.what());
        }
    }
    result.valid = result.errors.empty();
    return result;
}

bool ScenarioEditor::save(
    const std::filesystem::path& path,
    std::string& error
) const {
    const ScenarioEditorValidation validation = validate();
    if (!validation.valid) {
        error = validation.errors.front();
        return false;
    }
    try {
        save_scenario(scenario_, path);
        error.clear();
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

ScenarioEditor ScenarioEditor::load(
    const std::filesystem::path& path
) {
    return ScenarioEditor{load_scenario(path)};
}

}  // namespace aoe
