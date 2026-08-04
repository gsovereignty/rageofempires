#include "aoe/rms_import.hpp"
#include "aoe/game_rules.hpp"
#include "aoe/legacy_assets.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <cmath>
#include <fstream>
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

std::string without_block_comments(
    std::string_view line, bool& in_comment
) {
    std::string result;
    for (std::size_t column = 0; column < line.size();) {
        if (!in_comment && column + 1 < line.size() &&
            line[column] == '/' && line[column + 1] == '*') {
            in_comment = true;
            column += 2;
        } else if (in_comment && column + 1 < line.size() &&
                   line[column] == '*' && line[column + 1] == '/') {
            in_comment = false;
            column += 2;
        } else {
            if (!in_comment) result.push_back(line[column]);
            ++column;
        }
    }
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
        "assign_to_player", "set_zone_by_team", "land_position",
        "create_terrain",
        "number_of_clumps", "number_of_tiles", "clumping_factor",
        "spacing_to_other_terrain_types", "set_avoid_player_start_areas",
        "set_scale_by_groups", "set_scale_by_size", "create_elevation",
        "spacing",
        "create_cliffs", "min_number_of_cliffs", "max_number_of_cliffs",
        "min_length_of_cliff", "max_length_of_cliff",
        "cliff_curliness", "min_distance_cliffs",
        "min_terrain_distance", "create_connect_all_lands",
        "create_connect_all_players_land",
        "create_connect_teams_lands", "create_connect_same_land_zones",
        "create_connect_land_zones", "create_connect_to_nonplayer_land",
        "default_terrain_replacement", "replace_terrain", "terrain_cost",
        "terrain_size",
        "create_object", "number_of_objects", "number_of_groups",
        "group_variance", "group_placement_radius",
        "set_place_for_every_player", "min_distance_to_players",
        "max_distance_to_players", "min_distance_group_placement",
        "temp_min_distance_group_placement",
        "max_distance_to_other_zones", "set_tight_grouping",
        "set_loose_grouping", "resource_delta", "second_object",
        "set_gaia_object_only", "set_gaia_unconvertible",
        "set_scaling_to_map_size", "set_flat_terrain_only",
        "land_id", "place_on_specific_land_id",
        "#define",
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
        "set_gaia_unconvertible", "set_scaling_to_map_size",
        "place_on_specific_land_id",
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
        name == "set_gaia_unconvertible" ||
        name == "set_scaling_to_map_size") {
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
        "number_of_objects", "number_of_groups", "group_variance",
        "group_placement_radius", "set_place_for_every_player",
        "min_distance_to_players", "max_distance_to_players",
        "min_distance_group_placement",
        "temp_min_distance_group_placement", "set_tight_grouping",
        "set_loose_grouping", "resource_delta", "second_object",
        "set_gaia_object_only", "set_scaling_to_map_size",
        "place_on_specific_land_id", "max_distance_to_other_zones",
        "set_gaia_unconvertible",
    };
    return names.contains(name);
}

bool implemented_object_name(const std::string& text) {
    static const std::set<std::string> names{
        "town_center", "villager", "scout", "gold", "stone",
        "berries", "forage", "sheep", "turkey", "boar", "javelina",
        "deer", "relic", "king", "castle", "wolf", "jaguar",
        "hawk", "macaw", "oaktree", "pinetree", "snowpinetree",
        "palmtree", "bamboo_tree", "jungletree", "shore_fish",
        "salmon", "tuna", "snapper", "marlin1", "marlin2", "dorado",
    };
    return names.contains(lower(text));
}

class RmsRandom {
public:
    explicit RmsRandom(std::uint64_t seed)
        : state_(static_cast<std::uint32_t>(seed)) {}

    int next() {
        state_ = state_ * 214013U + 2531011U;
        return static_cast<int>((state_ >> 16U) & 0x7fffU);
    }

    int between(int low, int high) {
        if (high <= low) return low;
        return low + next() * (high - low + 1) / 32768;
    }

private:
    std::uint32_t state_;
};

std::optional<Terrain> rms_terrain(std::string text) {
    text = lower(std::move(text));
    if (text == "grass" || text == "grass1" || text == "farm") return Terrain::grass;
    if (text == "grass2") return Terrain::grass2;
    if (text == "grass3" || text == "leaves") return Terrain::grass2;
    if (text == "dirt" || text == "desert" || text == "palm_desert") return Terrain::dirt;
    if (text == "dirt2") return Terrain::dirt2;
    if (text == "dirt3") return Terrain::dirt3;
    if (text == "road") return Terrain::road;
    if (text == "snow" || text == "grass_snow") return Terrain::snow;
    if (text == "ice") return Terrain::ice;
    if (text == "water") return Terrain::water;
    if (text == "med_water") return Terrain::water;
    if (text == "water_deep" || text == "deep_water" ||
        text == "ocean") return Terrain::deep_water;
    if (text == "shallows" || text == "shallow") return Terrain::shallows;
    if (text == "beach") return Terrain::beach;
    if (text == "forest") return Terrain::forest;
    if (text == "pine_forest" || text == "snow_forest") return Terrain::pine_forest;
    if (text == "oak_forest") return Terrain::oak_forest;
    if (text == "bamboo" || text == "bamboo_forest") return Terrain::bamboo_forest;
    if (text == "palm_forest") return Terrain::palm_forest;
    if (text == "jungle" || text == "jungle_forest") return Terrain::jungle_forest;
    return std::nullopt;
}

void fill_map(GameMap& map, Terrain terrain) {
    for (int y = 0; y < map.height(); ++y) {
        for (int x = 0; x < map.width(); ++x) {
            map.set_terrain({x, y}, terrain);
            map.set_resource_amount({x, y}, 0);
            map.set_elevation({x, y}, 0);
        }
    }
}

void paint_disc(
    GameMap& map, TilePosition center, int radius, Terrain terrain
) {
    for (int y = center.y - radius; y <= center.y + radius; ++y) {
        for (int x = center.x - radius; x <= center.x + radius; ++x) {
            const TilePosition tile{x, y};
            if (!map.contains(tile)) continue;
            const int dx = x - center.x;
            const int dy = y - center.y;
            if (dx * dx + dy * dy <= radius * radius) {
                map.set_terrain(tile, terrain);
                map.set_resource_amount(tile, 0);
            }
        }
    }
}

void paint_connection(
    GameMap& map, TilePosition from, TilePosition to, int radius,
    Terrain fallback,
    const std::vector<std::pair<Terrain, Terrain>>& replacements
) {
    const int steps = std::max(
        std::abs(to.x - from.x), std::abs(to.y - from.y)
    );
    for (int step = 0; step <= steps; ++step) {
        const double fraction = steps == 0
            ? 0.0 : static_cast<double>(step) / steps;
        const TilePosition center{
            static_cast<int>(std::lround(
                from.x + (to.x - from.x) * fraction
            )),
            static_cast<int>(std::lround(
                from.y + (to.y - from.y) * fraction
            )),
        };
        for (int y = center.y - radius; y <= center.y + radius; ++y) {
            for (int x = center.x - radius; x <= center.x + radius; ++x) {
                const TilePosition tile{x, y};
                if (!map.contains(tile)) continue;
                const int dx = x - center.x;
                const int dy = y - center.y;
                if (dx * dx + dy * dy > radius * radius) continue;
                Terrain replacement = fallback;
                const Terrain existing = map.terrain_at(tile);
                const auto rule = std::ranges::find_if(
                    replacements, [existing](const auto& candidate) {
                        return candidate.first == existing;
                    }
                );
                if (rule != replacements.end()) replacement = rule->second;
                map.set_terrain(tile, replacement);
                map.set_resource_amount(tile, 0);
            }
        }
    }
}

int directive_value(const RmsDirective& directive, int fallback) {
    if (directive.arguments.empty()) return fallback;
    const auto value = integer(directive.arguments.front());
    return value ? *value : fallback;
}

struct LandGeneration {
    Terrain terrain{Terrain::grass};
    int percent{100};
    int tiles{};
    int base_size{8};
    int left_border{};
    int right_border{};
    int top_border{};
    int bottom_border{};
    int zone{-10};
    int id{-1};
    std::optional<TilePosition> position_percent;
    std::optional<int> player;
    bool player_lands{};
};

struct TerrainGeneration {
    enum class Scale { none, size, groups };

    Terrain terrain{Terrain::grass};
    std::optional<Terrain> base;
    int clumps{1};
    int tiles{};
    int percent{};
    int clumping{8};
    int spacing{};
    Scale scale{Scale::none};
    bool avoid_starts{};
    bool flat_only{};
};

struct ElevationGeneration {
    enum class Scale { none, size, groups };

    std::optional<Terrain> base;
    int height{};
    int clumps{1};
    int tiles{20};
    int spacing{1};
    Scale scale{Scale::none};
};

struct CliffGeneration {
    int minimum_count{};
    int maximum_count{};
    int minimum_length{3};
    int maximum_length{6};
    int curliness{};
    int minimum_spacing{};
    int minimum_terrain_distance{};
};

bool cliff_forbidden_terrain(Terrain terrain) {
    switch (terrain) {
        case Terrain::water:
        case Terrain::deep_water:
        case Terrain::shallows:
        case Terrain::beach:
        case Terrain::forest:
        case Terrain::pine_forest:
        case Terrain::oak_forest:
        case Terrain::bamboo_forest:
        case Terrain::palm_forest:
        case Terrain::jungle_forest:
        case Terrain::berry_bush:
        case Terrain::gold_mine:
        case Terrain::stone_mine:
        case Terrain::fish:
        case Terrain::fish_shore:
        case Terrain::fish_deep:
            return true;
        case Terrain::grass:
        case Terrain::grass2:
        case Terrain::dirt:
        case Terrain::dirt2:
        case Terrain::dirt3:
        case Terrain::road:
        case Terrain::snow:
        case Terrain::ice:
            return false;
    }
    return true;
}

template <typename Active>
std::optional<CliffGeneration> cliff_generation(
    const RmsDocument& document, const Active& active
) {
    std::optional<CliffGeneration> result;
    for (const RmsDirective& directive : document.directives) {
        if (!active(directive) ||
            directive.section != "cliff_generation") continue;
        if (directive.name == "create_cliffs") {
            result.emplace();
            continue;
        }
        if (!result) continue;
        const int value = directive_value(directive, 0);
        if (directive.name == "min_number_of_cliffs") {
            result->minimum_count = std::max(0, value);
        } else if (directive.name == "max_number_of_cliffs") {
            result->maximum_count = std::max(0, value);
        } else if (directive.name == "min_length_of_cliff") {
            result->minimum_length = std::max(1, value);
        } else if (directive.name == "max_length_of_cliff") {
            result->maximum_length = std::max(1, value);
        } else if (directive.name == "cliff_curliness") {
            result->curliness = std::clamp(value, 0, 100);
        } else if (directive.name == "min_distance_cliffs") {
            result->minimum_spacing = std::max(0, value);
        } else if (directive.name == "min_terrain_distance") {
            result->minimum_terrain_distance = std::max(0, value);
        }
    }
    if (result) {
        result->maximum_count = std::max(
            result->minimum_count, result->maximum_count
        );
        result->maximum_length = std::max(
            result->minimum_length, result->maximum_length
        );
    }
    return result;
}

struct ConnectionGeneration {
    enum class Kind {
        all_players,
        teams,
        all_lands,
        same_zones,
        explicit_zones,
        nonplayer,
    };

    Kind kind{Kind::all_lands};
    Terrain terrain{Terrain::grass};
    int width{1};
    std::optional<int> zone_one;
    std::optional<int> zone_two;
    std::vector<std::pair<Terrain, Terrain>> replacements;
};

struct LandSite {
    TilePosition origin;
    std::optional<Player> player;
    int zone{-10};
    int id{-1};
};

template <typename Active>
std::vector<LandGeneration> land_generations(
    const RmsDocument& document, const Active& active
) {
    std::vector<LandGeneration> result;
    LandGeneration* current{};
    for (const RmsDirective& directive : document.directives) {
        if (!active(directive) ||
            directive.section != "land_generation") continue;
        if (directive.name == "create_player_lands" ||
            directive.name == "create_land") {
            result.push_back({});
            current = &result.back();
            current->player_lands =
                directive.name == "create_player_lands";
            continue;
        }
        if (!current) continue;
        if (directive.name == "terrain_type" &&
            !directive.arguments.empty()) {
            if (const auto terrain = rms_terrain(
                    directive.arguments.front())) current->terrain = *terrain;
        } else if (directive.name == "land_percent") {
            current->percent = std::max(0, directive_value(directive, 100));
        } else if (directive.name == "number_of_tiles") {
            current->tiles = std::max(0, directive_value(directive, 0));
        } else if (directive.name == "base_size") {
            current->base_size = std::max(1, directive_value(directive, 8));
        } else if (directive.name == "left_border") {
            current->left_border = std::clamp(
                directive_value(directive, 0), 0, 49
            );
        } else if (directive.name == "right_border") {
            current->right_border = std::clamp(
                directive_value(directive, 0), 0, 49
            );
        } else if (directive.name == "top_border") {
            current->top_border = std::clamp(
                directive_value(directive, 0), 0, 49
            );
        } else if (directive.name == "bottom_border") {
            current->bottom_border = std::clamp(
                directive_value(directive, 0), 0, 49
            );
        } else if (directive.name == "assign_to_player") {
            current->player = directive_value(directive, 0);
        } else if (directive.name == "zone") {
            current->zone = directive_value(directive, -10);
        } else if (directive.name == "land_id") {
            current->id = directive_value(directive, -1);
        } else if (directive.name == "land_position" &&
                   directive.arguments.size() >= 2) {
            const auto x = integer(directive.arguments[0]);
            const auto y = integer(directive.arguments[1]);
            if (x && y) {
                current->position_percent = TilePosition{
                    std::clamp(*x, 0, 100),
                    std::clamp(*y, 0, 100),
                };
            }
        }
    }
    return result;
}

template <typename Active>
std::vector<TerrainGeneration> terrain_generations(
    const RmsDocument& document, const Active& active
) {
    std::vector<TerrainGeneration> result;
    TerrainGeneration* current{};
    for (const RmsDirective& directive : document.directives) {
        if (!active(directive) ||
            directive.section != "terrain_generation") continue;
        if (directive.name == "create_terrain") {
            result.push_back({});
            current = &result.back();
            if (!directive.arguments.empty()) {
                if (const auto terrain = rms_terrain(
                        directive.arguments.front())) {
                    current->terrain = *terrain;
                }
            }
            continue;
        }
        if (!current) continue;
        if (directive.name == "base_terrain" &&
            !directive.arguments.empty()) {
            current->base = rms_terrain(directive.arguments.front());
        } else if (directive.name == "number_of_clumps") {
            current->clumps = std::max(1, directive_value(directive, 1));
        } else if (directive.name == "number_of_tiles") {
            current->tiles = std::max(0, directive_value(directive, 0));
        } else if (directive.name == "land_percent") {
            current->percent = std::max(0, directive_value(directive, 0));
        } else if (directive.name == "clumping_factor") {
            current->clumping = std::max(1, directive_value(directive, 8));
        } else if (
            directive.name == "spacing_to_other_terrain_types"
        ) {
            current->spacing = std::max(
                0, directive_value(directive, 0)
            );
        } else if (directive.name == "set_scale_by_size") {
            current->scale = TerrainGeneration::Scale::size;
        } else if (directive.name == "set_scale_by_groups") {
            current->scale = TerrainGeneration::Scale::groups;
        } else if (directive.name == "set_avoid_player_start_areas") {
            current->avoid_starts = true;
        } else if (directive.name == "set_flat_terrain_only") {
            current->flat_only = true;
        }
    }
    return result;
}

template <typename Active>
std::vector<ElevationGeneration> elevation_generations(
    const RmsDocument& document, const Active& active
) {
    std::vector<ElevationGeneration> result;
    ElevationGeneration* current{};
    for (const RmsDirective& directive : document.directives) {
        if (!active(directive) ||
            directive.section != "elevation_generation") continue;
        if (directive.name == "create_elevation") {
            result.push_back({});
            current = &result.back();
            current->height = std::clamp(
                directive_value(directive, 1), 0, 255
            );
            continue;
        }
        if (!current) continue;
        if (directive.name == "base_terrain" &&
            !directive.arguments.empty()) {
            current->base = rms_terrain(directive.arguments.front());
        } else if (directive.name == "number_of_clumps") {
            current->clumps = std::max(1, directive_value(directive, 1));
        } else if (directive.name == "number_of_tiles") {
            current->tiles = std::max(1, directive_value(directive, 20));
        } else if (directive.name == "spacing") {
            current->spacing = std::max(1, directive_value(directive, 1));
        } else if (directive.name == "set_scale_by_size") {
            current->scale = ElevationGeneration::Scale::size;
        } else if (directive.name == "set_scale_by_groups") {
            current->scale = ElevationGeneration::Scale::groups;
        }
    }
    return result;
}

template <typename Active>
std::vector<ConnectionGeneration> connection_generations(
    const RmsDocument& document, const Active& active
) {
    std::vector<ConnectionGeneration> result;
    ConnectionGeneration* current{};
    for (const RmsDirective& directive : document.directives) {
        if (!active(directive) ||
            directive.section != "connection_generation") continue;
        if (directive.name.starts_with("create_connect_")) {
            result.push_back({});
            current = &result.back();
            if (directive.name == "create_connect_all_players_land") {
                current->kind = ConnectionGeneration::Kind::all_players;
            } else if (directive.name == "create_connect_teams_lands") {
                current->kind = ConnectionGeneration::Kind::teams;
            } else if (directive.name == "create_connect_same_land_zones") {
                current->kind = ConnectionGeneration::Kind::same_zones;
            } else if (directive.name == "create_connect_land_zones") {
                current->kind = ConnectionGeneration::Kind::explicit_zones;
                if (directive.arguments.size() >= 2) {
                    current->zone_one = integer(directive.arguments[0]);
                    current->zone_two = integer(directive.arguments[1]);
                }
            } else if (
                directive.name == "create_connect_to_nonplayer_land"
            ) {
                current->kind = ConnectionGeneration::Kind::nonplayer;
            } else {
                current->kind = ConnectionGeneration::Kind::all_lands;
            }
            continue;
        }
        if (!current) continue;
        if ((directive.name == "terrain_type" ||
             directive.name == "default_terrain_replacement") &&
            !directive.arguments.empty()) {
            if (const auto terrain = rms_terrain(
                    directive.arguments.front())) current->terrain = *terrain;
        } else if (directive.name == "replace_terrain" &&
                   directive.arguments.size() >= 2) {
            const auto source = rms_terrain(directive.arguments[0]);
            const auto replacement = rms_terrain(directive.arguments[1]);
            if (source && replacement) {
                current->replacements.emplace_back(*source, *replacement);
            }
        } else if (directive.name == "terrain_size") {
            current->width = std::max(
                1, directive_value(directive, 1) / 2
            );
        }
    }
    return result;
}

struct ObjectGeneration {
    std::string name;
    std::optional<std::string> second_name;
    int objects{1};
    int groups{1};
    int group_variance{};
    int group_radius{2};
    int minimum_player_distance{};
    std::optional<int> maximum_player_distance;
    int minimum_group_distance{};
    int resource_delta{};
    bool every_player{};
    bool gaia_only{};
    bool scale_to_map_size{};
    std::optional<int> land_id;
    std::optional<int> maximum_other_zone_distance;
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
            ObjectGeneration object;
            object.name = lower(directive.arguments.front());
            result.push_back(std::move(object));
            continue;
        }
        const auto found = indexes.find(directive.object_block);
        if (found == indexes.end()) continue;
        ObjectGeneration& object = result[found->second];
        const int value = directive.arguments.empty()
            ? 0 : *integer(directive.arguments.front());
        if (directive.name == "number_of_objects") object.objects = value;
        else if (directive.name == "number_of_groups") object.groups = value;
        else if (directive.name == "group_variance") {
            object.group_variance = value;
        }
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
        } else if (directive.name == "second_object") {
            object.second_name = lower(directive.arguments.front());
        } else if (directive.name == "set_scaling_to_map_size") {
            object.scale_to_map_size = true;
        } else if (directive.name == "place_on_specific_land_id") {
            object.land_id = value;
        } else if (directive.name == "max_distance_to_other_zones") {
            object.maximum_other_zone_distance = value;
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
    int items_in_group,
    int random_x,
    int random_y,
    bool reverse
) {
    int distance = object.minimum_player_distance +
        group * std::max(1, object.minimum_group_distance);
    if (object.maximum_player_distance) {
        distance = std::min(distance, *object.maximum_player_distance);
    }
    const int diameter = object.group_radius * 2 + 1;
    const int offset_x =
        object.group_radius == 0 || items_in_group == 1 ? 0 :
        std::clamp(
            item % diameter - object.group_radius + random_x,
            -object.group_radius, object.group_radius
        );
    const int offset_y =
        object.group_radius == 0 || items_in_group == 1 ? 0 :
        std::clamp(
            item / diameter % diameter - object.group_radius + random_y,
            -object.group_radius, object.group_radius
        );
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
    const std::vector<ObjectGeneration>& objects,
    const std::vector<LandSite>& land_sites,
    RmsRandom& random
) {
    const auto blue_start = start_for(scenario, Player::blue);
    const auto red_start = start_for(scenario, Player::red);
    if (!blue_start || !red_start) return;
    const std::array<std::pair<Player, TilePosition>, 2> players{{
        {Player::blue, *blue_start}, {Player::red, *red_start},
    }};

    for (const ObjectGeneration& source_object : objects) {
        ObjectGeneration effective = source_object;
        if (effective.scale_to_map_size) {
            const double scale = static_cast<double>(
                scenario.map.width() * scenario.map.height()
            ) / 10000.0;
            effective.objects = std::max(
                1, static_cast<int>(std::lround(effective.objects * scale))
            );
        }
        const ObjectGeneration& object = effective;
        const int total = object.objects * object.groups;
        const auto unit_kind = [&]() -> std::optional<UnitKind> {
            if (object.name == "villager") return UnitKind::villager;
            if (object.name == "scout") return UnitKind::scout_cavalry;
            if (object.name == "sheep" || object.name == "turkey") return UnitKind::sheep;
            if (object.name == "boar" || object.name == "javelina" ||
                object.name == "wolf" || object.name == "jaguar") return UnitKind::boar;
            if (object.name == "deer") return UnitKind::deer;
            if (object.name == "relic") return UnitKind::relic;
            if (object.name == "king") return UnitKind::king;
            if (object.name == "hawk" || object.name == "macaw") return UnitKind::deer;
            return std::nullopt;
        }();
        const auto terrain_kind = [&]() -> std::optional<Terrain> {
            if (object.name == "gold") return Terrain::gold_mine;
            if (object.name == "stone") return Terrain::stone_mine;
            if (object.name == "berries" || object.name == "forage") return Terrain::berry_bush;
            if (object.name == "oaktree") return Terrain::oak_forest;
            if (object.name == "pinetree" || object.name == "snowpinetree") return Terrain::pine_forest;
            if (object.name == "palmtree") return Terrain::palm_forest;
            if (object.name == "bamboo_tree") return Terrain::bamboo_forest;
            if (object.name == "jungletree") return Terrain::jungle_forest;
            if (object.name == "shore_fish" || object.name == "salmon") return Terrain::fish_shore;
            if (object.name == "tuna" || object.name == "snapper" ||
                object.name == "marlin1" || object.name == "marlin2" ||
                object.name == "dorado") return Terrain::fish_deep;
            return std::nullopt;
        }();
        const auto second_unit_kind = [&]() -> std::optional<UnitKind> {
            if (!object.second_name) return std::nullopt;
            if (*object.second_name == "villager") return UnitKind::villager;
            if (*object.second_name == "scout") {
                return UnitKind::scout_cavalry;
            }
            if (*object.second_name == "sheep") return UnitKind::sheep;
            if (*object.second_name == "boar") return UnitKind::boar;
            if (*object.second_name == "deer") return UnitKind::deer;
            if (*object.second_name == "relic") return UnitKind::relic;
            return std::nullopt;
        }();
        const auto second_terrain_kind = [&]() -> std::optional<Terrain> {
            if (!object.second_name) return std::nullopt;
            if (*object.second_name == "gold") return Terrain::gold_mine;
            if (*object.second_name == "stone") return Terrain::stone_mine;
            if (*object.second_name == "berries") return Terrain::berry_bush;
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
        } else if (object.name == "town_center" || object.name == "castle") {
            scenario.buildings.erase(
                std::remove_if(
                    scenario.buildings.begin(), scenario.buildings.end(),
                    [&](const BuildingPlacement& building) {
                        return building.kind == (object.name == "castle"
                            ? BuildingKind::castle : BuildingKind::town_center);
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
            std::vector<TilePosition> group_centers;
            for (int group = 0; group < object.groups; ++group) {
                TilePosition group_center = center;
                ObjectGeneration positioned = object;
                if (player == Player::neutral) {
                    bool found{};
                    for (int attempt = 0; attempt < 256; ++attempt) {
                        const TilePosition candidate{
                            random.between(
                                object.group_radius + 1,
                                scenario.map.width() -
                                    object.group_radius - 2
                            ),
                            random.between(
                                object.group_radius + 1,
                                scenario.map.height() -
                                    object.group_radius - 2
                            ),
                        };
                        const int blue_distance =
                            std::abs(candidate.x - blue_start->x) +
                            std::abs(candidate.y - blue_start->y);
                        const int red_distance =
                            std::abs(candidate.x - red_start->x) +
                            std::abs(candidate.y - red_start->y);
                        const int nearest_player =
                            std::min(blue_distance, red_distance);
                        if (nearest_player <
                            object.minimum_player_distance) continue;
                        if (object.maximum_player_distance &&
                            nearest_player >
                                *object.maximum_player_distance) continue;
                        if (object.maximum_other_zone_distance &&
                            std::ranges::none_of(
                                land_sites,
                                [&](const LandSite& site) {
                                    return std::abs(candidate.x - site.origin.x) +
                                        std::abs(candidate.y - site.origin.y) <=
                                        *object.maximum_other_zone_distance;
                                })) continue;
                        const bool separated = std::ranges::all_of(
                            group_centers,
                            [&](TilePosition existing) {
                                return std::abs(
                                    candidate.x - existing.x
                                ) + std::abs(
                                    candidate.y - existing.y
                                ) >= object.minimum_group_distance;
                            }
                        );
                        if (!separated) continue;
                        group_center = candidate;
                        found = true;
                        break;
                    }
                    if (!found) continue;
                    group_centers.push_back(group_center);
                    positioned.minimum_player_distance = 0;
                    positioned.maximum_player_distance.reset();
                    positioned.minimum_group_distance = 0;
                }
                const int variance = object.group_variance == 0 ? 0 :
                    random.between(
                        -object.group_variance,
                        object.group_variance - 1
                    );
                const int items = object.group_variance == 0
                    ? object.objects
                    : std::max(1, object.objects + variance);
                for (int item = 0; item < items; ++item) {
                    const int random_x = object.group_radius == 0 ? 0 :
                        random.between(
                            -object.group_radius, object.group_radius
                        );
                    const int random_y = object.group_radius == 0 ? 0 :
                        random.between(
                            -object.group_radius, object.group_radius
                    );
                    TilePosition position = bounded_object_position(
                        scenario.map, group_center, positioned,
                        player == Player::neutral ? 0 : group,
                        item, items,
                        random_x, random_y, reverse
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
                    } else if (object.name == "town_center" || object.name == "castle") {
                        const BuildingKind building_kind = object.name == "castle"
                            ? BuildingKind::castle : BuildingKind::town_center;
                        const BuildingRules& rules =
                            rules_for(building_kind);
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
                            building_kind, owner, position,
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
                    if (second_unit_kind) {
                        scenario.units.push_back({
                            *second_unit_kind, owner, position,
                            std::nullopt, std::nullopt,
                            std::nullopt, std::nullopt, false, {},
                            UnitStance::aggressive, std::nullopt,
                        });
                    } else if (second_terrain_kind) {
                        scenario.map.set_terrain(position, *second_terrain_kind);
                        const int amount =
                            *second_terrain_kind == Terrain::gold_mine ? 800 :
                            *second_terrain_kind == Terrain::stone_mine ? 700 :
                                                                        125;
                        scenario.map.set_resource_amount(position, amount);
                    } else if (
                        object.second_name &&
                        *object.second_name == "town_center"
                    ) {
                        scenario.buildings.push_back({
                            BuildingKind::town_center, owner, position,
                            std::nullopt, std::nullopt, std::nullopt,
                        });
                    }
                }
            }
        };

        if (object.every_player) {
            place_for(players[0].first, players[0].second, false);
            place_for(players[1].first, players[1].second, true);
        } else {
            TilePosition neutral_center{
                scenario.map.width() / 2, scenario.map.height() / 2
            };
            if (object.land_id) {
                const auto site = std::ranges::find_if(
                    land_sites, [&](const LandSite& candidate) {
                        return candidate.id == *object.land_id;
                    });
                if (site == land_sites.end()) continue;
                neutral_center = site->origin;
            }
            place_for(
                Player::neutral, neutral_center,
                false
            );
        }
        (void)total;
    }
}

}  // namespace

std::vector<int> msvcrt_rms_random_sequence(
    std::uint32_t seed, std::size_t count
) {
    RmsRandom random(seed);
    std::vector<int> result;
    result.reserve(count);
    while (result.size() < count) result.push_back(random.next());
    return result;
}

namespace {

RmsDocument include_failure(std::string message) {
    RmsDocument failed;
    failed.error = std::move(message);
    return failed;
}

std::optional<std::string> read_text_file(
    const std::filesystem::path& path
) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return std::nullopt;
    return std::string(
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{}
    );
}

std::optional<std::string> read_drs_include(
    const std::filesystem::path& root, int resource_id
) {
    // Expansion archives override base data, matching other legacy resource
    // lookup in this reconstruction.
    constexpr std::array<std::string_view, 3> names{
        "gamedata_x1_p1.drs", "gamedata_x1.drs", "gamedata.drs"
    };
    for (const std::string_view name : names) {
        const auto archive_path = root / "Data" / name;
        if (!std::filesystem::is_regular_file(archive_path)) continue;
        try {
            const DrsArchive archive{archive_path};
            if (!archive.contains("bina", resource_id)) continue;
            const auto bytes = archive.read("bina", resource_id);
            return std::string(
                reinterpret_cast<const char*>(bytes.data()), bytes.size()
            );
        } catch (const LegacyAssetError&) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

}  // namespace

RmsDocument parse_rms_file(
    const std::filesystem::path& path,
    const std::optional<std::filesystem::path>& installation_root,
    const RmsImportLimits& limits
) {
    std::set<std::string> active;
    std::string expansion_error;
    const auto expand = [&](auto&& self, std::string_view text,
                            const std::filesystem::path& source_path,
                            std::size_t depth) -> std::optional<std::string> {
        if (depth > limits.maximum_include_depth) {
            expansion_error = "too many nested #includes";
            return std::nullopt;
        }
        std::istringstream input{std::string(text)};
        std::ostringstream output;
        std::string line;
        std::size_t line_number{};
        bool comment{};
        while (std::getline(input, line)) {
            ++line_number;
            const auto parts = words(trim(without_block_comments(
                line, comment
            )));
            if (parts.empty() ||
                (lower(parts[0]) != "#include" &&
                 lower(parts[0]) != "#include_drs")) {
                output << line << '\n';
                continue;
            }
            const bool drs = lower(parts[0]) == "#include_drs";
            if ((!drs && parts.size() != 2) ||
                (drs && (parts.size() != 3 || !integer(parts[2])))) {
                expansion_error = "invalid RMS include at " +
                    source_path.string() + ":" +
                    std::to_string(line_number);
                return std::nullopt;
            }
            std::string identity;
            std::optional<std::string> body;
            std::filesystem::path nested_path = source_path;
            if (drs) {
                identity = "drs:" + parts[2];
                if (installation_root) {
                    body = read_drs_include(
                        *installation_root, *integer(parts[2])
                    );
                }
            } else {
                nested_path = std::filesystem::path(parts[1]);
                if (nested_path.is_relative()) {
                    nested_path = source_path.parent_path() / nested_path;
                }
                nested_path = std::filesystem::weakly_canonical(nested_path);
                identity = "file:" + nested_path.generic_string();
                body = read_text_file(nested_path);
            }
            if (!body) {
                expansion_error = "RMS include not found at " +
                    source_path.string() + ":" +
                    std::to_string(line_number) + ": " + parts[1];
                return std::nullopt;
            }
            if (active.contains(identity)) {
                expansion_error = "cyclic RMS include at " +
                    source_path.string() + ":" +
                    std::to_string(line_number) + ": " + parts[1];
                return std::nullopt;
            }
            active.insert(identity);
            const auto nested = self(
                self, *body, nested_path, depth + 1
            );
            active.erase(identity);
            if (!nested) return std::nullopt;
            output << *nested;
            if (!nested->empty() && nested->back() != '\n') output << '\n';
            if (static_cast<std::size_t>(output.tellp()) >
                limits.maximum_bytes) {
                expansion_error = "RMS exceeds byte limit";
                return std::nullopt;
            }
        }
        return output.str();
    };

    const auto canonical = std::filesystem::weakly_canonical(path);
    const auto source = read_text_file(canonical);
    if (!source) return include_failure("could not open RMS: " + path.string());
    active.insert("file:" + canonical.generic_string());
    const auto expanded = expand(expand, *source, canonical, 0);
    if (!expanded) {
        return include_failure(expansion_error.empty()
            ? "RMS include expansion failed" : expansion_error);
    }
    return parse_rms(*expanded, limits);
}

RmsDocument parse_rms(
    std::string_view source,
    const std::unordered_map<std::string, std::string>& includes,
    const RmsImportLimits& limits
) {
    std::set<std::string> active;
    std::string expansion_error;
    const auto expand = [&](auto&& self, std::string_view text,
                            std::size_t depth) -> std::optional<std::string> {
        if (depth > limits.maximum_include_depth) {
            expansion_error = "too many nested #includes";
            return std::nullopt;
        }
        std::istringstream input{std::string(text)};
        std::ostringstream output;
        std::string line;
        bool comment{};
        while (std::getline(input, line)) {
            const std::vector<std::string> parts = words(trim(
                without_block_comments(line, comment)
            ));
            if (!parts.empty() &&
                (lower(parts[0]) == "#include" ||
                 lower(parts[0]) == "#include_drs")) {
                const bool drs = lower(parts[0]) == "#include_drs";
                if ((!drs && parts.size() != 2) ||
                    (drs && (parts.size() != 3 || !integer(parts[2])))) {
                    expansion_error = "invalid RMS include";
                    return std::nullopt;
                }
                const std::string key = lower(parts[1]);
                const std::string resource_key = drs ? parts[2] : "";
                auto found = includes.end();
                std::string selected_key;
                for (auto candidate = includes.begin();
                     candidate != includes.end(); ++candidate) {
                    const std::string candidate_key = lower(candidate->first);
                    const bool numeric_match = !resource_key.empty() &&
                        candidate_key == resource_key;
                    const bool name_match = candidate_key == key;
                    if (!numeric_match && !name_match) continue;
                    if (found == includes.end() ||
                        (numeric_match && selected_key != resource_key) ||
                        (candidate_key == selected_key &&
                         candidate->first < found->first)) {
                        found = candidate;
                        selected_key = candidate_key;
                    }
                }
                const std::string identity = drs ? resource_key : key;
                if (found == includes.end() || active.contains(identity)) {
                    expansion_error = found == includes.end()
                        ? "RMS include not found: " + parts[1]
                        : "cyclic RMS include: " + parts[1];
                    return std::nullopt;
                }
                active.insert(identity);
                const auto nested = self(self, found->second, depth + 1);
                active.erase(identity);
                if (!nested) return std::nullopt;
                output << *nested;
                if (!nested->empty() && nested->back() != '\n') output << '\n';
            } else {
                output << line << '\n';
            }
            if (static_cast<std::size_t>(output.tellp()) >
                limits.maximum_bytes) {
                expansion_error = "RMS exceeds byte limit";
                return std::nullopt;
            }
        }
        return output.str();
    };
    const auto expanded = expand(expand, source, 0);
    if (!expanded) {
        RmsDocument failed;
        failed.error = expansion_error.empty()
            ? "RMS include expansion failed" : expansion_error;
        return failed;
    }
    return parse_rms(*expanded, limits);
}

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
    struct ConditionalFrame {
        std::vector<std::string> prior_symbols;
        std::optional<std::string> current_symbol;
        bool else_branch{};
    };
    std::vector<ConditionalFrame> conditions;
    std::unordered_map<std::string, std::string> constants;
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
        if (name == "#const") {
            if (parts.size() != 3 || !integer(parts[2])) {
                document.error = "invalid #const";
                return document;
            }
            constants[lower(parts[1])] = parts[2];
            continue;
        }
        if (name == "if") {
            if (parts.size() != 2) {
                document.error = "invalid if";
                return document;
            }
            if (conditions.size() >= limits.maximum_nesting) {
                document.error = "RMS exceeds nesting limit";
                return document;
            }
            conditions.push_back({{}, lower(parts[1]), false});
            continue;
        }
        if (name == "elseif") {
            if (parts.size() != 2 || conditions.empty() ||
                conditions.back().else_branch) {
                document.error = "invalid elseif";
                return document;
            }
            ConditionalFrame& frame = conditions.back();
            if (frame.current_symbol) {
                frame.prior_symbols.push_back(*frame.current_symbol);
            }
            frame.current_symbol = lower(parts[1]);
            continue;
        }
        if (name == "else") {
            if (parts.size() != 1 || conditions.empty() ||
                conditions.back().else_branch) {
                document.error = "invalid else";
                return document;
            }
            ConditionalFrame& frame = conditions.back();
            if (frame.current_symbol) {
                frame.prior_symbols.push_back(*frame.current_symbol);
            }
            frame.current_symbol.reset();
            frame.else_branch = true;
            continue;
        }
        if (name == "endif") {
            if (parts.size() != 1 || conditions.empty()) {
                document.error = "endif without if";
                return document;
            }
            conditions.pop_back();
            continue;
        }
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
                "unsupported directive " + name, true,
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
            std::vector<std::string> validated_parts = parts;
            for (std::size_t part = 2; part < validated_parts.size(); ++part) {
                if (const auto found = constants.find(lower(validated_parts[part]));
                    found != constants.end()) validated_parts[part] = found->second;
            }
            if (!valid_object_arity(name, validated_parts)) {
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
            std::vector<std::string> validated_parts = parts;
            for (std::size_t part = 1; part < validated_parts.size(); ++part) {
                if (name == "second_object") break;
                if (const auto found = constants.find(lower(validated_parts[part]));
                    found != constants.end()) validated_parts[part] = found->second;
            }
            if (!valid_object_arity(name, validated_parts)) {
                document.error = "invalid object attribute " + name;
                return document;
            }
            if (!implemented_object_attribute(name)) {
                document.unsupported.push_back({
                    {index + 1, index + 1}, original,
                    "unsupported create_object attribute " + name,
                    true,
                });
            } else if (name == "second_object" &&
                       !implemented_object_name(parts[1])) {
                document.unsupported.push_back({
                    {index + 1, index + 1}, original,
                    "unsupported second_object type " + lower(parts[1]),
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
        for (const ConditionalFrame& frame : conditions) {
            for (const std::string& symbol : frame.prior_symbols) {
                directive.conditions.emplace_back(symbol, false);
            }
            if (frame.current_symbol) {
                directive.conditions.emplace_back(
                    *frame.current_symbol, true
                );
            }
        }
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
            const auto constant = constants.find(lower(parts[part]));
            const bool semantic_name =
                name == "create_object" || name == "second_object" ||
                name == "create_terrain" || name == "terrain_type" ||
                name == "base_terrain" || name == "replace_terrain";
            directive.arguments.push_back(
                constant == constants.end() || semantic_name
                    ? parts[part] : constant->second
            );
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
    if (!conditions.empty()) {
        document.error = "unterminated if";
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
    return evaluate_rms(document, seed, RmsEvaluationContext{}, blue, red);
}

std::optional<Scenario> evaluate_rms(
    const RmsDocument& document,
    std::uint64_t seed,
    const RmsEvaluationContext& context,
    Civilization blue,
    Civilization red
) {
    if (!document.playable()) return std::nullopt;
    RmsRandom random(seed);
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
        // Classic percent_chance entries occupy a fixed 100-slot table.
        // Unclaimed remainder selects no branch; do not renormalize weights.
        int choice = random.between(1, 100);
        selected[directive.random_group] = 0;
        for (const auto& branch : branches) {
            if (choice <= branch.second) {
                selected[directive.random_group] = branch.first;
                break;
            }
            choice -= branch.second;
        }
    }
    std::set<std::string> definitions;
    for (const std::string& definition : context.definitions) {
        definitions.insert(lower(definition));
    }
    const auto add_size_definition = [&definitions](RandomMapSize size) {
        switch (size) {
            case RandomMapSize::tiny: definitions.insert("tiny_map"); break;
            case RandomMapSize::small: definitions.insert("small_map"); break;
            case RandomMapSize::medium: definitions.insert("medium_map"); break;
            case RandomMapSize::normal: definitions.insert("large_map"); break;
            case RandomMapSize::large: definitions.insert("huge_map"); break;
            case RandomMapSize::giant: definitions.insert("gigantic_map"); break;
        }
    };
    if (context.map_size) add_size_definition(*context.map_size);
    const auto random_active = [&](const RmsDirective& directive) {
        return directive.random_group == 0 ||
            selected[directive.random_group] == directive.random_branch;
    };
    const auto conditions_active = [&](const RmsDirective& directive) {
        return std::ranges::all_of(
            directive.conditions,
            [&definitions](const auto& condition) {
                return definitions.contains(condition.first) ==
                    condition.second;
            }
        );
    };
    // Defines can themselves be conditional. Iterate to a fixed point; RMS
    // symbols are monotonic because classic scripts only define, never undef.
    bool changed{};
    do {
        changed = false;
        for (const RmsDirective& directive : document.directives) {
            if (directive.name != "#define" ||
                directive.arguments.empty() ||
                !random_active(directive) ||
                !conditions_active(directive)) continue;
            changed |= definitions.insert(
                lower(directive.arguments.front())
            ).second;
        }
    } while (changed);
    // No override_map_size directive means the script inherits the
    // lobby-selected size, so track the shared default.
    int dimension = random_map_dimension(
        context.map_size.value_or(RandomMapSettings{}.size)
    );
    const auto active = [&](const RmsDirective& directive) {
        return random_active(directive) && conditions_active(directive) &&
            directive.name != "#define";
    };
    for (const RmsDirective& directive : document.directives) {
        if (!active(directive)) continue;
        if (directive.name == "override_map_size" &&
            !directive.arguments.empty()) {
            if (const auto value = integer(directive.arguments.front())) {
                // Snap the requested tile count up to the nearest
                // original preset; oversized requests clamp to the
                // engine maximum.
                dimension = *value <= 120 ? 120 :
                            *value <= 144 ? 144 :
                            *value <= 168 ? 168 :
                            *value <= 200 ? 200 :
                            *value <= 220 ? 220 :
                            *value <= 240 ? 240 : 255;
            }
        }
    }

    Scenario scenario(dimension, dimension);
    scenario.blue_civilization = blue;
    scenario.red_civilization = red;
    Terrain base = Terrain::grass;
    for (const RmsDirective& directive : document.directives) {
        if (!active(directive) ||
            directive.section != "land_generation" ||
            directive.name != "base_terrain" ||
            directive.arguments.empty()) continue;
        if (const auto terrain = rms_terrain(directive.arguments.front())) {
            base = *terrain;
        }
    }
    fill_map(scenario.map, base);

    const TilePosition blue_start{dimension / 4, dimension / 2};
    const TilePosition red_start{
        dimension - 1 - blue_start.x,
        dimension - 1 - blue_start.y,
    };
    std::vector<LandSite> land_sites;

    for (const LandGeneration& land :
         land_generations(document, active)) {
        const int wanted = land.tiles > 0
            ? land.tiles
            : std::max(
                1,
                dimension * dimension * std::min(100, land.percent) / 100
            );
        if (land.player_lands) {
            const int radius = std::max(
                land.base_size,
                static_cast<int>(std::sqrt(
                    static_cast<double>(wanted) / (2.0 * 3.141592653589793)
                ))
            );
            paint_disc(scenario.map, blue_start, radius, land.terrain);
            paint_disc(scenario.map, red_start, radius, land.terrain);
            land_sites.push_back({
                blue_start, Player::blue,
                land.zone == -10 ? -9 : land.zone, land.id,
            });
            land_sites.push_back({
                red_start, Player::red,
                land.zone == -10 ? -8 : land.zone, land.id,
            });
        } else {
            const int minimum_x = std::max(
                land.base_size,
                dimension * land.left_border / 100
            );
            const int maximum_x = std::min(
                dimension - land.base_size - 1,
                dimension - 1 - dimension * land.right_border / 100
            );
            const int minimum_y = std::max(
                land.base_size,
                dimension * land.top_border / 100
            );
            const int maximum_y = std::min(
                dimension - land.base_size - 1,
                dimension - 1 - dimension * land.bottom_border / 100
            );
            TilePosition center{
                random.between(minimum_x, maximum_x),
                random.between(minimum_y, maximum_y),
            };
            if (land.position_percent) {
                center = {
                    dimension * land.position_percent->x / 100,
                    dimension * land.position_percent->y / 100,
                };
            }
            if (land.player == 1) center = blue_start;
            else if (land.player == 2) center = red_start;
            const int radius = std::max(
                land.base_size,
                static_cast<int>(std::sqrt(
                    static_cast<double>(wanted) / 3.141592653589793
                ))
            );
            paint_disc(scenario.map, center, radius, land.terrain);
            const std::optional<Player> owner =
                land.player == 1 ? std::optional{Player::blue} :
                land.player == 2 ? std::optional{Player::red} :
                                   std::nullopt;
            land_sites.push_back({center, owner, land.zone, land.id});
        }
    }
    if (land_sites.empty()) {
        land_sites.push_back({blue_start, Player::blue, -9, -1});
        land_sites.push_back({red_start, Player::red, -8, -1});
    }

    // Starts are anchors for every-player object placement. Explicit
    // TOWN_CENTER blocks replace these provisional buildings.
    scenario.buildings.push_back({
        BuildingKind::town_center, Player::blue, blue_start,
        std::nullopt, std::nullopt, std::nullopt,
    });
    scenario.buildings.push_back({
        BuildingKind::town_center, Player::red, red_start,
        std::nullopt, std::nullopt, std::nullopt,
    });
    paint_disc(scenario.map, blue_start, 3, Terrain::grass);
    paint_disc(scenario.map, red_start, 3, Terrain::grass);

    for (const ElevationGeneration& elevation :
         elevation_generations(document, active)) {
        const double size_scale =
            static_cast<double>(dimension * dimension) / 10000.0;
        const int elevation_tiles =
            elevation.scale == ElevationGeneration::Scale::size
            ? std::max(
                1, static_cast<int>(std::lround(
                    elevation.tiles * size_scale
                ))
            )
            : elevation.tiles;
        const int elevation_clumps =
            elevation.scale == ElevationGeneration::Scale::groups
            ? std::max(
                1, static_cast<int>(std::lround(
                    elevation.clumps * size_scale
                ))
            )
            : elevation.clumps;
        const int radius = std::max(
            1, static_cast<int>(std::sqrt(
                static_cast<double>(elevation_tiles) /
                (elevation_clumps * 3.141592653589793)
            ))
        );
        for (int clump = 0; clump < elevation_clumps; ++clump) {
            TilePosition center;
            bool accepted{};
            for (int attempt = 0; attempt < 64; ++attempt) {
                center = {
                    random.between(radius, dimension - radius - 1),
                    random.between(radius, dimension - radius - 1),
                };
                const auto clear_of_start = [&](TilePosition start) {
                    return std::abs(center.x - start.x) +
                        std::abs(center.y - start.y) > radius + 9;
                };
                if ((!elevation.base ||
                     scenario.map.terrain_at(center) == *elevation.base) &&
                    clear_of_start(blue_start) &&
                    clear_of_start(red_start)) {
                    accepted = true;
                    break;
                }
            }
            if (!accepted) continue;
            for (int y = center.y - radius; y <= center.y + radius; ++y) {
                for (int x = center.x - radius; x <= center.x + radius; ++x) {
                    const TilePosition tile{x, y};
                    if (!scenario.map.contains(tile)) continue;
                    if (elevation.base &&
                        scenario.map.terrain_at(tile) != *elevation.base) {
                        continue;
                    }
                    const int distance = std::abs(x - center.x) +
                        std::abs(y - center.y);
                    if (distance <= radius) {
                        const int level = std::max(
                            1,
                            elevation.height -
                                distance / elevation.spacing
                        );
                        scenario.map.set_elevation(tile, level);
                    }
                }
            }
        }
    }

    // Classic module order is land, elevation, cliffs, terrain,
    // connections, then objects. A cliff line is accepted atomically: a
    // failed tail never leaves a shorter line or consumes spacing for later
    // lines.
    if (const auto cliffs = cliff_generation(document, active)) {
        const int count = random.between(
            cliffs->minimum_count, cliffs->maximum_count
        );
        std::vector<std::vector<TilePosition>> placed_lines;
        constexpr std::array<TilePosition, 8> directions{{
            {1, 0}, {1, 1}, {0, 1}, {-1, 1},
            {-1, 0}, {-1, -1}, {0, -1}, {1, -1},
        }};
        const int edge_clearance = std::max(
            2, cliffs->minimum_terrain_distance
        );
        const auto clear_of_player_land = [&](TilePosition tile) {
            return std::ranges::all_of(
                land_sites, [&](const LandSite& land) {
                    if (!land.player) return true;
                    return std::max(
                        std::abs(tile.x - land.origin.x),
                        std::abs(tile.y - land.origin.y)
                    ) > 8;
                }
            );
        };
        const auto clear_of_terrain = [&](TilePosition tile) {
            for (int y = -cliffs->minimum_terrain_distance;
                 y <= cliffs->minimum_terrain_distance; ++y) {
                for (int x = -cliffs->minimum_terrain_distance;
                     x <= cliffs->minimum_terrain_distance; ++x) {
                    const TilePosition nearby{tile.x + x, tile.y + y};
                    if (!scenario.map.contains(nearby) ||
                        cliff_forbidden_terrain(
                            scenario.map.terrain_at(nearby)
                        )) return false;
                }
            }
            return true;
        };
        const auto clear_of_other_lines = [&](TilePosition tile) {
            return std::ranges::all_of(
                placed_lines, [&](const auto& line) {
                    return std::ranges::all_of(
                        line, [&](TilePosition existing) {
                            return std::max(
                                std::abs(existing.x - tile.x),
                                std::abs(existing.y - tile.y)
                            ) >= cliffs->minimum_spacing;
                        }
                    );
                }
            );
        };
        for (int index = 0; index < count; ++index) {
            bool accepted{};
            for (int attempt = 0; attempt < 1000 && !accepted; ++attempt) {
                TilePosition position{
                    random.between(
                        edge_clearance,
                        dimension - edge_clearance - 1
                    ),
                    random.between(
                        edge_clearance,
                        dimension - edge_clearance - 1
                    ),
                };
                int direction = random.between(0, 7);
                const int length = random.between(
                    cliffs->minimum_length, cliffs->maximum_length
                );
                std::vector<TilePosition> candidate;
                candidate.reserve(static_cast<std::size_t>(length));
                for (int step = 0; step < length; ++step) {
                    const bool duplicate = std::ranges::find(
                        candidate, position
                    ) != candidate.end();
                    if (duplicate ||
                        position.x < edge_clearance ||
                        position.y < edge_clearance ||
                        position.x >= dimension - edge_clearance ||
                        position.y >= dimension - edge_clearance ||
                        !clear_of_player_land(position) ||
                        !clear_of_terrain(position) ||
                        !clear_of_other_lines(position)) {
                        candidate.clear();
                        break;
                    }
                    candidate.push_back(position);
                    if (random.between(0, 99) < cliffs->curliness) {
                        direction = (
                            direction +
                            (random.between(0, 1) == 0 ? -1 : 1) + 8
                        ) % 8;
                    }
                    position.x += directions[direction].x;
                    position.y += directions[direction].y;
                }
                if (static_cast<int>(candidate.size()) != length) continue;
                for (TilePosition tile : candidate) {
                    scenario.map.set_cliff(tile, true);
                }
                placed_lines.push_back(std::move(candidate));
                accepted = true;
            }
        }
    }

    for (const TerrainGeneration& terrain :
         terrain_generations(document, active)) {
        const double size_scale =
            static_cast<double>(dimension * dimension) / 10000.0;
        int clumps = terrain.clumps;
        int tiles = terrain.tiles;
        if (tiles > 0 && terrain.scale != TerrainGeneration::Scale::none) {
            tiles = std::max(
                1, static_cast<int>(std::lround(tiles * size_scale))
            );
        }
        if (terrain.scale == TerrainGeneration::Scale::groups) {
            clumps = std::max(
                1, static_cast<int>(std::lround(clumps * size_scale))
            );
        }
        if (tiles == 0 && terrain.percent > 0) {
            tiles = dimension * dimension *
                std::min(100, terrain.percent) / 100;
        }
        if (tiles == 0) tiles = clumps * 12;
        const int per_clump = std::max(1, tiles / clumps);
        const int radius = std::max(
            1, static_cast<int>(std::sqrt(
                static_cast<double>(per_clump) / 3.141592653589793
            ))
        );
        for (int clump = 0; clump < clumps; ++clump) {
            TilePosition center;
            bool accepted{};
            for (int attempt = 0; attempt < 32; ++attempt) {
                center = {
                    random.between(radius, dimension - radius - 1),
                    random.between(radius, dimension - radius - 1),
                };
                const auto far_from_starts = [&](TilePosition start) {
                    return std::abs(center.x - start.x) +
                        std::abs(center.y - start.y) > 10;
                };
                const bool starts_allowed =
                    !terrain.avoid_starts ||
                    (far_from_starts(blue_start) &&
                     far_from_starts(red_start));
                const Terrain substrate =
                    scenario.map.terrain_at(center);
                bool spacing_allowed = true;
                const int spacing_radius = radius + terrain.spacing;
                for (int y = center.y - spacing_radius;
                     y <= center.y + spacing_radius &&
                     spacing_allowed; ++y) {
                    for (int x = center.x - spacing_radius;
                         x <= center.x + spacing_radius; ++x) {
                        const TilePosition tile{x, y};
                        if (!scenario.map.contains(tile)) continue;
                        const Terrain existing =
                            scenario.map.terrain_at(tile);
                        if (existing != substrate &&
                            existing != terrain.terrain) {
                            spacing_allowed = false;
                            break;
                        }
                    }
                }
                if (starts_allowed && spacing_allowed) {
                    accepted = true;
                    break;
                }
            }
            if (!accepted) continue;
            for (int y = center.y - radius; y <= center.y + radius; ++y) {
                for (int x = center.x - radius; x <= center.x + radius; ++x) {
                    const TilePosition tile{x, y};
                    if (!scenario.map.contains(tile)) continue;
                    const int dx = x - center.x;
                    const int dy = y - center.y;
                    const int irregularity = std::max(
                        0, (20 - terrain.clumping) / 4
                    );
                    const int edge_noise = irregularity == 0 ? 0 :
                        random.between(-irregularity, irregularity);
                    const int edge = std::max(1, radius + edge_noise);
                    if (dx * dx + dy * dy > edge * edge) continue;
                    if (terrain.base &&
                        scenario.map.terrain_at(tile) != *terrain.base) {
                        continue;
                    }
                    if (terrain.flat_only &&
                        scenario.map.elevation_at(tile) != 0) continue;
                    scenario.map.set_terrain(tile, terrain.terrain);
                }
            }
        }
    }

    for (const ConnectionGeneration& connection :
         connection_generations(document, active)) {
        const auto connects = [&](const LandSite& first,
                                  const LandSite& second) {
            switch (connection.kind) {
                case ConnectionGeneration::Kind::all_players:
                    return first.player && second.player;
                case ConnectionGeneration::Kind::teams:
                    // Two reconstruction players are opponents, so no
                    // same-team pair exists.
                    return false;
                case ConnectionGeneration::Kind::all_lands:
                    return true;
                case ConnectionGeneration::Kind::same_zones:
                    return first.zone == second.zone;
                case ConnectionGeneration::Kind::explicit_zones:
                    return connection.zone_one && connection.zone_two &&
                        ((first.zone == *connection.zone_one &&
                          second.zone == *connection.zone_two) ||
                         (first.zone == *connection.zone_two &&
                          second.zone == *connection.zone_one));
                case ConnectionGeneration::Kind::nonplayer:
                    return first.player.has_value() !=
                        second.player.has_value();
            }
            return false;
        };
        for (std::size_t first = 0; first < land_sites.size(); ++first) {
            for (std::size_t second = first + 1;
                 second < land_sites.size(); ++second) {
                if (!connects(land_sites[first], land_sites[second])) {
                    continue;
                }
                paint_connection(
                    scenario.map,
                    land_sites[first].origin,
                    land_sites[second].origin,
                    connection.width,
                    connection.terrain,
                    connection.replacements
                );
            }
        }
    }
    paint_disc(scenario.map, blue_start, 3, Terrain::grass);
    paint_disc(scenario.map, red_start, 3, Terrain::grass);

    apply_object_generations(
        scenario, object_generations(document, active), land_sites, random
    );
    return scenario;
}

RmsMapResult generate_rms_map(
    const RandomMapSettings& settings,
    std::optional<std::string_view> source
) {
    static constexpr std::string_view arabia = R"rms(
<LAND_GENERATION>
base_terrain GRASS
create_player_lands {
 terrain_type GRASS
 land_percent 42
 base_size 10
 border_fuzziness 12
 clumping_factor 12
}
<ELEVATION_GENERATION>
create_elevation 3 {
 number_of_clumps 10
 number_of_tiles 700
 set_scale_by_size
}
<CLIFF_GENERATION>
create_cliffs
min_number_of_cliffs 3
max_number_of_cliffs 7
min_length_of_cliff 4
max_length_of_cliff 12
cliff_curliness 35
min_distance_cliffs 6
min_terrain_distance 2
<TERRAIN_GENERATION>
create_terrain PALM_FOREST {
 base_terrain GRASS
 land_percent 7
 number_of_clumps 18
 clumping_factor 13
 set_avoid_player_start_areas
}
<CONNECTION_GENERATION>
create_connect_all_players_land {
 default_terrain_replacement GRASS
 terrain_size 3
}
<OBJECTS_GENERATION>
create_object TOWN_CENTER {
 set_place_for_every_player
 min_distance_to_players 0
}
create_object VILLAGER {
 set_place_for_every_player
 number_of_objects 3
 group_placement_radius 3
}
create_object SCOUT {
 set_place_for_every_player
 number_of_objects 1
 min_distance_to_players 5
}
create_object BERRIES {
 set_place_for_every_player
 set_gaia_object_only
 number_of_objects 6
 group_placement_radius 2
 min_distance_to_players 8
}
create_object GOLD {
 set_place_for_every_player
 set_gaia_object_only
 number_of_objects 7
 group_placement_radius 2
 min_distance_to_players 11
}
create_object STONE {
 set_place_for_every_player
 set_gaia_object_only
 number_of_objects 5
 group_placement_radius 2
 min_distance_to_players 15
}
create_object SHEEP {
 set_place_for_every_player
 set_gaia_object_only
 number_of_objects 4
 group_placement_radius 3
 min_distance_to_players 5
}
create_object BOAR {
 set_place_for_every_player
 set_gaia_object_only
 number_of_objects 2
 min_distance_to_players 13
}
create_object DEER {
 set_place_for_every_player
 set_gaia_object_only
 number_of_objects 4
 group_placement_radius 3
 min_distance_to_players 17
}
)rms";
    static constexpr std::string_view black_forest = R"rms(
<LAND_GENERATION>
base_terrain FOREST
create_player_lands {
 terrain_type GRASS
 land_percent 18
 base_size 12
}
<ELEVATION_GENERATION>
create_elevation 2 {
 base_terrain GRASS
 number_of_clumps 6
 number_of_tiles 400
}
<CONNECTION_GENERATION>
create_connect_all_players_land {
 default_terrain_replacement GRASS
 replace_terrain FOREST GRASS
 terrain_size 7
}
<TERRAIN_GENERATION>
create_terrain PINE_FOREST {
 base_terrain GRASS
 land_percent 3
 number_of_clumps 8
 set_avoid_player_start_areas
}
<OBJECTS_GENERATION>
create_object TOWN_CENTER {
 set_place_for_every_player
 min_distance_to_players 0
}
create_object VILLAGER {
 set_place_for_every_player
 number_of_objects 3
}
create_object SCOUT {
 set_place_for_every_player
 min_distance_to_players 5
}
create_object BERRIES {
 set_place_for_every_player
 set_gaia_object_only
 number_of_objects 6
 group_placement_radius 2
 min_distance_to_players 8
}
create_object GOLD {
 set_place_for_every_player
 set_gaia_object_only
 number_of_objects 7
 group_placement_radius 2
 min_distance_to_players 11
}
create_object STONE {
 set_place_for_every_player
 set_gaia_object_only
 number_of_objects 5
 group_placement_radius 2
 min_distance_to_players 15
}
create_object SHEEP {
 set_place_for_every_player
 set_gaia_object_only
 number_of_objects 4
 min_distance_to_players 5
}
)rms";
    static constexpr std::string_view islands = R"rms(
<LAND_GENERATION>
base_terrain WATER
create_player_lands {
 terrain_type GRASS
 land_percent 18
 base_size 14
 clumping_factor 18
}
<TERRAIN_GENERATION>
create_terrain JUNGLE_FOREST {
 base_terrain GRASS
 land_percent 8
 number_of_clumps 10
 set_avoid_player_start_areas
}
<OBJECTS_GENERATION>
create_object TOWN_CENTER {
 set_place_for_every_player
 min_distance_to_players 0
}
create_object VILLAGER {
 set_place_for_every_player
 number_of_objects 3
}
create_object SCOUT {
 set_place_for_every_player
 min_distance_to_players 5
}
create_object BERRIES {
 set_place_for_every_player
 set_gaia_object_only
 number_of_objects 6
 group_placement_radius 2
 min_distance_to_players 8
}
create_object GOLD {
 set_place_for_every_player
 set_gaia_object_only
 number_of_objects 7
 group_placement_radius 2
 min_distance_to_players 11
}
create_object STONE {
 set_place_for_every_player
 set_gaia_object_only
 number_of_objects 5
 group_placement_radius 2
 min_distance_to_players 15
}
create_object SHEEP {
 set_place_for_every_player
 set_gaia_object_only
 number_of_objects 4
 min_distance_to_players 5
}
create_object SHORE_FISH {
 set_gaia_object_only
 number_of_groups 12
 number_of_objects 3
 min_distance_to_players 10
 set_scaling_to_map_size
}
)rms";
    static constexpr std::string_view rivers = R"rms(
<LAND_GENERATION>
base_terrain GRASS
create_player_lands {
 terrain_type GRASS
 land_percent 55
 base_size 12
}
create_land {
 terrain_type WATER
 number_of_tiles 4200
 land_position 50 50
 base_size 12
 zone 20
}
<ELEVATION_GENERATION>
create_elevation 3 {
 base_terrain GRASS
 number_of_clumps 8
 number_of_tiles 600
}
<TERRAIN_GENERATION>
create_terrain OAK_FOREST {
 base_terrain GRASS
 land_percent 6
 number_of_clumps 14
 set_avoid_player_start_areas
}
<CONNECTION_GENERATION>
create_connect_all_players_land {
 default_terrain_replacement GRASS
 replace_terrain WATER SHALLOWS
 terrain_size 5
}
<OBJECTS_GENERATION>
create_object TOWN_CENTER {
 set_place_for_every_player
 min_distance_to_players 0
}
create_object VILLAGER {
 set_place_for_every_player
 number_of_objects 3
}
create_object SCOUT {
 set_place_for_every_player
 min_distance_to_players 5
}
create_object SALMON {
 set_gaia_object_only
 number_of_groups 8
 number_of_objects 2
 min_distance_to_players 12
 set_scaling_to_map_size
}
create_object BERRIES {
 set_place_for_every_player
 set_gaia_object_only
 number_of_objects 6
 group_placement_radius 2
 min_distance_to_players 8
}
create_object GOLD {
 set_place_for_every_player
 set_gaia_object_only
 number_of_objects 7
 group_placement_radius 2
 min_distance_to_players 11
}
create_object STONE {
 set_place_for_every_player
 set_gaia_object_only
 number_of_objects 5
 group_placement_radius 2
 min_distance_to_players 15
}
)rms";

    const std::string_view selected = source ? *source :
        settings.kind == RandomMapKind::black_forest ? black_forest :
        settings.kind == RandomMapKind::islands ? islands :
        settings.kind == RandomMapKind::rivers ? rivers : arabia;
    std::string complete =
        "<PLAYER_SETUP>\noverride_map_size " +
        std::to_string(random_map_dimension(settings.kind, settings.size)) +
        "\n";
    complete.append(selected);
    const RmsDocument document = parse_rms(complete);
    if (!document.syntactically_valid) {
        return {std::nullopt, document.error};
    }
    if (!document.playable()) {
        const std::string reason = document.unsupported.empty()
            ? "RMS refused"
            : document.unsupported.front().reason + " at line " +
                std::to_string(document.unsupported.front().span.first_line);
        return {std::nullopt, reason};
    }
    RmsEvaluationContext context;
    context.map_size = settings.size;
    std::optional<Scenario> scenario = evaluate_rms(
        document,
        settings.seed,
        context,
        settings.blue_civilization,
        settings.red_civilization
    );
    if (!scenario) return {std::nullopt, "RMS evaluation failed"};
    return {std::move(scenario), {}};
}

RmsMapResult generate_rms_map_file(
    const RandomMapSettings& settings,
    const std::filesystem::path& path,
    const std::optional<std::filesystem::path>& installation_root
) {
    const RmsDocument document = parse_rms_file(path, installation_root);
    if (!document.syntactically_valid) {
        return {std::nullopt, document.error};
    }
    if (!document.playable()) {
        const std::string reason = document.unsupported.empty()
            ? "RMS refused"
            : document.unsupported.front().reason + " at line " +
                std::to_string(document.unsupported.front().span.first_line);
        return {std::nullopt, reason};
    }
    RmsEvaluationContext context;
    context.map_size = settings.size;
    auto scenario = evaluate_rms(
        document, settings.seed, context, settings.blue_civilization,
        settings.red_civilization
    );
    if (!scenario) return {std::nullopt, "RMS evaluation failed"};
    return {std::move(scenario), {}};
}

}  // namespace aoe
