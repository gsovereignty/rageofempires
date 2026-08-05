#include "aoe/ordinary_match_setup.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace aoe {
namespace {

constexpr double pi = 3.14159265358979323846;

TilePosition start_position(
    int dimension, std::size_t ordinal, std::size_t count
) {
    const double angle = -pi / 2.0 +
        2.0 * pi * static_cast<double>(ordinal) /
            static_cast<double>(count);
    const double radius = static_cast<double>(dimension) * 0.34;
    const int center = dimension / 2;
    return {
        std::clamp(
            center + static_cast<int>(std::lround(std::cos(angle) * radius)),
            8, dimension - 9
        ),
        std::clamp(
            center + static_cast<int>(std::lround(std::sin(angle) * radius)),
            8, dimension - 9
        ),
    };
}

void clear_start(Scenario& scenario, TilePosition start) {
    const auto nearby = [start](TilePosition position) {
        return std::abs(position.x - start.x) <= 5 &&
            std::abs(position.y - start.y) <= 5;
    };
    scenario.units.erase(
        std::remove_if(
            scenario.units.begin(), scenario.units.end(),
            [&](const UnitPlacement& unit) { return nearby(unit.position); }
        ), scenario.units.end()
    );
    scenario.buildings.erase(
        std::remove_if(
            scenario.buildings.begin(), scenario.buildings.end(),
            [&](const BuildingPlacement& building) {
                return nearby(building.position);
            }
        ), scenario.buildings.end()
    );
    for (int y = start.y - 4; y <= start.y + 4; ++y) {
        for (int x = start.x - 4; x <= start.x + 4; ++x) {
            const TilePosition tile{x, y};
            if (!scenario.map.contains(tile)) continue;
            scenario.map.set_terrain(tile, Terrain::grass);
            scenario.map.set_resource_amount(tile, 0);
            scenario.map.set_elevation(tile, 0);
        }
    }
}

void place_resource(
    Scenario& scenario, TilePosition tile, Terrain terrain, int amount
) {
    if (!scenario.map.contains(tile)) return;
    scenario.map.set_terrain(tile, terrain);
    scenario.map.set_resource_amount(tile, amount);
}

}  // namespace

OrdinaryMatchSetup OrdinaryMatchSetup::standard() {
    OrdinaryMatchSetup result;
    result.slots[0].kind = OrdinarySlotKind::human;
    result.slots[1].kind = OrdinarySlotKind::computer;
    result.slots[1].civilization = Civilization::franks;
    return result;
}

std::size_t OrdinaryMatchSetup::occupied_count() const {
    return static_cast<std::size_t>(std::count_if(
        slots.begin(), slots.end(), [](const OrdinaryPlayerSlot& slot) {
            return slot.kind != OrdinarySlotKind::closed;
        }
    ));
}

std::optional<std::string> OrdinaryMatchSetup::validate() const {
    if (occupied_count() < 2) return "ordinary match needs two players";
    const auto local = local_slot.index();
    if (!local || slots[*local].kind != OrdinarySlotKind::human) {
        return "local slot must be human";
    }
    if (std::count_if(
            slots.begin(), slots.end(), [](const OrdinaryPlayerSlot& slot) {
                return slot.kind == OrdinarySlotKind::human;
            }) != 1) {
        return "single-player ordinary match needs one human";
    }
    return std::nullopt;
}

Scenario configure_ordinary_random_map(
    Scenario scenario, const OrdinaryMatchSetup& setup
) {
    if (const auto error = setup.validate()) {
        throw std::invalid_argument(*error);
    }

    scenario.units.erase(
        std::remove_if(
            scenario.units.begin(), scenario.units.end(),
            [](const UnitPlacement& unit) { return !unit.owner.is_neutral(); }
        ), scenario.units.end()
    );
    scenario.buildings.erase(
        std::remove_if(
            scenario.buildings.begin(), scenario.buildings.end(),
            [](const BuildingPlacement& building) {
                return !building.owner.is_neutral();
            }
        ), scenario.buildings.end()
    );

    scenario.roster_schema = true;
    scenario.roster_entries.clear();
    scenario.directed_diplomacy.clear();
    std::vector<PlayerSlotId> occupied;
    for (std::size_t index = 0; index < setup.slots.size(); ++index) {
        const OrdinaryPlayerSlot& configured = setup.slots[index];
        if (configured.kind == OrdinarySlotKind::closed) continue;
        const PlayerSlotId slot = *PlayerSlotId::from_index(index);
        occupied.push_back(slot);
        const RosterControllerKind controller_kind =
            configured.kind == OrdinarySlotKind::computer
                ? RosterControllerKind::computer
                : RosterControllerKind::human;
        scenario.roster_entries.push_back({
            {slot, true, configured.team, false,
             {{std::string{player_slot_name(slot)}, controller_kind}}},
            {100, 200, 200, 200}, Age::dark, configured.civilization,
            {}, FormationKind::compact,
        });
    }
    for (PlayerSlotId from : occupied) {
        for (PlayerSlotId to : occupied) {
            if (from == to) continue;
            const auto& first = setup.slots[*from.index()];
            const auto& second = setup.slots[*to.index()];
            const bool allies = first.team.has_team() &&
                first.team == second.team;
            scenario.directed_diplomacy.push_back({
                from, to, allies ? Diplomacy::ally : Diplomacy::enemy,
            });
        }
    }

    for (std::size_t ordinal = 0; ordinal < occupied.size(); ++ordinal) {
        const PlayerSlotId slot = occupied[ordinal];
        const EntityOwner owner = entity_owner_from_slot(slot);
        const TilePosition start = start_position(
            scenario.map.width(), ordinal, occupied.size()
        );
        clear_start(scenario, start);
        scenario.buildings.push_back({
            BuildingKind::town_center, owner, start,
            std::nullopt, std::nullopt, std::nullopt,
        });
        constexpr std::array<TilePosition, 3> villager_offsets{{
            {-3, -2}, {-2, -2}, {-1, -2},
        }};
        for (TilePosition offset : villager_offsets) {
            scenario.units.push_back({
                UnitKind::villager, owner,
                {start.x + offset.x, start.y + offset.y},
                std::nullopt, std::nullopt, std::nullopt, std::nullopt,
                false, {}, UnitStance::aggressive, std::nullopt, false,
            });
        }
        scenario.units.push_back({
            UnitKind::scout_cavalry, owner, {start.x - 3, start.y + 1},
            std::nullopt, std::nullopt, std::nullopt, std::nullopt,
            false, {}, UnitStance::aggressive, std::nullopt, false,
        });
        const auto place_cluster = [&](TilePosition center, Terrain terrain,
                                       int amount, int count) {
            for (int item = 0; item < count; ++item) {
                place_resource(
                    scenario,
                    {center.x + item % 3, center.y + item / 3},
                    terrain, amount
                );
            }
        };
        place_cluster(
            {start.x - 8, start.y}, Terrain::berry_bush, 125, 6
        );
        place_cluster(
            {start.x + 6, start.y - 2}, Terrain::gold_mine, 800, 7
        );
        place_cluster(
            {start.x - 1, start.y + 6}, Terrain::stone_mine, 700, 5
        );
        for (int sheep = 0; sheep < 4; ++sheep) {
            scenario.units.push_back({
                UnitKind::sheep, EntityOwner{Player::neutral},
                {start.x - 5 + sheep, start.y + 4},
                std::nullopt, std::nullopt, std::nullopt, std::nullopt,
                false, {}, UnitStance::aggressive, std::nullopt, false,
            });
        }
    }
    return scenario;
}

}  // namespace aoe
