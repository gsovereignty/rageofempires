#pragma once

#include "aoe/types.hpp"

namespace aoe {

struct UnitRules {
    int hit_points;
    int attack;
    int attack_interval_ticks;
    int movement_interval_ticks;
    int attack_range;
    int minimum_attack_range{};
    int splash_radius{};
    DamageClass damage_class;
    int bonus_vs_cavalry;
    int bonus_vs_buildings{};
    int bonus_vs_ships{};
    int bonus_vs_unique_units{};
    int bonus_vs_infantry{};
    int bonus_vs_siege{};
    int bonus_vs_archers{};
    int bonus_vs_spearmen{};
    int bonus_vs_war_elephants{};
    int bonus_vs_camels{};
    int bonus_vs_walls{};
    int bonus_vs_eagle_warriors{};
    int melee_armor;
    int pierce_armor;
    int wood_cost{};
    int food_cost;
    int gold_cost;
    int training_ticks;
    int vision_range;
    int accuracy_percent{100};
    int attack_frame_delay{};
    int movement_speed_percent{100};
    int splash_radius_half_tiles{};
    int projectile_count{1};
    int projectile_spread{};
    BuildingKind trained_at;
    Age minimum_age;
};

struct BuildingRules {
    int hit_points;
    int melee_armor;
    int pierce_armor;
    int wood_cost;
    int stone_cost;
    int gold_cost{};
    int construction_ticks;
    int vision_range;
    int population_support;
    Age minimum_age;
    int footprint_width{1};
    int footprint_height{1};
    int attack{};
    int attack_interval_ticks{};
    int attack_range{};
    int minimum_attack_range{};
    int projectile_count{};
    DamageClass damage_class{DamageClass::melee};
    int bonus_vs_camels{};
    int bonus_vs_ships{};
    int accuracy_percent{100};
    int projectile_speed_tenths{};
};

struct AgeRules {
    int food_cost;
    int gold_cost;
    int research_ticks;
};

struct TechnologyRules {
    BuildingKind researched_at;
    Age minimum_age;
    int wood_cost;
    int food_cost;
    int gold_cost;
    int stone_cost;
    int research_ticks;
};

struct ConversionCheck {
    int scaled_roll{};
    int threshold{};
    bool succeeds{};
};

// Pure represented garrison-volley contract. The baseline is the shelter's
// normal projectile count; contributing occupants are already filtered to
// Villagers and archer-class units.
int garrison_volley_projectile_count(
    BuildingKind shelter,
    int baseline_projectiles,
    int contributing_occupants
);

// Deterministic represented fee arithmetic. Percentage products use a wide
// intermediate and truncate toward zero at the integer resource boundary.
int percentage_fee_floor(int amount, int percent);
int market_price_after_fee(int base_price, int fee_percent, bool buying);

// Exact arithmetic contract recovered from AoK HD.exe 0x413e2c..0x413fed.
// resistance already includes target class/ID/resource contributions.
ConversionCheck evaluate_conversion_check(
    int crt_random_value,
    float resistance,
    int base_chance,
    float elapsed_time,
    float minimum_time,
    float maximum_time
);

// Central rules table for reconstructed behavior. Keeping balance values here
// makes assumptions visible and prevents engine code from hiding magic numbers.
const UnitRules& rules_for(UnitKind kind);
const BuildingRules& rules_for(BuildingKind kind);
bool can_train(BuildingKind building, UnitKind unit);
bool is_cavalry(UnitKind unit);
bool is_archer(UnitKind unit);
bool is_infantry(UnitKind unit);
bool is_unique_unit(UnitKind unit);
bool civilization_has_unit(Civilization civilization, UnitKind unit);
bool civilization_has_building(
    Civilization civilization,
    BuildingKind building
);
bool civilization_has_technology(
    Civilization civilization,
    Technology technology
);
bool is_herdable(UnitKind unit);
bool is_huntable(UnitKind unit);
bool is_animal(UnitKind unit);
bool is_relic(UnitKind unit);
bool is_organic(UnitKind unit);
bool is_ship(UnitKind unit);
const AgeRules& rules_for(Age age);
const TechnologyRules& rules_for(Technology technology);

}  // namespace aoe
