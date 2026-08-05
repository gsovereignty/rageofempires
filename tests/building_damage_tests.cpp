#include "aoe/building_damage.hpp"

#include <array>
#include <iostream>

namespace {

int failures{};

void check(bool value, const char* message) {
    if (!value) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_boundaries() {
    constexpr std::array records{
        aoe::BuildingDamageRecord{1, 25, 0},
        aoe::BuildingDamageRecord{2, 50, 0},
        aoe::BuildingDamageRecord{3, 75, 0},
    };
    check(!aoe::select_building_damage_record(75, 100, records),
          "25 percent equality selects no damage");
    check(aoe::select_building_damage_record(74, 100, records) == 0,
          "26 percent selects 25 record");
    check(aoe::select_building_damage_record(50, 100, records) == 0,
          "50 percent equality retains 25 record");
    check(aoe::select_building_damage_record(49, 100, records) == 1,
          "51 percent selects 50 record");
    check(aoe::select_building_damage_record(25, 100, records) == 1,
          "75 percent equality retains 50 record");
    check(aoe::select_building_damage_record(24, 100, records) == 2,
          "76 percent selects 75 record");
}

void test_floor_and_flag_two_thresholds() {
    constexpr std::array records{
        aoe::BuildingDamageRecord{1, 537, 2},
        aoe::BuildingDamageRecord{2, 562, 2},
        aoe::BuildingDamageRecord{3, 587, 2},
    };
    check(!aoe::select_building_damage_record(755, 1000, records),
          "75.5 percent HP floors to 25 damage equality");
    check(aoe::select_building_damage_record(745, 1000, records) == 0,
          "74.5 percent HP floors to 26 damage");
    check(aoe::select_building_damage_record(495, 1000, records) == 1,
          "49.5 percent HP floors to 51 damage");
    check(aoe::select_building_damage_record(245, 1000, records) == 2,
          "24.5 percent HP floors to 76 damage");
    check(!aoe::select_building_damage_record(0, 1000, records),
          "death removes damage state");
    check(!aoe::select_building_damage_record(50, 0, records),
          "invalid maximum HP fails closed");
}

void test_wall_civilization_roots_and_serialized_flags() {
    constexpr std::array civilizations{
        aoe::Civilization::britons, aoe::Civilization::franks,
        aoe::Civilization::celts, aoe::Civilization::spanish,
        aoe::Civilization::goths, aoe::Civilization::teutons,
        aoe::Civilization::vikings, aoe::Civilization::huns,
        aoe::Civilization::japanese, aoe::Civilization::chinese,
        aoe::Civilization::mongols, aoe::Civilization::koreans,
        aoe::Civilization::byzantines, aoe::Civilization::persians,
        aoe::Civilization::saracens, aoe::Civilization::turks,
        aoe::Civilization::aztecs, aoe::Civilization::mayans,
    };
    constexpr std::array<std::array<std::int16_t, 3>, 18> stone{{
        {3794,3798,3802},{3794,3798,3802},{3794,3798,3802},
        {3794,3798,3802},{3791,3795,3799},{3791,3795,3799},
        {3791,3795,3799},{3791,3795,3799},{3792,3796,3800},
        {3792,3796,3800},{3792,3796,3800},{3792,3796,3800},
        {3793,3797,3801},{3793,3797,3801},{3793,3797,3801},
        {3793,3797,3801},{7150,7152,7154},{7150,7152,7154},
    }};
    constexpr std::array<std::array<std::int16_t, 3>, 18> fortified{{
        {3806,3810,3814},{3806,3810,3814},{3806,3810,3814},
        {3806,3810,3814},{3803,3807,3811},{3803,3807,3811},
        {3803,3807,3811},{3803,3807,3811},{3804,3808,3812},
        {3804,3808,3812},{3804,3808,3812},{3804,3808,3812},
        {3805,3809,3813},{3805,3809,3813},{3805,3809,3813},
        {3805,3809,3813},{7156,7158,7160},{7156,7158,7160},
    }};
    constexpr std::array<std::uint16_t, 3> thresholds{537,562,587};
    for (std::size_t civ = 0; civ < civilizations.size(); ++civ) {
        for (const auto [kind, expected] : {
                 std::pair{aoe::BuildingKind::stone_wall, stone[civ]},
                 std::pair{aoe::BuildingKind::fortified_wall,
                           fortified[civ]},
             }) {
            const auto records = aoe::canonical_building_damage_records(
                kind, civilizations[civ]
            );
            for (std::size_t stage = 0; stage < records.size(); ++stage) {
                check(records[stage].graphic_id == expected[stage],
                      "wall civilization damage root");
                check(records[stage].serialized_threshold == thresholds[stage],
                      "wall high-byte serialized threshold");
                check(records[stage].flag == 2,
                      "wall replacement flag");
            }
        }
    }
}

}  // namespace

int main() {
    test_boundaries();
    test_floor_and_flag_two_thresholds();
    test_wall_civilization_roots_and_serialized_flags();
    return failures == 0 ? 0 : 1;
}
