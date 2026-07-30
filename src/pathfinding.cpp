#include "aoe/pathfinding.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <queue>

namespace aoe {
namespace {

struct SearchNode {
    TilePosition position;
    int estimated_total_cost;
};

struct CheapestFirst {
    bool operator()(const SearchNode& left, const SearchNode& right) const {
        return left.estimated_total_cost > right.estimated_total_cost;
    }
};

int distance(TilePosition left, TilePosition right) {
    return std::abs(left.x - right.x) + std::abs(left.y - right.y);
}

std::size_t index(const GameMap& map, TilePosition position) {
    return static_cast<std::size_t>(position.y * map.width() + position.x);
}

}  // namespace

std::vector<TilePosition> find_path(
    const GameMap& map,
    TilePosition start,
    TilePosition goal,
    const TileBlocked& blocked,
    const TileTraversable& traversable
) {
    const auto can_traverse = [&map, &traversable](TilePosition position) {
        return traversable ? traversable(position) : map.walkable(position);
    };
    if (start == goal || !map.contains(start) || !can_traverse(goal)) {
        return {};
    }

    const std::size_t tile_count =
        static_cast<std::size_t>(map.width() * map.height());
    const int unreachable = std::numeric_limits<int>::max();
    std::vector<int> cost(tile_count, unreachable);
    std::vector<TilePosition> previous(tile_count, {-1, -1});
    std::priority_queue<
        SearchNode,
        std::vector<SearchNode>,
        CheapestFirst
    > frontier;

    cost[index(map, start)] = 0;
    frontier.push({start, distance(start, goal)});

    constexpr std::array<TilePosition, 4> directions{
        TilePosition{1, 0},
        TilePosition{-1, 0},
        TilePosition{0, 1},
        TilePosition{0, -1},
    };

    while (!frontier.empty()) {
        const TilePosition current = frontier.top().position;
        frontier.pop();

        if (current == goal) {
            break;
        }

        for (TilePosition direction : directions) {
            const TilePosition next{
                current.x + direction.x,
                current.y + direction.y,
            };
            if (!can_traverse(next) ||
                (!traversable && !map.traversable(current, next)) ||
                (next != goal && blocked(next))) {
                continue;
            }

            const int next_cost = cost[index(map, current)] + 1;
            if (next_cost >= cost[index(map, next)]) {
                continue;
            }
            cost[index(map, next)] = next_cost;
            previous[index(map, next)] = current;
            frontier.push({next, next_cost + distance(next, goal)});
        }
    }

    if (cost[index(map, goal)] == unreachable) {
        return {};
    }

    std::vector<TilePosition> path;
    for (TilePosition current = goal; current != start;) {
        path.push_back(current);
        current = previous[index(map, current)];
    }
    std::ranges::reverse(path);
    return path;
}

}  // namespace aoe
