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

}  // namespace

int main() {
    test_boundaries();
    test_floor_and_flag_two_thresholds();
    return failures == 0 ? 0 : 1;
}
