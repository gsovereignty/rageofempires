#include "aoe/computer_player.hpp"

#include "aoe/game_rules.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <fstream>
#include <limits>
#include <optional>
#include <stdexcept>

namespace aoe {
namespace {

int distance(TilePosition left, TilePosition right) {
    return std::abs(left.x - right.x) + std::abs(left.y - right.y);
}

struct AiTarget {
    EntityId id{};
    TilePosition position{};
};

std::optional<AiTarget> nearest_enemy(
    const Unit& unit,
    const Simulation& simulation,
    EntityId previous_target,
    ComputerDifficulty difficulty
) {
    std::optional<AiTarget> nearest;
    int nearest_distance = std::numeric_limits<int>::max();
    bool nearest_is_previous = false;
    const int acquisition_radius = computer_target_acquisition_radius(
        difficulty, simulation.effective_unit_vision_range(unit)
    );
    const auto within_acquisition_radius =
        [&unit, acquisition_radius](TilePosition position) {
            const int x = position.x - unit.position.x;
            const int y = position.y - unit.position.y;
            return x * x + y * y <=
                acquisition_radius * acquisition_radius;
        };

    for (const Unit& candidate : simulation.units()) {
        if (!simulation.is_enemy(candidate.owner, unit.owner) ||
            (is_animal(candidate.kind) || is_relic(candidate.kind)) ||
            candidate.hit_points <= 0 ||
            !simulation.is_visible(unit.owner, candidate.position) ||
            !within_acquisition_radius(candidate.position)) {
            continue;
        }
        const int candidate_distance =
            distance(unit.position, candidate.position);
        const bool is_previous = candidate.id == previous_target;
        if (candidate_distance < nearest_distance ||
            (candidate_distance == nearest_distance &&
             is_previous && !nearest_is_previous) ||
            (candidate_distance == nearest_distance &&
             is_previous == nearest_is_previous &&
             (!nearest || candidate.id < nearest->id))) {
            nearest = AiTarget{candidate.id, candidate.position};
            nearest_distance = candidate_distance;
            nearest_is_previous = is_previous;
        }
    }
    for (const Building& candidate : simulation.buildings()) {
        if (!simulation.is_enemy(candidate.owner, unit.owner) ||
            candidate.hit_points <= 0 ||
            !simulation.is_building_visible(unit.owner, candidate) ||
            !within_acquisition_radius(candidate.position)) {
            continue;
        }
        const int candidate_distance =
            distance(unit.position, candidate.position);
        const bool is_previous = candidate.id == previous_target;
        if (candidate_distance < nearest_distance ||
            (candidate_distance == nearest_distance &&
             is_previous && !nearest_is_previous) ||
            (candidate_distance == nearest_distance &&
             is_previous == nearest_is_previous &&
             (!nearest || candidate.id < nearest->id))) {
            nearest = AiTarget{candidate.id, candidate.position};
            nearest_distance = candidate_distance;
            nearest_is_previous = is_previous;
        }
    }
    return nearest;
}

std::optional<TilePosition> nearest_unexplored(
    const Unit& unit,
    const Simulation& simulation
) {
    const GameMap& map = simulation.map();
    const auto index = [&map](TilePosition position) {
        return static_cast<std::size_t>(
            position.y * map.width() + position.x
        );
    };
    std::vector<bool> reachable(
        static_cast<std::size_t>(map.width() * map.height()), false
    );
    std::deque<TilePosition> frontier{unit.position};
    reachable[index(unit.position)] = true;
    constexpr std::array<TilePosition, 4> directions{{
        {1, 0}, {-1, 0}, {0, 1}, {0, -1},
    }};
    while (!frontier.empty()) {
        const TilePosition current = frontier.front();
        frontier.pop_front();
        for (const TilePosition direction : directions) {
            const TilePosition next{
                current.x + direction.x, current.y + direction.y,
            };
            if (!map.traversable(current, next) || reachable[index(next)]) {
                continue;
            }
            reachable[index(next)] = true;
            frontier.push_back(next);
        }
    }
    std::optional<TilePosition> nearest;
    int nearest_distance = std::numeric_limits<int>::max();
    int best_edge_clearance = -1;
    for (int y = 0; y < simulation.map().height(); ++y) {
        for (int x = 0; x < simulation.map().width(); ++x) {
            const TilePosition candidate{x, y};
            if (simulation.is_explored(unit.owner, candidate) ||
                !reachable[index(candidate)]) {
                continue;
            }
            const int candidate_distance =
                distance(unit.position, candidate);
            const int edge_clearance = std::min({
                candidate.x,
                candidate.y,
                map.width() - 1 - candidate.x,
                map.height() - 1 - candidate.y,
            });
            if (candidate_distance < nearest_distance ||
                (candidate_distance == nearest_distance &&
                 edge_clearance > best_edge_clearance)) {
                nearest = candidate;
                nearest_distance = candidate_distance;
                best_edge_clearance = edge_clearance;
            }
        }
    }
    return nearest;
}

std::optional<TilePosition> adjacent_walkable(
    TilePosition target,
    const Unit& unit,
    const Simulation& simulation
) {
    std::optional<TilePosition> best;
    int best_distance = std::numeric_limits<int>::max();
    for (TilePosition candidate : {
             TilePosition{target.x + 1, target.y},
             TilePosition{target.x - 1, target.y},
             TilePosition{target.x, target.y + 1},
             TilePosition{target.x, target.y - 1},
         }) {
        if (!simulation.map().contains(candidate) ||
            !simulation.map().walkable(candidate)) continue;
        const int candidate_distance =
            distance(unit.position, candidate);
        if (candidate_distance < best_distance) {
            best = candidate;
            best_distance = candidate_distance;
        }
    }
    return best;
}

std::optional<TilePosition> adjacent_to_building(
    const Building& building,
    const Unit& unit,
    const Simulation& simulation
) {
    const BuildingRules& rules = rules_for(building.kind);
    std::optional<TilePosition> best;
    int best_distance = std::numeric_limits<int>::max();
    for (int y = building.position.y - 1;
         y <= building.position.y + rules.footprint_height; ++y) {
        for (int x = building.position.x - 1;
             x <= building.position.x + rules.footprint_width; ++x) {
            const bool perimeter =
                x == building.position.x - 1 ||
                y == building.position.y - 1 ||
                x == building.position.x + rules.footprint_width ||
                y == building.position.y + rules.footprint_height;
            const TilePosition candidate{x, y};
            if (!perimeter || !simulation.map().contains(candidate) ||
                !simulation.map().walkable(candidate)) continue;
            const int candidate_distance =
                distance(unit.position, candidate);
            if (candidate_distance < best_distance) {
                best = candidate;
                best_distance = candidate_distance;
            }
        }
    }
    return best;
}

bool is_active_builder(
    EntityId unit_id,
    const Simulation& simulation
) {
    return std::ranges::any_of(
        simulation.buildings(),
        [unit_id](const Building& building) {
            return !building.completed() &&
                std::ranges::find(building.builder_ids, unit_id) !=
                    building.builder_ids.end();
        }
    );
}

ResourceKind resource_kind(Terrain terrain) {
    switch (terrain) {
        case Terrain::forest:
        case Terrain::pine_forest:
        case Terrain::oak_forest:
        case Terrain::bamboo_forest:
        case Terrain::palm_forest:
        case Terrain::jungle_forest:
            return ResourceKind::wood;
        case Terrain::berry_bush:
            return ResourceKind::food;
        case Terrain::gold_mine:
            return ResourceKind::gold;
        case Terrain::stone_mine:
            return ResourceKind::stone;
        case Terrain::fish:
        case Terrain::fish_shore:
        case Terrain::fish_deep:
            return ResourceKind::none;
        case Terrain::grass:
        case Terrain::grass2:
        case Terrain::dirt:
        case Terrain::dirt2:
        case Terrain::dirt3:
        case Terrain::road:
        case Terrain::snow:
        case Terrain::ice:
        case Terrain::water:
        case Terrain::deep_water:
        case Terrain::beach:
        case Terrain::shallows:
            return ResourceKind::none;
        default:
            return ResourceKind::none;
    }
}

std::optional<TilePosition> nearest_resource(
    const Unit& villager,
    ResourceKind resource,
    const Simulation& simulation
) {
    std::optional<TilePosition> nearest;
    int nearest_distance = std::numeric_limits<int>::max();
    for (int y = 0; y < simulation.map().height(); ++y) {
        for (int x = 0; x < simulation.map().width(); ++x) {
            const TilePosition candidate{x, y};
            if (!simulation.is_explored(villager.owner, candidate) ||
                resource_kind(simulation.map().terrain_at(candidate)) !=
                    resource ||
                simulation.map().resource_amount_at(candidate) <= 0) {
                continue;
            }
            const int candidate_distance =
                distance(villager.position, candidate);
            if (candidate_distance < nearest_distance) {
                nearest = candidate;
                nearest_distance = candidate_distance;
            }
        }
    }
    if (resource == ResourceKind::food) {
        for (const Unit& unit : simulation.units()) {
            if (!is_animal(unit.kind) ||
                (is_herdable(unit.kind) &&
                 unit.owner != villager.owner) ||
                unit.food_remaining <= 0 ||
                (is_huntable(unit.kind) &&
                 unit.hit_points <= 0 &&
                 !simulation.is_explored(
                     villager.owner,
                     unit.position
                 )) ||
                !simulation.is_explored(
                    villager.owner,
                    unit.position
                )) {
                continue;
            }
            const int candidate_distance =
                distance(villager.position, unit.position);
            if (candidate_distance < nearest_distance) {
                nearest = unit.position;
                nearest_distance = candidate_distance;
            }
        }
        for (const Building& building : simulation.buildings()) {
            if (building.owner != villager.owner ||
                building.kind != BuildingKind::farm ||
                !building.completed() || building.resource_amount <= 0) {
                continue;
            }
            const int candidate_distance =
                distance(villager.position, building.position);
            if (candidate_distance < nearest_distance) {
                nearest = building.position;
                nearest_distance = candidate_distance;
            }
        }
    }
    return nearest;
}

bool has_explored_natural_food(
    Player player,
    const Simulation& simulation
) {
    if (std::ranges::any_of(
            simulation.units(),
            [player, &simulation](const Unit& unit) {
                return is_animal(unit.kind) &&
                    (!is_herdable(unit.kind) ||
                     unit.owner == player) &&
                    unit.food_remaining > 0 &&
                    simulation.is_explored(player, unit.position);
            }
        )) {
        return true;
    }
    for (int y = 0; y < simulation.map().height(); ++y) {
        for (int x = 0; x < simulation.map().width(); ++x) {
            const TilePosition position{x, y};
            if (simulation.is_explored(player, position) &&
                simulation.map().terrain_at(position) ==
                    Terrain::berry_bush &&
                simulation.map().resource_amount_at(position) > 0) {
                return true;
            }
        }
    }
    return false;
}

bool try_construct_nearby(
    Simulation& simulation,
    const Unit& villager,
    BuildingKind kind
) {
    const BuildingRules& rules = rules_for(kind);
    const Economy& economy = simulation.economy(villager.owner);
    if (economy.wood < rules.wood_cost ||
        economy.stone < rules.stone_cost ||
        economy.gold < rules.gold_cost) {
        return false;
    }
    const int maximum_radius =
        kind == BuildingKind::dock ? 4 : 2;
    for (int radius = 0; radius <= maximum_radius; ++radius) {
        for (int y = villager.position.y - radius;
             y <= villager.position.y + radius;
             ++y) {
            for (int x = villager.position.x - radius;
                 x <= villager.position.x + radius;
                 ++x) {
                if (std::abs(x - villager.position.x) +
                        std::abs(y - villager.position.y) != radius) {
                    continue;
                }
                if (simulation.construct_building_at(
                        villager.id,
                        kind,
                        {x, y}
                    )) {
                    return true;
                }
            }
        }
    }
    return false;
}

}  // namespace

int computer_target_acquisition_radius(
    ComputerDifficulty difficulty,
    int effective_line_of_sight
) {
    if (difficulty < ComputerDifficulty::easiest ||
        difficulty > ComputerDifficulty::hardest ||
        effective_line_of_sight < 0) {
        throw std::invalid_argument("invalid AI acquisition radius input");
    }
    return (difficulty == ComputerDifficulty::easiest ||
            difficulty == ComputerDifficulty::easy)
        ? effective_line_of_sight
        : effective_line_of_sight * 2;
}

ClassicAiDifficultyProfile classic_ai_difficulty_profile(
    ComputerDifficulty difficulty
) {
    switch (difficulty) {
        case ComputerDifficulty::easiest:
            return {10, 100, 100, 0};
        case ComputerDifficulty::easy:
            return {25, 75, 75, 50};
        case ComputerDifficulty::moderate:
            return {75, 50, 50, 25};
        case ComputerDifficulty::hard:
            return {99, 25, 25, 10};
        case ComputerDifficulty::hardest:
            return {99, 0, 0, 10};
    }
    throw std::invalid_argument("invalid computer difficulty");
}

ClassicAiGatherPlan classic_ai_gather_plan(
    Age age,
    int civilian_population,
    int mining_camp_count,
    bool reserving_for_age,
    bool needs_first_castle,
    ComputerDifficulty difficulty
) {
    if (civilian_population < 0 || mining_camp_count < 0 ||
        difficulty < ComputerDifficulty::easiest ||
        difficulty > ComputerDifficulty::hardest) {
        throw std::invalid_argument("invalid classic AI gather input");
    }
    if (age == Age::dark) {
        return civilian_population < 9
            ? ClassicAiGatherPlan{{0, 100, 0, 0}}
            : ClassicAiGatherPlan{{30, 70, 0, 0}};
    }
    if (age == Age::feudal) {
        if (mining_camp_count == 0) return {{30, 70, 0, 0}};
        if (mining_camp_count == 1) {
            return reserving_for_age
                ? ClassicAiGatherPlan{{25, 55, 20, 0}}
                : ClassicAiGatherPlan{{45, 40, 15, 0}};
        }
        return {{35, 40, 15, 10}};
    }
    if (age == Age::castle) {
        if (mining_camp_count == 0) return {{30, 70, 0, 0}};
        if (mining_camp_count == 1) return {{25, 55, 20, 0}};
        if (difficulty <= ComputerDifficulty::moderate &&
            needs_first_castle) {
            return {{35, 35, 15, 15}};
        }
        return {{30, 35, 25, 10}};
    }
    // Imperial Petersen rules retask toward the active unit/upgrade goal.
    // This represented controller has no complete goal lattice, so preserve
    // its stable late-game base mix instead of inventing a resource goal.
    return {{30, 35, 25, 10}};
}

int classic_ai_villager_target(
    Age age,
    int population_cap,
    ComputerDifficulty difficulty
) {
    if (population_cap <= 0 ||
        difficulty < ComputerDifficulty::easiest ||
        difficulty > ComputerDifficulty::hardest) {
        throw std::invalid_argument("invalid classic AI population input");
    }
    constexpr std::array caps{25, 50, 75, 100, 125, 150, 175, 200};
    constexpr std::array dark{11, 20, 25, 25, 25, 25, 25, 25};
    constexpr std::array feudal{13, 30, 35, 40, 40, 40, 40, 40};
    constexpr std::array castle{15, 35, 40, 50, 55, 60, 65, 70};
    std::size_t profile = 0;
    while (profile + 1 < caps.size() && population_cap > caps[profile]) {
        ++profile;
    }
    // Petersen moderate rules use reduced civ-*-mod constants.
    if (difficulty == ComputerDifficulty::moderate) {
        if (age == Age::dark) return population_cap <= 25 ? 10 : 15;
        if (age == Age::feudal) return population_cap <= 25 ? 12 : 20;
    }
    if (age == Age::dark) return dark[profile];
    if (age == Age::feudal) return feudal[profile];
    return castle[profile];
}

ClassicAiAttackProfile classic_ai_attack_profile(
    ComputerDifficulty difficulty
) {
    switch (difficulty) {
        case ComputerDifficulty::easiest:
            return {1800, 900, Age::castle};
        case ComputerDifficulty::easy:
            return {1200, 120, Age::castle};
        case ComputerDifficulty::moderate:
            return {1, 120, Age::castle};
        case ComputerDifficulty::hard:
        case ComputerDifficulty::hardest:
            return {1, 300, Age::feudal};
    }
    throw std::invalid_argument("invalid computer difficulty");
}

ComputerPlayer::ComputerPlayer(
    Player player,
    ComputerDifficulty difficulty
) {
    ComputerPlayerState state;
    state.player = player;
    state.difficulty = difficulty;
    restore_state(state);
}

void ComputerPlayer::set_difficulty(ComputerDifficulty difficulty) {
    if (difficulty < ComputerDifficulty::easiest ||
        difficulty > ComputerDifficulty::hardest) {
        throw std::invalid_argument("invalid computer difficulty");
    }
    state_.difficulty = difficulty;
}

void ComputerPlayer::restore_state(ComputerPlayerState state) {
    if (state.player < Player::blue ||
        state.player > Player::red ||
        state.difficulty < ComputerDifficulty::easiest ||
        state.difficulty > ComputerDifficulty::hardest) {
        throw std::invalid_argument("invalid computer player state");
    }
    state_ = state;
}

void save_computer_player(
    const ComputerPlayer& computer,
    const std::filesystem::path& path
) {
    std::ofstream output(path);
    if (!output) {
        throw std::runtime_error("could not write computer player state");
    }
    const ComputerPlayerState& state = computer.state();
    output << "AOE-COMPUTER-PLAYER 4\n"
           << static_cast<int>(state.player) << ' '
           << static_cast<int>(state.difficulty) << ' '
           << state.last_command_tick << ' '
           << state.last_attack_tick << ' '
           << state.strategy_epoch << ' '
           << state.next_attack_tick << ' '
           << state.next_resource_bonus_tick << ' '
           << state.last_target_id << ' '
           << state.home_anchor.x << ' ' << state.home_anchor.y << ' '
           << state.rally_point.x << ' ' << state.rally_point.y << ' '
           << state.retreating << ' '
           << state.attack_timer_armed << ' '
           << state.resource_bonus_timer_armed << '\n';
}

ComputerPlayer load_computer_player(const std::filesystem::path& path) {
    std::ifstream input(path);
    std::string magic;
    int version{};
    int player{};
    int difficulty{};
    ComputerPlayerState state;
    input >> magic >> version >> player >> difficulty >>
        state.last_command_tick >> state.last_attack_tick >>
        state.strategy_epoch;
    if (version >= 4) {
        input >> state.next_attack_tick >> state.next_resource_bonus_tick;
    }
    if (version >= 2) {
        input >> state.last_target_id;
    }
    input >> state.home_anchor.x >> state.home_anchor.y >>
        state.rally_point.x >> state.rally_point.y >> state.retreating;
    if (version >= 4) {
        input >> state.attack_timer_armed >>
            state.resource_bonus_timer_armed;
    }
    if (!input || magic != "AOE-COMPUTER-PLAYER" ||
        (version != 1 && version != 2 && version != 3 && version != 4) ||
        player < static_cast<int>(Player::blue) ||
        player > static_cast<int>(Player::red) ||
        difficulty < 0 ||
        difficulty > (version < 3 ? 3 :
            static_cast<int>(ComputerDifficulty::hardest))) {
        throw std::runtime_error("unsupported or corrupt computer state");
    }
    state.player = static_cast<Player>(player);
    // v1/v2 encoded the reconstruction-only four-level order
    // easy/standard/hard/expert. Preserve its effective intent while
    // migrating to the commercial five-level order.
    if (version < 3) {
        constexpr std::array legacy_difficulties{
            ComputerDifficulty::easy,
            ComputerDifficulty::moderate,
            ComputerDifficulty::hard,
            ComputerDifficulty::hardest,
        };
        state.difficulty = legacy_difficulties[
            static_cast<std::size_t>(difficulty)
        ];
    } else {
        state.difficulty = static_cast<ComputerDifficulty>(difficulty);
    }
    ComputerPlayer computer(state.player, state.difficulty);
    computer.restore_state(state);
    return computer;
}

void ComputerPlayer::update(Simulation& simulation) {
    const Player player_ = state_.player;
    const std::uint64_t command_interval =
        state_.difficulty == ComputerDifficulty::easiest ? 10 :
        state_.difficulty == ComputerDifficulty::easy ? 7 :
        state_.difficulty == ComputerDifficulty::moderate ? 5 :
        state_.difficulty == ComputerDifficulty::hard ? 4 : 3;
    if (simulation.outcome() != MatchOutcome::ongoing) {
        return;
    }
    if (simulation.tick_number() <
        state_.last_command_tick + command_interval) {
        return;
    }
    state_.last_command_tick = simulation.tick_number();
    ++state_.strategy_epoch;

    std::vector<const Building*> owned_buildings;
    for (const Building& building : simulation.buildings()) {
        if (building.owner == player_) {
            owned_buildings.push_back(&building);
        }
    }
    int villager_count = 0;
    std::vector<EntityId> idle_villagers;
    for (const Unit& unit : simulation.units()) {
        if (unit.owner != player_ || unit.kind != UnitKind::villager) {
            continue;
        }
        ++villager_count;
        if (!unit.moving && !unit.has_resource_target &&
            unit.repair_target_id == 0 && unit.garrison_target_id == 0 &&
            unit.garrisoned_in == 0 &&
            !is_active_builder(unit.id, simulation)) {
            idle_villagers.push_back(unit.id);
        }
    }
    if (state_.home_anchor.x < 0) {
        const auto town_center = std::ranges::find_if(
            owned_buildings, [](const Building* building) {
                return building->kind == BuildingKind::town_center &&
                    building->completed();
            }
        );
        if (town_center != owned_buildings.end()) {
            state_.home_anchor = (*town_center)->position;
        } else if (!owned_buildings.empty()) {
            state_.home_anchor = owned_buildings.front()->position;
        }
        if (state_.home_anchor.x >= 0) {
            for (int radius = 1;
                 radius <= 6 && state_.rally_point.x < 0; ++radius) {
                for (int y = state_.home_anchor.y - radius;
                     y <= state_.home_anchor.y + radius; ++y) {
                    for (int x = state_.home_anchor.x - radius;
                         x <= state_.home_anchor.x + radius; ++x) {
                        const TilePosition candidate{x, y};
                        if (std::abs(x - state_.home_anchor.x) +
                                std::abs(y - state_.home_anchor.y) !=
                            radius ||
                            !simulation.map().contains(candidate) ||
                            !simulation.map().walkable(candidate)) {
                            continue;
                        }
                        state_.rally_point = candidate;
                        break;
                    }
                    if (state_.rally_point.x >= 0) break;
                }
            }
        }
    }
    status_ = {};
    status_.villagers = villager_count;
    status_.home = state_.home_anchor;
    status_.rally = state_.rally_point;
    status_.retreating = state_.retreating;

    const auto has_building = [&owned_buildings](
                                  BuildingKind kind,
                                  bool completed
                              ) {
        return std::ranges::any_of(
            owned_buildings,
            [kind, completed](const Building* building) {
                return building->kind == kind &&
                    (!completed || building->completed());
            }
        );
    };
    const auto mill = std::ranges::find_if(
        owned_buildings,
        [](const Building* building) {
            return building->kind == BuildingKind::mill &&
                building->completed();
        }
    );
    for (const Building* building : owned_buildings) {
        if (building->kind != BuildingKind::farm ||
            !building->completed() || building->resource_amount != 0) {
            continue;
        }
        if (simulation.farm_reseed_queue(player_) == 0 &&
            mill != owned_buildings.end()) {
            simulation.reseed_farm((*mill)->id);
        }
        simulation.consume_farm_reseed(building->id);
    }
    const bool farm_near_exhaustion = std::ranges::any_of(
        owned_buildings,
        [](const Building* building) {
            return building->kind == BuildingKind::farm &&
                building->completed() &&
                building->resource_amount > 0 &&
                building->resource_amount <= 10;
        }
    );
    if (farm_near_exhaustion &&
        simulation.farm_reseed_queue(player_) == 0) {
        if (mill != owned_buildings.end()) {
            simulation.reseed_farm((*mill)->id);
        }
    }
    std::vector<EntityId> sheltered_villagers;
    for (const Unit& villager : simulation.units()) {
        if (villager.owner != player_ ||
            villager.kind != UnitKind::villager ||
            villager.garrisoned_in != 0) {
            continue;
        }
        const bool threatened = std::ranges::any_of(
            simulation.units(),
            [&](const Unit& enemy) {
                return simulation.is_enemy(enemy.owner, player_) &&
                    !is_animal(enemy.kind) && !is_relic(enemy.kind) &&
                    enemy.hit_points > 0 &&
                    simulation.is_visible(player_, enemy.position) &&
                    distance(villager.position, enemy.position) <= 5;
            }
        );
        if (!threatened) continue;
        std::vector<const Building*> shelters;
        for (const Building* building : owned_buildings) {
            if (building->completed() &&
                (building->kind == BuildingKind::town_center ||
                 building->kind == BuildingKind::castle ||
                 building->kind == BuildingKind::watch_tower ||
                 building->kind == BuildingKind::bombard_tower)) {
                shelters.push_back(building);
            }
        }
        std::ranges::sort(
            shelters,
            [&](const Building* left, const Building* right) {
                return distance(villager.position, left->position) <
                    distance(villager.position, right->position);
            }
        );
        if (std::ranges::any_of(
            shelters,
            [&](const Building* shelter) {
                return simulation.command_unit(
                    villager.id, shelter->position
                );
            }
        )) {
            sheltered_villagers.push_back(villager.id);
        }
    }
    std::erase_if(idle_villagers, [&](EntityId id) {
        return std::ranges::find(sheltered_villagers, id) !=
            sheltered_villagers.end();
    });
    const Age current_age = simulation.age(player_);
    if (state_.difficulty == ComputerDifficulty::hardest &&
        current_age == Age::imperial) {
        if (!state_.resource_bonus_timer_armed) {
            state_.resource_bonus_timer_armed = true;
            state_.next_resource_bonus_tick =
                simulation.tick_number() + 1800;
        } else if (simulation.tick_number() >=
                   state_.next_resource_bonus_tick) {
            const auto slot = player_slot_from_legacy(player_);
            if (slot) {
                Simulation::PlayerState player_state =
                    simulation.player_state(*slot);
                player_state.economy.wood += 500;
                player_state.economy.food += 500;
                player_state.economy.gold += 500;
                player_state.economy.stone += 500;
                simulation.replace_player_state(
                    *slot, std::move(player_state)
                );
            }
            state_.next_resource_bonus_tick =
                simulation.tick_number() + 1200;
        }
    } else {
        state_.resource_bonus_timer_armed = false;
        state_.next_resource_bonus_tick = 0;
    }
    status_.age_goal =
        current_age == Age::dark ? Age::feudal :
        current_age == Age::feudal ? Age::castle : Age::imperial;
    status_.phase =
        current_age == Age::dark ? ComputerStrategyPhase::opening :
        current_age == Age::feudal ? ComputerStrategyPhase::developing :
        current_age == Age::castle ? ComputerStrategyPhase::pressure :
        ComputerStrategyPhase::conquest;
    const bool visible_relic = std::ranges::any_of(
        simulation.units(), [player_, &simulation](const Unit& unit) {
            return unit.kind == UnitKind::relic &&
                unit.garrisoned_in == 0 &&
                simulation.is_visible(player_, unit.position);
        }
    );
    const bool owns_monk = std::ranges::any_of(
        simulation.units(), [player_](const Unit& unit) {
            return unit.owner == player_ &&
                unit.kind == UnitKind::monk;
        }
    );
    UnitKind planned_counter{UnitKind::militia};
    for (const Unit& observed : simulation.units()) {
        if (!simulation.is_enemy(observed.owner, player_) ||
            !simulation.is_visible(player_, observed.position) ||
            is_animal(observed.kind) || is_relic(observed.kind)) {
            continue;
        }
        const bool cavalry =
            observed.kind == UnitKind::scout_cavalry ||
            observed.kind == UnitKind::knight ||
            observed.kind == UnitKind::cavalier ||
            observed.kind == UnitKind::paladin ||
            observed.kind == UnitKind::light_cavalry ||
            observed.kind == UnitKind::hussar ||
            observed.kind == UnitKind::camel_rider ||
            observed.kind == UnitKind::heavy_camel;
        planned_counter = cavalry
            ? UnitKind::spearman
            : rules_for(observed.kind).attack_range > 1
                ? UnitKind::skirmisher
                : UnitKind::archer;
        break;
    }
    status_.desired_counter = planned_counter;
    const int villager_target = classic_ai_villager_target(
        current_age, 200, state_.difficulty
    );
    bool ready_for_next_age = false;
    if (current_age == Age::dark) {
        int prerequisites = 0;
        for (BuildingKind kind : {
                 BuildingKind::barracks,
                 BuildingKind::mill,
                 BuildingKind::lumber_camp,
                 BuildingKind::mining_camp,
             }) {
            prerequisites += has_building(kind, true) ? 1 : 0;
        }
        ready_for_next_age = prerequisites >= 2;
    } else if (current_age == Age::feudal) {
        int prerequisites = 0;
        for (BuildingKind kind : {
                 BuildingKind::archery_range,
                 BuildingKind::stable,
                 BuildingKind::blacksmith,
             }) {
            prerequisites += has_building(kind, true) ? 1 : 0;
        }
        ready_for_next_age = prerequisites >= 2;
    } else if (current_age == Age::castle) {
        ready_for_next_age =
            has_building(BuildingKind::castle, true) ||
            (has_building(BuildingKind::university, true) &&
             has_building(BuildingKind::siege_workshop, true));
    }
    const bool age_in_progress = std::ranges::any_of(
        owned_buildings,
        [](const Building* building) {
            return building->age_research_ticks_remaining > 0;
        }
    );
    bool reserving_for_age =
        ready_for_next_age && villager_count >= 4;
    if (reserving_for_age && !age_in_progress) {
        const auto town_center = std::ranges::find_if(
            owned_buildings,
            [](const Building* building) {
                return building->kind == BuildingKind::town_center &&
                    building->completed();
            }
        );
        if (town_center != owned_buildings.end()) {
            simulation.advance_age_at((*town_center)->id);
        }
    }

    for (const Building* building : owned_buildings) {
        if (!building->completed() ||
            !building->production_queue.empty()) {
            continue;
        }
        if (reserving_for_age) {
            continue;
        }
        if (building->kind == BuildingKind::town_center) {
            constexpr std::array town_defense_technologies{
                Technology::town_watch, Technology::town_patrol,
            };
            bool started_town_defense = false;
            for (Technology technology : town_defense_technologies) {
                if (!simulation.has_technology(player_, technology) &&
                    simulation.research_technology_at(
                        building->id, technology
                    )) {
                    started_town_defense = true;
                    break;
                }
            }
            if (started_town_defense) continue;
            if (villager_count >= 4 &&
                !simulation.has_technology(player_, Technology::loom) &&
                simulation.research_technology_at(
                    building->id,
                    Technology::loom
                )) {
                continue;
            }
            if (current_age >= Age::feudal &&
                !simulation.has_technology(
                    player_, Technology::wheelbarrow
                ) &&
                simulation.research_technology_at(
                    building->id, Technology::wheelbarrow
                )) {
                continue;
            }
            if (current_age >= Age::castle &&
                !simulation.has_technology(player_, Technology::hand_cart) &&
                simulation.research_technology_at(
                    building->id, Technology::hand_cart
                )) {
                continue;
            }
            if (villager_count < villager_target &&
                simulation.queue_unit_at(
                    building->id,
                    UnitKind::villager
                )) {
                ++villager_count;
            }
        } else if (building->kind == BuildingKind::barracks) {
            if (current_age >= Age::castle &&
                !simulation.has_technology(player_, Technology::pikeman) &&
                simulation.research_technology_at(
                    building->id, Technology::pikeman
                )) {
                continue;
            }
            if (current_age >= Age::imperial &&
                simulation.has_technology(player_, Technology::pikeman) &&
                civilization_has_technology(
                    simulation.civilization(player_), Technology::halberdier
                ) &&
                !simulation.has_technology(player_, Technology::halberdier) &&
                simulation.research_technology_at(
                    building->id, Technology::halberdier
                )) {
                continue;
            }
            if (current_age >= Age::imperial &&
                civilization_has_technology(
                    simulation.civilization(player_),
                    Technology::elite_eagle_warrior
                ) &&
                !simulation.has_technology(
                    player_, Technology::elite_eagle_warrior
                ) &&
                simulation.research_technology_at(
                    building->id, Technology::elite_eagle_warrior
                )) {
                continue;
            }
            if (current_age >= Age::castle &&
                !simulation.has_technology(
                    player_,
                    Technology::long_swordsman
                ) &&
                simulation.research_technology_at(
                    building->id,
                    Technology::long_swordsman
                )) {
                continue;
            }
            if (current_age >= Age::imperial &&
                !simulation.has_technology(
                    player_,
                    Technology::two_handed_swordsman
                ) &&
                simulation.research_technology_at(
                    building->id,
                    Technology::two_handed_swordsman
                )) {
                continue;
            }
            if (current_age >= Age::imperial &&
                !simulation.has_technology(
                    player_,
                    Technology::champion
                ) &&
                simulation.research_technology_at(
                    building->id,
                    Technology::champion
                )) {
                continue;
            }
            if (current_age >= Age::feudal &&
                !simulation.has_technology(
                    player_,
                    Technology::man_at_arms
                ) &&
                simulation.research_technology_at(
                    building->id,
                    Technology::man_at_arms
                )) {
                continue;
            }
            const bool mesoamerican =
                simulation.civilization(player_) == Civilization::aztecs ||
                simulation.civilization(player_) == Civilization::mayans;
            const UnitKind choice =
                mesoamerican && simulation.tick_number() % 3 == 0
                    ? UnitKind::eagle_warrior :
                planned_counter == UnitKind::spearman ||
                    simulation.tick_number() % 2 == 0
                    ? UnitKind::spearman
                    : UnitKind::militia;
            if (!simulation.queue_unit_at(building->id, choice)) {
                simulation.queue_unit_at(building->id, UnitKind::militia);
            }
        } else if (building->kind == BuildingKind::archery_range) {
            if (current_age >= Age::imperial &&
                simulation.has_technology(player_, Technology::chemistry) &&
                civilization_has_unit(
                    simulation.civilization(player_),
                    UnitKind::hand_cannoneer
                ) &&
                simulation.tick_number() % 3 == 0 &&
                simulation.queue_unit_at(
                    building->id, UnitKind::hand_cannoneer
                )) {
                continue;
            }
            if (current_age >= Age::imperial &&
                civilization_has_technology(
                    simulation.civilization(player_),
                    Technology::heavy_cavalry_archer
                ) &&
                !simulation.has_technology(
                    player_, Technology::heavy_cavalry_archer
                ) &&
                simulation.research_technology_at(
                    building->id, Technology::heavy_cavalry_archer
                )) {
                continue;
            }
            if (current_age >= Age::castle &&
                !simulation.has_technology(
                    player_,
                    Technology::crossbowman
                ) &&
                simulation.research_technology_at(
                    building->id,
                    Technology::crossbowman
                )) {
                continue;
            }
            if (current_age >= Age::castle &&
                !simulation.has_technology(
                    player_,
                    Technology::elite_skirmisher
                ) &&
                simulation.research_technology_at(
                    building->id,
                    Technology::elite_skirmisher
                )) {
                continue;
            }
            if (current_age >= Age::imperial &&
                !simulation.has_technology(
                    player_,
                    Technology::arbalester
                ) &&
                simulation.research_technology_at(
                    building->id,
                    Technology::arbalester
                )) {
                continue;
            }
            const UnitKind choice =
                planned_counter == UnitKind::skirmisher
                    ? UnitKind::skirmisher :
                planned_counter == UnitKind::archer
                    ? UnitKind::archer :
                current_age >= Age::castle &&
                simulation.tick_number() % 3 == 0
                    ? UnitKind::cavalry_archer :
                simulation.tick_number() % 2 == 0
                    ? UnitKind::archer
                    : UnitKind::skirmisher;
            simulation.queue_unit_at(building->id, choice);
        } else if (building->kind == BuildingKind::mill) {
            if (current_age >= Age::imperial &&
                !simulation.has_technology(
                    player_, Technology::crop_rotation
                ) &&
                simulation.research_technology_at(
                    building->id, Technology::crop_rotation
                )) {
                continue;
            }
            if (current_age >= Age::castle &&
                !simulation.has_technology(player_, Technology::heavy_plow) &&
                simulation.research_technology_at(
                    building->id, Technology::heavy_plow
                )) {
                continue;
            }
            if (current_age >= Age::feudal &&
                !simulation.has_technology(
                    player_,
                    Technology::horse_collar
                )) {
                simulation.research_technology_at(
                    building->id,
                    Technology::horse_collar
                );
            }
        } else if (building->kind == BuildingKind::lumber_camp) {
            if (current_age >= Age::imperial &&
                !simulation.has_technology(
                    player_, Technology::two_man_saw
                ) &&
                simulation.research_technology_at(
                    building->id, Technology::two_man_saw
                )) {
                continue;
            }
            if (current_age >= Age::castle &&
                !simulation.has_technology(player_, Technology::bow_saw) &&
                simulation.research_technology_at(
                    building->id, Technology::bow_saw
                )) {
                continue;
            }
            if (current_age >= Age::feudal &&
                !simulation.has_technology(
                    player_,
                    Technology::double_bit_axe
                )) {
                simulation.research_technology_at(
                    building->id,
                    Technology::double_bit_axe
                );
            }
        } else if (building->kind == BuildingKind::mining_camp) {
            constexpr std::array mining_technologies{
                Technology::gold_mining, Technology::stone_mining,
                Technology::gold_shaft_mining,
                Technology::stone_shaft_mining,
            };
            for (Technology technology : mining_technologies) {
                if (civilization_has_technology(
                        simulation.civilization(player_), technology
                    ) &&
                    !simulation.has_technology(player_, technology) &&
                    simulation.research_technology_at(
                        building->id, technology
                    )) {
                    break;
                }
            }
        } else if (building->kind == BuildingKind::blacksmith) {
            if (!simulation.has_technology(
                    player_,
                    Technology::scale_mail_armor
                ) &&
                simulation.research_technology_at(
                    building->id,
                    Technology::scale_mail_armor
                )) {
                continue;
            }
            if (!simulation.has_technology(
                    player_,
                    Technology::chain_mail_armor
                ) &&
                simulation.research_technology_at(
                    building->id,
                    Technology::chain_mail_armor
                )) {
                continue;
            }
            if (!simulation.has_technology(
                    player_,
                    Technology::plate_mail_armor
                ) &&
                simulation.research_technology_at(
                    building->id,
                    Technology::plate_mail_armor
                )) {
                continue;
            }
            if (!simulation.has_technology(
                    player_,
                    Technology::scale_barding_armor
                ) &&
                simulation.research_technology_at(
                    building->id,
                    Technology::scale_barding_armor
                )) {
                continue;
            }
            if (!simulation.has_technology(
                    player_,
                    Technology::chain_barding_armor
                ) &&
                simulation.research_technology_at(
                    building->id,
                    Technology::chain_barding_armor
                )) {
                continue;
            }
            if (!simulation.has_technology(
                    player_,
                    Technology::plate_barding_armor
                ) &&
                simulation.research_technology_at(
                    building->id,
                    Technology::plate_barding_armor
                )) {
                continue;
            }
            if (!simulation.has_technology(
                    player_,
                    Technology::padded_archer_armor
                ) &&
                simulation.research_technology_at(
                    building->id,
                    Technology::padded_archer_armor
                )) {
                continue;
            }
            if (!simulation.has_technology(
                    player_,
                    Technology::leather_archer_armor
                ) &&
                simulation.research_technology_at(
                    building->id,
                    Technology::leather_archer_armor
                )) {
                continue;
            }
            if (!simulation.has_technology(
                    player_,
                    Technology::ring_archer_armor
                ) &&
                simulation.research_technology_at(
                    building->id,
                    Technology::ring_archer_armor
                )) {
                continue;
            }
            if (!simulation.has_technology(
                    player_,
                    Technology::fletching
                ) &&
                simulation.research_technology_at(
                    building->id,
                    Technology::fletching
                )) {
                continue;
            }
            if (!simulation.has_technology(
                    player_,
                    Technology::bodkin_arrow
                ) &&
                simulation.research_technology_at(
                    building->id,
                    Technology::bodkin_arrow
                )) {
                continue;
            }
            if (current_age >= Age::imperial &&
                !simulation.has_technology(
                    player_,
                    Technology::bracer
                ) &&
                simulation.research_technology_at(
                    building->id,
                    Technology::bracer
                )) {
                continue;
            }
            if (!simulation.has_technology(
                    player_,
                    Technology::forging
                )) {
                simulation.research_technology_at(
                    building->id,
                    Technology::forging
                );
            }
            if (current_age >= Age::castle &&
                !simulation.has_technology(
                    player_,
                    Technology::iron_casting
                )) {
                simulation.research_technology_at(
                    building->id,
                    Technology::iron_casting
                );
            }
            if (current_age >= Age::imperial &&
                !simulation.has_technology(
                    player_,
                    Technology::blast_furnace
                )) {
                simulation.research_technology_at(
                    building->id,
                    Technology::blast_furnace
                );
            }
        } else if (building->kind == BuildingKind::monastery) {
            const bool monk_already_queued = std::ranges::any_of(
                building->production_queue,
                [](const ProductionOrder& order) {
                    return order.kind == UnitKind::monk;
                }
            );
            if (visible_relic && !owns_monk &&
                !monk_already_queued &&
                simulation.queue_unit_at(
                    building->id, UnitKind::monk
                )) {
                continue;
            }
            constexpr std::array monastery_technologies{
                Technology::sanctity, Technology::fervor,
                Technology::redemption, Technology::atonement,
                Technology::heresy, Technology::illumination,
                Technology::block_printing, Technology::faith,
                Technology::theocracy, Technology::herbal_medicine,
            };
            bool researching = false;
            for (Technology technology : monastery_technologies) {
                if (civilization_has_technology(
                        simulation.civilization(player_), technology
                    ) &&
                    !simulation.has_technology(player_, technology) &&
                    simulation.research_technology_at(
                        building->id, technology
                    )) {
                    researching = true;
                    break;
                }
            }
            if (!researching) {
                simulation.queue_unit_at(
                    building->id,
                    visible_relic &&
                        !owns_monk && !monk_already_queued
                        ? UnitKind::monk :
                    civilization_has_unit(
                        simulation.civilization(player_),
                        UnitKind::missionary
                    ) ? UnitKind::missionary : UnitKind::monk
                );
            }
        } else if (building->kind == BuildingKind::market) {
            constexpr std::array trade_technologies{
                Technology::coinage, Technology::cartography,
                Technology::banking, Technology::caravan,
                Technology::guilds,
            };
            bool started_trade_technology = false;
            for (Technology technology : trade_technologies) {
                if (civilization_has_technology(
                        simulation.civilization(player_), technology
                    ) &&
                    !simulation.has_technology(player_, technology) &&
                    simulation.research_technology_at(
                        building->id, technology
                    )) {
                    started_trade_technology = true;
                    break;
                }
            }
            if (started_trade_technology) continue;
            const bool allied_market = std::ranges::any_of(
                simulation.buildings(), [&](const Building& candidate) {
                    return candidate.owner != player_ &&
                        !simulation.is_enemy(candidate.owner, player_) &&
                        candidate.kind == BuildingKind::market &&
                        candidate.completed();
                }
            );
            const bool owns_trade_cart = std::ranges::any_of(
                simulation.units(), [&](const Unit& unit) {
                    return unit.owner == player_ &&
                        unit.kind == UnitKind::trade_cart;
                }
            );
            const Economy& trade_economy = simulation.economy(player_);
            if (allied_market && !owns_trade_cart &&
                trade_economy.wood > 300 && trade_economy.gold > 300) {
                simulation.queue_unit_at(
                    building->id, UnitKind::trade_cart
                );
            }
        } else if (building->kind == BuildingKind::dock) {
            const auto unit_count = [&](UnitKind kind) {
                return static_cast<int>(std::ranges::count_if(
                    simulation.units(), [&, kind](const Unit& unit) {
                        return unit.owner == player_ && unit.kind == kind;
                    }
                ));
            };
            const int transports = unit_count(UnitKind::transport_ship);
            const int fishing_ships = unit_count(UnitKind::fishing_ship);
            const int galleys = unit_count(UnitKind::galley) +
                unit_count(UnitKind::war_galley) +
                unit_count(UnitKind::galleon);
            const int warboats = static_cast<int>(std::ranges::count_if(
                simulation.units(), [&](const Unit& unit) {
                    return unit.owner == player_ && is_ship(unit.kind) &&
                        unit.kind != UnitKind::fishing_ship &&
                        unit.kind != UnitKind::transport_ship &&
                        unit.kind != UnitKind::trade_cog;
                }
            ));
            const int military_population = static_cast<int>(
                std::ranges::count_if(
                    simulation.units(), [&](const Unit& unit) {
                        return unit.owner == player_ &&
                            unit.kind != UnitKind::villager &&
                            unit.kind != UnitKind::monk &&
                            unit.kind != UnitKind::missionary &&
                            !is_ship(unit.kind) &&
                            !is_animal(unit.kind) && !is_relic(unit.kind);
                    }
                )
            );
            const bool allied_dock = std::ranges::any_of(
                simulation.buildings(), [&](const Building& candidate) {
                    return candidate.owner != player_ &&
                        !simulation.is_enemy(candidate.owner, player_) &&
                        candidate.kind == BuildingKind::dock &&
                        candidate.completed();
                }
            );
            UnitKind desired_ship = UnitKind::fishing_ship;
            if (military_population > 10 && transports < 2) {
                desired_ship = UnitKind::transport_ship;
            } else if (current_age >= Age::feudal && warboats < 10 &&
                       galleys == 0) {
                const Civilization civilization =
                    simulation.civilization(player_);
                desired_ship = civilization == Civilization::vikings
                    ? UnitKind::longboat
                    : civilization == Civilization::koreans
                        ? UnitKind::turtle_ship
                        : UnitKind::galley;
            } else if (current_age >= Age::feudal && galleys > 0 &&
                       fishing_ships < 20) {
                desired_ship = UnitKind::fishing_ship;
            } else if (current_age >= Age::castle &&
                       unit_count(UnitKind::demolition_ship) == 0) {
                desired_ship = UnitKind::demolition_ship;
            } else if (current_age >= Age::castle &&
                       civilization_has_unit(
                           simulation.civilization(player_),
                           UnitKind::fire_ship
                       )) {
                desired_ship = UnitKind::fire_ship;
            } else if (allied_dock &&
                       unit_count(UnitKind::trade_cog) == 0) {
                desired_ship = UnitKind::trade_cog;
            } else {
                desired_ship = UnitKind::galley;
            }
            if (!civilization_has_unit(
                    simulation.civilization(player_), desired_ship
                )) {
                desired_ship = current_age >= Age::feudal
                    ? UnitKind::galley : UnitKind::fishing_ship;
            }
            simulation.queue_unit_at(building->id, desired_ship);
        } else if (building->kind == BuildingKind::castle) {
            constexpr std::array castle_defense_technologies{
                Technology::hoardings, Technology::sappers,
            };
            bool started_castle_defense = false;
            for (Technology technology : castle_defense_technologies) {
                if (civilization_has_technology(
                        simulation.civilization(player_), technology
                    ) &&
                    !simulation.has_technology(player_, technology) &&
                    simulation.research_technology_at(
                        building->id, technology
                    )) {
                    started_castle_defense = true;
                    break;
                }
            }
            if (started_castle_defense) continue;
            if (current_age >= Age::imperial &&
                civilization_has_technology(
                    simulation.civilization(player_),
                    Technology::conscription
                ) &&
                !simulation.has_technology(
                    player_, Technology::conscription
                ) &&
                simulation.research_technology_at(
                    building->id, Technology::conscription
                )) {
                continue;
            }
            const Technology unique_technology =
                simulation.civilization(player_) == Civilization::britons
                    ? Technology::yeomen
                    : simulation.civilization(player_) ==
                            Civilization::franks
                        ? Technology::bearded_axe
                        : simulation.civilization(player_) ==
                                Civilization::goths
                            ? Technology::anarchy
                            : simulation.civilization(player_) ==
                                    Civilization::teutons
                                ? Technology::crenellations
                                : simulation.civilization(player_) ==
                                        Civilization::japanese
                                    ? Technology::kataparuto
                                    : simulation.civilization(player_) ==
                                            Civilization::chinese
                                        ? Technology::rocketry
                                        : simulation.civilization(player_) ==
                                                Civilization::byzantines
                                            ? Technology::logistica
                                            : simulation.civilization(
                                                  player_
                                              ) == Civilization::persians
                                                ? Technology::mahouts
                                                : simulation.civilization(
                                                      player_
                                                  ) ==
                                                      Civilization::vikings
                                                    ? Technology::berserkergang
                                                    : simulation.civilization(
                                                          player_
                                                      ) ==
                                                          Civilization::saracens
                                                        ? Technology::zealotry
                                                        : simulation.civilization(
                                                              player_
                                                          ) ==
                                                              Civilization::turks
                                                            ? Technology::artillery
                                                            : simulation.civilization(
                                                                  player_
                                                              ) ==
                                                                  Civilization::mongols
                                                                ? Technology::drill
                                                                : simulation.civilization(
                                                                      player_
                                                                  ) ==
                                                                      Civilization::spanish
                                                                    ? Technology::supremacy
                                                                    : simulation.civilization(
                                                                          player_
                                                                      ) ==
                                                                          Civilization::huns
                                                                        ? Technology::atheism
                                                                        : simulation.civilization(
                                                                              player_
                                                                          ) ==
                                                                              Civilization::koreans
                                                                            ? Technology::shinkichon
                                                                            : Technology::el_dorado;
            if (civilization_has_technology(
                    simulation.civilization(player_), unique_technology
                ) &&
                !simulation.has_technology(player_, unique_technology)) {
                simulation.research_technology_at(
                    building->id, unique_technology
                );
            } else if (
                simulation.civilization(player_) == Civilization::celts &&
                current_age >= Age::imperial &&
                !simulation.has_technology(
                    player_, Technology::elite_woad_raider
                ) &&
                simulation.research_technology_at(
                    building->id, Technology::elite_woad_raider
                )) {
                continue;
            } else if (
                simulation.civilization(player_) == Civilization::celts &&
                current_age >= Age::castle &&
                simulation.queue_unit_at(
                    building->id, UnitKind::woad_raider
                )) {
                continue;
            } else if (current_age >= Age::castle &&
                       state_.strategy_epoch % 4 == 0 &&
                       simulation.queue_unit_at(
                           building->id, UnitKind::petard
                       )) {
                continue;
            } else if (current_age >= Age::imperial) {
                simulation.queue_unit_at(
                    building->id, UnitKind::trebuchet
                );
            }
        } else if (building->kind == BuildingKind::university) {
            constexpr std::array university_defense_technologies{
                Technology::masonry, Technology::architecture,
                Technology::ballistics, Technology::heated_shot,
                Technology::stone_cutting,
            };
            bool started_university_defense = false;
            for (Technology technology : university_defense_technologies) {
                if (civilization_has_technology(
                        simulation.civilization(player_), technology
                    ) &&
                    !simulation.has_technology(player_, technology) &&
                    simulation.research_technology_at(
                        building->id, technology
                    )) {
                    started_university_defense = true;
                    break;
                }
            }
            if (started_university_defense) continue;
            if (current_age >= Age::imperial &&
                civilization_has_technology(
                    simulation.civilization(player_),
                    Technology::siege_engineers
                ) &&
                !simulation.has_technology(
                    player_, Technology::siege_engineers
                ) &&
                simulation.research_technology_at(
                    building->id, Technology::siege_engineers
                )) {
                continue;
            }
            if (current_age >= Age::imperial &&
                !simulation.has_technology(player_, Technology::chemistry) &&
                simulation.research_technology_at(
                    building->id, Technology::chemistry
                )) {
                continue;
            }
            if (current_age >= Age::imperial &&
                civilization_has_technology(
                    simulation.civilization(player_),
                    Technology::bombard_tower
                ) &&
                !simulation.has_technology(
                    player_, Technology::bombard_tower
                ) &&
                simulation.research_technology_at(
                    building->id, Technology::bombard_tower
                )) {
                continue;
            }
            if (current_age >= Age::castle &&
                !simulation.has_technology(
                    player_,
                    Technology::guard_tower
                ) &&
                simulation.research_technology_at(
                    building->id,
                    Technology::guard_tower
                )) {
                continue;
            }
            if (current_age >= Age::imperial &&
                !simulation.has_technology(
                    player_,
                    Technology::keep
                ) &&
                simulation.research_technology_at(
                    building->id,
                    Technology::keep
                )) {
                continue;
            }
            if (current_age >= Age::castle &&
                !simulation.has_technology(
                    player_,
                    Technology::fortified_wall
                ) &&
                simulation.research_technology_at(
                    building->id,
                    Technology::fortified_wall
                )) {
                continue;
            }
            if (!simulation.has_technology(
                    player_,
                    Technology::murder_holes
                )) {
                simulation.research_technology_at(
                    building->id,
                    Technology::murder_holes
                );
            }
        } else if (building->kind == BuildingKind::stable) {
            if (current_age >= Age::imperial &&
                civilization_has_technology(
                    simulation.civilization(player_),
                    Technology::heavy_camel
                ) &&
                !simulation.has_technology(
                    player_, Technology::heavy_camel
                ) &&
                simulation.research_technology_at(
                    building->id, Technology::heavy_camel
                )) {
                continue;
            }
            if (!simulation.has_technology(
                    player_,
                    Technology::bloodlines
                ) &&
                simulation.research_technology_at(
                    building->id,
                    Technology::bloodlines
                )) {
                continue;
            }
            if (!simulation.has_technology(
                    player_,
                    Technology::husbandry
                ) &&
                simulation.research_technology_at(
                    building->id,
                    Technology::husbandry
                )) {
                continue;
            }
            if (!simulation.has_technology(
                    player_,
                    Technology::light_cavalry
                ) &&
                simulation.research_technology_at(
                    building->id,
                    Technology::light_cavalry
                )) {
                continue;
            }
            if (!simulation.has_technology(
                    player_,
                    Technology::hussar
                ) &&
                simulation.research_technology_at(
                    building->id,
                    Technology::hussar
                )) {
                continue;
            }
            if (!simulation.has_technology(
                    player_,
                    Technology::cavalier
                ) &&
                simulation.research_technology_at(
                    building->id,
                    Technology::cavalier
                )) {
                continue;
            }
            if (!simulation.has_technology(
                    player_,
                    Technology::paladin
                ) &&
                simulation.research_technology_at(
                    building->id,
                    Technology::paladin
                )) {
                continue;
            }
            if (current_age >= Age::castle &&
                civilization_has_unit(
                    simulation.civilization(player_),
                    UnitKind::camel_rider
                ) &&
                simulation.tick_number() % 3 == 0 &&
                simulation.queue_unit_at(
                    building->id, UnitKind::camel_rider
                )) {
                continue;
            }
            if (!simulation.queue_unit_at(
                    building->id,
                    UnitKind::knight
                )) {
                simulation.queue_unit_at(
                    building->id,
                    UnitKind::scout_cavalry
                );
            }
        } else if (building->kind == BuildingKind::siege_workshop) {
            if (current_age >= Age::imperial &&
                simulation.has_technology(player_, Technology::chemistry) &&
                civilization_has_unit(
                    simulation.civilization(player_),
                    UnitKind::bombard_cannon
                ) &&
                simulation.tick_number() % 4 == 0 &&
                simulation.queue_unit_at(
                    building->id, UnitKind::bombard_cannon
                )) {
                continue;
            }
            if (current_age >= Age::imperial &&
                !simulation.has_technology(
                    player_, Technology::capped_ram
                ) &&
                simulation.research_technology_at(
                    building->id, Technology::capped_ram
                )) {
                continue;
            }
            if (current_age >= Age::imperial &&
                simulation.has_technology(player_, Technology::capped_ram) &&
                civilization_has_technology(
                    simulation.civilization(player_), Technology::siege_ram
                ) &&
                !simulation.has_technology(player_, Technology::siege_ram) &&
                simulation.research_technology_at(
                    building->id, Technology::siege_ram
                )) {
                continue;
            }
            if (current_age >= Age::imperial &&
                !simulation.has_technology(player_, Technology::onager) &&
                simulation.research_technology_at(
                    building->id, Technology::onager
                )) {
                continue;
            }
            if (current_age >= Age::imperial &&
                simulation.has_technology(player_, Technology::onager) &&
                !simulation.has_technology(
                    player_, Technology::siege_onager
                ) &&
                simulation.research_technology_at(
                    building->id, Technology::siege_onager
                )) {
                continue;
            }
            if (current_age >= Age::imperial &&
                !simulation.has_technology(
                    player_, Technology::heavy_scorpion
                ) &&
                simulation.research_technology_at(
                    building->id, Technology::heavy_scorpion
                )) {
                continue;
            }
            simulation.queue_unit_at(
                building->id,
                simulation.tick_number() % 3 == 0
                    ? UnitKind::scorpion :
                simulation.tick_number() % 2 == 0
                    ? UnitKind::battering_ram
                    : UnitKind::mangonel
            );
        }
    }

    const bool house_under_construction = std::ranges::any_of(
        owned_buildings,
        [](const Building* building) {
            return building->kind == BuildingKind::house &&
                !building->completed();
        }
    );
    if (!idle_villagers.empty()) {
        const auto builder = std::ranges::find_if(
            simulation.units(),
            [id = idle_villagers.front()](const Unit& unit) {
                return unit.id == id;
            }
        );
        if (builder != simulation.units().end()) {
            const int spare_population =
                simulation.population_capacity(player_) -
                simulation.population(player_);
            const int farm_count = static_cast<int>(std::ranges::count_if(
                owned_buildings,
                [](const Building* building) {
                    return building->kind == BuildingKind::farm;
                }
            ));
            std::optional<BuildingKind> desired;
            if (spare_population <= 2 && !house_under_construction) {
                desired = BuildingKind::house;
            } else if (current_age == Age::dark) {
                if (!has_building(BuildingKind::mill, false)) {
                    desired = BuildingKind::mill;
                } else if (!has_building(
                               BuildingKind::lumber_camp, false
                           )) {
                    desired = BuildingKind::lumber_camp;
                } else if (!has_building(BuildingKind::barracks, false)) {
                    desired = BuildingKind::barracks;
                } else if (villager_count >= 10 &&
                           !has_building(
                               BuildingKind::mining_camp, false
                           )) {
                    desired = BuildingKind::mining_camp;
                }
            } else if (current_age == Age::feudal) {
                const bool explored_water = std::ranges::any_of(
                    simulation.units(),
                    [player_, &simulation](const Unit& unit) {
                        if (unit.owner != player_ ||
                            unit.kind != UnitKind::villager) {
                            return false;
                        }
                        for (int dy = -2; dy <= 2; ++dy) {
                            for (int dx = -2; dx <= 2; ++dx) {
                                if (std::abs(dx) + std::abs(dy) > 2) {
                                    continue;
                                }
                                const TilePosition nearby{
                                    unit.position.x + dx,
                                    unit.position.y + dy,
                                };
                                if (simulation.map().contains(nearby) &&
                                    simulation.map().sailable(nearby) &&
                                    simulation.is_explored(
                                        player_, nearby
                                    )) {
                                    return true;
                                }
                            }
                        }
                        return false;
                    }
                );
                if (explored_water &&
                    !has_building(BuildingKind::dock, false)) {
                    desired = BuildingKind::dock;
                } else if (!has_building(BuildingKind::market, false)) {
                    desired = BuildingKind::market;
                } else if (!has_building(BuildingKind::blacksmith, false)) {
                    desired = BuildingKind::blacksmith;
                } else if (planned_counter == UnitKind::skirmisher ||
                           planned_counter == UnitKind::archer) {
                    if (!has_building(
                            BuildingKind::archery_range, false
                        )) {
                        desired = BuildingKind::archery_range;
                    }
                } else if (!has_building(BuildingKind::stable, false)) {
                    desired = BuildingKind::stable;
                }
            } else if (current_age == Age::castle) {
                const int town_centers = static_cast<int>(
                    std::ranges::count_if(
                        owned_buildings, [](const Building* building) {
                            return building->kind ==
                                BuildingKind::town_center;
                        }
                    )
                );
                if (state_.difficulty <= ComputerDifficulty::moderate &&
                    town_centers < 2) {
                    desired = BuildingKind::town_center;
                } else if (!has_building(
                               BuildingKind::siege_workshop,
                               false
                           )) {
                    desired = BuildingKind::siege_workshop;
                } else if (!has_building(
                           BuildingKind::university,
                           false
                       )) {
                    desired = BuildingKind::university;
                } else if (!has_building(
                               BuildingKind::monastery, false
                           )) {
                    desired = BuildingKind::monastery;
                }
            } else if (current_age == Age::imperial &&
                       simulation.match_rules().wonder_enabled &&
                       !has_building(BuildingKind::wonder, false)) {
                desired = BuildingKind::wonder;
            } else if (current_age == Age::imperial &&
                       simulation.has_technology(
                           player_, Technology::bombard_tower
                       ) &&
                       !has_building(
                           BuildingKind::bombard_tower, false
                       )) {
                desired = BuildingKind::bombard_tower;
            }
            if (!desired &&
                has_building(BuildingKind::mill, true) &&
                !has_explored_natural_food(player_, simulation) &&
                simulation.economy(player_).food < 300 &&
                farm_count < std::min(villager_count, 4)) {
                desired = BuildingKind::farm;
            }
            const bool built = desired &&
                try_construct_nearby(simulation, *builder, *desired);
            if (built) {
                idle_villagers.erase(idle_villagers.begin());
                owned_buildings.clear();
                for (const Building& building : simulation.buildings()) {
                    if (building.owner == player_) {
                        owned_buildings.push_back(&building);
                    }
                }
            }
        }
    }

    const int mining_camp_count = static_cast<int>(std::ranges::count_if(
        owned_buildings, [](const Building* building) {
            return building->kind == BuildingKind::mining_camp &&
                building->completed();
        }
    ));
    const bool needs_first_castle =
        !has_building(BuildingKind::castle, true) &&
        static_cast<int>(std::ranges::count_if(
            owned_buildings, [](const Building* building) {
                return building->kind == BuildingKind::town_center &&
                    building->completed();
            }
        )) < 2;
    const ClassicAiGatherPlan gather_plan = classic_ai_gather_plan(
        current_age, villager_count, mining_camp_count, reserving_for_age,
        needs_first_castle, state_.difficulty
    );
    for (const Unit& worker : simulation.units()) {
        if (worker.owner != player_ ||
            worker.kind != UnitKind::villager) continue;
        ResourceKind assigned = worker.carried_resource;
        if (assigned == ResourceKind::none &&
            worker.has_resource_target &&
            simulation.map().contains(worker.resource_target)) {
            assigned = resource_kind(
                simulation.map().terrain_at(worker.resource_target)
            );
            if (assigned == ResourceKind::none &&
                worker.resource_building_id != 0) {
                assigned = ResourceKind::food;
            }
        }
        if (assigned != ResourceKind::none) {
            ++status_.resource_workers[
                static_cast<std::size_t>(assigned) - 1
            ];
        }
    }
    for (std::size_t index = 0; index < idle_villagers.size(); ++index) {
        const auto villager = std::ranges::find_if(
            simulation.units(),
            [id = idle_villagers[index]](const Unit& unit) {
                return unit.id == id;
            }
        );
        if (villager == simulation.units().end()) {
            continue;
        }
        std::array<ResourceKind, 4> priorities{
            ResourceKind::wood, ResourceKind::food,
            ResourceKind::gold, ResourceKind::stone,
        };
        std::ranges::stable_sort(
            priorities, [&](ResourceKind left, ResourceKind right) {
                const auto left_index = static_cast<std::size_t>(left) - 1;
                const auto right_index = static_cast<std::size_t>(right) - 1;
                const int left_deficit =
                    gather_plan.percentages[left_index] * villager_count -
                    status_.resource_workers[left_index] * 100;
                const int right_deficit =
                    gather_plan.percentages[right_index] * villager_count -
                    status_.resource_workers[right_index] * 100;
                return left_deficit > right_deficit;
            }
        );
        for (const ResourceKind resource : priorities) {
            if (const auto target =
                    nearest_resource(*villager, resource, simulation);
                target && simulation.command_unit(villager->id, *target)) {
                ++status_.resource_workers[
                    static_cast<std::size_t>(resource) - 1
                ];
                break;
            }
        }
    }

    std::vector<EntityId> land_army;
    std::vector<EntityId> naval_army;
    std::vector<EntityId> transports;
    for (const Unit& unit : simulation.units()) {
        if (unit.owner != player_ || unit.garrisoned_in != 0 ||
            unit.kind == UnitKind::villager ||
            is_animal(unit.kind) || is_relic(unit.kind)) continue;
        if (unit.kind == UnitKind::monk) {
            if (unit.carrying_relic) {
                const auto monastery = std::ranges::find_if(
                    owned_buildings, [](const Building* building) {
                        return building->kind == BuildingKind::monastery &&
                            building->completed();
                    }
                );
                if (monastery != owned_buildings.end()) {
                    if (unit.relic_deposit_target_id !=
                            (*monastery)->id &&
                        !simulation.command_deposit_relic(
                            unit.id, (*monastery)->id
                        )) {
                        if (!unit.moving) {
                            if (const auto approach = adjacent_to_building(
                                **monastery, unit, simulation)) {
                                simulation.command_unit(unit.id, *approach);
                            }
                        }
                    }
                    status_.objective = ComputerObjective::relic;
                    status_.target = (*monastery)->position;
                }
            } else {
                const auto relic = std::ranges::find_if(
                    simulation.units(),
                    [player_, &simulation](const Unit& candidate) {
                        return candidate.kind == UnitKind::relic &&
                            candidate.garrisoned_in == 0 &&
                            simulation.is_visible(
                                player_, candidate.position
                            );
                    }
                );
                if (relic != simulation.units().end()) {
                    if (unit.relic_target_id != relic->id &&
                        !simulation.command_collect_relic(
                            unit.id, relic->id
                        )) {
                        if (!unit.moving) {
                            if (const auto approach = adjacent_walkable(
                                relic->position, unit, simulation)) {
                                simulation.command_unit(unit.id, *approach);
                            }
                        }
                    }
                    status_.objective = ComputerObjective::relic;
                    status_.target = relic->position;
                }
            }
            continue;
        }
        if (unit.kind == UnitKind::trade_cart ||
            unit.kind == UnitKind::trade_cog) {
            if (unit.trade_target_market_id == 0) {
                const auto market = std::ranges::find_if(
                    simulation.buildings(),
                    [&, player_](const Building& building) {
                        const bool correct_kind =
                            unit.kind == UnitKind::trade_cog
                                ? building.kind == BuildingKind::dock
                                : building.kind == BuildingKind::market;
                        return correct_kind && building.completed() &&
                            building.owner != player_ &&
                            !simulation.is_enemy(
                                building.owner, player_
                            ) &&
                            simulation.is_building_visible(
                                player_, building
                            );
                    }
                );
                if (market != simulation.buildings().end() &&
                    simulation.command_trade_route(unit.id, market->id)) {
                    status_.objective = ComputerObjective::trade;
                    status_.target = market->position;
                }
            }
            continue;
        }
        if (unit.kind == UnitKind::fishing_ship) {
            if (!unit.has_resource_target && !unit.moving) {
                const auto trap = std::ranges::find_if(
                    owned_buildings,
                    [](const Building* building) {
                        return building->kind == BuildingKind::fish_trap &&
                            building->completed() &&
                            building->resource_amount > 0;
                    }
                );
                if (trap != owned_buildings.end() &&
                    simulation.command_unit(unit.id, (*trap)->position)) {
                    continue;
                }
                std::optional<TilePosition> fish;
                int fish_distance = std::numeric_limits<int>::max();
                for (int y = 0; y < simulation.map().height(); ++y) {
                    for (int x = 0; x < simulation.map().width(); ++x) {
                        const TilePosition candidate{x, y};
                        if (simulation.map().terrain_at(candidate) !=
                                Terrain::fish ||
                            simulation.map().resource_amount_at(candidate) <=
                                0 ||
                            !simulation.is_explored(player_, candidate)) {
                            continue;
                        }
                        const int candidate_distance =
                            distance(unit.position, candidate);
                        if (candidate_distance < fish_distance) {
                            fish = candidate;
                            fish_distance = candidate_distance;
                        }
                    }
                }
                if (fish && simulation.command_unit(unit.id, *fish)) {
                    continue;
                }
                if (simulation.has_technology(
                        player_, Technology::fish_trap_gate
                    )) {
                    try_construct_nearby(
                        simulation, unit, BuildingKind::fish_trap
                    );
                }
            }
            continue;
        }
        if (unit.kind == UnitKind::transport_ship) {
            transports.push_back(unit.id);
            continue;
        }
        if (is_ship(unit.kind)) {
            naval_army.push_back(unit.id);
            ++status_.naval_units;
        } else {
            land_army.push_back(unit.id);
            if (unit.kind == UnitKind::battering_ram ||
                unit.kind == UnitKind::capped_ram ||
                unit.kind == UnitKind::siege_ram ||
                unit.kind == UnitKind::mangonel ||
                unit.kind == UnitKind::onager ||
                unit.kind == UnitKind::siege_onager ||
                unit.kind == UnitKind::scorpion ||
                unit.kind == UnitKind::heavy_scorpion ||
                unit.kind == UnitKind::trebuchet ||
                unit.kind == UnitKind::packed_trebuchet ||
                unit.kind == UnitKind::bombard_cannon) {
                ++status_.siege_units;
            } else if (unit.kind == UnitKind::scout_cavalry ||
                       unit.kind == UnitKind::knight ||
                       unit.kind == UnitKind::cavalier ||
                       unit.kind == UnitKind::paladin ||
                       unit.kind == UnitKind::light_cavalry ||
                       unit.kind == UnitKind::hussar ||
                       unit.kind == UnitKind::camel_rider ||
                       unit.kind == UnitKind::heavy_camel) {
                ++status_.cavalry_units;
            } else if (rules_for(unit.kind).attack_range > 1) {
                ++status_.ranged_units;
            } else {
                ++status_.melee_units;
            }
        }
    }

    std::optional<TilePosition> land_target;
    std::optional<ComputerObjective> victory_target_objective;
    if (!land_army.empty()) {
        const auto representative = std::ranges::find_if(
            simulation.units(), [id = land_army.front()](const Unit& unit) {
                return unit.id == id;
            }
        );
        if (representative != simulation.units().end()) {
            if (const auto target = nearest_enemy(
                    *representative, simulation, state_.last_target_id,
                    state_.difficulty
                )) {
                land_target = target->position;
                state_.last_target_id = target->id;
            } else {
                state_.last_target_id = 0;
            }
        }
    }
    const Player opponent =
        player_ == Player::blue ? Player::red : Player::blue;
    if (!land_army.empty() &&
        simulation.is_enemy(opponent, player_) &&
        simulation.victory_countdown(opponent) > 0) {
        const VictoryCountdownKind countdown =
            simulation.countdown_kind(opponent);
        const auto objective = std::ranges::find_if(
            simulation.buildings(),
            [&, countdown](const Building& building) {
                if (building.owner != opponent ||
                    !building.completed() ||
                    !simulation.is_building_visible(player_, building)) {
                    return false;
                }
                if (countdown == VictoryCountdownKind::wonder) {
                    return building.kind == BuildingKind::wonder;
                }
                return countdown == VictoryCountdownKind::relic &&
                    building.kind == BuildingKind::monastery &&
                    building.relic_count > 0;
            }
        );
        if (objective != simulation.buildings().end()) {
            land_target = objective->position;
            victory_target_objective =
                countdown == VictoryCountdownKind::wonder
                    ? ComputerObjective::wonder
                    : ComputerObjective::relic;
        }
    }
    std::optional<TilePosition> naval_target;
    if (!naval_army.empty()) {
        const auto representative = std::ranges::find_if(
            simulation.units(), [id = naval_army.front()](const Unit& unit) {
                return unit.id == id;
            }
        );
        if (representative != simulation.units().end()) {
            int nearest_distance = std::numeric_limits<int>::max();
            for (const Unit& enemy : simulation.units()) {
                if (!is_ship(enemy.kind) ||
                    !simulation.is_enemy(enemy.owner, player_) ||
                    !simulation.is_visible(player_, enemy.position)) {
                    continue;
                }
                const int candidate_distance = distance(
                    representative->position, enemy.position
                );
                if (candidate_distance < nearest_distance) {
                    naval_target = enemy.position;
                    nearest_distance = candidate_distance;
                }
            }
            for (const Building& enemy : simulation.buildings()) {
                if (!simulation.is_enemy(enemy.owner, player_) ||
                    !simulation.is_building_visible(player_, enemy)) {
                    continue;
                }
                bool water_access = false;
                const BuildingRules& rules = rules_for(enemy.kind);
                for (int y = enemy.position.y - 1;
                     y <= enemy.position.y + rules.footprint_height;
                     ++y) {
                    for (int x = enemy.position.x - 1;
                         x <= enemy.position.x + rules.footprint_width;
                         ++x) {
                        const TilePosition tile{x, y};
                        water_access = water_access ||
                            (simulation.map().contains(tile) &&
                             (simulation.map().terrain_at(tile) ==
                                  Terrain::water ||
                              simulation.map().terrain_at(tile) ==
                                  Terrain::fish));
                    }
                }
                const int candidate_distance = distance(
                    representative->position, enemy.position
                );
                if (water_access &&
                    candidate_distance < nearest_distance) {
                    naval_target = enemy.position;
                    nearest_distance = candidate_distance;
                }
            }
        }
    }

    int enemy_land_military = 0;
    for (const Unit& unit : simulation.units()) {
        if (!simulation.is_enemy(unit.owner, player_) ||
            is_ship(unit.kind) || is_animal(unit.kind) ||
            is_relic(unit.kind) || unit.kind == UnitKind::villager ||
            unit.kind == UnitKind::monk ||
            unit.kind == UnitKind::missionary ||
            unit.kind == UnitKind::trade_cart ||
            unit.kind == UnitKind::fishing_ship ||
            unit.kind == UnitKind::transport_ship) {
            continue;
        }
        ++enemy_land_military;
    }
    const int own_land_military = static_cast<int>(land_army.size());
    const bool outnumbered =
        (enemy_land_military > 50 && own_land_military < 45) ||
        (enemy_land_military > 40 && own_land_military < 35) ||
        (enemy_land_military > 30 && own_land_military < 25) ||
        (enemy_land_military > 20 && own_land_military < 15) ||
        (enemy_land_military > 10 && own_land_military < 5) ||
        (enemy_land_military > 0 && own_land_military == 0);
    if (outnumbered && state_.home_anchor.x >= 0 &&
        !land_army.empty()) {
        if (state_.rally_point.x >= 0 &&
            simulation.command_formation(
                land_army, state_.rally_point, FormationKind::box
            )) {
            state_.retreating = true;
            status_.objective = ComputerObjective::regroup;
            status_.target = state_.rally_point;
        }
    } else if (state_.retreating && !outnumbered) {
        state_.retreating = false;
    }

    const ClassicAiAttackProfile attack_profile =
        classic_ai_attack_profile(state_.difficulty);
    if (!state_.attack_timer_armed && current_age >=
            attack_profile.minimum_age) {
        state_.attack_timer_armed = true;
        state_.next_attack_tick = simulation.tick_number() +
            attack_profile.initial_delay;
    }
    bool attack_timer_ready = state_.attack_timer_armed &&
        simulation.tick_number() >= state_.next_attack_tick;
    if (attack_timer_ready) {
        state_.next_attack_tick = simulation.tick_number() +
            attack_profile.repeat_interval;
    }

    int attack_threshold = enemy_land_military <= 10 ? 10 :
        enemy_land_military <= 20 ? 20 : 30;
    if (state_.difficulty == ComputerDifficulty::easiest) {
        attack_threshold = 1;
    }
    if (victory_target_objective) {
        attack_threshold = std::max(
            1, simulation.population_capacity(player_) * 15 / 100
        );
    }

    if (!state_.retreating && land_target && !land_army.empty()) {
        const bool defending = state_.home_anchor.x >= 0 &&
            distance(state_.home_anchor, *land_target) <= 14;
        if (defending || (victory_target_objective &&
            static_cast<int>(land_army.size()) >= attack_threshold) ||
            state_.home_anchor.x < 0 || (attack_timer_ready &&
            static_cast<int>(land_army.size()) >= attack_threshold)) {
            std::vector<EntityId> order_army;
            for (const EntityId id : land_army) {
                const auto unit = std::ranges::find_if(
                    simulation.units(), [id](const Unit& candidate) {
                        return candidate.id == id;
                    }
                );
                if (unit == simulation.units().end() ||
                    unit->attack_target_id == 0) {
                    order_army.push_back(id);
                }
            }
            bool formed = order_army.empty();
            if ((defending && order_army.size() == 1) ||
                state_.home_anchor.x < 0) {
                for (const EntityId id : order_army) {
                    formed = simulation.command_unit(id, *land_target) ||
                        formed;
                }
            } else {
                formed = simulation.command_formation_order(
                    order_army, *land_target,
                    defending ? FormationKind::box : FormationKind::flank,
                    FormationOrderKind::attack_move
                );
            }
            if (!formed) {
                for (const EntityId id : order_army) {
                    simulation.command_unit(id, *land_target);
                }
            }
            state_.last_attack_tick = simulation.tick_number();
            status_.objective = defending
                ? ComputerObjective::defend
                : victory_target_objective.value_or(
                      ComputerObjective::attack
                  );
            status_.target = *land_target;
        } else if (state_.home_anchor.x < 0 && attack_timer_ready) {
            for (EntityId id : land_army) {
                const auto unit = std::ranges::find_if(
                    simulation.units(), [id](const Unit& candidate) {
                        return candidate.id == id;
                    }
                );
                if (unit != simulation.units().end() && !unit->moving) {
                    if (unit->kind == UnitKind::packed_trebuchet) {
                        simulation.command_pack_trebuchet(id, false);
                    } else {
                        simulation.command_unit(id, *land_target);
                    }
                }
            }
            status_.objective = defending
                ? ComputerObjective::defend
                : victory_target_objective.value_or(
                      ComputerObjective::attack
                  );
            status_.target = *land_target;
        } else if (state_.rally_point.x >= 0) {
            simulation.command_formation(
                land_army, state_.rally_point, FormationKind::box
            );
            status_.objective = ComputerObjective::regroup;
            status_.target = state_.rally_point;
        }
    } else if (!state_.retreating) {
        for (EntityId id : land_army) {
            const auto unit = std::ranges::find_if(
                simulation.units(), [id](const Unit& candidate) {
                    return candidate.id == id;
                }
            );
            if (unit == simulation.units().end() || unit->moving) continue;
            if (unit->kind == UnitKind::trebuchet) {
                simulation.command_pack_trebuchet(id, true);
            } else if (const auto unexplored =
                           nearest_unexplored(*unit, simulation)) {
                simulation.command_unit(id, *unexplored);
                status_.objective = ComputerObjective::scout;
                status_.target = *unexplored;
            } else if (state_.rally_point.x >= 0) {
                simulation.command_unit(id, state_.rally_point);
                status_.objective = ComputerObjective::regroup;
                status_.target = state_.rally_point;
            }
        }
    }

    if (naval_target && !naval_army.empty()) {
        simulation.command_formation_order(
            naval_army, *naval_target, FormationKind::line,
            FormationOrderKind::attack_move
        );
        status_.objective = ComputerObjective::naval;
        status_.target = *naval_target;
    }

    for (EntityId transport_id : transports) {
        const auto transport = std::ranges::find_if(
            simulation.units(), [transport_id](const Unit& unit) {
                return unit.id == transport_id;
            }
        );
        if (transport == simulation.units().end()) continue;
        const bool loaded = std::ranges::any_of(
            simulation.units(), [transport_id](const Unit& unit) {
                return unit.garrisoned_in == transport_id;
            }
        );
        if (!loaded) {
            for (EntityId unit_id : land_army) {
                const auto passenger = std::ranges::find_if(
                    simulation.units(), [unit_id](const Unit& unit) {
                        return unit.id == unit_id;
                    }
                );
                if (passenger != simulation.units().end() &&
                    distance(
                        passenger->position, transport->position
                    ) <= 1 &&
                    simulation.command_embark(unit_id, transport_id)) {
                    status_.objective = ComputerObjective::transport;
                    break;
                } else if (passenger != simulation.units().end()) {
                    if (const auto rendezvous = adjacent_walkable(
                            transport->position,
                            *passenger,
                            simulation)) {
                        simulation.command_unit(unit_id, *rendezvous);
                        status_.objective =
                            ComputerObjective::transport;
                        status_.target = *rendezvous;
                        break;
                    }
                }
            }
        } else {
            std::vector<TilePosition> landing_candidates;
            if (land_target) {
                for (int y = 0; y < simulation.map().height(); ++y) {
                    for (int x = 0; x < simulation.map().width(); ++x) {
                        const TilePosition water{x, y};
                        if (!simulation.map().sailable(water)) continue;
                        bool shore = false;
                        for (TilePosition adjacent : {
                                 TilePosition{x + 1, y},
                                 TilePosition{x - 1, y},
                                 TilePosition{x, y + 1},
                                 TilePosition{x, y - 1},
                             }) {
                            shore = shore ||
                                (simulation.map().contains(adjacent) &&
                                 simulation.map().walkable(adjacent));
                        }
                        if (shore) landing_candidates.push_back(water);
                    }
                }
            }
            std::ranges::sort(
                landing_candidates,
                [land_target](TilePosition left, TilePosition right) {
                    const int left_distance =
                        distance(left, *land_target);
                    const int right_distance =
                        distance(right, *land_target);
                    if (left_distance != right_distance) {
                        return left_distance < right_distance;
                    }
                    if (left.y != right.y) return left.y < right.y;
                    return left.x < right.x;
                }
            );
            if (transport->moving) {
                status_.objective = ComputerObjective::transport;
                status_.target = transport->destination;
                continue;
            }
            std::optional<TilePosition> landing_water;
            bool routed = false;
            for (TilePosition candidate : landing_candidates) {
                if (candidate == transport->position) {
                    landing_water = candidate;
                    break;
                }
                if (simulation.command_unit(transport_id, candidate)) {
                    routed = true;
                    status_.objective = ComputerObjective::transport;
                    status_.target = candidate;
                    break;
                }
            }
            if (routed) continue;
            if (landing_water) {
                std::optional<TilePosition> landing_shore;
                int shore_distance = std::numeric_limits<int>::max();
                for (TilePosition shore : {
                         TilePosition{transport->position.x + 1,
                                      transport->position.y},
                         TilePosition{transport->position.x - 1,
                                      transport->position.y},
                         TilePosition{transport->position.x,
                                      transport->position.y + 1},
                         TilePosition{transport->position.x,
                                      transport->position.y - 1},
                     }) {
                    const int candidate_distance =
                        land_target ? distance(shore, *land_target) : 0;
                    if (simulation.map().contains(shore) &&
                        simulation.map().walkable(shore) &&
                        candidate_distance < shore_distance) {
                        landing_shore = shore;
                        shore_distance = candidate_distance;
                    }
                }
                if (landing_shore &&
                    simulation.command_disembark(
                        transport_id, *landing_shore
                    )) {
                    status_.objective = ComputerObjective::transport;
                    status_.target = *landing_shore;
                }
            }
        }
    }

    if (current_age == Age::imperial &&
        std::ranges::any_of(
            owned_buildings, [](const Building* building) {
                return building->kind == BuildingKind::wonder;
            }
        )) {
        status_.objective = ComputerObjective::wonder;
    }

    const bool has_surviving_unit = std::ranges::any_of(
        simulation.units(), [player_](const Unit& unit) {
            return unit.owner == player_ &&
                !is_animal(unit.kind) && !is_relic(unit.kind);
        }
    );
    if (owned_buildings.empty() && !has_surviving_unit) {
        simulation.resign(player_);
    }
    status_.retreating = state_.retreating;
}

}  // namespace aoe
