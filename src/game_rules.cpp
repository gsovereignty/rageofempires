#include "aoe/game_rules.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace aoe {
namespace {

constexpr UnitRules villager_rules{
    .hit_points = 25,
    .attack = 3,
    .attack_interval_ticks = 5,
    .movement_interval_ticks = 2,
    .attack_range = 1,
    .damage_class = DamageClass::melee,
    .bonus_vs_cavalry = 0,
    .melee_armor = 0,
    .pierce_armor = 0,
    .food_cost = 50,
    .gold_cost = 0,
    .training_ticks = 125,
    .vision_range = 4,
    .trained_at = BuildingKind::town_center,
    .minimum_age = Age::dark,
};

constexpr UnitRules sheep_rules{
    .hit_points = 7,
    .attack = 0,
    .attack_interval_ticks = 5,
    .movement_interval_ticks = 3,
    .attack_range = 0,
    .damage_class = DamageClass::melee,
    .bonus_vs_cavalry = 0,
    .melee_armor = 0,
    .pierce_armor = 0,
    .food_cost = 0,
    .gold_cost = 0,
    .training_ticks = 150,
    .vision_range = 3,
    .trained_at = BuildingKind::town_center,
    .minimum_age = Age::dark,
};

constexpr UnitRules deer_rules{
    .hit_points = 5,
    .attack = 0,
    .attack_interval_ticks = 5,
    .movement_interval_ticks = 3,
    .attack_range = 0,
    .damage_class = DamageClass::melee,
    .bonus_vs_cavalry = 0,
    .melee_armor = 0,
    .pierce_armor = 0,
    .food_cost = 0,
    .gold_cost = 0,
    .training_ticks = 150,
    .vision_range = 2,
    .trained_at = BuildingKind::town_center,
    .minimum_age = Age::dark,
};

constexpr UnitRules boar_rules{
    .hit_points = 75,
    .attack = 7,
    .attack_interval_ticks = 4,
    .movement_interval_ticks = 2,
    .attack_range = 1,
    .damage_class = DamageClass::melee,
    .bonus_vs_cavalry = 0,
    .melee_armor = 0,
    .pierce_armor = 0,
    .food_cost = 0,
    .gold_cost = 0,
    .training_ticks = 0,
    .vision_range = 4,
    .trained_at = BuildingKind::town_center,
    .minimum_age = Age::dark,
};

constexpr UnitRules monk_rules{
    .hit_points = 30,
    .attack = 0,
    .attack_interval_ticks = 20,
    .movement_interval_ticks = 2,
    .attack_range = 9,
    .damage_class = DamageClass::melee,
    .bonus_vs_cavalry = 0,
    .melee_armor = 0,
    .pierce_armor = 0,
    .food_cost = 0,
    .gold_cost = 100,
    .training_ticks = 255,
    .vision_range = 11,
    .trained_at = BuildingKind::monastery,
    .minimum_age = Age::castle,
};

constexpr UnitRules missionary_rules{
    .hit_points = 30,
    .attack = 0,
    .attack_interval_ticks = 20,
    .movement_interval_ticks = 2,
    .attack_range = 7,
    .damage_class = DamageClass::melee,
    .bonus_vs_cavalry = 0,
    .melee_armor = 0,
    .pierce_armor = 0,
    .food_cost = 0,
    .gold_cost = 100,
    .training_ticks = 255,
    .vision_range = 9,
    .movement_speed_percent = 110,
    .trained_at = BuildingKind::monastery,
    .minimum_age = Age::castle,
};
constexpr UnitRules trade_cog_rules{
    .hit_points = 80,
    .attack = 0,
    .attack_interval_ticks = 0,
    .movement_interval_ticks = 1,
    .attack_range = 0,
    .damage_class = DamageClass::melee,
    .bonus_vs_cavalry = 0,
    .melee_armor = 0,
    .pierce_armor = 6,
    .wood_cost = 100,
    .food_cost = 0,
    .gold_cost = 50,
    .training_ticks = 180,
    .vision_range = 6,
    .movement_speed_percent = 132,
    .trained_at = BuildingKind::dock,
    .minimum_age = Age::feudal,
};

constexpr UnitRules relic_rules{
    .hit_points = 30,
    .attack = 0,
    .attack_interval_ticks = 0,
    .movement_interval_ticks = 0,
    .attack_range = 0,
    .damage_class = DamageClass::melee,
    .bonus_vs_cavalry = 0,
    .melee_armor = 0,
    .pierce_armor = 0,
    .food_cost = 0,
    .gold_cost = 0,
    .training_ticks = 150,
    .vision_range = 7,
    .trained_at = BuildingKind::monastery,
    .minimum_age = Age::castle,
};

constexpr UnitRules trade_cart_rules{
    .hit_points = 70,
    .attack = 0,
    .attack_interval_ticks = 0,
    .movement_interval_ticks = 1,
    .attack_range = 0,
    .damage_class = DamageClass::melee,
    .bonus_vs_cavalry = 0,
    .melee_armor = 0,
    .pierce_armor = 0,
    .wood_cost = 100,
    .food_cost = 0,
    .gold_cost = 50,
    .training_ticks = 255,
    .vision_range = 7,
    .trained_at = BuildingKind::market,
    .minimum_age = Age::feudal,
};

constexpr UnitRules fishing_ship_rules{
    .hit_points = 60,
    .attack = 0,
    .attack_interval_ticks = 0,
    .movement_interval_ticks = 1,
    .attack_range = 0,
    .damage_class = DamageClass::melee,
    .bonus_vs_cavalry = 0,
    .melee_armor = 0,
    .pierce_armor = 4,
    .wood_cost = 75,
    .food_cost = 0,
    .gold_cost = 0,
    .training_ticks = 200,
    .vision_range = 5,
    .trained_at = BuildingKind::dock,
    .minimum_age = Age::dark,
};

constexpr UnitRules galley_rules{
    .hit_points = 120, .attack = 6, .attack_interval_ticks = 3,
    .movement_interval_ticks = 1, .attack_range = 5,
    .damage_class = DamageClass::pierce, .bonus_vs_cavalry = 0,
    .melee_armor = 0, .pierce_armor = 6, .wood_cost = 90,
    .food_cost = 0, .gold_cost = 30, .training_ticks = 300,
    .vision_range = 7, .trained_at = BuildingKind::dock,
    .minimum_age = Age::feudal,
};
constexpr UnitRules war_galley_rules{
    .hit_points = 135, .attack = 7, .attack_interval_ticks = 3,
    .movement_interval_ticks = 1, .attack_range = 6,
    .damage_class = DamageClass::pierce, .bonus_vs_cavalry = 0,
    .melee_armor = 0, .pierce_armor = 6, .wood_cost = 90,
    .food_cost = 0, .gold_cost = 30, .training_ticks = 180,
    .vision_range = 8, .trained_at = BuildingKind::dock,
    .minimum_age = Age::castle,
};
constexpr UnitRules galleon_rules{
    .hit_points = 165, .attack = 8, .attack_interval_ticks = 3,
    .movement_interval_ticks = 1, .attack_range = 7,
    .damage_class = DamageClass::pierce, .bonus_vs_cavalry = 0,
    .melee_armor = 0, .pierce_armor = 8, .wood_cost = 90,
    .food_cost = 0, .gold_cost = 30, .training_ticks = 180,
    .vision_range = 9, .trained_at = BuildingKind::dock,
    .minimum_age = Age::imperial,
};
constexpr UnitRules transport_ship_rules{
    .hit_points = 100, .attack = 0, .attack_interval_ticks = 0,
    .movement_interval_ticks = 1, .attack_range = 0,
    .damage_class = DamageClass::melee, .bonus_vs_cavalry = 0,
    .melee_armor = 4, .pierce_armor = 8, .wood_cost = 125,
    .food_cost = 0, .gold_cost = 0, .training_ticks = 230,
    .vision_range = 5, .trained_at = BuildingKind::dock,
    .minimum_age = Age::dark,
};
constexpr UnitRules fire_ship_rules{
    .hit_points = 100, .attack = 2, .attack_interval_ticks = 1,
    .movement_interval_ticks = 1, .attack_range = 2,
    .damage_class = DamageClass::pierce, .bonus_vs_cavalry = 0,
    .melee_armor = 0, .pierce_armor = 6, .wood_cost = 75,
    .food_cost = 0, .gold_cost = 45, .training_ticks = 180,
    .vision_range = 5, .trained_at = BuildingKind::dock,
    .minimum_age = Age::castle,
};
constexpr UnitRules fast_fire_ship_rules{
    .hit_points = 120, .attack = 3, .attack_interval_ticks = 1,
    .movement_interval_ticks = 1, .attack_range = 2,
    .damage_class = DamageClass::pierce, .bonus_vs_cavalry = 0,
    .melee_armor = 0, .pierce_armor = 8, .wood_cost = 75,
    .food_cost = 0, .gold_cost = 45, .training_ticks = 180,
    .vision_range = 6, .trained_at = BuildingKind::dock,
    .minimum_age = Age::imperial,
};
constexpr UnitRules demolition_ship_rules{
    .hit_points = 50, .attack = 110, .attack_interval_ticks = 1,
    .movement_interval_ticks = 1, .attack_range = 1,
    .splash_radius = 1, .damage_class = DamageClass::melee,
    .bonus_vs_cavalry = 0, .melee_armor = 0, .pierce_armor = 3,
    .wood_cost = 70, .food_cost = 0, .gold_cost = 50,
    .training_ticks = 155, .vision_range = 6,
    .trained_at = BuildingKind::dock, .minimum_age = Age::castle,
};
constexpr UnitRules heavy_demolition_ship_rules{
    .hit_points = 60, .attack = 140, .attack_interval_ticks = 1,
    .movement_interval_ticks = 1, .attack_range = 1,
    .splash_radius = 1, .damage_class = DamageClass::melee,
    .bonus_vs_cavalry = 0, .melee_armor = 0, .pierce_armor = 5,
    .wood_cost = 70, .food_cost = 0, .gold_cost = 50,
    .training_ticks = 155, .vision_range = 6,
    .trained_at = BuildingKind::dock, .minimum_age = Age::imperial,
};
constexpr UnitRules cannon_galleon_rules{
    .hit_points = 120, .attack = 35, .attack_interval_ticks = 10,
    .movement_interval_ticks = 1, .attack_range = 13,
    .minimum_attack_range = 3,
    .damage_class = DamageClass::melee, .bonus_vs_cavalry = 0,
    .bonus_vs_buildings = 200, .bonus_vs_ships = 40,
    .melee_armor = 0, .pierce_armor = 6,
    .wood_cost = 200, .food_cost = 0, .gold_cost = 150,
    .training_ticks = 230, .vision_range = 15,
    .accuracy_percent = 50,
    .trained_at = BuildingKind::dock, .minimum_age = Age::imperial,
};
constexpr UnitRules elite_cannon_galleon_rules{
    .hit_points = 150, .attack = 45, .attack_interval_ticks = 10,
    .movement_interval_ticks = 1, .attack_range = 15,
    .minimum_attack_range = 3,
    .damage_class = DamageClass::melee, .bonus_vs_cavalry = 0,
    .bonus_vs_buildings = 275, .bonus_vs_ships = 40,
    .melee_armor = 0, .pierce_armor = 8,
    .wood_cost = 200, .food_cost = 0, .gold_cost = 150,
    .training_ticks = 230, .vision_range = 17,
    .accuracy_percent = 50,
    .trained_at = BuildingKind::dock, .minimum_age = Age::imperial,
};
constexpr UnitRules longboat_rules{
    .hit_points = 130, .attack = 7, .attack_interval_ticks = 3,
    .movement_interval_ticks = 1, .attack_range = 6,
    .damage_class = DamageClass::pierce, .bonus_vs_cavalry = 0,
    .melee_armor = 0, .pierce_armor = 6,
    .wood_cost = 100, .food_cost = 0, .gold_cost = 50,
    .training_ticks = 125, .vision_range = 8,
    .accuracy_percent = 100, .projectile_count = 5,
    .projectile_spread = 2,
    .trained_at = BuildingKind::dock, .minimum_age = Age::castle,
};
constexpr UnitRules elite_longboat_rules{
    .hit_points = 160, .attack = 8, .attack_interval_ticks = 3,
    .movement_interval_ticks = 1, .attack_range = 7,
    .damage_class = DamageClass::pierce, .bonus_vs_cavalry = 0,
    .melee_armor = 0, .pierce_armor = 8,
    .wood_cost = 100, .food_cost = 0, .gold_cost = 50,
    .training_ticks = 125, .vision_range = 9,
    .accuracy_percent = 100, .projectile_count = 5,
    .projectile_spread = 1,
    .trained_at = BuildingKind::dock, .minimum_age = Age::imperial,
};
constexpr UnitRules turtle_ship_rules{
    .hit_points = 200, .attack = 50, .attack_interval_ticks = 6,
    .movement_interval_ticks = 1, .attack_range = 6,
    .minimum_attack_range = 0,
    .damage_class = DamageClass::melee, .bonus_vs_cavalry = 0,
    .melee_armor = 6, .pierce_armor = 5,
    .wood_cost = 200, .food_cost = 0, .gold_cost = 200,
    .training_ticks = 250, .vision_range = 8,
    .accuracy_percent = 100,
    .trained_at = BuildingKind::dock, .minimum_age = Age::castle,
};
constexpr UnitRules elite_turtle_ship_rules{
    .hit_points = 300, .attack = 50, .attack_interval_ticks = 6,
    .movement_interval_ticks = 1, .attack_range = 6,
    .minimum_attack_range = 0,
    .damage_class = DamageClass::melee, .bonus_vs_cavalry = 0,
    .melee_armor = 8, .pierce_armor = 6,
    .wood_cost = 200, .food_cost = 0, .gold_cost = 200,
    .training_ticks = 250, .vision_range = 8,
    .accuracy_percent = 100,
    .trained_at = BuildingKind::dock, .minimum_age = Age::imperial,
};
constexpr UnitRules longbowman_rules{
    .hit_points = 35, .attack = 6, .attack_interval_ticks = 2,
    .movement_interval_ticks = 1, .attack_range = 5,
    .damage_class = DamageClass::pierce, .bonus_vs_cavalry = 0,
    .bonus_vs_spearmen = 2,
    .melee_armor = 0, .pierce_armor = 0,
    .wood_cost = 35, .food_cost = 0, .gold_cost = 40,
    .training_ticks = 95, .vision_range = 7,
    .accuracy_percent = 70,
    .trained_at = BuildingKind::castle, .minimum_age = Age::castle,
};
constexpr UnitRules elite_longbowman_rules{
    .hit_points = 40, .attack = 7, .attack_interval_ticks = 2,
    .movement_interval_ticks = 1, .attack_range = 6,
    .damage_class = DamageClass::pierce, .bonus_vs_cavalry = 0,
    .bonus_vs_spearmen = 2,
    .melee_armor = 0, .pierce_armor = 1,
    .wood_cost = 35, .food_cost = 0, .gold_cost = 40,
    .training_ticks = 95, .vision_range = 8,
    .accuracy_percent = 80,
    .trained_at = BuildingKind::castle, .minimum_age = Age::imperial,
};
constexpr UnitRules throwing_axeman_rules{
    .hit_points = 50, .attack = 7, .attack_interval_ticks = 2,
    .movement_interval_ticks = 1, .attack_range = 3,
    .damage_class = DamageClass::melee, .bonus_vs_cavalry = 0,
    .bonus_vs_buildings = 1,
    .melee_armor = 0, .pierce_armor = 0,
    .wood_cost = 0, .food_cost = 55, .gold_cost = 25,
    .training_ticks = 85, .vision_range = 5,
    .accuracy_percent = 100,
    .trained_at = BuildingKind::castle, .minimum_age = Age::castle,
};
constexpr UnitRules elite_throwing_axeman_rules{
    .hit_points = 60, .attack = 8, .attack_interval_ticks = 2,
    .movement_interval_ticks = 1, .attack_range = 4,
    .damage_class = DamageClass::melee, .bonus_vs_cavalry = 0,
    .bonus_vs_buildings = 2,
    .melee_armor = 1, .pierce_armor = 0,
    .wood_cost = 0, .food_cost = 55, .gold_cost = 25,
    .training_ticks = 85, .vision_range = 6,
    .accuracy_percent = 100,
    .trained_at = BuildingKind::castle, .minimum_age = Age::imperial,
};
constexpr UnitRules huskarl_rules{
    .hit_points = 60, .attack = 10, .attack_interval_ticks = 2,
    .movement_interval_ticks = 1, .attack_range = 1,
    .damage_class = DamageClass::melee, .bonus_vs_cavalry = 0,
    .bonus_vs_buildings = 2, .bonus_vs_archers = 6,
    .melee_armor = 0, .pierce_armor = 6,
    .wood_cost = 0, .food_cost = 80, .gold_cost = 40,
    .training_ticks = 80, .vision_range = 3,
    .trained_at = BuildingKind::castle, .minimum_age = Age::castle,
};
constexpr UnitRules elite_huskarl_rules{
    .hit_points = 70, .attack = 12, .attack_interval_ticks = 2,
    .movement_interval_ticks = 1, .attack_range = 1,
    .damage_class = DamageClass::melee, .bonus_vs_cavalry = 0,
    .bonus_vs_buildings = 3, .bonus_vs_archers = 10,
    .melee_armor = 0, .pierce_armor = 8,
    .wood_cost = 0, .food_cost = 80, .gold_cost = 40,
    .training_ticks = 80, .vision_range = 5,
    .trained_at = BuildingKind::castle, .minimum_age = Age::imperial,
};
constexpr UnitRules teutonic_knight_rules{
    .hit_points = 70, .attack = 12, .attack_interval_ticks = 2,
    .movement_interval_ticks = 1, .attack_range = 1,
    .damage_class = DamageClass::melee, .bonus_vs_cavalry = 0,
    .bonus_vs_buildings = 4,
    .melee_armor = 5, .pierce_armor = 2,
    .wood_cost = 0, .food_cost = 85, .gold_cost = 40,
    .training_ticks = 60, .vision_range = 3,
    .trained_at = BuildingKind::castle, .minimum_age = Age::castle,
};
constexpr UnitRules elite_teutonic_knight_rules{
    .hit_points = 100, .attack = 17, .attack_interval_ticks = 2,
    .movement_interval_ticks = 1, .attack_range = 1,
    .damage_class = DamageClass::melee, .bonus_vs_cavalry = 0,
    .bonus_vs_buildings = 4,
    .melee_armor = 10, .pierce_armor = 2,
    .wood_cost = 0, .food_cost = 85, .gold_cost = 40,
    .training_ticks = 60, .vision_range = 5,
    .trained_at = BuildingKind::castle, .minimum_age = Age::imperial,
};
constexpr UnitRules samurai_rules{
    .hit_points = 60, .attack = 8, .attack_interval_ticks = 2,
    .movement_interval_ticks = 1, .attack_range = 1,
    .damage_class = DamageClass::melee, .bonus_vs_cavalry = 0,
    .bonus_vs_buildings = 2, .bonus_vs_unique_units = 10,
    .melee_armor = 1, .pierce_armor = 1,
    .wood_cost = 0, .food_cost = 60, .gold_cost = 30,
    .training_ticks = 45, .vision_range = 4,
    .trained_at = BuildingKind::castle, .minimum_age = Age::castle,
};
constexpr UnitRules elite_samurai_rules{
    .hit_points = 80, .attack = 12, .attack_interval_ticks = 2,
    .movement_interval_ticks = 1, .attack_range = 1,
    .damage_class = DamageClass::melee, .bonus_vs_cavalry = 0,
    .bonus_vs_buildings = 3, .bonus_vs_unique_units = 12,
    .melee_armor = 1, .pierce_armor = 1,
    .wood_cost = 0, .food_cost = 60, .gold_cost = 30,
    .training_ticks = 45, .vision_range = 5,
    .trained_at = BuildingKind::castle, .minimum_age = Age::imperial,
};
constexpr UnitRules chu_ko_nu_rules{
    .hit_points = 45, .attack = 8, .attack_interval_ticks = 3,
    .movement_interval_ticks = 1, .attack_range = 4,
    .damage_class = DamageClass::pierce, .bonus_vs_cavalry = 0,
    .bonus_vs_spearmen = 2,
    .melee_armor = 0, .pierce_armor = 0,
    .wood_cost = 40, .food_cost = 0, .gold_cost = 35,
    .training_ticks = 95, .vision_range = 6,
    .accuracy_percent = 85, .projectile_count = 4,
    .projectile_spread = 0,
    .trained_at = BuildingKind::castle, .minimum_age = Age::castle,
};
constexpr UnitRules elite_chu_ko_nu_rules{
    .hit_points = 50, .attack = 8, .attack_interval_ticks = 3,
    .movement_interval_ticks = 1, .attack_range = 4,
    .damage_class = DamageClass::pierce, .bonus_vs_cavalry = 0,
    .bonus_vs_spearmen = 2,
    .melee_armor = 0, .pierce_armor = 0,
    .wood_cost = 40, .food_cost = 0, .gold_cost = 35,
    .training_ticks = 65, .vision_range = 6,
    .accuracy_percent = 85, .projectile_count = 6,
    .projectile_spread = 1,
    .trained_at = BuildingKind::castle, .minimum_age = Age::imperial,
};
constexpr UnitRules cataphract_rules{
    .hit_points = 110, .attack = 9, .attack_interval_ticks = 2,
    .movement_interval_ticks = 1, .attack_range = 1,
    .damage_class = DamageClass::melee, .bonus_vs_cavalry = 0,
    .bonus_vs_infantry = 9,
    .melee_armor = 2, .pierce_armor = 1,
    .wood_cost = 0, .food_cost = 70, .gold_cost = 75,
    .training_ticks = 100, .vision_range = 4,
    .trained_at = BuildingKind::castle, .minimum_age = Age::castle,
};
constexpr UnitRules elite_cataphract_rules{
    .hit_points = 150, .attack = 12, .attack_interval_ticks = 2,
    .movement_interval_ticks = 1, .attack_range = 1,
    .damage_class = DamageClass::melee, .bonus_vs_cavalry = 0,
    .bonus_vs_infantry = 12,
    .melee_armor = 2, .pierce_armor = 1,
    .wood_cost = 0, .food_cost = 70, .gold_cost = 75,
    .training_ticks = 100, .vision_range = 5,
    .trained_at = BuildingKind::castle, .minimum_age = Age::imperial,
};
constexpr UnitRules war_elephant_rules{
    .hit_points = 450, .attack = 15, .attack_interval_ticks = 2,
    .movement_interval_ticks = 1, .attack_range = 1,
    .damage_class = DamageClass::melee, .bonus_vs_cavalry = 0,
    .bonus_vs_buildings = 7,
    .melee_armor = 1, .pierce_armor = 2,
    .wood_cost = 0, .food_cost = 200, .gold_cost = 75,
    .training_ticks = 155, .vision_range = 4,
    .trained_at = BuildingKind::castle, .minimum_age = Age::castle,
};
constexpr UnitRules elite_war_elephant_rules{
    .hit_points = 600, .attack = 20, .attack_interval_ticks = 2,
    .movement_interval_ticks = 1, .attack_range = 1,
    .splash_radius = 1,
    .damage_class = DamageClass::melee, .bonus_vs_cavalry = 0,
    .bonus_vs_buildings = 10,
    .melee_armor = 1, .pierce_armor = 3,
    .wood_cost = 0, .food_cost = 200, .gold_cost = 75,
    .training_ticks = 155, .vision_range = 5,
    .trained_at = BuildingKind::castle, .minimum_age = Age::imperial,
};
constexpr UnitRules mameluke_rules{
    .hit_points = 65, .attack = 7, .attack_interval_ticks = 2,
    .movement_interval_ticks = 1, .attack_range = 3,
    .damage_class = DamageClass::melee, .bonus_vs_cavalry = 9,
    .melee_armor = 0, .pierce_armor = 0,
    .wood_cost = 0, .food_cost = 55, .gold_cost = 85,
    .training_ticks = 115, .vision_range = 5,
    .trained_at = BuildingKind::castle, .minimum_age = Age::castle,
};
constexpr UnitRules elite_mameluke_rules{
    .hit_points = 80, .attack = 10, .attack_interval_ticks = 2,
    .movement_interval_ticks = 1, .attack_range = 3,
    .damage_class = DamageClass::melee, .bonus_vs_cavalry = 12,
    .melee_armor = 1, .pierce_armor = 0,
    .wood_cost = 0, .food_cost = 55, .gold_cost = 85,
    .training_ticks = 115, .vision_range = 5,
    .trained_at = BuildingKind::castle, .minimum_age = Age::imperial,
};
constexpr UnitRules janissary_rules{
    .hit_points = 35, .attack = 17, .attack_interval_ticks = 3,
    .movement_interval_ticks = 1, .attack_range = 8,
    .damage_class = DamageClass::pierce, .bonus_vs_cavalry = 0,
    .melee_armor = 1, .pierce_armor = 0,
    .wood_cost = 0, .food_cost = 60, .gold_cost = 55,
    .training_ticks = 105, .vision_range = 10,
    .accuracy_percent = 50,
    .trained_at = BuildingKind::castle, .minimum_age = Age::castle,
};
constexpr UnitRules elite_janissary_rules{
    .hit_points = 40, .attack = 22, .attack_interval_ticks = 3,
    .movement_interval_ticks = 1, .attack_range = 8,
    .damage_class = DamageClass::pierce, .bonus_vs_cavalry = 0,
    .melee_armor = 2, .pierce_armor = 0,
    .wood_cost = 0, .food_cost = 60, .gold_cost = 55,
    .training_ticks = 105, .vision_range = 10,
    .accuracy_percent = 50,
    .trained_at = BuildingKind::castle, .minimum_age = Age::imperial,
};
constexpr UnitRules berserk_rules{
    .hit_points = 48, .attack = 9, .attack_interval_ticks = 2,
    .movement_interval_ticks = 1, .attack_range = 1,
    .damage_class = DamageClass::melee, .bonus_vs_cavalry = 0,
    .bonus_vs_buildings = 2,
    .melee_armor = 0, .pierce_armor = 1,
    .wood_cost = 0, .food_cost = 65, .gold_cost = 25,
    .training_ticks = 80, .vision_range = 3,
    .trained_at = BuildingKind::castle, .minimum_age = Age::castle,
};
constexpr UnitRules elite_berserk_rules{
    .hit_points = 60, .attack = 14, .attack_interval_ticks = 2,
    .movement_interval_ticks = 1, .attack_range = 1,
    .damage_class = DamageClass::melee, .bonus_vs_cavalry = 0,
    .bonus_vs_buildings = 3,
    .melee_armor = 2, .pierce_armor = 1,
    .wood_cost = 0, .food_cost = 65, .gold_cost = 25,
    .training_ticks = 80, .vision_range = 5,
    .trained_at = BuildingKind::castle, .minimum_age = Age::imperial,
};
// Live empires2_x1_p1.dat records 232 and 534.
constexpr UnitRules woad_raider_rules{
    .hit_points = 65, .attack = 8, .attack_interval_ticks = 2,
    .movement_interval_ticks = 1, .attack_range = 1,
    .damage_class = DamageClass::melee, .bonus_vs_cavalry = 0,
    .bonus_vs_buildings = 2, .bonus_vs_eagle_warriors = 2,
    .melee_armor = 0, .pierce_armor = 1,
    .wood_cost = 0, .food_cost = 65, .gold_cost = 25,
    .training_ticks = 10, .vision_range = 3,
    .movement_speed_percent = 120,
    .trained_at = BuildingKind::castle, .minimum_age = Age::castle,
};
constexpr UnitRules elite_woad_raider_rules{
    .hit_points = 80, .attack = 13, .attack_interval_ticks = 2,
    .movement_interval_ticks = 1, .attack_range = 1,
    .damage_class = DamageClass::melee, .bonus_vs_cavalry = 0,
    .bonus_vs_buildings = 3, .bonus_vs_eagle_warriors = 3,
    .melee_armor = 0, .pierce_armor = 1,
    .wood_cost = 0, .food_cost = 65, .gold_cost = 25,
    .training_ticks = 10, .vision_range = 5,
    .movement_speed_percent = 120,
    .trained_at = BuildingKind::castle, .minimum_age = Age::imperial,
};
// Live VER 5.7 DAT unit 434: non-combatant Regicide King.
constexpr UnitRules king_rules{
    .hit_points = 75, .attack = 0, .attack_interval_ticks = 1,
    .movement_interval_ticks = 1, .attack_range = 0,
    .damage_class = DamageClass::melee, .bonus_vs_cavalry = 0,
    .melee_armor = 0, .pierce_armor = 0,
    .wood_cost = 0, .food_cost = 0, .gold_cost = 0,
    .training_ticks = 30, .vision_range = 6,
    .movement_speed_percent = 132,
    .trained_at = BuildingKind::castle, .minimum_age = Age::castle,
};
constexpr UnitRules mangudai_rules{
    .hit_points = 60, .attack = 6, .attack_interval_ticks = 2,
    .movement_interval_ticks = 1, .attack_range = 4,
    .damage_class = DamageClass::pierce, .bonus_vs_cavalry = 0,
    .bonus_vs_siege = 3, .bonus_vs_spearmen = 1,
    .melee_armor = 0, .pierce_armor = 0,
    .wood_cost = 55, .food_cost = 0, .gold_cost = 65,
    .training_ticks = 130, .vision_range = 6,
    .accuracy_percent = 95,
    .trained_at = BuildingKind::castle, .minimum_age = Age::castle,
};
constexpr UnitRules elite_mangudai_rules{
    .hit_points = 60, .attack = 8, .attack_interval_ticks = 2,
    .movement_interval_ticks = 1, .attack_range = 4,
    .damage_class = DamageClass::pierce, .bonus_vs_cavalry = 0,
    .bonus_vs_siege = 5, .bonus_vs_spearmen = 1,
    .melee_armor = 1, .pierce_armor = 0,
    .wood_cost = 55, .food_cost = 0, .gold_cost = 65,
    .training_ticks = 130, .vision_range = 6,
    .accuracy_percent = 95,
    .trained_at = BuildingKind::castle, .minimum_age = Age::imperial,
};
constexpr UnitRules jaguar_warrior_rules{
    .hit_points = 50, .attack = 10, .attack_interval_ticks = 2,
    .movement_interval_ticks = 1, .attack_range = 1,
    .damage_class = DamageClass::melee, .bonus_vs_cavalry = 0,
    .bonus_vs_infantry = 10,
    .melee_armor = 1, .pierce_armor = 0,
    .wood_cost = 0, .food_cost = 60, .gold_cost = 30,
    .training_ticks = 100, .vision_range = 3,
    .trained_at = BuildingKind::castle, .minimum_age = Age::castle,
};
constexpr UnitRules elite_jaguar_warrior_rules{
    .hit_points = 75, .attack = 12, .attack_interval_ticks = 2,
    .movement_interval_ticks = 1, .attack_range = 1,
    .damage_class = DamageClass::melee, .bonus_vs_cavalry = 0,
    .bonus_vs_infantry = 10,
    .melee_armor = 2, .pierce_armor = 0,
    .wood_cost = 0, .food_cost = 60, .gold_cost = 30,
    .training_ticks = 100, .vision_range = 5,
    .trained_at = BuildingKind::castle, .minimum_age = Age::imperial,
};
constexpr UnitRules plumed_archer_rules{
    .hit_points = 50, .attack = 5, .attack_interval_ticks = 2,
    .movement_interval_ticks = 1, .attack_range = 4,
    .damage_class = DamageClass::pierce, .bonus_vs_cavalry = 0,
    .bonus_vs_infantry = 1,
    .melee_armor = 0, .pierce_armor = 1,
    .wood_cost = 46, .food_cost = 0, .gold_cost = 46,
    .training_ticks = 80, .vision_range = 6,
    .accuracy_percent = 80,
    .trained_at = BuildingKind::castle, .minimum_age = Age::castle,
};
constexpr UnitRules elite_plumed_archer_rules{
    .hit_points = 65, .attack = 5, .attack_interval_ticks = 2,
    .movement_interval_ticks = 1, .attack_range = 5,
    .damage_class = DamageClass::pierce, .bonus_vs_cavalry = 0,
    .bonus_vs_infantry = 2,
    .melee_armor = 0, .pierce_armor = 2,
    .wood_cost = 46, .food_cost = 0, .gold_cost = 46,
    .training_ticks = 80, .vision_range = 7,
    .accuracy_percent = 90,
    .trained_at = BuildingKind::castle, .minimum_age = Age::imperial,
};
constexpr UnitRules conquistador_rules{
    .hit_points = 55, .attack = 16, .attack_interval_ticks = 3,
    .movement_interval_ticks = 1, .attack_range = 6,
    .damage_class = DamageClass::pierce, .bonus_vs_cavalry = 0,
    .melee_armor = 2, .pierce_armor = 2,
    .wood_cost = 0, .food_cost = 60, .gold_cost = 70,
    .training_ticks = 120, .vision_range = 8,
    .accuracy_percent = 65,
    .trained_at = BuildingKind::castle, .minimum_age = Age::castle,
};
constexpr UnitRules elite_conquistador_rules{
    .hit_points = 70, .attack = 18, .attack_interval_ticks = 3,
    .movement_interval_ticks = 1, .attack_range = 6,
    .damage_class = DamageClass::pierce, .bonus_vs_cavalry = 0,
    .melee_armor = 2, .pierce_armor = 2,
    .wood_cost = 0, .food_cost = 60, .gold_cost = 70,
    .training_ticks = 120, .vision_range = 9,
    .accuracy_percent = 70,
    .trained_at = BuildingKind::castle, .minimum_age = Age::imperial,
};
constexpr UnitRules tarkan_rules{
    .hit_points = 90, .attack = 7, .attack_interval_ticks = 2,
    .movement_interval_ticks = 1, .attack_range = 1,
    .damage_class = DamageClass::melee, .bonus_vs_cavalry = 0,
    .bonus_vs_buildings = 8,
    .melee_armor = 1, .pierce_armor = 2,
    .wood_cost = 0, .food_cost = 60, .gold_cost = 60,
    .training_ticks = 70, .vision_range = 5,
    .trained_at = BuildingKind::castle, .minimum_age = Age::castle,
};
constexpr UnitRules elite_tarkan_rules{
    .hit_points = 150, .attack = 11, .attack_interval_ticks = 2,
    .movement_interval_ticks = 1, .attack_range = 1,
    .damage_class = DamageClass::melee, .bonus_vs_cavalry = 0,
    .bonus_vs_buildings = 10,
    .melee_armor = 1, .pierce_armor = 3,
    .wood_cost = 0, .food_cost = 60, .gold_cost = 60,
    .training_ticks = 70, .vision_range = 7,
    .trained_at = BuildingKind::castle, .minimum_age = Age::imperial,
};

constexpr UnitRules knight_rules{
    .hit_points = 100,
    .attack = 10,
    .attack_interval_ticks = 4,
    .movement_interval_ticks = 1,
    .attack_range = 1,
    .damage_class = DamageClass::melee,
    .bonus_vs_cavalry = 0,
    .melee_armor = 2,
    .pierce_armor = 2,
    .food_cost = 60,
    .gold_cost = 75,
    .training_ticks = 150,
    .vision_range = 4,
    .trained_at = BuildingKind::stable,
    .minimum_age = Age::castle,
};

constexpr UnitRules cavalier_unit_rules{
    .hit_points = 120,
    .attack = 12,
    .attack_interval_ticks = 4,
    .movement_interval_ticks = 1,
    .attack_range = 1,
    .damage_class = DamageClass::melee,
    .bonus_vs_cavalry = 0,
    .melee_armor = 2,
    .pierce_armor = 2,
    .food_cost = 60,
    .gold_cost = 75,
    .training_ticks = 150,
    .vision_range = 4,
    .trained_at = BuildingKind::stable,
    .minimum_age = Age::imperial,
};

constexpr UnitRules paladin_unit_rules{
    .hit_points = 160,
    .attack = 14,
    .attack_interval_ticks = 4,
    .movement_interval_ticks = 1,
    .attack_range = 1,
    .damage_class = DamageClass::melee,
    .bonus_vs_cavalry = 0,
    .melee_armor = 2,
    .pierce_armor = 3,
    .food_cost = 60,
    .gold_cost = 75,
    .training_ticks = 150,
    .vision_range = 5,
    .trained_at = BuildingKind::stable,
    .minimum_age = Age::imperial,
};

constexpr UnitRules archer_rules{
    .hit_points = 30,
    .attack = 4,
    .attack_interval_ticks = 5,
    .movement_interval_ticks = 2,
    .attack_range = 4,
    .damage_class = DamageClass::pierce,
    .bonus_vs_cavalry = 0,
    .melee_armor = 0,
    .pierce_armor = 0,
    .wood_cost = 25,
    .food_cost = 0,
    .gold_cost = 45,
    .training_ticks = 175,
    .vision_range = 6,
    .trained_at = BuildingKind::archery_range,
    .minimum_age = Age::feudal,
};

constexpr UnitRules crossbowman_unit_rules{
    .hit_points = 35,
    .attack = 5,
    .attack_interval_ticks = 5,
    .movement_interval_ticks = 2,
    .attack_range = 5,
    .damage_class = DamageClass::pierce,
    .bonus_vs_cavalry = 0,
    .bonus_vs_buildings = 0,
    .bonus_vs_archers = 0,
    .melee_armor = 0,
    .pierce_armor = 0,
    .wood_cost = 25,
    .food_cost = 0,
    .gold_cost = 45,
    .training_ticks = 135,
    .vision_range = 7,
    .trained_at = BuildingKind::archery_range,
    .minimum_age = Age::castle,
};

constexpr UnitRules arbalester_unit_rules{
    .hit_points = 40,
    .attack = 6,
    .attack_interval_ticks = 5,
    .movement_interval_ticks = 2,
    .attack_range = 5,
    .damage_class = DamageClass::pierce,
    .bonus_vs_cavalry = 0,
    .bonus_vs_buildings = 0,
    .bonus_vs_archers = 0,
    .melee_armor = 0,
    .pierce_armor = 0,
    .wood_cost = 25,
    .food_cost = 0,
    .gold_cost = 45,
    .training_ticks = 135,
    .vision_range = 7,
    .trained_at = BuildingKind::archery_range,
    .minimum_age = Age::imperial,
};

constexpr UnitRules scout_cavalry_rules{
    .hit_points = 45,
    .attack = 3,
    .attack_interval_ticks = 4,
    .movement_interval_ticks = 1,
    .attack_range = 1,
    .damage_class = DamageClass::melee,
    .bonus_vs_cavalry = 0,
    .melee_armor = 0,
    .pierce_armor = 2,
    .food_cost = 80,
    .gold_cost = 0,
    .training_ticks = 150,
    .vision_range = 4,
    .trained_at = BuildingKind::stable,
    .minimum_age = Age::feudal,
};

constexpr UnitRules light_cavalry_unit_rules{
    .hit_points = 60,
    .attack = 7,
    .attack_interval_ticks = 4,
    .movement_interval_ticks = 1,
    .attack_range = 1,
    .damage_class = DamageClass::melee,
    .bonus_vs_cavalry = 0,
    .melee_armor = 0,
    .pierce_armor = 2,
    .food_cost = 80,
    .gold_cost = 0,
    .training_ticks = 150,
    .vision_range = 8,
    .trained_at = BuildingKind::stable,
    .minimum_age = Age::castle,
};

constexpr UnitRules hussar_unit_rules{
    .hit_points = 75,
    .attack = 7,
    .attack_interval_ticks = 4,
    .movement_interval_ticks = 1,
    .attack_range = 1,
    .damage_class = DamageClass::melee,
    .bonus_vs_cavalry = 0,
    .melee_armor = 0,
    .pierce_armor = 2,
    .food_cost = 80,
    .gold_cost = 0,
    .training_ticks = 150,
    .vision_range = 10,
    .trained_at = BuildingKind::stable,
    .minimum_age = Age::imperial,
};

constexpr UnitRules militia_rules{
    .hit_points = 40,
    .attack = 4,
    .attack_interval_ticks = 5,
    .movement_interval_ticks = 2,
    .attack_range = 1,
    .damage_class = DamageClass::melee,
    .bonus_vs_cavalry = 0,
    .melee_armor = 0,
    .pierce_armor = 1,
    .food_cost = 60,
    .gold_cost = 20,
    .training_ticks = 105,
    .vision_range = 4,
    .trained_at = BuildingKind::barracks,
    .minimum_age = Age::dark,
};

constexpr UnitRules spearman_rules{
    .hit_points = 45,
    .attack = 3,
    .attack_interval_ticks = 5,
    .movement_interval_ticks = 2,
    .attack_range = 1,
    .damage_class = DamageClass::melee,
    .bonus_vs_cavalry = 15,
    .melee_armor = 0,
    .pierce_armor = 0,
    .wood_cost = 25,
    .food_cost = 35,
    .gold_cost = 0,
    .training_ticks = 110,
    .vision_range = 4,
    .trained_at = BuildingKind::barracks,
    .minimum_age = Age::feudal,
};

constexpr UnitRules pikeman_unit_rules{
    .hit_points = 55,
    .attack = 4,
    .attack_interval_ticks = 5,
    .movement_interval_ticks = 2,
    .attack_range = 1,
    .damage_class = DamageClass::melee,
    .bonus_vs_cavalry = 22,
    .bonus_vs_buildings = 0,
    .bonus_vs_archers = 0,
    .melee_armor = 0,
    .pierce_armor = 0,
    .wood_cost = 25,
    .food_cost = 35,
    .gold_cost = 0,
    .training_ticks = 110,
    .vision_range = 4,
    .trained_at = BuildingKind::barracks,
    .minimum_age = Age::castle,
};

constexpr UnitRules halberdier_unit_rules{
    .hit_points = 60,
    .attack = 6,
    .attack_interval_ticks = 6,
    .movement_interval_ticks = 2,
    .attack_range = 1,
    .damage_class = DamageClass::melee,
    .bonus_vs_cavalry = 32,
    .bonus_vs_war_elephants = 28,
    .bonus_vs_camels = 16,
    .melee_armor = 0,
    .pierce_armor = 0,
    .wood_cost = 25,
    .food_cost = 35,
    .gold_cost = 0,
    .training_ticks = 110,
    .vision_range = 4,
    .trained_at = BuildingKind::barracks,
    .minimum_age = Age::imperial,
};

constexpr UnitRules hand_cannoneer_rules{
    .hit_points = 35, .attack = 17, .attack_interval_ticks = 7,
    .movement_interval_ticks = 1, .attack_range = 7,
    .damage_class = DamageClass::pierce,
    .bonus_vs_infantry = 10, .bonus_vs_spearmen = 2,
    .melee_armor = 1, .pierce_armor = 0,
    .food_cost = 45, .gold_cost = 50,
    .training_ticks = 170, .vision_range = 9,
    .accuracy_percent = 65, .attack_frame_delay = 5,
    .movement_speed_percent = 96,
    .trained_at = BuildingKind::archery_range,
    .minimum_age = Age::imperial,
};

constexpr UnitRules bombard_cannon_rules{
    .hit_points = 80, .attack = 40, .attack_interval_ticks = 13,
    .movement_interval_ticks = 1, .attack_range = 12,
    .minimum_attack_range = 5,
    .damage_class = DamageClass::melee,
    .bonus_vs_buildings = 200, .bonus_vs_siege = 20,
    .bonus_vs_camels = 40,
    .melee_armor = 2, .pierce_armor = 5,
    .wood_cost = 225, .gold_cost = 225,
    .training_ticks = 280, .vision_range = 14,
    .accuracy_percent = 92, .attack_frame_delay = 7,
    .movement_speed_percent = 70,
    .splash_radius_half_tiles = 1,
    .trained_at = BuildingKind::siege_workshop,
    .minimum_age = Age::imperial,
};

constexpr UnitRules petard_rules{
    .hit_points = 50, .attack = 25, .attack_interval_ticks = 10,
    .movement_interval_ticks = 1, .attack_range = 1,
    .damage_class = DamageClass::melee,
    .bonus_vs_buildings = 500, .bonus_vs_siege = 60,
    .bonus_vs_walls = 900,
    .melee_armor = 0, .pierce_armor = 2,
    .food_cost = 80, .gold_cost = 20,
    .training_ticks = 125, .vision_range = 4,
    .movement_speed_percent = 80,
    .splash_radius_half_tiles = 1,
    .trained_at = BuildingKind::castle,
    .minimum_age = Age::castle,
};

constexpr UnitRules battering_ram_rules{
    .hit_points = 175,
    .attack = 2,
    .attack_interval_ticks = 10,
    .movement_interval_ticks = 3,
    .attack_range = 1,
    .damage_class = DamageClass::melee,
    .bonus_vs_cavalry = 0,
    .bonus_vs_buildings = 125,
    .melee_armor = -3,
    .pierce_armor = 180,
    .wood_cost = 160,
    .food_cost = 0,
    .gold_cost = 75,
    .training_ticks = 180,
    .vision_range = 3,
    .trained_at = BuildingKind::siege_workshop,
    .minimum_age = Age::castle,
};

constexpr UnitRules capped_ram_rules{
    .hit_points = 200,
    .attack = 3,
    .attack_interval_ticks = 10,
    .movement_interval_ticks = 3,
    .attack_range = 1,
    .splash_radius = 1,
    .damage_class = DamageClass::melee,
    .bonus_vs_cavalry = 0,
    .bonus_vs_buildings = 150,
    .melee_armor = -3,
    .pierce_armor = 190,
    .wood_cost = 160,
    .food_cost = 0,
    .gold_cost = 75,
    .training_ticks = 180,
    .vision_range = 3,
    .trained_at = BuildingKind::siege_workshop,
    .minimum_age = Age::imperial,
};

constexpr UnitRules siege_ram_rules{
    .hit_points = 270,
    .attack = 4,
    .attack_interval_ticks = 10,
    .movement_interval_ticks = 3,
    .attack_range = 1,
    .splash_radius = 2,
    .damage_class = DamageClass::melee,
    .bonus_vs_cavalry = 0,
    .bonus_vs_buildings = 200,
    .melee_armor = -3,
    .pierce_armor = 195,
    .wood_cost = 160,
    .food_cost = 0,
    .gold_cost = 75,
    .training_ticks = 180,
    .vision_range = 3,
    .trained_at = BuildingKind::siege_workshop,
    .minimum_age = Age::imperial,
};

constexpr UnitRules skirmisher_rules{
    .hit_points = 30,
    .attack = 2,
    .attack_interval_ticks = 7,
    .movement_interval_ticks = 2,
    .attack_range = 4,
    .minimum_attack_range = 1,
    .damage_class = DamageClass::pierce,
    .bonus_vs_cavalry = 0,
    .bonus_vs_buildings = 0,
    .bonus_vs_archers = 3,
    .bonus_vs_spearmen = 3,
    .melee_armor = 0,
    .pierce_armor = 3,
    .wood_cost = 35,
    .food_cost = 25,
    .gold_cost = 0,
    .training_ticks = 110,
    .vision_range = 6,
    .trained_at = BuildingKind::archery_range,
    .minimum_age = Age::feudal,
};

constexpr UnitRules elite_skirmisher_unit_rules{
    .hit_points = 35,
    .attack = 3,
    .attack_interval_ticks = 6,
    .movement_interval_ticks = 2,
    .attack_range = 5,
    .minimum_attack_range = 1,
    .damage_class = DamageClass::pierce,
    .bonus_vs_cavalry = 0,
    .bonus_vs_buildings = 0,
    .bonus_vs_archers = 4,
    .bonus_vs_spearmen = 3,
    .melee_armor = 0,
    .pierce_armor = 4,
    .wood_cost = 35,
    .food_cost = 25,
    .gold_cost = 0,
    .training_ticks = 110,
    .vision_range = 7,
    .trained_at = BuildingKind::archery_range,
    .minimum_age = Age::castle,
};

constexpr UnitRules mangonel_rules{
    .hit_points = 50,
    .attack = 40,
    .attack_interval_ticks = 12,
    .movement_interval_ticks = 3,
    .attack_range = 7,
    .minimum_attack_range = 3,
    .splash_radius = 1,
    .damage_class = DamageClass::melee,
    .bonus_vs_cavalry = 0,
    .bonus_vs_buildings = 0,
    .bonus_vs_archers = 0,
    .melee_armor = 0,
    .pierce_armor = 6,
    .wood_cost = 160,
    .food_cost = 0,
    .gold_cost = 135,
    .training_ticks = 230,
    .vision_range = 9,
    .trained_at = BuildingKind::siege_workshop,
    .minimum_age = Age::castle,
};
constexpr UnitRules onager_rules{
    .hit_points = 60, .attack = 50, .attack_interval_ticks = 12,
    .movement_interval_ticks = 3, .attack_range = 8,
    .minimum_attack_range = 3, .splash_radius = 1,
    .damage_class = DamageClass::melee, .bonus_vs_cavalry = 0,
    .melee_armor = 0, .pierce_armor = 7,
    .wood_cost = 160, .food_cost = 0, .gold_cost = 135,
    .training_ticks = 230, .vision_range = 10,
    .projectile_count = 8, .projectile_spread = 1,
    .trained_at = BuildingKind::siege_workshop,
    .minimum_age = Age::imperial,
};
constexpr UnitRules siege_onager_rules{
    .hit_points = 70, .attack = 75, .attack_interval_ticks = 12,
    .movement_interval_ticks = 3, .attack_range = 8,
    .minimum_attack_range = 3, .splash_radius = 2,
    .damage_class = DamageClass::melee, .bonus_vs_cavalry = 0,
    .melee_armor = 0, .pierce_armor = 8,
    .wood_cost = 160, .food_cost = 0, .gold_cost = 135,
    .training_ticks = 230, .vision_range = 10,
    .projectile_count = 10, .projectile_spread = 2,
    .trained_at = BuildingKind::siege_workshop,
    .minimum_age = Age::imperial,
};
constexpr UnitRules scorpion_rules{
    .hit_points = 40, .attack = 12, .attack_interval_ticks = 7,
    .movement_interval_ticks = 3, .attack_range = 7,
    .minimum_attack_range = 2,
    .damage_class = DamageClass::pierce, .bonus_vs_cavalry = 0,
    .melee_armor = 0, .pierce_armor = 6,
    .wood_cost = 75, .food_cost = 0, .gold_cost = 75,
    .training_ticks = 150, .vision_range = 9,
    .trained_at = BuildingKind::siege_workshop,
    .minimum_age = Age::castle,
};
constexpr UnitRules heavy_scorpion_rules{
    .hit_points = 50, .attack = 16, .attack_interval_ticks = 7,
    .movement_interval_ticks = 3, .attack_range = 7,
    .minimum_attack_range = 2,
    .damage_class = DamageClass::pierce, .bonus_vs_cavalry = 0,
    .melee_armor = 0, .pierce_armor = 7,
    .wood_cost = 75, .food_cost = 0, .gold_cost = 75,
    .training_ticks = 150, .vision_range = 9,
    .trained_at = BuildingKind::siege_workshop,
    .minimum_age = Age::imperial,
};
constexpr UnitRules eagle_warrior_rules{
    .hit_points = 50, .attack = 4, .attack_interval_ticks = 4,
    .movement_interval_ticks = 1, .attack_range = 1,
    .damage_class = DamageClass::melee, .bonus_vs_cavalry = 0,
    .bonus_vs_siege = 3, .melee_armor = 0, .pierce_armor = 2,
    .wood_cost = 0, .food_cost = 20, .gold_cost = 50,
    .training_ticks = 175, .vision_range = 6,
    .trained_at = BuildingKind::barracks, .minimum_age = Age::castle,
};
constexpr UnitRules elite_eagle_warrior_rules{
    .hit_points = 60, .attack = 9, .attack_interval_ticks = 4,
    .movement_interval_ticks = 1, .attack_range = 1,
    .damage_class = DamageClass::melee, .bonus_vs_cavalry = 0,
    .bonus_vs_siege = 3, .melee_armor = 0, .pierce_armor = 4,
    .wood_cost = 0, .food_cost = 20, .gold_cost = 50,
    .training_ticks = 100, .vision_range = 6,
    .trained_at = BuildingKind::barracks, .minimum_age = Age::imperial,
};
constexpr UnitRules packed_trebuchet_rules{
    .hit_points = 150, .attack = 0, .attack_interval_ticks = 20,
    .movement_interval_ticks = 2, .attack_range = 1,
    .damage_class = DamageClass::melee, .bonus_vs_cavalry = 0,
    .melee_armor = 2, .pierce_armor = 8,
    .wood_cost = 200, .food_cost = 0, .gold_cost = 200,
    .training_ticks = 250, .vision_range = 18,
    .trained_at = BuildingKind::castle, .minimum_age = Age::imperial,
};
constexpr UnitRules trebuchet_rules{
    .hit_points = 150, .attack = 200, .attack_interval_ticks = 20,
    .movement_interval_ticks = 1000000, .attack_range = 16,
    .minimum_attack_range = 4,
    .damage_class = DamageClass::pierce, .bonus_vs_cavalry = 0,
    .bonus_vs_buildings = 250,
    .melee_armor = 1, .pierce_armor = 150,
    .wood_cost = 200, .food_cost = 0, .gold_cost = 200,
    .training_ticks = 250, .vision_range = 18,
    .accuracy_percent = 15,
    .trained_at = BuildingKind::castle, .minimum_age = Age::imperial,
};
constexpr UnitRules cavalry_archer_rules{
    .hit_points = 50, .attack = 6, .attack_interval_ticks = 4,
    .movement_interval_ticks = 1, .attack_range = 4,
    .damage_class = DamageClass::pierce, .bonus_vs_cavalry = 0,
    .melee_armor = 0, .pierce_armor = 0,
    .wood_cost = 40, .food_cost = 0, .gold_cost = 70,
    .training_ticks = 170, .vision_range = 5,
    .accuracy_percent = 50,
    .trained_at = BuildingKind::archery_range,
    .minimum_age = Age::castle,
};
constexpr UnitRules heavy_cavalry_archer_rules{
    .hit_points = 60, .attack = 7, .attack_interval_ticks = 4,
    .movement_interval_ticks = 1, .attack_range = 4,
    .damage_class = DamageClass::pierce, .bonus_vs_cavalry = 0,
    .melee_armor = 1, .pierce_armor = 0,
    .wood_cost = 40, .food_cost = 0, .gold_cost = 70,
    .training_ticks = 135, .vision_range = 6,
    .accuracy_percent = 50,
    .trained_at = BuildingKind::archery_range,
    .minimum_age = Age::imperial,
};
constexpr UnitRules camel_rider_rules{
    .hit_points = 100, .attack = 5, .attack_interval_ticks = 4,
    .movement_interval_ticks = 1, .attack_range = 1,
    .damage_class = DamageClass::melee, .bonus_vs_cavalry = 10,
    .melee_armor = 0, .pierce_armor = 0,
    .wood_cost = 0, .food_cost = 55, .gold_cost = 60,
    .training_ticks = 110, .vision_range = 4,
    .trained_at = BuildingKind::stable, .minimum_age = Age::castle,
};
constexpr UnitRules heavy_camel_rules{
    .hit_points = 120, .attack = 7, .attack_interval_ticks = 4,
    .movement_interval_ticks = 1, .attack_range = 1,
    .damage_class = DamageClass::melee, .bonus_vs_cavalry = 18,
    .melee_armor = 0, .pierce_armor = 0,
    .wood_cost = 0, .food_cost = 55, .gold_cost = 60,
    .training_ticks = 110, .vision_range = 5,
    .trained_at = BuildingKind::stable, .minimum_age = Age::imperial,
};

constexpr UnitRules man_at_arms_unit_rules{
    .hit_points = 45,
    .attack = 6,
    .attack_interval_ticks = 5,
    .movement_interval_ticks = 2,
    .attack_range = 1,
    .damage_class = DamageClass::melee,
    .bonus_vs_cavalry = 0,
    .bonus_vs_buildings = 2,
    .bonus_vs_archers = 0,
    .melee_armor = 0,
    .pierce_armor = 1,
    .wood_cost = 0,
    .food_cost = 60,
    .gold_cost = 20,
    .training_ticks = 105,
    .vision_range = 4,
    .trained_at = BuildingKind::barracks,
    .minimum_age = Age::feudal,
};

constexpr UnitRules long_swordsman_unit_rules{
    .hit_points = 55,
    .attack = 9,
    .attack_interval_ticks = 5,
    .movement_interval_ticks = 2,
    .attack_range = 1,
    .damage_class = DamageClass::melee,
    .bonus_vs_cavalry = 0,
    .bonus_vs_buildings = 3,
    .bonus_vs_archers = 0,
    .melee_armor = 0,
    .pierce_armor = 1,
    .wood_cost = 0,
    .food_cost = 60,
    .gold_cost = 20,
    .training_ticks = 105,
    .vision_range = 4,
    .trained_at = BuildingKind::barracks,
    .minimum_age = Age::castle,
};

constexpr UnitRules two_handed_swordsman_unit_rules{
    .hit_points = 60,
    .attack = 11,
    .attack_interval_ticks = 5,
    .movement_interval_ticks = 2,
    .attack_range = 1,
    .damage_class = DamageClass::melee,
    .bonus_vs_cavalry = 0,
    .bonus_vs_buildings = 4,
    .bonus_vs_archers = 0,
    .melee_armor = 0,
    .pierce_armor = 1,
    .wood_cost = 0,
    .food_cost = 60,
    .gold_cost = 20,
    .training_ticks = 105,
    .vision_range = 5,
    .trained_at = BuildingKind::barracks,
    .minimum_age = Age::imperial,
};

constexpr UnitRules champion_unit_rules{
    .hit_points = 70,
    .attack = 13,
    .attack_interval_ticks = 5,
    .movement_interval_ticks = 2,
    .attack_range = 1,
    .damage_class = DamageClass::melee,
    .bonus_vs_cavalry = 0,
    .bonus_vs_buildings = 4,
    .bonus_vs_archers = 0,
    .melee_armor = 1,
    .pierce_armor = 1,
    .wood_cost = 0,
    .food_cost = 60,
    .gold_cost = 20,
    .training_ticks = 105,
    .vision_range = 5,
    .trained_at = BuildingKind::barracks,
    .minimum_age = Age::imperial,
};

constexpr BuildingRules town_center_rules{
    .hit_points = 2400,
    .melee_armor = 3,
    .pierce_armor = 5,
    .wood_cost = 275,
    .stone_cost = 100,
    .construction_ticks = 500,
    .vision_range = 8,
    .population_support = 5,
    .minimum_age = Age::castle,
    .footprint_width = 4,
    .footprint_height = 4,
    .attack = 5,
    .attack_interval_ticks = 10,
    .attack_range = 6,
    .projectile_count = 0,
    .damage_class = DamageClass::pierce,
};

constexpr BuildingRules barracks_rules{
    .hit_points = 1200,
    .melee_armor = 0,
    .pierce_armor = 7,
    .wood_cost = 175,
    .stone_cost = 0,
    .construction_ticks = 250,
    .vision_range = 5,
    .population_support = 0,
    .minimum_age = Age::dark,
};

constexpr BuildingRules archery_range_rules{
    .hit_points = 1500,
    .melee_armor = 1,
    .pierce_armor = 8,
    .wood_cost = 175,
    .stone_cost = 0,
    .construction_ticks = 250,
    .vision_range = 5,
    .population_support = 0,
    .minimum_age = Age::feudal,
};

constexpr BuildingRules house_rules{
    .hit_points = 900,
    .melee_armor = 0,
    .pierce_armor = 7,
    .wood_cost = 30,
    .stone_cost = 0,
    .construction_ticks = 125,
    .vision_range = 3,
    .population_support = 5,
    .minimum_age = Age::dark,
};

constexpr BuildingRules mill_rules{
    .hit_points = 1000,
    .melee_armor = 0,
    .pierce_armor = 7,
    .wood_cost = 100,
    .stone_cost = 0,
    .construction_ticks = 175,
    .vision_range = 4,
    .population_support = 0,
    .minimum_age = Age::dark,
};

constexpr BuildingRules lumber_camp_rules{
    .hit_points = 1000,
    .melee_armor = 0,
    .pierce_armor = 7,
    .wood_cost = 100,
    .stone_cost = 0,
    .construction_ticks = 175,
    .vision_range = 4,
    .population_support = 0,
    .minimum_age = Age::dark,
};

constexpr BuildingRules mining_camp_rules{
    .hit_points = 1000,
    .melee_armor = 0,
    .pierce_armor = 7,
    .wood_cost = 100,
    .stone_cost = 0,
    .construction_ticks = 175,
    .vision_range = 4,
    .population_support = 0,
    .minimum_age = Age::dark,
};

constexpr BuildingRules farm_rules{
    .hit_points = 480,
    .melee_armor = 0,
    .pierce_armor = 0,
    .wood_cost = 60,
    .stone_cost = 0,
    .construction_ticks = 75,
    .vision_range = 2,
    .population_support = 0,
    .minimum_age = Age::dark,
};

constexpr BuildingRules stable_rules{
    .hit_points = 1500,
    .melee_armor = 1,
    .pierce_armor = 8,
    .wood_cost = 175,
    .stone_cost = 0,
    .construction_ticks = 250,
    .vision_range = 5,
    .population_support = 0,
    .minimum_age = Age::feudal,
};

constexpr BuildingRules blacksmith_rules{
    .hit_points = 2100,
    .melee_armor = 0,
    .pierce_armor = 7,
    .wood_cost = 150,
    .stone_cost = 0,
    .construction_ticks = 200,
    .vision_range = 5,
    .population_support = 0,
    .minimum_age = Age::feudal,
};

constexpr BuildingRules castle_rules{
    .hit_points = 4800,
    .melee_armor = 8,
    .pierce_armor = 11,
    .wood_cost = 0,
    .stone_cost = 650,
    .construction_ticks = 1000,
    .vision_range = 11,
    .population_support = 20,
    .minimum_age = Age::castle,
    .footprint_width = 4,
    .footprint_height = 4,
    .attack = 11,
    .attack_interval_ticks = 5,
    .attack_range = 8,
    .minimum_attack_range = 1,
    .projectile_count = 5,
    .damage_class = DamageClass::pierce,
};

constexpr BuildingRules university_rules{
    .hit_points = 2100,
    .melee_armor = 2,
    .pierce_armor = 9,
    .wood_cost = 200,
    .stone_cost = 0,
    .construction_ticks = 300,
    .vision_range = 5,
    .population_support = 0,
    .minimum_age = Age::castle,
    .footprint_width = 3,
    .footprint_height = 3,
};

constexpr BuildingRules monastery_rules{
    .hit_points = 2100,
    .melee_armor = 0,
    .pierce_armor = 7,
    .wood_cost = 175,
    .stone_cost = 0,
    .construction_ticks = 200,
    .vision_range = 5,
    .population_support = 0,
    .minimum_age = Age::castle,
    .footprint_width = 3,
    .footprint_height = 3,
};

constexpr BuildingRules market_rules{
    .hit_points = 2100,
    .melee_armor = 1,
    .pierce_armor = 8,
    .wood_cost = 175,
    .stone_cost = 0,
    .construction_ticks = 300,
    .vision_range = 5,
    .population_support = 0,
    .minimum_age = Age::feudal,
    .footprint_width = 3,
    .footprint_height = 3,
};

constexpr BuildingRules dock_rules{
    .hit_points = 1800,
    .melee_armor = 0,
    .pierce_armor = 7,
    .wood_cost = 150,
    .stone_cost = 0,
    .construction_ticks = 175,
    .vision_range = 6,
    .population_support = 0,
    .minimum_age = Age::dark,
    .footprint_width = 3,
    .footprint_height = 3,
};

constexpr BuildingRules bombard_tower_rules{
    .hit_points = 2220,
    .melee_armor = 3,
    .pierce_armor = 9,
    .wood_cost = 0,
    .stone_cost = 125,
    .gold_cost = 100,
    .construction_ticks = 400,
    .vision_range = 10,
    .population_support = 0,
    .minimum_age = Age::imperial,
    .footprint_width = 1,
    .footprint_height = 1,
    .attack = 120,
    .attack_interval_ticks = 12,
    .attack_range = 8,
    .minimum_attack_range = 1,
    .projectile_count = 1,
    .damage_class = DamageClass::pierce,
    .bonus_vs_camels = 40,
    .bonus_vs_ships = 40,
    .accuracy_percent = 92,
    .projectile_speed_tenths = 30,
};
constexpr BuildingRules fish_trap_rules{
    .hit_points = 50,
    .melee_armor = 0,
    .pierce_armor = 0,
    .wood_cost = 100,
    .stone_cost = 0,
    .construction_ticks = 265,
    .vision_range = 1,
    .population_support = 0,
    .minimum_age = Age::feudal,
    .footprint_width = 1,
    .footprint_height = 1,
};
constexpr BuildingRules outpost_rules{
    .hit_points = 500,
    .melee_armor = 0,
    .pierce_armor = 0,
    .wood_cost = 25,
    .stone_cost = 10,
    .construction_ticks = 75,
    .vision_range = 6,
    .population_support = 0,
    .minimum_age = Age::dark,
    .footprint_width = 1,
    .footprint_height = 1,
};
constexpr BuildingRules wonder_rules{
    .hit_points = 4800,
    .melee_armor = 3,
    .pierce_armor = 10,
    .wood_cost = 1000,
    .stone_cost = 1000,
    .gold_cost = 1000,
    .construction_ticks = 17500,
    .vision_range = 8,
    .population_support = 0,
    .minimum_age = Age::imperial,
    .footprint_width = 4,
    .footprint_height = 4,
};

constexpr BuildingRules siege_workshop_rules{
    .hit_points = 2100,
    .melee_armor = 2,
    .pierce_armor = 9,
    .wood_cost = 200,
    .stone_cost = 0,
    .construction_ticks = 200,
    .vision_range = 5,
    .population_support = 0,
    .minimum_age = Age::castle,
    .footprint_width = 3,
    .footprint_height = 3,
};

constexpr BuildingRules palisade_wall_rules{
    .hit_points = 250,
    .melee_armor = 2,
    .pierce_armor = 5,
    .wood_cost = 2,
    .stone_cost = 0,
    .construction_ticks = 25,
    .vision_range = 2,
    .population_support = 0,
    .minimum_age = Age::dark,
};

constexpr BuildingRules palisade_gate_x_rules{
    .hit_points = 600,
    .melee_armor = 2,
    .pierce_armor = 6,
    .wood_cost = 30,
    .stone_cost = 0,
    .construction_ticks = 150,
    .vision_range = 2,
    .population_support = 0,
    .minimum_age = Age::dark,
    .footprint_width = 4,
    .footprint_height = 1,
};

constexpr BuildingRules palisade_gate_y_rules{
    .hit_points = 600,
    .melee_armor = 2,
    .pierce_armor = 6,
    .wood_cost = 30,
    .stone_cost = 0,
    .construction_ticks = 350,
    .vision_range = 2,
    .population_support = 0,
    .minimum_age = Age::dark,
    .footprint_width = 1,
    .footprint_height = 4,
};

constexpr BuildingRules watch_tower_rules{
    .hit_points = 1020,
    .melee_armor = 1,
    .pierce_armor = 7,
    .wood_cost = 25,
    .stone_cost = 125,
    .construction_ticks = 400,
    .vision_range = 10,
    .population_support = 0,
    .minimum_age = Age::feudal,
    .attack = 5,
    .attack_interval_ticks = 10,
    .attack_range = 8,
    .minimum_attack_range = 1,
    .projectile_count = 1,
    .damage_class = DamageClass::pierce,
    .bonus_vs_ships = 7,
};

constexpr BuildingRules guard_tower_building_rules{
    .hit_points = 1500, .melee_armor = 2, .pierce_armor = 8,
    .wood_cost = 25, .stone_cost = 125, .construction_ticks = 16,
    .vision_range = 10, .population_support = 0, .minimum_age = Age::castle,
    .attack = 7, .attack_interval_ticks = 10, .attack_range = 8,
    .minimum_attack_range = 1, .projectile_count = 1,
    .damage_class = DamageClass::pierce, .bonus_vs_ships = 7,
};

constexpr BuildingRules keep_building_rules{
    .hit_points = 2250, .melee_armor = 3, .pierce_armor = 9,
    .wood_cost = 25, .stone_cost = 125, .construction_ticks = 16,
    .vision_range = 10, .population_support = 0, .minimum_age = Age::imperial,
    .attack = 8, .attack_interval_ticks = 10, .attack_range = 8,
    .minimum_attack_range = 1, .projectile_count = 1,
    .damage_class = DamageClass::pierce, .bonus_vs_ships = 7,
};

constexpr BuildingRules stone_wall_rules{
    .hit_points = 1800,
    .melee_armor = 8,
    .pierce_armor = 10,
    .wood_cost = 0,
    .stone_cost = 5,
    .construction_ticks = 40,
    .vision_range = 2,
    .population_support = 0,
    .minimum_age = Age::feudal,
};

constexpr BuildingRules stone_gate_x_rules{
    .hit_points = 2750,
    .melee_armor = 6,
    .pierce_armor = 6,
    .wood_cost = 0,
    .stone_cost = 30,
    .construction_ticks = 350,
    .vision_range = 5,
    .population_support = 0,
    .minimum_age = Age::feudal,
    .footprint_width = 4,
    .footprint_height = 1,
};

constexpr BuildingRules stone_gate_y_rules{
    .hit_points = 2750,
    .melee_armor = 6,
    .pierce_armor = 6,
    .wood_cost = 0,
    .stone_cost = 30,
    .construction_ticks = 350,
    .vision_range = 5,
    .population_support = 0,
    .minimum_age = Age::feudal,
    .footprint_width = 1,
    .footprint_height = 4,
};

constexpr BuildingRules fortified_wall_building_rules{
    .hit_points = 3000, .melee_armor = 12, .pierce_armor = 12,
    .wood_cost = 0, .stone_cost = 5, .construction_ticks = 10,
    .vision_range = 2, .population_support = 0, .minimum_age = Age::castle,
};

constexpr BuildingRules fortified_gate_x_rules{
    .hit_points = 4000, .melee_armor = 6, .pierce_armor = 6,
    .wood_cost = 0, .stone_cost = 30, .construction_ticks = 14,
    .vision_range = 5, .population_support = 0, .minimum_age = Age::castle,
    .footprint_width = 4, .footprint_height = 1,
};

constexpr BuildingRules fortified_gate_y_rules{
    .hit_points = 4000, .melee_armor = 6, .pierce_armor = 6,
    .wood_cost = 0, .stone_cost = 30, .construction_ticks = 14,
    .vision_range = 5, .population_support = 0, .minimum_age = Age::castle,
    .footprint_width = 1, .footprint_height = 4,
};

constexpr AgeRules dark_age_rules{
    .food_cost = 0,
    .gold_cost = 0,
    .research_ticks = 0,
};

constexpr AgeRules feudal_age_rules{
    .food_cost = 500,
    .gold_cost = 0,
    .research_ticks = 20,
};

constexpr AgeRules castle_age_rules{
    .food_cost = 800,
    .gold_cost = 200,
    .research_ticks = 30,
};

constexpr AgeRules imperial_age_rules{
    .food_cost = 1000,
    .gold_cost = 800,
    .research_ticks = 40,
};

constexpr TechnologyRules wheelbarrow_rules{
    .researched_at = BuildingKind::town_center,
    .minimum_age = Age::feudal,
    .wood_cost = 50,
    .food_cost = 175,
    .gold_cost = 0,
    .stone_cost = 0,
    .research_ticks = 15,
};

constexpr TechnologyRules loom_rules{
    .researched_at = BuildingKind::town_center,
    .minimum_age = Age::dark,
    .wood_cost = 0,
    .food_cost = 0,
    .gold_cost = 50,
    .stone_cost = 0,
    .research_ticks = 5,
};

constexpr TechnologyRules double_bit_axe_rules{
    .researched_at = BuildingKind::lumber_camp,
    .minimum_age = Age::feudal,
    .wood_cost = 50,
    .food_cost = 100,
    .gold_cost = 0,
    .stone_cost = 0,
    .research_ticks = 5,
};

constexpr TechnologyRules horse_collar_rules{
    .researched_at = BuildingKind::mill,
    .minimum_age = Age::feudal,
    .wood_cost = 75,
    .food_cost = 75,
    .gold_cost = 0,
    .stone_cost = 0,
    .research_ticks = 4,
};

constexpr TechnologyRules fortified_wall_rules{
    .researched_at = BuildingKind::university,
    .minimum_age = Age::castle,
    .wood_cost = 100,
    .food_cost = 200,
    .gold_cost = 0,
    .stone_cost = 0,
    .research_ticks = 10,
};

constexpr TechnologyRules guard_tower_rules{
    .researched_at = BuildingKind::university,
    .minimum_age = Age::castle,
    .wood_cost = 250,
    .food_cost = 100,
    .gold_cost = 0,
    .stone_cost = 0,
    .research_ticks = 6,
};

constexpr TechnologyRules keep_rules{
    .researched_at = BuildingKind::university,
    .minimum_age = Age::imperial,
    .wood_cost = 350,
    .food_cost = 500,
    .gold_cost = 0,
    .stone_cost = 0,
    .research_ticks = 15,
};

constexpr TechnologyRules bodkin_arrow_rules{
    .researched_at = BuildingKind::blacksmith,
    .minimum_age = Age::castle,
    .wood_cost = 0,
    .food_cost = 200,
    .gold_cost = 100,
    .stone_cost = 0,
    .research_ticks = 7,
};

constexpr TechnologyRules bracer_rules{
    .researched_at = BuildingKind::blacksmith,
    .minimum_age = Age::imperial,
    .wood_cost = 0,
    .food_cost = 300,
    .gold_cost = 200,
    .stone_cost = 0,
    .research_ticks = 8,
};

constexpr TechnologyRules iron_casting_rules{
    .researched_at = BuildingKind::blacksmith,
    .minimum_age = Age::castle,
    .wood_cost = 0,
    .food_cost = 220,
    .gold_cost = 120,
    .stone_cost = 0,
    .research_ticks = 15,
};

constexpr TechnologyRules blast_furnace_rules{
    .researched_at = BuildingKind::blacksmith,
    .minimum_age = Age::imperial,
    .wood_cost = 0,
    .food_cost = 275,
    .gold_cost = 225,
    .stone_cost = 0,
    .research_ticks = 20,
};

constexpr TechnologyRules scale_mail_armor_rules{
    .researched_at = BuildingKind::blacksmith,
    .minimum_age = Age::feudal,
    .wood_cost = 0,
    .food_cost = 100,
    .gold_cost = 0,
    .stone_cost = 0,
    .research_ticks = 8,
};

constexpr TechnologyRules chain_mail_armor_rules{
    .researched_at = BuildingKind::blacksmith,
    .minimum_age = Age::castle,
    .wood_cost = 0,
    .food_cost = 200,
    .gold_cost = 100,
    .stone_cost = 0,
    .research_ticks = 11,
};

constexpr TechnologyRules plate_mail_armor_rules{
    .researched_at = BuildingKind::blacksmith,
    .minimum_age = Age::imperial,
    .wood_cost = 0,
    .food_cost = 300,
    .gold_cost = 150,
    .stone_cost = 0,
    .research_ticks = 14,
};

constexpr TechnologyRules scale_barding_armor_rules{
    .researched_at = BuildingKind::blacksmith,
    .minimum_age = Age::feudal,
    .wood_cost = 0,
    .food_cost = 150,
    .gold_cost = 0,
    .stone_cost = 0,
    .research_ticks = 9,
};

constexpr TechnologyRules chain_barding_armor_rules{
    .researched_at = BuildingKind::blacksmith,
    .minimum_age = Age::castle,
    .wood_cost = 0,
    .food_cost = 250,
    .gold_cost = 150,
    .stone_cost = 0,
    .research_ticks = 12,
};

constexpr TechnologyRules plate_barding_armor_rules{
    .researched_at = BuildingKind::blacksmith,
    .minimum_age = Age::imperial,
    .wood_cost = 0,
    .food_cost = 350,
    .gold_cost = 200,
    .stone_cost = 0,
    .research_ticks = 15,
};

constexpr TechnologyRules padded_archer_armor_rules{
    .researched_at = BuildingKind::blacksmith,
    .minimum_age = Age::feudal,
    .wood_cost = 0,
    .food_cost = 100,
    .gold_cost = 0,
    .stone_cost = 0,
    .research_ticks = 8,
};

constexpr TechnologyRules leather_archer_armor_rules{
    .researched_at = BuildingKind::blacksmith,
    .minimum_age = Age::castle,
    .wood_cost = 0,
    .food_cost = 150,
    .gold_cost = 150,
    .stone_cost = 0,
    .research_ticks = 11,
};

constexpr TechnologyRules ring_archer_armor_rules{
    .researched_at = BuildingKind::blacksmith,
    .minimum_age = Age::imperial,
    .wood_cost = 0,
    .food_cost = 250,
    .gold_cost = 250,
    .stone_cost = 0,
    .research_ticks = 14,
};

constexpr TechnologyRules bloodlines_rules{
    .researched_at = BuildingKind::stable,
    .minimum_age = Age::feudal,
    .wood_cost = 0,
    .food_cost = 150,
    .gold_cost = 100,
    .stone_cost = 0,
    .research_ticks = 10,
};

constexpr TechnologyRules husbandry_rules{
    .researched_at = BuildingKind::stable,
    .minimum_age = Age::castle,
    .wood_cost = 0,
    .food_cost = 250,
    .gold_cost = 0,
    .stone_cost = 0,
    .research_ticks = 8,
};

constexpr TechnologyRules cavalier_technology_rules{
    .researched_at = BuildingKind::stable,
    .minimum_age = Age::imperial,
    .wood_cost = 0,
    .food_cost = 300,
    .gold_cost = 300,
    .stone_cost = 0,
    .research_ticks = 20,
};

constexpr TechnologyRules paladin_technology_rules{
    .researched_at = BuildingKind::stable,
    .minimum_age = Age::imperial,
    .wood_cost = 0,
    .food_cost = 1300,
    .gold_cost = 750,
    .stone_cost = 0,
    .research_ticks = 34,
};

constexpr TechnologyRules light_cavalry_technology_rules{
    .researched_at = BuildingKind::stable,
    .minimum_age = Age::castle,
    .wood_cost = 0,
    .food_cost = 150,
    .gold_cost = 50,
    .stone_cost = 0,
    .research_ticks = 9,
};

constexpr TechnologyRules hussar_technology_rules{
    .researched_at = BuildingKind::stable,
    .minimum_age = Age::imperial,
    .wood_cost = 0,
    .food_cost = 500,
    .gold_cost = 600,
    .stone_cost = 0,
    .research_ticks = 10,
};

constexpr TechnologyRules two_handed_swordsman_technology_rules{
    .researched_at = BuildingKind::barracks,
    .minimum_age = Age::imperial,
    .wood_cost = 0,
    .food_cost = 300,
    .gold_cost = 100,
    .stone_cost = 0,
    .research_ticks = 15,
};

constexpr TechnologyRules champion_technology_rules{
    .researched_at = BuildingKind::barracks,
    .minimum_age = Age::imperial,
    .wood_cost = 0,
    .food_cost = 750,
    .gold_cost = 350,
    .stone_cost = 0,
    .research_ticks = 20,
};

constexpr TechnologyRules arbalester_technology_rules{
    .researched_at = BuildingKind::archery_range,
    .minimum_age = Age::imperial,
    .wood_cost = 0,
    .food_cost = 350,
    .gold_cost = 300,
    .stone_cost = 0,
    .research_ticks = 10,
};

constexpr TechnologyRules elite_skirmisher_technology_rules{
    .researched_at = BuildingKind::archery_range,
    .minimum_age = Age::castle,
    .wood_cost = 250,
    .food_cost = 0,
    .gold_cost = 160,
    .stone_cost = 0,
    .research_ticks = 10,
};
constexpr TechnologyRules war_galley_technology_rules{
    .researched_at = BuildingKind::dock, .minimum_age = Age::castle,
    .wood_cost = 0, .food_cost = 230, .gold_cost = 100,
    .stone_cost = 0, .research_ticks = 10,
};
constexpr TechnologyRules galleon_technology_rules{
    .researched_at = BuildingKind::dock, .minimum_age = Age::imperial,
    .wood_cost = 315, .food_cost = 400, .gold_cost = 0,
    .stone_cost = 0, .research_ticks = 12,
};
constexpr TechnologyRules fast_fire_ship_technology_rules{
    .researched_at = BuildingKind::dock, .minimum_age = Age::imperial,
    .wood_cost = 280, .food_cost = 0, .gold_cost = 250,
    .stone_cost = 0, .research_ticks = 12,
};
constexpr TechnologyRules heavy_demolition_ship_technology_rules{
    .researched_at = BuildingKind::dock, .minimum_age = Age::imperial,
    .wood_cost = 200, .food_cost = 0, .gold_cost = 300,
    .stone_cost = 0, .research_ticks = 12,
};
constexpr TechnologyRules cannon_galleon_technology_rules{
    .researched_at = BuildingKind::dock, .minimum_age = Age::imperial,
    .wood_cost = 500, .food_cost = 400, .gold_cost = 0,
    .stone_cost = 0, .research_ticks = 17,
};
constexpr TechnologyRules elite_cannon_galleon_technology_rules{
    .researched_at = BuildingKind::dock, .minimum_age = Age::imperial,
    .wood_cost = 525, .food_cost = 0, .gold_cost = 500,
    .stone_cost = 0, .research_ticks = 10,
};
constexpr TechnologyRules careening_technology_rules{
    .researched_at = BuildingKind::dock, .minimum_age = Age::castle,
    .wood_cost = 0, .food_cost = 250, .gold_cost = 150,
    .stone_cost = 0, .research_ticks = 17,
};
constexpr TechnologyRules dry_dock_technology_rules{
    .researched_at = BuildingKind::dock, .minimum_age = Age::imperial,
    .wood_cost = 0, .food_cost = 600, .gold_cost = 400,
    .stone_cost = 0, .research_ticks = 20,
};
constexpr TechnologyRules shipwright_technology_rules{
    .researched_at = BuildingKind::dock, .minimum_age = Age::imperial,
    .wood_cost = 0, .food_cost = 1000, .gold_cost = 300,
    .stone_cost = 0, .research_ticks = 20,
};
constexpr TechnologyRules longboat_technology_rules{
    .researched_at = BuildingKind::dock, .minimum_age = Age::castle,
    .wood_cost = 0, .food_cost = 0, .gold_cost = 0,
    .stone_cost = 0, .research_ticks = 1,
};
constexpr TechnologyRules elite_longboat_technology_rules{
    .researched_at = BuildingKind::dock, .minimum_age = Age::imperial,
    .wood_cost = 0, .food_cost = 750, .gold_cost = 475,
    .stone_cost = 0, .research_ticks = 20,
};
constexpr TechnologyRules turtle_ship_technology_rules{
    .researched_at = BuildingKind::dock, .minimum_age = Age::castle,
    .wood_cost = 0, .food_cost = 0, .gold_cost = 0,
    .stone_cost = 0, .research_ticks = 1,
};
constexpr TechnologyRules elite_turtle_ship_technology_rules{
    .researched_at = BuildingKind::dock, .minimum_age = Age::imperial,
    .wood_cost = 0, .food_cost = 1000, .gold_cost = 800,
    .stone_cost = 0, .research_ticks = 22,
};
constexpr TechnologyRules longbowman_technology_rules{
    .researched_at = BuildingKind::castle, .minimum_age = Age::castle,
    .wood_cost = 0, .food_cost = 0, .gold_cost = 0,
    .stone_cost = 0, .research_ticks = 1,
};
constexpr TechnologyRules elite_longbowman_technology_rules{
    .researched_at = BuildingKind::castle, .minimum_age = Age::imperial,
    .wood_cost = 0, .food_cost = 850, .gold_cost = 850,
    .stone_cost = 0, .research_ticks = 20,
};
constexpr TechnologyRules throwing_axeman_technology_rules{
    .researched_at = BuildingKind::castle, .minimum_age = Age::castle,
    .wood_cost = 0, .food_cost = 0, .gold_cost = 0,
    .stone_cost = 0, .research_ticks = 1,
};
constexpr TechnologyRules elite_throwing_axeman_technology_rules{
    .researched_at = BuildingKind::castle, .minimum_age = Age::imperial,
    .wood_cost = 0, .food_cost = 1000, .gold_cost = 850,
    .stone_cost = 0, .research_ticks = 15,
};
constexpr TechnologyRules huskarl_technology_rules{
    .researched_at = BuildingKind::castle, .minimum_age = Age::castle,
    .wood_cost = 0, .food_cost = 0, .gold_cost = 0,
    .stone_cost = 0, .research_ticks = 1,
};
constexpr TechnologyRules elite_huskarl_technology_rules{
    .researched_at = BuildingKind::castle, .minimum_age = Age::imperial,
    .wood_cost = 0, .food_cost = 1200, .gold_cost = 550,
    .stone_cost = 0, .research_ticks = 13,
};
constexpr TechnologyRules teutonic_knight_technology_rules{
    .researched_at = BuildingKind::castle, .minimum_age = Age::castle,
    .wood_cost = 0, .food_cost = 0, .gold_cost = 0,
    .stone_cost = 0, .research_ticks = 1,
};
constexpr TechnologyRules elite_teutonic_knight_technology_rules{
    .researched_at = BuildingKind::castle, .minimum_age = Age::imperial,
    .wood_cost = 0, .food_cost = 1200, .gold_cost = 600,
    .stone_cost = 0, .research_ticks = 17,
};
constexpr TechnologyRules samurai_technology_rules{
    .researched_at = BuildingKind::castle, .minimum_age = Age::castle,
    .wood_cost = 0, .food_cost = 0, .gold_cost = 0,
    .stone_cost = 0, .research_ticks = 1,
};
constexpr TechnologyRules elite_samurai_technology_rules{
    .researched_at = BuildingKind::castle, .minimum_age = Age::imperial,
    .wood_cost = 0, .food_cost = 950, .gold_cost = 875,
    .stone_cost = 0, .research_ticks = 20,
};
constexpr TechnologyRules chu_ko_nu_technology_rules{
    .researched_at = BuildingKind::castle, .minimum_age = Age::castle,
    .wood_cost = 0, .food_cost = 0, .gold_cost = 0,
    .stone_cost = 0, .research_ticks = 1,
};
constexpr TechnologyRules elite_chu_ko_nu_technology_rules{
    .researched_at = BuildingKind::castle, .minimum_age = Age::imperial,
    .wood_cost = 0, .food_cost = 950, .gold_cost = 950,
    .stone_cost = 0, .research_ticks = 17,
};
constexpr TechnologyRules cataphract_technology_rules{
    .researched_at = BuildingKind::castle, .minimum_age = Age::castle,
    .wood_cost = 0, .food_cost = 0, .gold_cost = 0,
    .stone_cost = 0, .research_ticks = 1,
};
constexpr TechnologyRules elite_cataphract_technology_rules{
    .researched_at = BuildingKind::castle, .minimum_age = Age::imperial,
    .wood_cost = 0, .food_cost = 1600, .gold_cost = 800,
    .stone_cost = 0, .research_ticks = 17,
};
constexpr TechnologyRules war_elephant_technology_rules{
    .researched_at = BuildingKind::castle, .minimum_age = Age::castle,
    .wood_cost = 0, .food_cost = 0, .gold_cost = 0,
    .stone_cost = 0, .research_ticks = 1,
};
constexpr TechnologyRules elite_war_elephant_technology_rules{
    .researched_at = BuildingKind::castle, .minimum_age = Age::imperial,
    .wood_cost = 0, .food_cost = 1600, .gold_cost = 1200,
    .stone_cost = 0, .research_ticks = 25,
};
constexpr TechnologyRules mameluke_technology_rules{
    .researched_at = BuildingKind::castle, .minimum_age = Age::castle,
    .wood_cost = 0, .food_cost = 0, .gold_cost = 0,
    .stone_cost = 0, .research_ticks = 1,
};
constexpr TechnologyRules elite_mameluke_technology_rules{
    .researched_at = BuildingKind::castle, .minimum_age = Age::imperial,
    .wood_cost = 0, .food_cost = 600, .gold_cost = 500,
    .stone_cost = 0, .research_ticks = 17,
};
constexpr TechnologyRules janissary_technology_rules{
    .researched_at = BuildingKind::castle, .minimum_age = Age::castle,
    .wood_cost = 0, .food_cost = 0, .gold_cost = 0,
    .stone_cost = 0, .research_ticks = 1,
};
constexpr TechnologyRules elite_janissary_technology_rules{
    .researched_at = BuildingKind::castle, .minimum_age = Age::imperial,
    .wood_cost = 0, .food_cost = 850, .gold_cost = 750,
    .stone_cost = 0, .research_ticks = 18,
};
constexpr TechnologyRules berserk_technology_rules{
    .researched_at = BuildingKind::castle, .minimum_age = Age::castle,
    .wood_cost = 0, .food_cost = 0, .gold_cost = 0,
    .stone_cost = 0, .research_ticks = 1,
};
constexpr TechnologyRules elite_berserk_technology_rules{
    .researched_at = BuildingKind::castle, .minimum_age = Age::imperial,
    .wood_cost = 0, .food_cost = 1300, .gold_cost = 550,
    .stone_cost = 0, .research_ticks = 15,
};
constexpr TechnologyRules mangudai_technology_rules{
    .researched_at = BuildingKind::castle, .minimum_age = Age::castle,
    .wood_cost = 0, .food_cost = 0, .gold_cost = 0,
    .stone_cost = 0, .research_ticks = 1,
};
constexpr TechnologyRules elite_mangudai_technology_rules{
    .researched_at = BuildingKind::castle, .minimum_age = Age::imperial,
    .wood_cost = 0, .food_cost = 1100, .gold_cost = 675,
    .stone_cost = 0, .research_ticks = 17,
};
constexpr TechnologyRules berserkergang_technology_rules{
    .researched_at = BuildingKind::castle, .minimum_age = Age::imperial,
    .wood_cost = 0, .food_cost = 500, .gold_cost = 850,
    .stone_cost = 0, .research_ticks = 13,
};
constexpr TechnologyRules jaguar_warrior_technology_rules{
    .researched_at = BuildingKind::castle, .minimum_age = Age::castle,
    .wood_cost = 0, .food_cost = 0, .gold_cost = 0,
    .stone_cost = 0, .research_ticks = 1,
};
constexpr TechnologyRules elite_jaguar_warrior_technology_rules{
    .researched_at = BuildingKind::castle, .minimum_age = Age::imperial,
    .wood_cost = 0, .food_cost = 1000, .gold_cost = 500,
    .stone_cost = 0, .research_ticks = 15,
};
constexpr TechnologyRules plumed_archer_technology_rules{
    .researched_at = BuildingKind::castle, .minimum_age = Age::castle,
    .wood_cost = 0, .food_cost = 0, .gold_cost = 0,
    .stone_cost = 0, .research_ticks = 1,
};
constexpr TechnologyRules elite_plumed_archer_technology_rules{
    .researched_at = BuildingKind::castle, .minimum_age = Age::imperial,
    .wood_cost = 1000, .food_cost = 500, .gold_cost = 0,
    .stone_cost = 0, .research_ticks = 15,
};
constexpr TechnologyRules conquistador_technology_rules{
    .researched_at = BuildingKind::castle, .minimum_age = Age::castle,
    .wood_cost = 0, .food_cost = 0, .gold_cost = 0,
    .stone_cost = 0, .research_ticks = 1,
};
constexpr TechnologyRules elite_conquistador_technology_rules{
    .researched_at = BuildingKind::castle, .minimum_age = Age::imperial,
    .wood_cost = 0, .food_cost = 1200, .gold_cost = 600,
    .stone_cost = 0, .research_ticks = 20,
};
constexpr TechnologyRules tarkan_technology_rules{
    .researched_at = BuildingKind::castle, .minimum_age = Age::castle,
    .wood_cost = 0, .food_cost = 0, .gold_cost = 0,
    .stone_cost = 0, .research_ticks = 1,
};
constexpr TechnologyRules elite_tarkan_technology_rules{
    .researched_at = BuildingKind::castle, .minimum_age = Age::imperial,
    .wood_cost = 0, .food_cost = 1000, .gold_cost = 500,
    .stone_cost = 0, .research_ticks = 15,
};
constexpr TechnologyRules woad_raider_technology_rules{
    .researched_at = BuildingKind::castle, .minimum_age = Age::castle,
    .wood_cost = 0, .food_cost = 0, .gold_cost = 0,
    .stone_cost = 0, .research_ticks = 1,
};
constexpr TechnologyRules elite_woad_raider_technology_rules{
    .researched_at = BuildingKind::castle, .minimum_age = Age::imperial,
    .wood_cost = 0, .food_cost = 1000, .gold_cost = 800,
    .stone_cost = 0, .research_ticks = 45,
};
constexpr TechnologyRules yeomen_technology_rules{
    .researched_at = BuildingKind::castle, .minimum_age = Age::imperial,
    .wood_cost = 750, .food_cost = 0, .gold_cost = 450,
    .stone_cost = 0, .research_ticks = 20,
};
constexpr TechnologyRules bearded_axe_technology_rules{
    .researched_at = BuildingKind::castle, .minimum_age = Age::imperial,
    .wood_cost = 0, .food_cost = 400, .gold_cost = 400,
    .stone_cost = 0, .research_ticks = 20,
};
constexpr TechnologyRules anarchy_technology_rules{
    .researched_at = BuildingKind::castle, .minimum_age = Age::castle,
    .wood_cost = 0, .food_cost = 450, .gold_cost = 250,
    .stone_cost = 0, .research_ticks = 13,
};
constexpr TechnologyRules crenellations_technology_rules{
    .researched_at = BuildingKind::castle, .minimum_age = Age::imperial,
    .wood_cost = 0, .food_cost = 600, .gold_cost = 0,
    .stone_cost = 400, .research_ticks = 20,
};
constexpr TechnologyRules kataparuto_technology_rules{
    .researched_at = BuildingKind::castle, .minimum_age = Age::imperial,
    .wood_cost = 750, .food_cost = 0, .gold_cost = 400,
    .stone_cost = 0, .research_ticks = 20,
};
constexpr TechnologyRules rocketry_technology_rules{
    .researched_at = BuildingKind::castle, .minimum_age = Age::imperial,
    .wood_cost = 750, .food_cost = 0, .gold_cost = 750,
    .stone_cost = 0, .research_ticks = 20,
};
constexpr TechnologyRules logistica_technology_rules{
    .researched_at = BuildingKind::castle, .minimum_age = Age::imperial,
    .wood_cost = 0, .food_cost = 1000, .gold_cost = 600,
    .stone_cost = 0, .research_ticks = 17,
};
constexpr TechnologyRules mahouts_technology_rules{
    .researched_at = BuildingKind::castle, .minimum_age = Age::imperial,
    .wood_cost = 0, .food_cost = 300, .gold_cost = 300,
    .stone_cost = 0, .research_ticks = 17,
};
constexpr TechnologyRules zealotry_technology_rules{
    .researched_at = BuildingKind::castle, .minimum_age = Age::imperial,
    .wood_cost = 0, .food_cost = 750, .gold_cost = 800,
    .stone_cost = 0, .research_ticks = 17,
};
constexpr TechnologyRules artillery_technology_rules{
    .researched_at = BuildingKind::castle, .minimum_age = Age::imperial,
    .wood_cost = 0, .food_cost = 0, .gold_cost = 500,
    .stone_cost = 450, .research_ticks = 13,
};
constexpr TechnologyRules drill_technology_rules{
    .researched_at = BuildingKind::castle, .minimum_age = Age::imperial,
    .wood_cost = 500, .food_cost = 0, .gold_cost = 450,
    .stone_cost = 0, .research_ticks = 20,
};
constexpr TechnologyRules supremacy_technology_rules{
    .researched_at = BuildingKind::castle, .minimum_age = Age::imperial,
    .wood_cost = 0, .food_cost = 400, .gold_cost = 250,
    .stone_cost = 0, .research_ticks = 20,
};
constexpr TechnologyRules atheism_technology_rules{
    .researched_at = BuildingKind::castle, .minimum_age = Age::imperial,
    .wood_cost = 0, .food_cost = 500, .gold_cost = 500,
    .stone_cost = 0, .research_ticks = 20,
};
constexpr TechnologyRules shinkichon_technology_rules{
    .researched_at = BuildingKind::castle, .minimum_age = Age::imperial,
    .wood_cost = 800, .food_cost = 0, .gold_cost = 500,
    .stone_cost = 0, .research_ticks = 20,
};
constexpr TechnologyRules el_dorado_technology_rules{
    .researched_at = BuildingKind::castle, .minimum_age = Age::imperial,
    .wood_cost = 0, .food_cost = 750, .gold_cost = 450,
    .stone_cost = 0, .research_ticks = 17,
};
constexpr TechnologyRules elite_eagle_warrior_technology_rules{
    .researched_at = BuildingKind::barracks, .minimum_age = Age::imperial,
    .wood_cost = 0, .food_cost = 800, .gold_cost = 500,
    .stone_cost = 0, .research_ticks = 13,
};
constexpr TechnologyRules heavy_scorpion_technology_rules{
    .researched_at = BuildingKind::siege_workshop,
    .minimum_age = Age::imperial,
    .wood_cost = 1100, .food_cost = 1000, .gold_cost = 0,
    .stone_cost = 0, .research_ticks = 17,
};
constexpr TechnologyRules onager_technology_rules{
    .researched_at = BuildingKind::siege_workshop,
    .minimum_age = Age::imperial,
    .wood_cost = 0, .food_cost = 800, .gold_cost = 500,
    .stone_cost = 0, .research_ticks = 25,
};
constexpr TechnologyRules siege_onager_technology_rules{
    .researched_at = BuildingKind::siege_workshop,
    .minimum_age = Age::imperial,
    .wood_cost = 0, .food_cost = 1450, .gold_cost = 1000,
    .stone_cost = 0, .research_ticks = 50,
};
constexpr TechnologyRules heavy_cavalry_archer_technology_rules{
    .researched_at = BuildingKind::archery_range,
    .minimum_age = Age::imperial,
    .wood_cost = 0, .food_cost = 900, .gold_cost = 500,
    .stone_cost = 0, .research_ticks = 17,
};
constexpr TechnologyRules heavy_camel_technology_rules{
    .researched_at = BuildingKind::stable,
    .minimum_age = Age::imperial,
    .wood_cost = 0, .food_cost = 325, .gold_cost = 360,
    .stone_cost = 0, .research_ticks = 42,
};
constexpr TechnologyRules capped_ram_technology_rules{
    .researched_at = BuildingKind::siege_workshop,
    .minimum_age = Age::imperial,
    .wood_cost = 0, .food_cost = 300, .gold_cost = 0,
    .stone_cost = 0, .research_ticks = 17,
};
constexpr TechnologyRules siege_ram_technology_rules{
    .researched_at = BuildingKind::siege_workshop,
    .minimum_age = Age::imperial,
    .wood_cost = 0, .food_cost = 1000, .gold_cost = 0,
    .stone_cost = 0, .research_ticks = 25,
};
constexpr TechnologyRules halberdier_technology_rules{
    .researched_at = BuildingKind::barracks,
    .minimum_age = Age::imperial,
    .wood_cost = 0, .food_cost = 300, .gold_cost = 600,
    .stone_cost = 0, .research_ticks = 17,
};
constexpr TechnologyRules chemistry_technology_rules{
    .researched_at = BuildingKind::university,
    .minimum_age = Age::imperial,
    .wood_cost = 0, .food_cost = 300, .gold_cost = 200,
    .stone_cost = 0, .research_ticks = 34,
};
constexpr TechnologyRules hand_cannoneer_gate_rules{
    .researched_at = BuildingKind::archery_range,
    .minimum_age = Age::imperial,
    .wood_cost = 0, .food_cost = 0, .gold_cost = 0,
    .stone_cost = 0, .research_ticks = 0,
};
constexpr TechnologyRules bombard_cannon_gate_rules{
    .researched_at = BuildingKind::siege_workshop,
    .minimum_age = Age::imperial,
    .wood_cost = 0, .food_cost = 0, .gold_cost = 0,
    .stone_cost = 0, .research_ticks = 0,
};
constexpr TechnologyRules siege_engineers_technology_rules{
    .researched_at = BuildingKind::university,
    .minimum_age = Age::imperial,
    .wood_cost = 600, .food_cost = 500, .gold_cost = 0,
    .stone_cost = 0, .research_ticks = 15,
};
constexpr TechnologyRules conscription_technology_rules{
    .researched_at = BuildingKind::castle,
    .minimum_age = Age::imperial,
    .wood_cost = 0, .food_cost = 150, .gold_cost = 150,
    .stone_cost = 0, .research_ticks = 20,
};
constexpr TechnologyRules petard_gate_rules{
    .researched_at = BuildingKind::barracks,
    .minimum_age = Age::castle,
    .wood_cost = 0, .food_cost = 0, .gold_cost = 0,
    .stone_cost = 0, .research_ticks = 0,
};
constexpr TechnologyRules bombard_tower_technology_rules{
    .researched_at = BuildingKind::university,
    .minimum_age = Age::imperial,
    .wood_cost = 400, .food_cost = 800, .gold_cost = 0,
    .stone_cost = 0, .research_ticks = 20,
};
constexpr TechnologyRules sanctity_rules{
    BuildingKind::monastery, Age::castle, 0, 0, 120, 0, 20
};
constexpr TechnologyRules fervor_rules{
    BuildingKind::monastery, Age::castle, 0, 0, 140, 0, 17
};
constexpr TechnologyRules redemption_rules{
    BuildingKind::monastery, Age::castle, 0, 0, 475, 0, 17
};
constexpr TechnologyRules atonement_rules{
    BuildingKind::monastery, Age::castle, 0, 0, 325, 0, 14
};
constexpr TechnologyRules illumination_rules{
    BuildingKind::monastery, Age::imperial, 0, 0, 120, 0, 22
};
constexpr TechnologyRules block_printing_rules{
    BuildingKind::monastery, Age::imperial, 0, 0, 200, 0, 19
};
constexpr TechnologyRules faith_rules{
    BuildingKind::monastery, Age::imperial, 0, 750, 1000, 0, 20
};
constexpr TechnologyRules theocracy_rules{
    BuildingKind::monastery, Age::imperial, 0, 0, 200, 0, 25
};
constexpr TechnologyRules heresy_rules{
    BuildingKind::monastery, Age::castle, 0, 0, 1000, 0, 20
};
constexpr TechnologyRules heavy_plow_rules{
    BuildingKind::mill, Age::castle, 125, 125, 0, 0, 14
};
constexpr TechnologyRules crop_rotation_rules{
    BuildingKind::mill, Age::imperial, 250, 250, 0, 0, 24
};
constexpr TechnologyRules bow_saw_rules{
    BuildingKind::lumber_camp, Age::castle, 100, 150, 0, 0, 17
};
constexpr TechnologyRules two_man_saw_rules{
    BuildingKind::lumber_camp, Age::imperial, 200, 300, 0, 0, 34
};
constexpr TechnologyRules gold_mining_rules{
    BuildingKind::mining_camp, Age::feudal, 75, 100, 0, 0, 10
};
constexpr TechnologyRules gold_shaft_mining_rules{
    BuildingKind::mining_camp, Age::castle, 150, 200, 0, 0, 25
};
constexpr TechnologyRules stone_mining_rules{
    BuildingKind::mining_camp, Age::feudal, 75, 100, 0, 0, 10
};
constexpr TechnologyRules stone_shaft_mining_rules{
    BuildingKind::mining_camp, Age::castle, 150, 200, 0, 0, 25
};
constexpr TechnologyRules hand_cart_rules{
    BuildingKind::town_center, Age::castle, 200, 300, 0, 0, 19
};
constexpr TechnologyRules fish_trap_gate_rules{
    BuildingKind::dock, Age::feudal, 0, 0, 0, 0, 0
};
constexpr TechnologyRules coinage_rules{
    BuildingKind::market, Age::feudal, 0, 150, 50, 0, 17
};
constexpr TechnologyRules banking_rules{
    BuildingKind::market, Age::castle, 0, 200, 100, 0, 17
};
constexpr TechnologyRules cartography_rules{
    BuildingKind::market, Age::feudal, 0, 100, 100, 0, 20
};
constexpr TechnologyRules caravan_rules{
    BuildingKind::market, Age::castle, 0, 200, 200, 0, 14
};
constexpr TechnologyRules guilds_rules{
    BuildingKind::market, Age::imperial, 0, 300, 200, 0, 17
};
constexpr TechnologyRules trade_cog_gate_rules{
    BuildingKind::dock, Age::feudal, 0, 0, 0, 0, 0
};
constexpr TechnologyRules outpost_gate_rules{
    BuildingKind::town_center, Age::dark, 0, 0, 0, 0, 0
};
constexpr TechnologyRules town_watch_rules{
    BuildingKind::town_center, Age::feudal, 0, 75, 0, 0, 9
};
constexpr TechnologyRules town_patrol_rules{
    BuildingKind::town_center, Age::castle, 0, 300, 200, 0, 14
};
constexpr TechnologyRules masonry_rules{
    BuildingKind::university, Age::castle, 175, 150, 0, 0, 17
};
constexpr TechnologyRules architecture_rules{
    BuildingKind::university, Age::imperial, 200, 300, 0, 0, 24
};
constexpr TechnologyRules ballistics_rules{
    BuildingKind::university, Age::castle, 300, 0, 175, 0, 20
};
constexpr TechnologyRules heated_shot_rules{
    BuildingKind::university, Age::castle, 0, 350, 100, 0, 10
};
constexpr TechnologyRules hoardings_rules{
    BuildingKind::castle, Age::imperial, 400, 400, 0, 0, 25
};
constexpr TechnologyRules sappers_rules{
    BuildingKind::castle, Age::imperial, 0, 400, 200, 0, 4
};
constexpr TechnologyRules wonder_plans_rules{
    BuildingKind::town_center, Age::imperial, 0, 0, 0, 0, 0
};
constexpr TechnologyRules thumb_ring_rules{
    BuildingKind::archery_range, Age::castle, 250, 300, 0, 0, 15
};
constexpr TechnologyRules parthian_tactics_rules{
    BuildingKind::archery_range, Age::imperial, 0, 200, 250, 0, 22
};
constexpr TechnologyRules squires_rules{
    BuildingKind::barracks, Age::castle, 0, 200, 0, 0, 14
};
constexpr TechnologyRules tracking_rules{
    BuildingKind::barracks, Age::feudal, 0, 75, 0, 0, 12
};
constexpr TechnologyRules herbal_medicine_rules{
    BuildingKind::monastery, Age::castle, 0, 0, 350, 0, 12
};
constexpr TechnologyRules stone_cutting_rules{
    BuildingKind::university, Age::castle, 200, 300, 0, 0, 17
};
constexpr TechnologyRules spy_technology_rules{
    BuildingKind::castle, Age::imperial, 0, 0, 200, 0, 1
};

constexpr TechnologyRules fletching_rules{
    .researched_at = BuildingKind::blacksmith,
    .minimum_age = Age::feudal,
    .wood_cost = 0,
    .food_cost = 100,
    .gold_cost = 50,
    .stone_cost = 0,
    .research_ticks = 15,
};

constexpr TechnologyRules forging_rules{
    .researched_at = BuildingKind::blacksmith,
    .minimum_age = Age::feudal,
    .wood_cost = 0,
    .food_cost = 150,
    .gold_cost = 0,
    .stone_cost = 0,
    .research_ticks = 15,
};

constexpr TechnologyRules murder_holes_rules{
    .researched_at = BuildingKind::university,
    .minimum_age = Age::castle,
    .wood_cost = 0,
    .food_cost = 200,
    .gold_cost = 0,
    .stone_cost = 200,
    .research_ticks = 18,
};

constexpr TechnologyRules man_at_arms_technology_rules{
    .researched_at = BuildingKind::barracks,
    .minimum_age = Age::feudal,
    .wood_cost = 0,
    .food_cost = 100,
    .gold_cost = 40,
    .stone_cost = 0,
    .research_ticks = 20,
};

constexpr TechnologyRules crossbowman_technology_rules{
    .researched_at = BuildingKind::archery_range,
    .minimum_age = Age::castle,
    .wood_cost = 0,
    .food_cost = 125,
    .gold_cost = 75,
    .stone_cost = 0,
    .research_ticks = 18,
};

constexpr TechnologyRules pikeman_technology_rules{
    .researched_at = BuildingKind::barracks,
    .minimum_age = Age::castle,
    .wood_cost = 0,
    .food_cost = 215,
    .gold_cost = 90,
    .stone_cost = 0,
    .research_ticks = 23,
};

constexpr TechnologyRules long_swordsman_technology_rules{
    .researched_at = BuildingKind::barracks,
    .minimum_age = Age::castle,
    .wood_cost = 0,
    .food_cost = 200,
    .gold_cost = 65,
    .stone_cost = 0,
    .research_ticks = 23,
};

}  // namespace

ConversionCheck evaluate_conversion_check(
    int crt_random_value,
    float resistance,
    int base_chance,
    float elapsed_time,
    float minimum_time,
    float maximum_time
) {
    if (crt_random_value < 0 || crt_random_value > 32767 ||
        !std::isfinite(resistance) || !std::isfinite(elapsed_time) ||
        !std::isfinite(minimum_time) || !std::isfinite(maximum_time)) {
        throw std::invalid_argument("invalid conversion check input");
    }

    int roll = crt_random_value * 100 / 32767;
    if (resistance > 0.0F) {
        // Original uses x87 FIMUL followed by FISTP through 0x72421c.
        // nearbyint honors active floating-point rounding mode, including
        // default round-to-nearest-even behavior.
        roll = static_cast<int>(std::nearbyint(
            static_cast<long double>(resistance) *
            static_cast<long double>(roll)
        ));
    }

    const int threshold =
        elapsed_time < minimum_time ? -1000 :
        elapsed_time >= maximum_time ? 1000 :
        base_chance;
    return {
        .scaled_roll = roll,
        .threshold = threshold,
        .succeeds = roll <= threshold,
    };
}

const UnitRules& rules_for(UnitKind kind) {
    switch (kind) {
        case UnitKind::villager:
            return villager_rules;
        case UnitKind::knight:
            return knight_rules;
        case UnitKind::archer:
            return archer_rules;
        case UnitKind::scout_cavalry:
            return scout_cavalry_rules;
        case UnitKind::militia:
            return militia_rules;
        case UnitKind::spearman:
            return spearman_rules;
        case UnitKind::battering_ram:
            return battering_ram_rules;
        case UnitKind::capped_ram: return capped_ram_rules;
        case UnitKind::siege_ram: return siege_ram_rules;
        case UnitKind::skirmisher:
            return skirmisher_rules;
        case UnitKind::mangonel:
            return mangonel_rules;
        case UnitKind::onager: return onager_rules;
        case UnitKind::siege_onager: return siege_onager_rules;
        case UnitKind::scorpion: return scorpion_rules;
        case UnitKind::heavy_scorpion: return heavy_scorpion_rules;
        case UnitKind::eagle_warrior: return eagle_warrior_rules;
        case UnitKind::elite_eagle_warrior:
            return elite_eagle_warrior_rules;
        case UnitKind::packed_trebuchet: return packed_trebuchet_rules;
        case UnitKind::trebuchet: return trebuchet_rules;
        case UnitKind::cavalry_archer: return cavalry_archer_rules;
        case UnitKind::heavy_cavalry_archer:
            return heavy_cavalry_archer_rules;
        case UnitKind::camel_rider: return camel_rider_rules;
        case UnitKind::heavy_camel: return heavy_camel_rules;
        case UnitKind::man_at_arms:
            return man_at_arms_unit_rules;
        case UnitKind::crossbowman:
            return crossbowman_unit_rules;
        case UnitKind::pikeman:
            return pikeman_unit_rules;
        case UnitKind::halberdier: return halberdier_unit_rules;
        case UnitKind::hand_cannoneer: return hand_cannoneer_rules;
        case UnitKind::bombard_cannon: return bombard_cannon_rules;
        case UnitKind::petard: return petard_rules;
        case UnitKind::long_swordsman:
            return long_swordsman_unit_rules;
        case UnitKind::cavalier:
            return cavalier_unit_rules;
        case UnitKind::paladin:
            return paladin_unit_rules;
        case UnitKind::light_cavalry:
            return light_cavalry_unit_rules;
        case UnitKind::hussar:
            return hussar_unit_rules;
        case UnitKind::two_handed_swordsman:
            return two_handed_swordsman_unit_rules;
        case UnitKind::champion:
            return champion_unit_rules;
        case UnitKind::arbalester:
            return arbalester_unit_rules;
        case UnitKind::elite_skirmisher:
            return elite_skirmisher_unit_rules;
        case UnitKind::sheep:
            return sheep_rules;
        case UnitKind::deer:
            return deer_rules;
        case UnitKind::boar:
            return boar_rules;
        case UnitKind::monk:
            return monk_rules;
        case UnitKind::missionary:
            return missionary_rules;
        case UnitKind::trade_cog:
            return trade_cog_rules;
        case UnitKind::relic:
            return relic_rules;
        case UnitKind::trade_cart:
            return trade_cart_rules;
        case UnitKind::fishing_ship:
            return fishing_ship_rules;
        case UnitKind::galley: return galley_rules;
        case UnitKind::war_galley: return war_galley_rules;
        case UnitKind::galleon: return galleon_rules;
        case UnitKind::transport_ship: return transport_ship_rules;
        case UnitKind::fire_ship: return fire_ship_rules;
        case UnitKind::fast_fire_ship: return fast_fire_ship_rules;
        case UnitKind::demolition_ship: return demolition_ship_rules;
        case UnitKind::heavy_demolition_ship:
            return heavy_demolition_ship_rules;
        case UnitKind::cannon_galleon: return cannon_galleon_rules;
        case UnitKind::elite_cannon_galleon:
            return elite_cannon_galleon_rules;
        case UnitKind::longboat: return longboat_rules;
        case UnitKind::elite_longboat: return elite_longboat_rules;
        case UnitKind::turtle_ship: return turtle_ship_rules;
        case UnitKind::elite_turtle_ship:
            return elite_turtle_ship_rules;
        case UnitKind::longbowman: return longbowman_rules;
        case UnitKind::elite_longbowman:
            return elite_longbowman_rules;
        case UnitKind::throwing_axeman: return throwing_axeman_rules;
        case UnitKind::elite_throwing_axeman:
            return elite_throwing_axeman_rules;
        case UnitKind::huskarl: return huskarl_rules;
        case UnitKind::elite_huskarl: return elite_huskarl_rules;
        case UnitKind::teutonic_knight: return teutonic_knight_rules;
        case UnitKind::elite_teutonic_knight:
            return elite_teutonic_knight_rules;
        case UnitKind::samurai: return samurai_rules;
        case UnitKind::elite_samurai: return elite_samurai_rules;
        case UnitKind::chu_ko_nu: return chu_ko_nu_rules;
        case UnitKind::elite_chu_ko_nu: return elite_chu_ko_nu_rules;
        case UnitKind::cataphract: return cataphract_rules;
        case UnitKind::elite_cataphract: return elite_cataphract_rules;
        case UnitKind::war_elephant: return war_elephant_rules;
        case UnitKind::elite_war_elephant:
            return elite_war_elephant_rules;
        case UnitKind::mameluke: return mameluke_rules;
        case UnitKind::elite_mameluke: return elite_mameluke_rules;
        case UnitKind::janissary: return janissary_rules;
        case UnitKind::elite_janissary: return elite_janissary_rules;
        case UnitKind::berserk: return berserk_rules;
        case UnitKind::elite_berserk: return elite_berserk_rules;
        case UnitKind::mangudai: return mangudai_rules;
        case UnitKind::elite_mangudai: return elite_mangudai_rules;
        case UnitKind::jaguar_warrior: return jaguar_warrior_rules;
        case UnitKind::elite_jaguar_warrior:
            return elite_jaguar_warrior_rules;
        case UnitKind::plumed_archer: return plumed_archer_rules;
        case UnitKind::elite_plumed_archer:
            return elite_plumed_archer_rules;
        case UnitKind::conquistador: return conquistador_rules;
        case UnitKind::elite_conquistador:
            return elite_conquistador_rules;
        case UnitKind::tarkan: return tarkan_rules;
        case UnitKind::elite_tarkan: return elite_tarkan_rules;
        case UnitKind::woad_raider: return woad_raider_rules;
        case UnitKind::elite_woad_raider: return elite_woad_raider_rules;
        case UnitKind::king: return king_rules;
    }
    return villager_rules;
}

const BuildingRules& rules_for(BuildingKind kind) {
    switch (kind) {
        case BuildingKind::town_center:
            return town_center_rules;
        case BuildingKind::barracks:
            return barracks_rules;
        case BuildingKind::archery_range:
            return archery_range_rules;
        case BuildingKind::house:
            return house_rules;
        case BuildingKind::mill:
            return mill_rules;
        case BuildingKind::lumber_camp:
            return lumber_camp_rules;
        case BuildingKind::mining_camp:
            return mining_camp_rules;
        case BuildingKind::farm:
            return farm_rules;
        case BuildingKind::stable:
            return stable_rules;
        case BuildingKind::blacksmith:
            return blacksmith_rules;
        case BuildingKind::castle:
            return castle_rules;
        case BuildingKind::university:
            return university_rules;
        case BuildingKind::siege_workshop:
            return siege_workshop_rules;
        case BuildingKind::palisade_wall:
            return palisade_wall_rules;
        case BuildingKind::watch_tower:
            return watch_tower_rules;
        case BuildingKind::stone_wall:
            return stone_wall_rules;
        case BuildingKind::palisade_gate_x:
            return palisade_gate_x_rules;
        case BuildingKind::palisade_gate_y:
            return palisade_gate_y_rules;
        case BuildingKind::stone_gate_x:
            return stone_gate_x_rules;
        case BuildingKind::stone_gate_y:
            return stone_gate_y_rules;
        case BuildingKind::monastery:
            return monastery_rules;
        case BuildingKind::market:
            return market_rules;
        case BuildingKind::dock:
            return dock_rules;
        case BuildingKind::bombard_tower:
            return bombard_tower_rules;
        case BuildingKind::fish_trap:
            return fish_trap_rules;
        case BuildingKind::outpost:
            return outpost_rules;
        case BuildingKind::wonder:
            return wonder_rules;
        case BuildingKind::guard_tower:
            return guard_tower_building_rules;
        case BuildingKind::keep:
            return keep_building_rules;
        case BuildingKind::fortified_wall:
            return fortified_wall_building_rules;
        case BuildingKind::fortified_gate_x:
            return fortified_gate_x_rules;
        case BuildingKind::fortified_gate_y:
            return fortified_gate_y_rules;
    }
    return town_center_rules;
}

int garrison_volley_projectile_count(
    BuildingKind shelter,
    int baseline_projectiles,
    int contributing_occupants
) {
    baseline_projectiles = std::max(0, baseline_projectiles);
    contributing_occupants = std::max(0, contributing_occupants);
    switch (shelter) {
        case BuildingKind::town_center:
            return std::min(10, contributing_occupants);
        case BuildingKind::castle:
            return std::min(
                20, baseline_projectiles + contributing_occupants
            );
        case BuildingKind::watch_tower:
            return std::min(
                5, baseline_projectiles + contributing_occupants
            );
        case BuildingKind::bombard_tower:
            return baseline_projectiles;
        default:
            return baseline_projectiles;
    }
}

int percentage_fee_floor(int amount, int percent) {
    if (amount <= 0 || percent <= 0) {
        return 0;
    }
    const std::int64_t fee =
        static_cast<std::int64_t>(amount) * percent / 100;
    return static_cast<int>(std::min<std::int64_t>(
        fee, std::numeric_limits<int>::max()
    ));
}

int market_price_after_fee(
    int base_price,
    int fee_percent,
    bool buying
) {
    if (base_price <= 0) {
        return 0;
    }
    const std::int64_t multiplier = buying
        ? 100LL + fee_percent : 100LL - fee_percent;
    const std::int64_t price =
        static_cast<std::int64_t>(base_price) *
        std::max<std::int64_t>(0, multiplier) / 100;
    return static_cast<int>(std::min<std::int64_t>(
        price, std::numeric_limits<int>::max()
    ));
}

bool can_train(BuildingKind building, UnitKind unit) {
    return rules_for(unit).trained_at == building;
}

bool is_cavalry(UnitKind unit) {
    return unit == UnitKind::knight ||
           unit == UnitKind::scout_cavalry ||
           unit == UnitKind::cavalier ||
           unit == UnitKind::paladin ||
           unit == UnitKind::light_cavalry ||
           unit == UnitKind::hussar ||
           unit == UnitKind::cataphract ||
           unit == UnitKind::elite_cataphract ||
           unit == UnitKind::war_elephant ||
           unit == UnitKind::elite_war_elephant ||
           unit == UnitKind::mameluke ||
           unit == UnitKind::elite_mameluke ||
           unit == UnitKind::mangudai ||
           unit == UnitKind::elite_mangudai ||
           unit == UnitKind::conquistador ||
           unit == UnitKind::elite_conquistador ||
           unit == UnitKind::tarkan ||
           unit == UnitKind::elite_tarkan ||
           unit == UnitKind::cavalry_archer ||
           unit == UnitKind::heavy_cavalry_archer ||
           unit == UnitKind::camel_rider ||
           unit == UnitKind::heavy_camel;
}

bool is_archer(UnitKind unit) {
    return unit == UnitKind::archer ||
           unit == UnitKind::skirmisher ||
           unit == UnitKind::crossbowman ||
           unit == UnitKind::arbalester ||
           unit == UnitKind::elite_skirmisher ||
           unit == UnitKind::longbowman ||
           unit == UnitKind::elite_longbowman ||
           unit == UnitKind::chu_ko_nu ||
           unit == UnitKind::elite_chu_ko_nu ||
           unit == UnitKind::janissary ||
           unit == UnitKind::elite_janissary ||
           unit == UnitKind::mangudai ||
           unit == UnitKind::elite_mangudai ||
           unit == UnitKind::plumed_archer ||
           unit == UnitKind::elite_plumed_archer ||
           unit == UnitKind::cavalry_archer ||
           unit == UnitKind::heavy_cavalry_archer ||
           unit == UnitKind::hand_cannoneer;
}

bool is_infantry(UnitKind unit) {
    return unit == UnitKind::militia ||
        unit == UnitKind::man_at_arms ||
        unit == UnitKind::long_swordsman ||
        unit == UnitKind::two_handed_swordsman ||
        unit == UnitKind::champion ||
        unit == UnitKind::spearman ||
        unit == UnitKind::pikeman ||
        unit == UnitKind::halberdier ||
        unit == UnitKind::throwing_axeman ||
        unit == UnitKind::elite_throwing_axeman ||
        unit == UnitKind::huskarl ||
        unit == UnitKind::elite_huskarl ||
        unit == UnitKind::teutonic_knight ||
        unit == UnitKind::elite_teutonic_knight ||
        unit == UnitKind::samurai ||
        unit == UnitKind::elite_samurai ||
        unit == UnitKind::berserk ||
        unit == UnitKind::elite_berserk ||
        unit == UnitKind::jaguar_warrior ||
        unit == UnitKind::elite_jaguar_warrior ||
        unit == UnitKind::woad_raider ||
        unit == UnitKind::elite_woad_raider;
}

bool is_unique_unit(UnitKind unit) {
    return unit == UnitKind::longbowman ||
        unit == UnitKind::elite_longbowman ||
        unit == UnitKind::throwing_axeman ||
        unit == UnitKind::elite_throwing_axeman ||
        unit == UnitKind::huskarl ||
        unit == UnitKind::elite_huskarl ||
        unit == UnitKind::teutonic_knight ||
        unit == UnitKind::elite_teutonic_knight ||
        unit == UnitKind::samurai ||
        unit == UnitKind::elite_samurai ||
        unit == UnitKind::chu_ko_nu ||
        unit == UnitKind::elite_chu_ko_nu ||
        unit == UnitKind::cataphract ||
        unit == UnitKind::elite_cataphract ||
        unit == UnitKind::war_elephant ||
        unit == UnitKind::elite_war_elephant ||
        unit == UnitKind::mameluke ||
        unit == UnitKind::elite_mameluke ||
        unit == UnitKind::janissary ||
        unit == UnitKind::elite_janissary ||
        unit == UnitKind::berserk ||
        unit == UnitKind::elite_berserk ||
        unit == UnitKind::mangudai ||
        unit == UnitKind::elite_mangudai ||
        unit == UnitKind::jaguar_warrior ||
        unit == UnitKind::elite_jaguar_warrior ||
        unit == UnitKind::plumed_archer ||
        unit == UnitKind::elite_plumed_archer ||
        unit == UnitKind::conquistador ||
        unit == UnitKind::elite_conquistador ||
        unit == UnitKind::tarkan ||
        unit == UnitKind::elite_tarkan ||
        unit == UnitKind::woad_raider ||
        unit == UnitKind::elite_woad_raider;
}

namespace {

using AvailabilityWords = std::array<std::uint64_t, 3>;

constexpr std::array<AvailabilityWords, 18> unit_availability{{
    {0x00000c0ffffebfffULL, 0x94f5000ULL}, // Britons
    {0x0000301ffff6ffffULL, 0xf4f7000ULL}, // Franks
    {0x0003001ffff67fffULL, 0xf47f000ULL}, // Teutons
    {0x0000c01ffff7bfffULL, 0xf4f7000ULL}, // Goths
    {0x0000001dfff7ffffULL, 0x9cff000ULL}, // Celts
    {0xc00000fcfffebfffULL, 0x8c77000ULL}, // Vikings
    {0x00c0003fffffffffULL, 0xfff5000ULL}, // Byzantines
    {0x000c0037fffebfffULL, 0xb4f7000ULL}, // Japanese
    {0x0030001dfffebfffULL, 0x9ff7000ULL}, // Chinese
    {0x0300003ffff1ffffULL, 0xfff7000ULL}, // Persians
    {0x0c00003dffff9fffULL, 0xeffd000ULL}, // Saracens
    {0x3000003dffe7b7ffULL, 0xeff3000ULL}, // Turks
    {0x0000003fffffbfffULL, 0x8fff003ULL}, // Mongols
    {0x0000003ffff7fbffULL, 0x1fcf50c0ULL}, // Spanish
    {0x0000001dfff3ffffULL, 0x9cf1300ULL}, // Huns
    {0x00000313ffffbfffULL, 0xf4fd000ULL}, // Koreans
    {0x00000003bffe1ff5ULL, 0x8c3dc0cULL}, // Aztecs
    {0x0000000ffffa1ff5ULL, 0x9c37c30ULL}, // Mayans
}};

constexpr std::array<std::uint64_t, 18> building_availability{{
    0x7fffffULL, 0x7fffffULL, 0xffffffULL, 0x737fffULL,
    0x7fffffULL, 0x7fffffULL, 0xffffffULL, 0x7fffffULL,
    0xffffffULL, 0x7fffffULL, 0x7fffffULL, 0xffffffULL,
    0x7fffffULL, 0xffffffULL, 0x7fffffULL, 0x7fffffULL,
    0x7ffeffULL, 0x7ffeffULL,
}};

constexpr std::array<AvailabilityWords, 18> technology_availability{{
    {0x000c39feb7ffffffULL, 0xebcdcd5000080000ULL, 0x0380000bULL}, // Britons
    {0x00301bf6f3ff5fffULL, 0xbfcdfd5800100000ULL, 0x0700000bULL}, // Franks
    {0x03000bf66fff7fffULL, 0xfffffd3800400000ULL, 0x0780000eULL}, // Teutons
    {0x00c02bf7bf6fc7ffULL, 0xfb4dbd5800200000ULL, 0x0580000eULL}, // Goths
    {0x00003b77f37f7fffULL, 0xad0dcf7800000000ULL, 0x03e0000fULL}, // Celts
    {0x0000df7ea77fdfffULL, 0xfda9cb18000004c0ULL, 0x03a0000bULL}, // Vikings
    {0xc0003ffff7fdffffULL, 0xffffbfd002000000ULL, 0x01a0000fULL}, // Byzantines
    {0x0c003efeb77fffffULL, 0xebfddd5800800000ULL, 0x03e0000aULL}, // Japanese
    {0x30003b7ebfffffffULL, 0xeb7f8fd801000000ULL, 0x07e0000fULL}, // Chinese
    {0x00001ff1ffff57ffULL, 0xfb89bfd804000003ULL, 0x05e0000fULL}, // Persians
    {0x00001f7f9fffffffULL, 0xeffdfbf00800000cULL, 0x01a0000bULL}, // Saracens
    {0x00003f67bfffffbfULL, 0xef3fbbc810000030ULL, 0x03e0000bULL}, // Turks
    {0x00002fffbb7fdfffULL, 0xad29cbf820000300ULL, 0x0700000fULL}, // Mongols
    {0x00003ff7ffffffdfULL, 0xefffbf5040018000ULL, 0x03a0000eULL}, // Spanish
    {0x00001b73fbefc7ffULL, 0xed6d8b4080060000ULL, 0x0780000bULL}, // Huns
    {0x00033affb77dcfffULL, 0xeb8dfd7100000000ULL, 0x07a0000fULL}, // Koreans
    {0x000038be031fdfffULL, 0xbffdcb3400001800ULL, 0x05e0000fULL}, // Aztecs
    {0x000039fa071fffffULL, 0xffad8f1e00006000ULL, 0x07a0000eULL}, // Mayans
}};

std::size_t civilization_index(Civilization civilization) {
    return static_cast<std::size_t>(civilization) - 1;
}

bool available(const AvailabilityWords& words, std::size_t index) {
    return (words[index / 64] & (std::uint64_t{1} << (index % 64))) != 0;
}

} // namespace

bool civilization_has_unit(Civilization civilization, UnitKind unit) {
    if (civilization == Civilization::generic) return true;
    if (unit == UnitKind::king) return true;
    if (unit == UnitKind::woad_raider ||
        unit == UnitKind::elite_woad_raider) {
        return civilization == Civilization::celts;
    }
    if (unit == UnitKind::trade_cog) return true;
    return available(
        unit_availability[civilization_index(civilization)],
        static_cast<std::size_t>(unit)
    );
}

bool civilization_has_building(
    Civilization civilization,
    BuildingKind building
) {
    if (civilization == Civilization::generic) return true;
    if (building == BuildingKind::fish_trap ||
        building == BuildingKind::outpost ||
        building == BuildingKind::wonder) return true;
    return (building_availability[civilization_index(civilization)] &
            (std::uint64_t{1} << static_cast<std::size_t>(building))) != 0;
}

bool civilization_has_technology(
    Civilization civilization,
    Technology technology
) {
    if (civilization == Civilization::generic) return true;
    if (technology == Technology::woad_raider ||
        technology == Technology::elite_woad_raider) {
        return civilization == Civilization::celts;
    }
    if (technology == Technology::fish_trap_gate ||
        technology == Technology::trade_cog_gate ||
        technology == Technology::coinage ||
        technology == Technology::banking ||
        technology == Technology::cartography ||
        technology == Technology::caravan ||
        technology == Technology::outpost_gate ||
        technology == Technology::town_watch ||
        technology == Technology::town_patrol ||
        technology == Technology::ballistics) {
        return true;
    }
    if (technology == Technology::wonder_plans) return true;
    if (technology == Technology::spy_technology) return true;
    if (technology == Technology::masonry) {
        return civilization != Civilization::aztecs &&
            civilization != Civilization::byzantines &&
            civilization != Civilization::mayans;
    }
    if (technology == Technology::architecture) {
        return civilization != Civilization::japanese &&
            civilization != Civilization::byzantines &&
            civilization != Civilization::saracens &&
            civilization != Civilization::teutons &&
            civilization != Civilization::celts &&
            civilization != Civilization::mongols &&
            civilization != Civilization::aztecs &&
            civilization != Civilization::huns;
    }
    if (technology == Technology::heated_shot) {
        return civilization != Civilization::japanese &&
            civilization != Civilization::byzantines &&
            civilization != Civilization::franks &&
            civilization != Civilization::saracens &&
            civilization != Civilization::mongols &&
            civilization != Civilization::spanish &&
            civilization != Civilization::huns;
    }
    if (technology == Technology::hoardings) {
        return civilization != Civilization::japanese &&
            civilization != Civilization::chinese &&
            civilization != Civilization::goths &&
            civilization != Civilization::aztecs &&
            civilization != Civilization::huns &&
            civilization != Civilization::koreans;
    }
    if (technology == Technology::sappers) {
        return civilization != Civilization::japanese &&
            civilization != Civilization::byzantines &&
            civilization != Civilization::franks &&
            civilization != Civilization::saracens &&
            civilization != Civilization::koreans;
    }
    if (technology == Technology::guilds) {
        return civilization != Civilization::franks &&
            civilization != Civilization::japanese &&
            civilization != Civilization::chinese &&
            civilization != Civilization::saracens &&
            civilization != Civilization::vikings &&
            civilization != Civilization::mongols &&
            civilization != Civilization::aztecs;
    }
    return available(
        technology_availability[civilization_index(civilization)],
        static_cast<std::size_t>(technology)
    );
}

bool is_herdable(UnitKind unit) {
    return unit == UnitKind::sheep;
}

bool is_huntable(UnitKind unit) {
    return unit == UnitKind::deer || unit == UnitKind::boar;
}

bool is_animal(UnitKind unit) {
    return is_herdable(unit) || is_huntable(unit);
}

bool is_relic(UnitKind unit) {
    return unit == UnitKind::relic;
}

bool is_organic(UnitKind unit) {
    return !is_animal(unit) && !is_relic(unit) &&
           unit != UnitKind::battering_ram &&
           unit != UnitKind::capped_ram &&
           unit != UnitKind::siege_ram &&
           unit != UnitKind::bombard_cannon &&
           unit != UnitKind::mangonel &&
           unit != UnitKind::onager &&
           unit != UnitKind::siege_onager &&
           unit != UnitKind::scorpion &&
           unit != UnitKind::heavy_scorpion &&
           unit != UnitKind::packed_trebuchet &&
           unit != UnitKind::trebuchet &&
           unit != UnitKind::trade_cart &&
           unit != UnitKind::fishing_ship &&
           unit != UnitKind::galley &&
           unit != UnitKind::war_galley &&
           unit != UnitKind::galleon &&
           unit != UnitKind::transport_ship &&
           unit != UnitKind::fire_ship &&
           unit != UnitKind::fast_fire_ship &&
           unit != UnitKind::demolition_ship &&
           unit != UnitKind::heavy_demolition_ship &&
           unit != UnitKind::cannon_galleon &&
           unit != UnitKind::elite_cannon_galleon &&
           unit != UnitKind::longboat &&
           unit != UnitKind::elite_longboat &&
           unit != UnitKind::turtle_ship &&
           unit != UnitKind::elite_turtle_ship;
}

bool is_ship(UnitKind unit) {
    return unit == UnitKind::fishing_ship ||
        unit == UnitKind::galley || unit == UnitKind::war_galley ||
        unit == UnitKind::galleon || unit == UnitKind::transport_ship ||
        unit == UnitKind::fire_ship ||
        unit == UnitKind::fast_fire_ship ||
        unit == UnitKind::demolition_ship ||
        unit == UnitKind::heavy_demolition_ship ||
        unit == UnitKind::cannon_galleon ||
        unit == UnitKind::elite_cannon_galleon ||
        unit == UnitKind::longboat ||
        unit == UnitKind::elite_longboat ||
        unit == UnitKind::turtle_ship ||
        unit == UnitKind::elite_turtle_ship ||
        unit == UnitKind::trade_cog;
}

const AgeRules& rules_for(Age age) {
    switch (age) {
        case Age::dark:
            return dark_age_rules;
        case Age::feudal:
            return feudal_age_rules;
        case Age::castle:
            return castle_age_rules;
        case Age::imperial:
            return imperial_age_rules;
    }
    return dark_age_rules;
}

const TechnologyRules& rules_for(Technology technology) {
    switch (technology) {
        case Technology::wheelbarrow:
            return wheelbarrow_rules;
        case Technology::fletching:
            return fletching_rules;
        case Technology::forging:
            return forging_rules;
        case Technology::murder_holes:
            return murder_holes_rules;
        case Technology::man_at_arms:
            return man_at_arms_technology_rules;
        case Technology::crossbowman:
            return crossbowman_technology_rules;
        case Technology::pikeman:
            return pikeman_technology_rules;
        case Technology::halberdier:
            return halberdier_technology_rules;
        case Technology::chemistry: return chemistry_technology_rules;
        case Technology::hand_cannoneer_gate:
            return hand_cannoneer_gate_rules;
        case Technology::bombard_cannon_gate:
            return bombard_cannon_gate_rules;
        case Technology::siege_engineers:
            return siege_engineers_technology_rules;
        case Technology::conscription:
            return conscription_technology_rules;
        case Technology::petard_gate: return petard_gate_rules;
        case Technology::bombard_tower:
            return bombard_tower_technology_rules;
        case Technology::sanctity: return sanctity_rules;
        case Technology::fervor: return fervor_rules;
        case Technology::redemption: return redemption_rules;
        case Technology::atonement: return atonement_rules;
        case Technology::illumination: return illumination_rules;
        case Technology::block_printing: return block_printing_rules;
        case Technology::faith: return faith_rules;
        case Technology::theocracy: return theocracy_rules;
        case Technology::heresy: return heresy_rules;
        case Technology::heavy_plow: return heavy_plow_rules;
        case Technology::crop_rotation: return crop_rotation_rules;
        case Technology::bow_saw: return bow_saw_rules;
        case Technology::two_man_saw: return two_man_saw_rules;
        case Technology::gold_mining: return gold_mining_rules;
        case Technology::gold_shaft_mining:
            return gold_shaft_mining_rules;
        case Technology::stone_mining: return stone_mining_rules;
        case Technology::stone_shaft_mining:
            return stone_shaft_mining_rules;
        case Technology::hand_cart: return hand_cart_rules;
        case Technology::fish_trap_gate: return fish_trap_gate_rules;
        case Technology::coinage: return coinage_rules;
        case Technology::banking: return banking_rules;
        case Technology::cartography: return cartography_rules;
        case Technology::caravan: return caravan_rules;
        case Technology::guilds: return guilds_rules;
        case Technology::trade_cog_gate: return trade_cog_gate_rules;
        case Technology::outpost_gate: return outpost_gate_rules;
        case Technology::town_watch: return town_watch_rules;
        case Technology::town_patrol: return town_patrol_rules;
        case Technology::masonry: return masonry_rules;
        case Technology::architecture: return architecture_rules;
        case Technology::ballistics: return ballistics_rules;
        case Technology::heated_shot: return heated_shot_rules;
        case Technology::hoardings: return hoardings_rules;
        case Technology::sappers: return sappers_rules;
        case Technology::wonder_plans: return wonder_plans_rules;
        case Technology::thumb_ring: return thumb_ring_rules;
        case Technology::parthian_tactics: return parthian_tactics_rules;
        case Technology::squires: return squires_rules;
        case Technology::tracking: return tracking_rules;
        case Technology::herbal_medicine: return herbal_medicine_rules;
        case Technology::stone_cutting: return stone_cutting_rules;
        case Technology::spy_technology: return spy_technology_rules;
        case Technology::long_swordsman:
            return long_swordsman_technology_rules;
        case Technology::loom:
            return loom_rules;
        case Technology::double_bit_axe:
            return double_bit_axe_rules;
        case Technology::horse_collar:
            return horse_collar_rules;
        case Technology::fortified_wall:
            return fortified_wall_rules;
        case Technology::guard_tower:
            return guard_tower_rules;
        case Technology::keep:
            return keep_rules;
        case Technology::bodkin_arrow:
            return bodkin_arrow_rules;
        case Technology::bracer:
            return bracer_rules;
        case Technology::iron_casting:
            return iron_casting_rules;
        case Technology::blast_furnace:
            return blast_furnace_rules;
        case Technology::scale_mail_armor:
            return scale_mail_armor_rules;
        case Technology::chain_mail_armor:
            return chain_mail_armor_rules;
        case Technology::plate_mail_armor:
            return plate_mail_armor_rules;
        case Technology::scale_barding_armor:
            return scale_barding_armor_rules;
        case Technology::chain_barding_armor:
            return chain_barding_armor_rules;
        case Technology::plate_barding_armor:
            return plate_barding_armor_rules;
        case Technology::padded_archer_armor:
            return padded_archer_armor_rules;
        case Technology::leather_archer_armor:
            return leather_archer_armor_rules;
        case Technology::ring_archer_armor:
            return ring_archer_armor_rules;
        case Technology::bloodlines:
            return bloodlines_rules;
        case Technology::husbandry:
            return husbandry_rules;
        case Technology::cavalier:
            return cavalier_technology_rules;
        case Technology::paladin:
            return paladin_technology_rules;
        case Technology::light_cavalry:
            return light_cavalry_technology_rules;
        case Technology::hussar:
            return hussar_technology_rules;
        case Technology::two_handed_swordsman:
            return two_handed_swordsman_technology_rules;
        case Technology::champion:
            return champion_technology_rules;
        case Technology::arbalester:
            return arbalester_technology_rules;
        case Technology::elite_skirmisher:
            return elite_skirmisher_technology_rules;
        case Technology::war_galley:
            return war_galley_technology_rules;
        case Technology::galleon:
            return galleon_technology_rules;
        case Technology::fast_fire_ship:
            return fast_fire_ship_technology_rules;
        case Technology::heavy_demolition_ship:
            return heavy_demolition_ship_technology_rules;
        case Technology::cannon_galleon:
            return cannon_galleon_technology_rules;
        case Technology::elite_cannon_galleon:
            return elite_cannon_galleon_technology_rules;
        case Technology::careening:
            return careening_technology_rules;
        case Technology::dry_dock:
            return dry_dock_technology_rules;
        case Technology::shipwright:
            return shipwright_technology_rules;
        case Technology::longboat:
            return longboat_technology_rules;
        case Technology::elite_longboat:
            return elite_longboat_technology_rules;
        case Technology::turtle_ship:
            return turtle_ship_technology_rules;
        case Technology::elite_turtle_ship:
            return elite_turtle_ship_technology_rules;
        case Technology::longbowman:
            return longbowman_technology_rules;
        case Technology::elite_longbowman:
            return elite_longbowman_technology_rules;
        case Technology::throwing_axeman:
            return throwing_axeman_technology_rules;
        case Technology::elite_throwing_axeman:
            return elite_throwing_axeman_technology_rules;
        case Technology::huskarl:
            return huskarl_technology_rules;
        case Technology::elite_huskarl:
            return elite_huskarl_technology_rules;
        case Technology::teutonic_knight:
            return teutonic_knight_technology_rules;
        case Technology::elite_teutonic_knight:
            return elite_teutonic_knight_technology_rules;
        case Technology::samurai:
            return samurai_technology_rules;
        case Technology::elite_samurai:
            return elite_samurai_technology_rules;
        case Technology::chu_ko_nu:
            return chu_ko_nu_technology_rules;
        case Technology::elite_chu_ko_nu:
            return elite_chu_ko_nu_technology_rules;
        case Technology::cataphract:
            return cataphract_technology_rules;
        case Technology::elite_cataphract:
            return elite_cataphract_technology_rules;
        case Technology::war_elephant:
            return war_elephant_technology_rules;
        case Technology::elite_war_elephant:
            return elite_war_elephant_technology_rules;
        case Technology::mameluke:
            return mameluke_technology_rules;
        case Technology::elite_mameluke:
            return elite_mameluke_technology_rules;
        case Technology::janissary:
            return janissary_technology_rules;
        case Technology::elite_janissary:
            return elite_janissary_technology_rules;
        case Technology::berserk:
            return berserk_technology_rules;
        case Technology::elite_berserk:
            return elite_berserk_technology_rules;
        case Technology::mangudai:
            return mangudai_technology_rules;
        case Technology::elite_mangudai:
            return elite_mangudai_technology_rules;
        case Technology::berserkergang:
            return berserkergang_technology_rules;
        case Technology::jaguar_warrior:
            return jaguar_warrior_technology_rules;
        case Technology::elite_jaguar_warrior:
            return elite_jaguar_warrior_technology_rules;
        case Technology::plumed_archer:
            return plumed_archer_technology_rules;
        case Technology::elite_plumed_archer:
            return elite_plumed_archer_technology_rules;
        case Technology::conquistador:
            return conquistador_technology_rules;
        case Technology::elite_conquistador:
            return elite_conquistador_technology_rules;
        case Technology::tarkan:
            return tarkan_technology_rules;
        case Technology::elite_tarkan:
            return elite_tarkan_technology_rules;
        case Technology::woad_raider:
            return woad_raider_technology_rules;
        case Technology::elite_woad_raider:
            return elite_woad_raider_technology_rules;
        case Technology::yeomen: return yeomen_technology_rules;
        case Technology::bearded_axe: return bearded_axe_technology_rules;
        case Technology::anarchy: return anarchy_technology_rules;
        case Technology::crenellations:
            return crenellations_technology_rules;
        case Technology::kataparuto: return kataparuto_technology_rules;
        case Technology::rocketry: return rocketry_technology_rules;
        case Technology::logistica: return logistica_technology_rules;
        case Technology::mahouts: return mahouts_technology_rules;
        case Technology::zealotry: return zealotry_technology_rules;
        case Technology::artillery: return artillery_technology_rules;
        case Technology::drill: return drill_technology_rules;
        case Technology::supremacy: return supremacy_technology_rules;
        case Technology::atheism: return atheism_technology_rules;
        case Technology::shinkichon: return shinkichon_technology_rules;
        case Technology::el_dorado: return el_dorado_technology_rules;
        case Technology::elite_eagle_warrior:
            return elite_eagle_warrior_technology_rules;
        case Technology::heavy_scorpion:
            return heavy_scorpion_technology_rules;
        case Technology::onager: return onager_technology_rules;
        case Technology::siege_onager:
            return siege_onager_technology_rules;
        case Technology::heavy_cavalry_archer:
            return heavy_cavalry_archer_technology_rules;
        case Technology::heavy_camel: return heavy_camel_technology_rules;
        case Technology::capped_ram: return capped_ram_technology_rules;
        case Technology::siege_ram: return siege_ram_technology_rules;
    }
    return wheelbarrow_rules;
}

}  // namespace aoe
