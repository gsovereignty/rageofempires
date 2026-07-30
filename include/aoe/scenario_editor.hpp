#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "aoe/scenario.hpp"

namespace aoe {

struct ScenarioEditorValidation {
    bool valid{};
    std::vector<std::string> errors;
};

class ScenarioEditor {
public:
    explicit ScenarioEditor(Scenario scenario);

    [[nodiscard]] const Scenario& scenario() const { return scenario_; }
    [[nodiscard]] bool can_undo() const { return !undo_.empty(); }
    [[nodiscard]] bool can_redo() const { return !redo_.empty(); }

    bool paint_terrain(TilePosition position, Terrain terrain);
    bool paint_elevation(TilePosition position, int elevation);
    bool place_unit(UnitPlacement placement);
    bool place_building(BuildingPlacement placement);
    bool remove_at(TilePosition position);
    void set_economy(Player player, Economy economy);
    void set_age(Player player, Age age);
    void set_civilization(Player player, Civilization civilization);
    void set_diplomacy(Diplomacy diplomacy);
    void set_match_rules(MatchRules rules);
    bool add_objective(ScenarioObjective objective);
    bool add_trigger(ScenarioTrigger trigger);

    bool undo();
    bool redo();
    [[nodiscard]] ScenarioEditorValidation validate() const;
    bool save(const std::filesystem::path& path, std::string& error) const;
    static ScenarioEditor load(const std::filesystem::path& path);

private:
    template <class Mutation>
    bool mutate(Mutation mutation) {
        Scenario changed = scenario_;
        if (!mutation(changed)) return false;
        undo_.push_back(std::move(scenario_));
        scenario_ = std::move(changed);
        redo_.clear();
        return true;
    }

    Scenario scenario_;
    std::vector<Scenario> undo_;
    std::vector<Scenario> redo_;
};

}  // namespace aoe
