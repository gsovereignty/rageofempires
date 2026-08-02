#include "aoe/initial_camera.hpp"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <vector>

int main() {
    using namespace aoe;

    Building red_town_center;
    red_town_center.owner = Player::red;
    red_town_center.kind = BuildingKind::town_center;
    red_town_center.position = {70, 71};

    Building blue_house;
    blue_house.owner = Player::blue;
    blue_house.kind = BuildingKind::house;
    blue_house.position = {8, 9};

    Building blue_town_center;
    blue_town_center.owner = Player::blue;
    blue_town_center.kind = BuildingKind::town_center;
    blue_town_center.position = {40, 41};

    Unit blue_unit;
    blue_unit.owner = Player::blue;
    blue_unit.position = {12, 13};

    Unit garrisoned_blue_unit;
    garrisoned_blue_unit.owner = Player::blue;
    garrisoned_blue_unit.position = {2, 3};
    garrisoned_blue_unit.garrisoned_in = 99;

    const std::vector buildings{
        red_town_center, blue_house, blue_town_center
    };
    const std::vector units{garrisoned_blue_unit, blue_unit};
    assert((
        initial_camera_tile(buildings, units, Player::blue, 120, 100) ==
        TilePosition{41, 42}
    ));

    assert((
        initial_camera_tile(
            std::span<const Building>{},
            units,
            Player::blue,
            120,
            100
        ) == TilePosition{12, 13}
    ));

    assert((
        initial_camera_tile(buildings, units, Player::red, 120, 100) ==
        TilePosition{71, 72}
    ));

    assert((
        initial_camera_tile(
            std::span<const Building>{},
            std::span<const Unit>{},
            Player::blue,
            121,
            99
        ) == TilePosition{60, 49}
    ));

    assert((
        initial_camera_tile(
            std::span<const Building>{},
            std::span<const Unit>{},
            Player::blue,
            0,
            -1
        ) == TilePosition{0, 0}
    ));
}
