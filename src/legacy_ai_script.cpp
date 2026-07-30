#include "aoe/legacy_ai_script.hpp"

#include <charconv>
#include <limits>
#include <stdexcept>

namespace aoe {
namespace {

constexpr std::size_t max_source_size = 4U * 1024U * 1024U;
constexpr std::size_t max_depth = 64;
constexpr std::size_t max_nodes = 200000;
constexpr std::size_t max_rules = 10000;
constexpr std::size_t max_rule_terms = 128;

struct Node {
    bool list{};
    std::string atom;
    std::vector<Node> children;
    std::size_t begin{};
    std::size_t end{};
};

class Parser {
public:
    explicit Parser(const std::string& source) : source_(source) {}

    std::vector<Node> parse() {
        std::vector<Node> nodes;
        skip();
        while (cursor_ < source_.size()) {
            nodes.push_back(node(0));
            skip();
        }
        return nodes;
    }

private:
    Node node(std::size_t depth) {
        if (++nodes_ > max_nodes) throw std::runtime_error("AI node limit");
        if (depth > max_depth) throw std::runtime_error("AI nesting limit");
        skip();
        if (cursor_ >= source_.size()) {
            throw std::runtime_error("truncated AI expression");
        }
        Node result;
        result.begin = cursor_;
        if (source_[cursor_] != '(') {
            result.atom = token();
            result.end = cursor_;
            return result;
        }
        result.list = true;
        ++cursor_;
        skip();
        while (cursor_ < source_.size() && source_[cursor_] != ')') {
            result.children.push_back(node(depth + 1));
            skip();
        }
        if (cursor_ >= source_.size()) {
            throw std::runtime_error("unterminated AI list");
        }
        ++cursor_;
        result.end = cursor_;
        return result;
    }

    std::string token() {
        if (source_[cursor_] == '"') {
            const auto begin = ++cursor_;
            while (cursor_ < source_.size() && source_[cursor_] != '"') {
                if (source_[cursor_] == '\\') {
                    throw std::runtime_error(
                        "AI quoted escapes are unsupported"
                    );
                }
                ++cursor_;
            }
            if (cursor_ >= source_.size()) {
                throw std::runtime_error("unterminated AI string");
            }
            std::string value = source_.substr(begin, cursor_ - begin);
            ++cursor_;
            return value;
        }
        const auto begin = cursor_;
        while (cursor_ < source_.size() &&
               source_[cursor_] != '(' && source_[cursor_] != ')' &&
               source_[cursor_] != ';' &&
               source_[cursor_] != ' ' && source_[cursor_] != '\t' &&
               source_[cursor_] != '\r' && source_[cursor_] != '\n') {
            ++cursor_;
        }
        if (begin == cursor_) throw std::runtime_error("empty AI token");
        return source_.substr(begin, cursor_ - begin);
    }

    void skip() {
        while (cursor_ < source_.size()) {
            if (source_[cursor_] == ';') {
                while (cursor_ < source_.size() &&
                       source_[cursor_] != '\n') ++cursor_;
            } else if (source_[cursor_] == ' ' ||
                       source_[cursor_] == '\t' ||
                       source_[cursor_] == '\r' ||
                       source_[cursor_] == '\n') {
                ++cursor_;
            } else {
                break;
            }
        }
    }

    const std::string& source_;
    std::size_t cursor_{};
    std::size_t nodes_{};
};

LegacyAiSpan span_for(const std::string& source, const Node& node) {
    return {node.begin, node.end - node.begin,
            source.substr(node.begin, node.end - node.begin)};
}

bool atom(const Node& node, const std::string& value) {
    return !node.list && node.atom == value;
}

std::optional<std::int64_t> integer(const std::string& value) {
    std::int64_t parsed{};
    const auto result = std::from_chars(
        value.data(), value.data() + value.size(), parsed
    );
    if (result.ec != std::errc{} ||
        result.ptr != value.data() + value.size()) return std::nullopt;
    return parsed;
}

std::optional<LegacyAiExpression> expression(
    const std::string& source, const Node& node
) {
    if (!node.list || node.children.empty() ||
        node.children.front().list) return std::nullopt;
    LegacyAiExpression result;
    result.name = node.children.front().atom;
    result.span = span_for(source, node);
    for (std::size_t i = 1; i < node.children.size(); ++i) {
        if (node.children[i].list) return std::nullopt;
        result.arguments.push_back(node.children[i].atom);
    }
    return result;
}

bool supported_fact(const LegacyAiExpression& value) {
    const auto comparison = [](const std::string& op) {
        return op == "==" || op == "!=" || op == "<" || op == "<=" ||
            op == ">" || op == ">=";
    };
    if (value.name == "true") return value.arguments.empty();
    if (value.name == "food-amount" ||
        value.name == "wood-amount" ||
        value.name == "gold-amount" ||
        value.name == "stone-amount" ||
        value.name == "population" ||
        value.name == "current-age") {
        return value.arguments.size() == 2 &&
            comparison(value.arguments[0]);
    }
    if (value.name == "unit-type-count") {
        return value.arguments.size() == 3 &&
            comparison(value.arguments[1]);
    }
    if (value.name == "random-number") {
        return value.arguments.size() == 2 &&
            comparison(value.arguments[0]);
    }
    return false;
}

bool supported_action(const LegacyAiExpression& value) {
    if (value.name == "train" || value.name == "research" ||
        value.name == "build") return value.arguments.size() == 1;
    if (value.name == "attack-now") return value.arguments.empty();
    if (value.name == "tribute-to-player") {
        return value.arguments.size() == 3;
    }
    if (value.name == "set-diplomacy") {
        return value.arguments.size() == 2;
    }
    return false;
}

bool compare(std::int64_t left, const std::string& op, std::int64_t right) {
    if (op == "==") return left == right;
    if (op == "!=") return left != right;
    if (op == "<") return left < right;
    if (op == "<=") return left <= right;
    if (op == ">") return left > right;
    if (op == ">=") return left >= right;
    return false;
}

}  // namespace

LegacyAiInspectResult inspect_legacy_ai_script(std::string source) {
    LegacyAiInspectResult result;
    try {
        if (source.size() > max_source_size) {
            throw std::runtime_error("AI script exceeds 4 MiB");
        }
        LegacyAiScript script;
        script.source = std::move(source);
        const auto nodes = Parser(script.source).parse();
        for (const Node& node : nodes) {
            bool accepted = false;
            if (node.list && !node.children.empty() &&
                atom(node.children[0], "defconst") &&
                node.children.size() == 3 &&
                !node.children[1].list && !node.children[2].list) {
                const auto value = integer(node.children[2].atom);
                if (value && !script.constants.contains(
                        node.children[1].atom)) {
                    script.constants.emplace(
                        node.children[1].atom, *value
                    );
                    accepted = true;
                }
            } else if (node.list && !node.children.empty() &&
                       atom(node.children[0], "load") &&
                       node.children.size() == 2 &&
                       !node.children[1].list) {
                script.loads.push_back({
                    node.children[1].atom, std::nullopt, false,
                    std::nullopt, 0,
                    span_for(script.source, node)
                });
                script.blockers.push_back(
                    "unresolved load: " + node.children[1].atom
                );
                accepted = true;
            } else if (node.list && !node.children.empty() &&
                       atom(node.children[0], "load-random") &&
                       node.children.size() >= 2) {
                bool valid = true;
                int total{};
                const std::size_t group = script.loads.size() + 1;
                std::vector<LegacyAiLoad> random_loads;
                std::size_t i = 1;
                while (i < node.children.size()) {
                    if (node.children[i].list) { valid = false; break; }
                    const auto chance = integer(node.children[i].atom);
                    if (!chance) {
                        if (i + 1 != node.children.size()) {
                            valid = false;
                            break;
                        }
                        random_loads.push_back({
                            node.children[i].atom, std::nullopt, false,
                            std::nullopt, group,
                            span_for(script.source, node)
                        });
                        ++i;
                        continue;
                    }
                    if (*chance < 0 || *chance > 100 ||
                        i + 1 >= node.children.size() ||
                        node.children[i + 1].list) {
                        valid = false;
                        break;
                    }
                    total += static_cast<int>(*chance);
                    if (total > 100) { valid = false; break; }
                    random_loads.push_back({
                        node.children[i + 1].atom, std::nullopt, false,
                        static_cast<std::uint8_t>(*chance), group,
                        span_for(script.source, node)
                    });
                    i += 2;
                }
                accepted = valid && i == node.children.size();
                if (accepted) {
                    script.loads.insert(
                        script.loads.end(),
                        random_loads.begin(), random_loads.end()
                    );
                    for (const auto& load : random_loads) {
                        script.blockers.push_back(
                            "unresolved random load: " + load.path
                        );
                    }
                }
            } else if (node.list && !node.children.empty() &&
                       atom(node.children[0], "defrule")) {
                if (script.rules.size() >= max_rules) {
                    throw std::runtime_error("AI rule limit");
                }
                LegacyAiRule rule;
                rule.span = span_for(script.source, node);
                bool actions = false;
                bool valid = true;
                for (std::size_t i = 1; i < node.children.size(); ++i) {
                    if (atom(node.children[i], "=>")) {
                        if (actions) valid = false;
                        actions = true;
                        continue;
                    }
                    const auto item = expression(
                        script.source, node.children[i]
                    );
                    if (!item) { valid = false; continue; }
                    (actions ? rule.actions : rule.facts).push_back(*item);
                }
                if (!actions || rule.facts.empty() || rule.actions.empty() ||
                    rule.facts.size() > max_rule_terms ||
                    rule.actions.size() > max_rule_terms) valid = false;
                if (valid) {
                    for (const auto& fact : rule.facts) {
                        if (!supported_fact(fact)) {
                            script.blockers.push_back(
                                "unsupported fact: " + fact.name
                            );
                        }
                    }
                    for (const auto& action : rule.actions) {
                        if (!supported_action(action)) {
                            script.blockers.push_back(
                                "unsupported action: " + action.name
                            );
                        }
                    }
                    script.rules.push_back(std::move(rule));
                    accepted = true;
                }
            }
            if (!accepted) {
                script.unsupported_spans.push_back(
                    span_for(script.source, node)
                );
                script.blockers.push_back("unsupported top-level form");
            }
        }
        script.executable =
            script.unsupported_spans.empty() && script.blockers.empty();
        result.script = std::move(script);
        result.diagnostic = "bounded AI script inspection complete";
    } catch (const std::exception& error) {
        result.diagnostic = error.what();
    }
    return result;
}

LegacyAiExecutionResult execute_legacy_ai_script(
    const LegacyAiScript& script,
    const LegacyAiExecutionState& state,
    const LegacyAiExecutionMappings& mappings,
    LegacyAiExecutionPolicy policy
) {
    LegacyAiExecutionResult result;
    if (!script.executable) {
        result.blockers = script.blockers;
        if (result.blockers.empty()) {
            result.blockers.push_back("script is inspection-only");
        }
        return result;
    }
    const auto resolve = [&](const std::string& value) {
        if (const auto number = integer(value)) return number;
        const auto found = script.constants.find(value);
        return found == script.constants.end()
            ? std::optional<std::int64_t>{}
            : std::optional<std::int64_t>{found->second};
    };
    for (const auto& rule : script.rules) {
        for (const auto& fact : rule.facts) {
            bool mapped = true;
            if (fact.name == "current-age") {
                mapped = mappings.ages.contains(fact.arguments[1]);
            } else if (fact.name == "unit-type-count") {
                mapped = mappings.units.contains(fact.arguments[0]) &&
                    resolve(fact.arguments[2]).has_value();
            } else if (fact.name != "true") {
                if (fact.name != "population" &&
                    fact.name != "random-number") {
                    const std::string resource_name =
                        fact.name.substr(0, fact.name.size() - 7);
                    mapped = mappings.resources.contains(resource_name);
                }
                mapped = mapped &&
                    resolve(fact.arguments.back()).has_value();
            }
            if (!mapped) {
                result.blockers.push_back(
                    "missing fact mapping: " + fact.name
                );
            }
        }
        for (const auto& action : rule.actions) {
            bool mapped = true;
            if (action.name == "train") {
                mapped = mappings.units.contains(action.arguments[0]);
            } else if (action.name == "research") {
                mapped = mappings.technologies.contains(
                    action.arguments[0]
                );
            } else if (action.name == "build") {
                mapped = mappings.buildings.contains(action.arguments[0]);
            } else if (action.name == "tribute-to-player") {
                const auto player = integer(action.arguments[0]);
                const auto amount = resolve(action.arguments[2]);
                mapped = player && *player >= 1 && *player <= 8 &&
                    mappings.resources.contains(action.arguments[1]) &&
                    amount && *amount >= 0 &&
                    *amount <= std::numeric_limits<int>::max();
            } else if (action.name == "set-diplomacy") {
                const auto player = integer(action.arguments[0]);
                mapped = player && *player >= 1 && *player <= 8 &&
                    (action.arguments[1] == "ally" ||
                     action.arguments[1] == "neutral" ||
                     action.arguments[1] == "enemy");
            }
            if (!mapped) {
                result.blockers.push_back(
                    "missing action mapping: " + action.name
                );
            }
        }
    }
    if (!result.blockers.empty()) return result;
    const auto fact_true = [&](const LegacyAiExpression& fact) {
        if (fact.name == "true") return true;
        std::int64_t left{};
        if (fact.name == "population") left = state.population;
        else if (fact.name == "current-age") {
            const auto age = mappings.ages.find(fact.arguments.back());
            if (age == mappings.ages.end()) return false;
            return compare(
                static_cast<int>(state.age), fact.arguments[0],
                static_cast<int>(age->second)
            );
        } else if (fact.name == "unit-type-count") {
            const auto right = resolve(fact.arguments.back());
            if (!right) return false;
            const auto unit = mappings.units.find(fact.arguments[0]);
            if (unit == mappings.units.end()) return false;
            const auto count = state.unit_counts.find(unit->second);
            left = count == state.unit_counts.end() ? 0 : count->second;
            return compare(left, fact.arguments[1], *right);
        } else if (fact.name == "random-number") {
            left = static_cast<std::int64_t>(
                (state.random_seed * 6364136223846793005ULL +
                 1442695040888963407ULL) % 100ULL
            );
        } else {
            const std::string resource_name =
                fact.name.substr(0, fact.name.size() - 7);
            const auto resource = mappings.resources.find(resource_name);
            if (resource == mappings.resources.end()) return false;
            const auto amount = state.resources.find(resource->second);
            left = amount == state.resources.end() ? 0 : amount->second;
        }
        const auto right = resolve(fact.arguments.back());
        if (!right) return false;
        return compare(left, fact.arguments[0], *right);
    };
    for (const auto& rule : script.rules) {
        if (result.rules_evaluated >= policy.max_rules) {
            result.budget_exhausted = true;
            break;
        }
        ++result.rules_evaluated;
        bool fire = true;
        for (const auto& fact : rule.facts) fire &= fact_true(fact);
        if (!fire) continue;
        for (const auto& action : rule.actions) {
            if (result.intents.size() >= policy.max_actions) {
                result.budget_exhausted = true;
                return result;
            }
            if (action.name == "train") {
                const auto value = mappings.units.find(action.arguments[0]);
                if (value == mappings.units.end()) {
                    result.blockers.push_back("missing unit mapping");
                    return result;
                }
                result.intents.emplace_back(LegacyAiTrainIntent{value->second});
            } else if (action.name == "research") {
                const auto value =
                    mappings.technologies.find(action.arguments[0]);
                if (value == mappings.technologies.end()) {
                    result.blockers.push_back("missing technology mapping");
                    return result;
                }
                result.intents.emplace_back(
                    LegacyAiResearchIntent{value->second}
                );
            } else if (action.name == "build") {
                const auto value =
                    mappings.buildings.find(action.arguments[0]);
                if (value == mappings.buildings.end()) {
                    result.blockers.push_back("missing building mapping");
                    return result;
                }
                result.intents.emplace_back(LegacyAiBuildIntent{value->second});
            } else if (action.name == "attack-now") {
                result.intents.emplace_back(LegacyAiAttackIntent{});
            } else if (action.name == "tribute-to-player") {
                const auto player = integer(action.arguments[0]);
                const auto resource =
                    mappings.resources.find(action.arguments[1]);
                const auto amount = resolve(action.arguments[2]);
                if (!player || *player < 1 || *player > 8 ||
                    resource == mappings.resources.end() || !amount ||
                    *amount < 0 || *amount > std::numeric_limits<int>::max()) {
                    result.blockers.push_back("invalid tribute mapping");
                    return result;
                }
                result.intents.emplace_back(LegacyAiTributeIntent{
                    static_cast<std::uint8_t>(*player),
                    resource->second, static_cast<int>(*amount)
                });
            } else {
                const auto player = integer(action.arguments[0]);
                if (!player || *player < 1 || *player > 8) {
                    result.blockers.push_back("invalid diplomacy player");
                    return result;
                }
                Diplomacy relation;
                if (action.arguments[1] == "ally") relation = Diplomacy::ally;
                else if (action.arguments[1] == "neutral") {
                    relation = Diplomacy::neutral;
                } else if (action.arguments[1] == "enemy") {
                    relation = Diplomacy::enemy;
                } else {
                    result.blockers.push_back("invalid diplomacy relation");
                    return result;
                }
                result.intents.emplace_back(LegacyAiDiplomacyIntent{
                    static_cast<std::uint8_t>(*player), relation
                });
            }
        }
    }
    return result;
}

}  // namespace aoe
