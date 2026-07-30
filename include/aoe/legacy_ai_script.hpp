#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

#include "aoe/types.hpp"

namespace aoe {

struct LegacyAiSpan {
    std::size_t offset{};
    std::size_t length{};
    std::string raw;
};

struct LegacyAiExpression {
    std::string name;
    std::vector<std::string> arguments;
    LegacyAiSpan span;
};

struct LegacyAiRule {
    std::vector<LegacyAiExpression> facts;
    std::vector<LegacyAiExpression> actions;
    LegacyAiSpan span;
};

struct LegacyAiLoad {
    std::string path;
    std::optional<std::string> required_constant;
    bool load_if_not_defined{};
    std::optional<std::uint8_t> probability;
    std::size_t random_group{};
    LegacyAiSpan span;
};

struct LegacyAiScript {
    std::string source;
    std::map<std::string, std::int64_t> constants;
    std::vector<LegacyAiLoad> loads;
    std::vector<LegacyAiRule> rules;
    std::vector<LegacyAiSpan> unsupported_spans;
    bool executable{};
    std::vector<std::string> blockers;
};

struct LegacyAiInspectResult {
    std::optional<LegacyAiScript> script;
    std::string diagnostic;
};

struct LegacyAiExecutionState {
    std::map<ResourceKind, std::int64_t> resources;
    std::int64_t population{};
    Age age{Age::dark};
    std::map<UnitKind, std::int64_t> unit_counts;
    std::uint64_t random_seed{1};
};

struct LegacyAiExecutionMappings {
    std::map<std::string, ResourceKind> resources;
    std::map<std::string, Age> ages;
    std::map<std::string, UnitKind> units;
    std::map<std::string, BuildingKind> buildings;
    std::map<std::string, Technology> technologies;
};

struct LegacyAiTrainIntent { UnitKind kind; };
struct LegacyAiResearchIntent { Technology technology; };
struct LegacyAiBuildIntent { BuildingKind kind; };
struct LegacyAiAttackIntent {};
struct LegacyAiTributeIntent {
    std::uint8_t player_number;
    ResourceKind resource;
    int amount;
};
struct LegacyAiDiplomacyIntent {
    std::uint8_t player_number;
    Diplomacy relation;
};

using LegacyAiIntent = std::variant<
    LegacyAiTrainIntent,
    LegacyAiResearchIntent,
    LegacyAiBuildIntent,
    LegacyAiAttackIntent,
    LegacyAiTributeIntent,
    LegacyAiDiplomacyIntent
>;

struct LegacyAiExecutionPolicy {
    std::size_t max_rules{4096};
    std::size_t max_actions{1024};
};

struct LegacyAiExecutionResult {
    std::vector<LegacyAiIntent> intents;
    std::size_t rules_evaluated{};
    bool budget_exhausted{};
    std::vector<std::string> blockers;
};

LegacyAiInspectResult inspect_legacy_ai_script(std::string source);
LegacyAiExecutionResult execute_legacy_ai_script(
    const LegacyAiScript& script,
    const LegacyAiExecutionState& state,
    const LegacyAiExecutionMappings& mappings,
    LegacyAiExecutionPolicy policy = {}
);

}  // namespace aoe
