#include "aoe/rms_import.hpp"
#include "aoe/game_rules.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <set>
#include <sstream>
#include <unordered_map>

namespace aoe {
namespace {

std::string lower(std::string text) {
    std::ranges::transform(text, text.begin(), [](unsigned char value) {
        return static_cast<char>(std::tolower(value));
    });
    return text;
}

std::string trim(std::string text) {
    const auto whitespace = [](unsigned char value) {
        return std::isspace(value) != 0;
    };
    text.erase(
        text.begin(),
        std::find_if_not(text.begin(), text.end(), whitespace)
    );
    text.erase(
        std::find_if_not(text.rbegin(), text.rend(), whitespace).base(),
        text.end()
    );
    return text;
}

std::vector<std::string> words(const std::string& line) {
    std::vector<std::string> result;
    std::istringstream input(line);
    std::string word;
    while (input >> word) result.push_back(std::move(word));
    return result;
}

bool supported_section(const std::string& section) {
    static const std::set<std::string> names{
        "player_setup", "land_generation", "elevation_generation",
        "cliff_generation", "terrain_generation",
        "connection_generation", "objects_generation",
    };
    return names.contains(section);
}

bool supported_directive(const std::string& name) {
    static const std::set<std::string> names{
        "random_placement", "grouped_by_team", "direct_placement",
        "override_map_size", "base_terrain", "create_player_lands",
        "create_land", "terrain_type", "land_percent", "base_size",
        "border_fuzziness", "left_border", "right_border", "top_border",
        "bottom_border", "zone", "other_zone_avoidance_distance",
        "assign_to_player", "set_zone_by_team", "create_terrain",
        "number_of_clumps", "number_of_tiles", "clumping_factor",
        "spacing_to_other_terrain_types", "set_avoid_player_start_areas",
        "set_scale_by_groups", "set_scale_by_size", "create_elevation",
        "create_cliffs", "min_number_of_cliffs", "max_number_of_cliffs",
        "min_length_of_cliff", "max_length_of_cliff",
        "cliff_curliness", "min_distance_cliffs",
        "min_terrain_distance", "create_connect_all_lands",
        "create_connect_teams_lands", "create_connect_same_land_zones",
        "create_object", "number_of_objects", "number_of_groups",
        "group_variance", "group_placement_radius",
        "set_place_for_every_player", "min_distance_to_players",
        "max_distance_to_players", "min_distance_group_placement",
        "temp_min_distance_group_placement",
        "max_distance_to_other_zones", "set_tight_grouping",
        "set_loose_grouping", "resource_delta", "second_object",
        "set_gaia_object_only", "set_gaia_unconvertible",
    };
    return names.contains(name);
}

std::optional<int> integer(const std::string& text) {
    int value{};
    const auto result = std::from_chars(
        text.data(), text.data() + text.size(), value
    );
    if (result.ec != std::errc{} ||
        result.ptr != text.data() + text.size()) return std::nullopt;
    return value;
}

bool object_attribute(const std::string& name) {
    static const std::set<std::string> names{
        "number_of_objects", "number_of_groups", "group_variance",
        "group_placement_radius", "set_place_for_every_player",
        "min_distance_to_players", "max_distance_to_players",
        "min_distance_group_placement",
        "temp_min_distance_group_placement",
        "max_distance_to_other_zones",
        "set_tight_grouping", "set_loose_grouping", "resource_delta",
        "second_object", "set_gaia_object_only",
        "set_gaia_unconvertible",
    };
    return names.contains(name);
}

bool valid_object_arity(
    const std::string& name, const std::vector<std::string>& parts
) {
    if (name == "create_object" || name == "second_object") {
        return parts.size() == 2;
    }
    if (name == "set_place_for_every_player" ||
        name == "set_tight_grouping" ||
        name == "set_loose_grouping" ||
        name == "set_gaia_object_only" ||
        name == "set_gaia_unconvertible") {
        return parts.size() == 1;
    }
    if (!object_attribute(name)) return true;
    if (parts.size() != 2) return false;
    const auto value = integer(parts[1]);
    if (!value) return false;
    if (name == "resource_delta") return true;
    if (name == "number_of_groups") return *value > 0;
    return *value >= 0;
}

bool implemented_object_attribute(const std::string& name) {
    static const std::set<std::string> names{
        "number_of_objects", "number_of_groups",
        "group_placement_radius", "set_place_for_every_player",
        "min_distance_to_players", "max_distance_to_players",
        "min_distance_group_placement",
        "temp_min_distance_group_placement", "set_tight_grouping",
        "set_loose_grouping", "resource_delta",
        "set_gaia_object_only",
    };
    return names.contains(name);
}

bool implemented_object_name(const std::string& text) {
    static const std::set<std::string> names{
        "town_center", "villager", "scout", "gold", "stone",
        "berries", "sheep", "boar", "deer", "relic",
    };
    return names.contains(lower(text));
}

std::uint64_t mix(std::uint64_t value) {
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
}

struct ObjectGeneration {
    std::string name;
    int objects{1};
    int groups{1};
    int group_radius{2};
    int minimum_player_distance{};
    std::optional<int> maximum_player_distance;
    int minimum_group_distance{};
    int resource_delta{};
    bool every_player{};
    bool gaia_only{};
};

std::vector<ObjectGeneration> object_generations(
    const RmsDocument& document,
    const auto& active
) {
    std::vector<ObjectGeneration> result;
    std::unordered_map<std::size_t, std::size_t> indexes;
    for (const RmsDirective& directive : document.directives) {
        if (!active(directive) || directive.object_block == 0) continue;
        if (directive.name == "create_object") {
            indexes[directive.object_block] = result.size();
            result.push_back({lower(directive.arguments.front())});
            continue;
        }
        const auto found = indexes.find(directive.object_block);
        if (found == indexes.end()) continue;
        ObjectGeneration& object = result[found->second];
        const int value = directive.arguments.empty()
            ? 0 : *integer(directive.arguments.front());
        if (directive.name == "number_of_objects") object.objects = value;
        else if (directive.name == "number_of_groups") object.groups = value;
        else if (directive.name == "group_placement_radius") {
            object.group_radius = value;
        } else if (directive.name == "set_place_for_every_player") {
            object.every_player = true;
        } else if (directive.name == "set_gaia_object_only") {
            object.gaia_only = true;
        } else if (directive.name == "min_distance_to_players") {
            object.minimum_player_distance = value;
        } else if (directive.name == "max_distance_to_players") {
            object.maximum_player_distance = value;
        } else if (
            directive.name == "min_distance_group_placement" ||
            directive.name == "temp_min_distance_group_placement"
        ) {
            object.minimum_group_distance = value;
        } else if (directive.name == "set_tight_grouping") {
            object.group_radius = 1;
        } else if (directive.name == "set_loose_grouping") {
            object.group_radius = 3;
        } else if (directive.name == "resource_delta") {
            object.resource_delta = value;
        }
    }
    return result;
}

bool owner_is(const EntityOwner& owner, Player player) {
    return owner.legacy_player() == player;
}

std::optional<TilePosition> start_for(
    const Scenario& scenario, Player player
) {
    const auto found = std::ranges::find_if(
        scenario.buildings, [player](const BuildingPlacement& building) {
            return building.kind == BuildingKind::town_center &&
                owner_is(building.owner, player);
        }
    );
    if (found == scenario.buildings.end()) return std::nullopt;
    return found->position;
}

TilePosition bounded_object_position(
    const GameMap& map,
    TilePosition center,
    const ObjectGeneration& object,
    int group,
    int item,
    bool reverse
) {
    int distance = object.minimum_player_distance +
        group * std::max(1, object.minimum_group_distance);
    if (object.maximum_player_distance) {
        distance = std::min(distance, *object.maximum_player_distance);
    }
    const int diameter = object.group_radius * 2 + 1;
    const int offset_x =
        object.group_radius == 0 || object.objects == 1 ? 0 :
        item % diameter - object.group_radius;
    const int offset_y =
        object.group_radius == 0 || object.objects == 1 ? 0 :
        item / diameter % diameter - object.group_radius;
    int x = center.x + (reverse ? -distance : distance) + offset_x;
    int y = center.y + (group % 2 == 0 ? distance : -distance) + offset_y;
    x = std::clamp(x, 1, map.width() - 2);
    y = std::clamp(y, 1, map.height() - 2);
    return {x, y};
}

bool tile_has_entity(const Scenario& scenario, TilePosition position) {
    if (std::ranges::any_of(
            scenario.units,
            [position](const UnitPlacement& unit) {
                return unit.position == position;
            })) {
        return true;
    }
    return std::ranges::any_of(
        scenario.buildings,
        [position](const BuildingPlacement& building) {
            const BuildingRules& rules = rules_for(building.kind);
            return position.x >= building.position.x &&
                position.x <
                    building.position.x + rules.footprint_width &&
                position.y >= building.position.y &&
                position.y <
                    building.position.y + rules.footprint_height;
        }
    );
}

TilePosition nearest_available_tile(
    const Scenario& scenario, TilePosition intended, bool require_walkable
) {
    const int limit = std::max(
        scenario.map.width(), scenario.map.height()
    );
    for (int radius = 0; radius < limit; ++radius) {
        for (int y = -radius; y <= radius; ++y) {
            for (int x = -radius; x <= radius; ++x) {
                if (radius != 0 &&
                    std::max(std::abs(x), std::abs(y)) != radius) {
                    continue;
                }
                const TilePosition candidate{
                    intended.x + x, intended.y + y
                };
                if (scenario.map.contains(candidate) &&
                    (!require_walkable ||
                     scenario.map.walkable(candidate)) &&
                    !tile_has_entity(scenario, candidate)) {
                    return candidate;
                }
            }
        }
    }
    return intended;
}

void apply_object_generations(
    Scenario& scenario,
    const std::vector<ObjectGeneration>& objects
) {
    const auto blue_start = start_for(scenario, Player::blue);
    const auto red_start = start_for(scenario, Player::red);
    if (!blue_start || !red_start) return;
    const std::array<std::pair<Player, TilePosition>, 2> players{{
        {Player::blue, *blue_start}, {Player::red, *red_start},
    }};

    for (const ObjectGeneration& object : objects) {
        const int total = object.objects * object.groups;
        const auto unit_kind = [&]() -> std::optional<UnitKind> {
            if (object.name == "villager") return UnitKind::villager;
            if (object.name == "scout") return UnitKind::scout_cavalry;
            if (object.name == "sheep") return UnitKind::sheep;
            if (object.name == "boar") return UnitKind::boar;
            if (object.name == "deer") return UnitKind::deer;
            if (object.name == "relic") return UnitKind::relic;
            return std::nullopt;
        }();
        const auto terrain_kind = [&]() -> std::optional<Terrain> {
            if (object.name == "gold") return Terrain::gold_mine;
            if (object.name == "stone") return Terrain::stone_mine;
            if (object.name == "berries") return Terrain::berry_bush;
            return std::nullopt;
        }();

        if (unit_kind) {
            scenario.units.erase(
                std::remove_if(
                    scenario.units.begin(), scenario.units.end(),
                    [&](const UnitPlacement& unit) {
                        const bool matching_kind =
                            unit.kind == *unit_kind ||
                            (object.name == "scout" &&
                             unit.kind == UnitKind::eagle_warrior);
                        if (!matching_kind) return false;
                        if (!object.every_player) {
                            return unit.owner == EntityOwner{Player::neutral};
                        }
                        return owner_is(unit.owner, Player::blue) ||
                            owner_is(unit.owner, Player::red);
                    }
                ),
                scenario.units.end()
            );
        } else if (object.name == "town_center") {
            scenario.buildings.erase(
                std::remove_if(
                    scenario.buildings.begin(), scenario.buildings.end(),
                    [](const BuildingPlacement& building) {
                        return building.kind == BuildingKind::town_center;
                    }
                ),
                scenario.buildings.end()
            );
        } else if (terrain_kind) {
            for (int y = 0; y < scenario.map.height(); ++y) {
                for (int x = 0; x < scenario.map.width(); ++x) {
                    const TilePosition tile{x, y};
                    if (scenario.map.terrain_at(tile) == *terrain_kind) {
                        scenario.map.set_terrain(tile, Terrain::grass);
                        scenario.map.set_resource_amount(tile, 0);
                    }
                }
            }
        }

        const auto place_for = [&](Player player, TilePosition center,
                                   bool reverse) {
            for (int group = 0; group < object.groups; ++group) {
                for (int item = 0; item < object.objects; ++item) {
                    TilePosition position = bounded_object_position(
                        scenario.map, center, object, group, item, reverse
                    );
                    const EntityOwner owner =
                        object.gaia_only || terrain_kind
                        ? EntityOwner{Player::neutral}
                        : EntityOwner{player};
                    if (unit_kind) {
                        position = nearest_available_tile(
                            scenario, position, true
                        );
                        UnitKind placed_kind = *unit_kind;
                        if (object.name == "scout" &&
                            (player == Player::blue ||
                             player == Player::red)) {
                            const Civilization civilization =
                                player == Player::blue
                                ? scenario.blue_civilization
                                : scenario.red_civilization;
                            if (civilization == Civilization::aztecs ||
                                civilization == Civilization::mayans) {
                                placed_kind = UnitKind::eagle_warrior;
                            }
                        }
                        scenario.units.push_back({
                            placed_kind, owner, position, std::nullopt,
                            std::nullopt, std::nullopt, std::nullopt,
                            false, {}, UnitStance::aggressive, std::nullopt,
                        });
                    } else if (object.name == "town_center") {
                        const BuildingRules& rules =
                            rules_for(BuildingKind::town_center);
                        for (int y = 0; y < rules.footprint_height; ++y) {
                            for (int x = 0; x < rules.footprint_width; ++x) {
                                const TilePosition tile{
                                    position.x + x, position.y + y
                                };
                                scenario.map.set_terrain(
                                    tile, Terrain::grass
                                );
                                scenario.map.set_resource_amount(tile, 0);
                            }
                        }
                        scenario.buildings.push_back({
                            BuildingKind::town_center, owner, position,
                            std::nullopt, std::nullopt, std::nullopt,
                        });
                    } else if (terrain_kind) {
                        position = nearest_available_tile(
                            scenario, position, false
                        );
                        scenario.map.set_terrain(position, *terrain_kind);
                        const int base = *terrain_kind == Terrain::gold_mine
                            ? 800 : *terrain_kind == Terrain::stone_mine
                            ? 700 : 125;
                        scenario.map.set_resource_amount(
                            position, std::max(0, base + object.resource_delta)
                        );
                    }
                }
            }
        };

        if (object.every_player) {
            place_for(players[0].first, players[0].second, false);
            place_for(players[1].first, players[1].second, true);
        } else {
            place_for(
                Player::neutral,
                {scenario.map.width() / 2, scenario.map.height() / 2},
                false
            );
        }
        (void)total;
    }
}

}  // namespace

bool RmsDocument::playable() const {
    return syntactically_valid &&
        std::ranges::none_of(unsupported, [](const RmsUnsupported& item) {
            return item.affects_map;
        });
}

RmsDocument parse_rms(
    std::string_view source, const RmsImportLimits& limits
) {
    RmsDocument document;
    if (source.size() > limits.maximum_bytes) {
        document.error = "RMS exceeds byte limit";
        return document;
    }
    std::vector<std::string> lines;
    std::istringstream input{std::string(source)};
    std::string line;
    while (std::getline(input, line)) {
        lines.push_back(line);
        if (lines.size() > limits.maximum_lines) {
            document.error = "RMS exceeds line limit";
            return document;
        }
    }
    std::string section;
    bool comment{};
    std::size_t tokens{};
    std::size_t nesting{};
    std::size_t random_group{};
    std::size_t random_branch{};
    std::size_t next_random_group{1};
    std::size_t next_object_block{1};
    std::size_t current_object_block{};
    std::size_t pending_object_block{};
    std::size_t object_block_depth{};
    int random_weight{100};
    for (std::size_t index = 0; index < lines.size(); ++index) {
        std::string cleaned;
        const std::string& original = lines[index];
        for (std::size_t column = 0; column < original.size();) {
            if (!comment && column + 1 < original.size() &&
                original[column] == '/' &&
                original[column + 1] == '*') {
                comment = true;
                column += 2;
            } else if (comment && column + 1 < original.size() &&
                       original[column] == '*' &&
                       original[column + 1] == '/') {
                comment = false;
                column += 2;
            } else if (!comment && column + 1 < original.size() &&
                       original[column] == '/' &&
                       original[column + 1] == '/') {
                break;
            } else {
                if (!comment) cleaned.push_back(original[column]);
                ++column;
            }
        }
        cleaned = trim(std::move(cleaned));
        if (cleaned.empty()) continue;
        if (cleaned.front() == '<' && cleaned.back() == '>') {
            if (current_object_block != 0 || pending_object_block != 0) {
                document.error = "section begins inside create_object block";
                return document;
            }
            section = lower(cleaned.substr(1, cleaned.size() - 2));
            if (!supported_section(section)) {
                document.unsupported.push_back({
                    {index + 1, index + 1}, original,
                    "unsupported section", true,
                });
            }
            continue;
        }
        if (cleaned == "{") {
            if (++nesting > limits.maximum_nesting) {
                document.error = "RMS exceeds nesting limit";
                return document;
            }
            if (pending_object_block != 0) {
                current_object_block = pending_object_block;
                pending_object_block = 0;
                object_block_depth = nesting;
            }
            continue;
        }
        if (cleaned == "}") {
            if (nesting == 0) {
                document.error = "unmatched closing brace";
                return document;
            }
            if (current_object_block != 0 &&
                nesting == object_block_depth) {
                current_object_block = 0;
                object_block_depth = 0;
            }
            --nesting;
            continue;
        }
        if (pending_object_block != 0) {
            document.error = "create_object requires a brace-delimited block";
            return document;
        }
        bool opens_block{};
        if (!cleaned.empty() && cleaned.back() == '{') {
            opens_block = true;
            cleaned.pop_back();
            cleaned = trim(std::move(cleaned));
        }
        const std::vector<std::string> parts = words(cleaned);
        tokens += parts.size();
        if (tokens > limits.maximum_tokens) {
            document.error = "RMS exceeds token limit";
            return document;
        }
        if (parts.empty()) continue;
        const std::string name = lower(parts.front());
        if (name == "start_random") {
            if (random_group != 0) {
                document.error = "nested start_random is unsupported";
                return document;
            }
            random_group = next_random_group++;
            random_branch = 0;
            random_weight = 0;
            continue;
        }
        if (name == "percent_chance") {
            if (random_group == 0 || parts.size() != 2 ||
                !integer(parts[1]) || *integer(parts[1]) < 0) {
                document.error = "invalid percent_chance";
                return document;
            }
            random_weight = *integer(parts[1]);
            ++random_branch;
            continue;
        }
        if (name == "end_random") {
            if (random_group == 0) {
                document.error = "end_random without start_random";
                return document;
            }
            random_group = 0;
            random_branch = 0;
            random_weight = 100;
            continue;
        }
        const bool include =
            name == "#include_drs" || name == "#include";
        if (!supported_directive(name)) {
            const std::size_t first = index;
            std::size_t last = index;
            bool block = opens_block;
            if (!block && index + 1 < lines.size() &&
                trim(lines[index + 1]) == "{") {
                block = true;
                last = ++index;
            }
            if (block) {
                int depth = 1;
                while (last + 1 < lines.size() && depth > 0) {
                    ++last;
                    depth += static_cast<int>(std::ranges::count(
                        lines[last], '{'
                    ));
                    depth -= static_cast<int>(std::ranges::count(
                        lines[last], '}'
                    ));
                }
                if (depth != 0) {
                    document.error = "unterminated unsupported block";
                    return document;
                }
            }
            std::string exact;
            for (std::size_t current = first; current <= last; ++current) {
                if (!exact.empty()) exact.push_back('\n');
                exact += lines[current];
            }
            document.unsupported.push_back({
                {first + 1, last + 1}, std::move(exact),
                "unsupported directive " + name, !include,
            });
            index = last;
            continue;
        }
        if (name == "create_object") {
            if (section != "objects_generation") {
                document.error = "create_object outside objects_generation";
                return document;
            }
            if (current_object_block != 0) {
                document.error = "nested create_object is unsupported";
                return document;
            }
            if (!valid_object_arity(name, parts)) {
                document.error = "invalid create_object arity";
                return document;
            }
            if (!implemented_object_name(parts[1])) {
                document.unsupported.push_back({
                    {index + 1, index + 1}, original,
                    "unsupported create_object type " + lower(parts[1]),
                    true,
                });
            }
        } else if (object_attribute(name)) {
            if (section != "objects_generation" ||
                current_object_block == 0) {
                document.error =
                    "object attribute outside create_object block";
                return document;
            }
            if (!valid_object_arity(name, parts)) {
                document.error = "invalid object attribute " + name;
                return document;
            }
            if (!implemented_object_attribute(name)) {
                document.unsupported.push_back({
                    {index + 1, index + 1}, original,
                    "unsupported create_object attribute " + name,
                    true,
                });
            }
        }
        if (opens_block) {
            if (++nesting > limits.maximum_nesting) {
                document.error = "RMS exceeds nesting limit";
                return document;
            }
        }
        RmsDirective directive;
        directive.section = section;
        directive.name = name;
        directive.span = {index + 1, index + 1};
        directive.random_group = random_group;
        directive.random_branch = random_branch;
        directive.random_weight = random_weight;
        if (name == "create_object") {
            directive.object_block = next_object_block++;
            if (opens_block) {
                current_object_block = directive.object_block;
                object_block_depth = nesting;
            } else {
                pending_object_block = directive.object_block;
            }
        } else {
            directive.object_block = current_object_block;
        }
        for (std::size_t part = 1; part < parts.size(); ++part) {
            directive.arguments.push_back(parts[part]);
        }
        document.directives.push_back(std::move(directive));
    }
    if (comment) {
        document.error = "unterminated block comment";
        return document;
    }
    if (nesting != 0) {
        document.error = "unterminated block";
        return document;
    }
    if (random_group != 0) {
        document.error = "unterminated start_random";
        return document;
    }
    if (pending_object_block != 0 || current_object_block != 0) {
        document.error = "unterminated create_object block";
        return document;
    }
    document.syntactically_valid = true;
    return document;
}

std::optional<Scenario> evaluate_rms(
    const RmsDocument& document,
    std::uint64_t seed,
    Civilization blue,
    Civilization red
) {
    if (!document.playable()) return std::nullopt;
    std::unordered_map<std::size_t, std::size_t> selected;
    for (const RmsDirective& directive : document.directives) {
        if (directive.random_group == 0 ||
            selected.contains(directive.random_group)) continue;
        std::vector<std::pair<std::size_t, int>> branches;
        for (const RmsDirective& candidate : document.directives) {
            if (candidate.random_group != directive.random_group) continue;
            const auto existing = std::ranges::find_if(
                branches, [&candidate](const auto& branch) {
                    return branch.first == candidate.random_branch;
                }
            );
            if (existing == branches.end()) {
                branches.emplace_back(
                    candidate.random_branch,
                    std::max(0, candidate.random_weight)
                );
            }
        }
        int total{};
        for (const auto& branch : branches) total += branch.second;
        int choice = total > 0
            ? static_cast<int>(mix(seed ^ directive.random_group) % total)
            : 0;
        selected[directive.random_group] =
            branches.empty() ? 0 : branches.front().first;
        for (const auto& branch : branches) {
            if (choice < branch.second) {
                selected[directive.random_group] = branch.first;
                break;
            }
            choice -= branch.second;
        }
    }
    RandomMapKind kind = RandomMapKind::arabia;
    // No override_map_size directive means the script inherits the
    // lobby-selected size, so track the shared default.
    RandomMapSize size = RandomMapSettings{}.size;
    bool water{};
    bool forest_base{};
    bool connection{};
    bool shallows{};
    const auto active = [&](const RmsDirective& directive) {
        if (directive.random_group == 0) return true;
        return selected[directive.random_group] ==
            directive.random_branch;
    };
    for (const RmsDirective& directive : document.directives) {
        if (!active(directive)) continue;
        if (directive.name == "override_map_size" &&
            !directive.arguments.empty()) {
            if (const auto value = integer(directive.arguments.front())) {
                // Snap the requested tile count up to the nearest
                // original preset; oversized requests clamp to the
                // engine maximum.
                size = *value <= 120 ? RandomMapSize::tiny :
                       *value <= 144 ? RandomMapSize::small :
                       *value <= 168 ? RandomMapSize::medium :
                       *value <= 200 ? RandomMapSize::normal :
                       *value <= 220 ? RandomMapSize::large :
                       *value <= 240 ? RandomMapSize::giant :
                                       RandomMapSize::maximum;
            }
        }
        if ((directive.name == "base_terrain" ||
             directive.name == "terrain_type") &&
            !directive.arguments.empty()) {
            const std::string terrain = lower(directive.arguments.front());
            water = water || terrain.find("water") != std::string::npos;
            forest_base = forest_base ||
                terrain.find("forest") != std::string::npos ||
                terrain == "bamboo" || terrain == "pine_forest";
            shallows = shallows ||
                terrain.find("shallows") != std::string::npos;
        }
        connection = connection ||
            directive.name.starts_with("create_connect_");
    }
    if (water && connection) kind = RandomMapKind::rivers;
    else if (water) kind = RandomMapKind::islands;
    else if (forest_base) kind = RandomMapKind::black_forest;
    else if (shallows || connection) kind = RandomMapKind::rivers;
    Scenario scenario = generate_random_map({kind, size, seed, blue, red});
    apply_object_generations(
        scenario, object_generations(document, active)
    );
    return scenario;
}

}  // namespace aoe
