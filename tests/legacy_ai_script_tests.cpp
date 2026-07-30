#include <cstdlib>
#include <iostream>
#include <source_location>
#include <string>

#include "aoe/legacy_ai_script.hpp"

namespace {

void require(
    bool value,
    const std::source_location location = std::source_location::current()
) {
    if (!value) {
        std::cerr << "Requirement failed at " << location.file_name() << ':'
                  << location.line() << '\n';
        std::abort();
    }
}

aoe::LegacyAiExecutionMappings mappings() {
    aoe::LegacyAiExecutionMappings value;
    value.resources = {
        {"food", aoe::ResourceKind::food},
        {"wood", aoe::ResourceKind::wood},
        {"gold", aoe::ResourceKind::gold},
        {"stone", aoe::ResourceKind::stone},
    };
    value.ages = {
        {"dark-age", aoe::Age::dark},
        {"feudal-age", aoe::Age::feudal},
        {"castle-age", aoe::Age::castle},
        {"imperial-age", aoe::Age::imperial},
    };
    value.units.emplace("villager", aoe::UnitKind::villager);
    value.buildings.emplace("house", aoe::BuildingKind::house);
    value.technologies.emplace("loom", aoe::Technology::loom);
    return value;
}

void parses_rules_constants_loads_and_raw_spans() {
    const std::string source = R"(
; classic-style fixture
(defconst food-target 500)
(load "economy")
(load-random 20 "rush" 30 "boom" "fallback")
(defrule
    (food-amount >= food-target)
    (population < 30)
=>
    (train villager)
    (research loom)
)
)";
    const auto result = aoe::inspect_legacy_ai_script(source);
    require(result.script.has_value());
    require(!result.script->executable);
    require(result.script->constants.at("food-target") == 500);
    require(result.script->loads.size() == 4);
    require(result.script->loads[0].path == "economy");
    require(result.script->loads[1].probability == 20);
    require(!result.script->loads[3].probability.has_value());
    require(result.script->blockers.size() == 4);
    require(result.script->rules.size() == 1);
    require(result.script->rules[0].facts.size() == 2);
    require(result.script->rules[0].actions.size() == 2);
    require(result.script->rules[0].span.raw.find("defrule") !=
            std::string::npos);
}

void executes_supported_rules_in_source_order() {
    const auto inspected = aoe::inspect_legacy_ai_script(R"(
(defrule
  (food-amount >= 500)
  (current-age == feudal-age)
  (unit-type-count villager >= 5)
  (random-number < 100)
=>
  (train villager)
  (research loom)
  (build house)
  (attack-now)
  (tribute-to-player 2 food 100)
  (set-diplomacy 2 ally)
)
)");
    require(inspected.script.has_value());
    aoe::LegacyAiExecutionState state;
    state.resources[aoe::ResourceKind::food] = 600;
    state.population = 20;
    state.age = aoe::Age::feudal;
    state.unit_counts[aoe::UnitKind::villager] = 6;
    const auto result = aoe::execute_legacy_ai_script(
        *inspected.script, state, mappings()
    );
    require(result.blockers.empty());
    require(result.rules_evaluated == 1);
    require(result.intents.size() == 6);
    require(std::holds_alternative<aoe::LegacyAiTrainIntent>(
        result.intents[0]
    ));
    require(std::get<aoe::LegacyAiTributeIntent>(
        result.intents[4]
    ).amount == 100);
    require(std::get<aoe::LegacyAiDiplomacyIntent>(
        result.intents[5]
    ).relation == aoe::Diplomacy::ally);
}

void unknown_semantics_block_whole_executable_mode() {
    const auto inspected = aoe::inspect_legacy_ai_script(R"(
(defrule
  (enemy-buildings-in-town > 0)
=>
  (chat-to-all "attack")
)
)");
    require(inspected.script.has_value());
    require(!inspected.script->executable);
    require(inspected.script->unsupported_spans.empty());
    require(inspected.script->blockers.size() == 2);
    const auto execution = aoe::execute_legacy_ai_script(
        *inspected.script, {}, mappings()
    );
    require(execution.intents.empty());
    require(execution.blockers.size() == 2);
}

void preserves_unsupported_top_level_and_enforces_budgets() {
    const auto inspected = aoe::inspect_legacy_ai_script(R"(
(defrule (true) => (attack-now) (attack-now))
(mystery-block 1 2 3)
)");
    require(inspected.script.has_value());
    require(!inspected.script->executable);
    require(inspected.script->unsupported_spans.size() == 1);
    require(
        inspected.script->unsupported_spans[0].raw ==
        "(mystery-block 1 2 3)"
    );

    const auto bounded = aoe::inspect_legacy_ai_script(
        "(defrule (true) => (attack-now) (attack-now))"
    );
    require(bounded.script.has_value());
    const auto execution = aoe::execute_legacy_ai_script(
        *bounded.script, {}, mappings(), {.max_rules = 1, .max_actions = 1}
    );
    require(execution.intents.size() == 1);
    require(execution.budget_exhausted);
}

void conditional_directives_never_select_a_branch_silently() {
    const auto inspected = aoe::inspect_legacy_ai_script(R"(
#load-if-defined DIFFICULTY-HARD
(defrule (true) => (attack-now))
#end-if
)");
    require(inspected.script.has_value());
    require(!inspected.script->executable);
    require(inspected.script->unsupported_spans.size() == 3);
    require(
        inspected.script->unsupported_spans[0].raw ==
        "#load-if-defined"
    );
}

}  // namespace

int main() {
    parses_rules_constants_loads_and_raw_spans();
    executes_supported_rules_in_source_order();
    unknown_semantics_block_whole_executable_mode();
    preserves_unsupported_top_level_and_enforces_budgets();
    conditional_directives_never_select_a_branch_silently();
    std::cout << "All legacy AI script tests passed\n";
}
