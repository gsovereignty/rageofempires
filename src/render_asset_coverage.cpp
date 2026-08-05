#include "aoe/render_asset_coverage.hpp"
#include "aoe/building_damage.hpp"
#include "aoe/game_rules.hpp"
#include "aoe/simulation.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace aoe {
namespace {

constexpr std::array resource_asset_sets{
    ResourceAssetSet{ResourceRenderKind::forest, 4652, 1, 100},
    ResourceAssetSet{ResourceRenderKind::berry_bush, 2560, 4, 125},
    ResourceAssetSet{ResourceRenderKind::gold_mine, 2561, 7, 800},
    ResourceAssetSet{ResourceRenderKind::stone_mine, 1034, 7, 350},
    ResourceAssetSet{ResourceRenderKind::fish, 420, 1, 225},
};

constexpr UnitAnimationSet unit_animation_sets[] = {
    {UnitKind::villager, 1479, 15, 1484, 15, 1473, 15, -1, 0},
    {UnitKind::sheep, 3629, 9, 3634, 16, 3623, 15, -1, 0},
    {UnitKind::deer, 342, 5, 348, 15, 336, 15, -1, 0},
    {UnitKind::boar, 2557, 10, 2559, 10, 2555, 17, -1, 0},
    {UnitKind::archer, 8, 10, 12, 10, 2, 10, 5, 10},
    {UnitKind::elite_skirmisher, 613, 9, 617, 10, 607, 12, 610, 10},
    {UnitKind::skirmisher, 1650, 8, 1654, 10, 1644, 12, 1647, 10},
    {UnitKind::crossbowman, 192, 10, 196, 15, 186, 10, 189, 10},
    {UnitKind::battering_ram, 179, 1, 183, 15, 173, 15, 176, 12},
    {UnitKind::capped_ram, 1689, 1, 1693, 15, 1683, 15, 1686, 10},
    {UnitKind::siege_ram, 3035, 1, 3039, 15, 3029, 15, 3032, 10},
    {UnitKind::knight, 669, 10, 673, 10, 663, 10, 666, 10},
    {UnitKind::militia, 993, 6, 997, 12, 987, 10, 990, 10},
    {UnitKind::man_at_arms, 1044, 11, 1048, 11, 1038, 11, 1041, 11},
    {UnitKind::long_swordsman, 1181, 6, 1185, 10, 1175, 10, 1178, 10},
    {UnitKind::spearman, 873, 9, 877, 10, 867, 10, 870, 10},
    {UnitKind::mangonel, 722, 1, 726, 10, 716, 10, 719, 10},
    {UnitKind::cavalier, 855, 10, 859, 10, 849, 10, 852, 10},
    {UnitKind::pikeman, 2832, 8, 2836, 10, 2826, 10, 2829, 10},
    {UnitKind::halberdier, 2793, 10, 2797, 15, 2787, 10, 2790, 10},
    {UnitKind::hand_cannoneer, 587, 10, 591, 10, 581, 10, 584, 10},
    {UnitKind::bombard_cannon, 67, 10, 71, 10, 61, 10, 64, 15},
    {UnitKind::petard, 4497, 10, 4498, 15, 4605, 10, 4605, 10},
    {UnitKind::hussar, 4855, 14, 4857, 11, 4853, 14, 4854, 14},
    {UnitKind::scout_cavalry, 2085, 10, 2089, 10, 2079, 10, 2082, 10},
    {UnitKind::two_handed_swordsman, 2806, 6, 2810, 10, 2800, 10, 2803, 10},
    {UnitKind::arbalester, 2704, 11, 2708, 11, 2698, 11, 2701, 10},
    {UnitKind::light_cavalry, 3004, 10, 3008, 10, 2998, 10, 3001, 10},
    {UnitKind::champion, 3091, 10, 3095, 10, 3085, 10, 3088, 10},
    {UnitKind::paladin, 3078, 10, 3082, 10, 3072, 10, 3075, 10},
    {UnitKind::monk, 774, 6, 779, 10, 768, 10, 771, 10},
    {UnitKind::longbowman, 708, 15, 713, 15, 702, 15, 705, 15},
    {UnitKind::elite_longbowman, 708, 15, 713, 15, 702, 15, 705, 15},
    {UnitKind::throwing_axeman, 1057, 10, 1061, 15, 1051, 16, 1054, 13},
    {UnitKind::elite_throwing_axeman, 1057, 10, 1061, 15, 1051, 16, 1054, 13},
    {UnitKind::huskarl, 4539, 6, 4541, 10, 4537, 10, 4538, 10},
    {UnitKind::elite_huskarl, 4539, 6, 4541, 10, 4537, 10, 4538, 10},
    {UnitKind::teutonic_knight, 1194, 10, 1198, 10, 1188, 15, 1191, 15},
    {UnitKind::elite_teutonic_knight, 1194, 10, 1198, 10, 1188, 15, 1191, 15},
    {UnitKind::samurai, 980, 10, 984, 10, 974, 10, 977, 10},
    {UnitKind::elite_samurai, 980, 10, 984, 10, 974, 10, 977, 10},
    {UnitKind::chu_ko_nu, 221, 10, 225, 15, 215, 10, 218, 10},
    {UnitKind::elite_chu_ko_nu, 221, 10, 225, 15, 215, 10, 218, 10},
    {UnitKind::cataphract, 205, 10, 209, 10, 199, 10, 202, 10},
    {UnitKind::elite_cataphract, 205, 10, 209, 10, 199, 10, 202, 10},
    {UnitKind::war_elephant, 801, 7, 805, 10, 795, 7, 798, 15},
    {UnitKind::elite_war_elephant, 801, 7, 805, 10, 795, 7, 798, 15},
    {UnitKind::woad_raider, 1598, 8, 1602, 12, 1592, 12, 1595, 10},
    {UnitKind::elite_woad_raider, 1598, 8, 1602, 12, 1592, 12, 1595, 10},
    {UnitKind::king, 1851, 6, 1855, 10, 1851, 6, 1848, 10},
    {UnitKind::cavalry_archer, 326, 10, 330, 10, 320, 13, 323, 10},
    {UnitKind::heavy_cavalry_archer, 3763, 10, 3767, 10, 3757, 13, 3760, 10},
    {UnitKind::mameluke, 357, 6, 361, 10, 351, 10, 354, 10},
    {UnitKind::elite_mameluke, 357, 6, 361, 10, 351, 10, 354, 10},
    {UnitKind::janissary, 640, 10, 644, 10, 634, 10, 637, 10},
    {UnitKind::elite_janissary, 640, 10, 644, 10, 634, 10, 637, 10},
    {UnitKind::berserk, 4392, 6, 4396, 12, 4386, 10, 4389, 10},
    {UnitKind::elite_berserk, 4379, 6, 4383, 12, 4373, 10, 4376, 10},
    {UnitKind::mangudai, 788, 10, 792, 10, 782, 13, 785, 10},
    {UnitKind::elite_mangudai, 788, 10, 792, 10, 782, 13, 785, 10},
    {UnitKind::jaguar_warrior, 4860, 10, 4862, 15, 4858, 10, 4859, 10},
    {UnitKind::elite_jaguar_warrior, 4860, 10, 4862, 15, 4858, 10, 4859, 10},
    {UnitKind::plumed_archer, 4873, 10, 4875, 15, 4871, 15, 4872, 10},
    {UnitKind::elite_plumed_archer, 4873, 10, 4875, 15, 4871, 15, 4872, 10},
    {UnitKind::conquistador, 4722, 14, 4726, 14, 4716, 14, 4719, 14},
    {UnitKind::elite_conquistador, 4722, 14, 4726, 14, 4716, 14, 4719, 14},
    {UnitKind::tarkan, 4918, 14, 4920, 10, 4916, 14, 4917, 14},
    {UnitKind::elite_tarkan, 4918, 14, 4920, 10, 4916, 14, 4917, 14},
    {UnitKind::eagle_warrior, 4828, 10, 4830, 15, 4826, 10, 4827, 10},
    {UnitKind::elite_eagle_warrior, 4828, 10, 4830, 15, 4826, 10, 4827, 10},
    {UnitKind::scorpion, 942, 1, 946, 10, 936, 10, 939, 10},
    {UnitKind::heavy_scorpion, 2819, 1, 2823, 10, 2813, 8, 2816, 8},
    {UnitKind::onager, 3023, 1, 3026, 10, 3017, 10, 3020, 10},
    {UnitKind::siege_onager, 3559, 1, 3563, 10, 3553, 10, 3556, 10},
    {UnitKind::packed_trebuchet, 2279, 10, 2279, 10, 4573, 5, 4572, 10},
    {UnitKind::trebuchet, 1244, 1, 1244, 1, 1237, 22, 1241, 12},
    {UnitKind::camel_rider, 682, 5, 686, 10, 676, 10, 679, 10},
    {UnitKind::heavy_camel, 2768, 6, 2772, 12, 2762, 10, 2765, 11},
    {UnitKind::missionary, 4867, 12, 4870, 12, 4865, 13, 4866, 12},
    {UnitKind::fishing_ship, 444, 1, 449, 1, -1, 0, -1, 0},
    {UnitKind::trade_cart, 1122, 10, 4486, 10, -1, 0, -1, 0},
    {UnitKind::relic, 53, 1, 53, 1, -1, 0, -1, 0},
};

// VER 5.7 Battering Ram graphics are DAT compositions. Root and present child
// SLPs supply action-synchronized layers at one hotspot-derived ground anchor;
// some declared child SLPs are intentionally absent from shipped archives.
constexpr UnitActionCompositeSet unit_action_composite_sets[] = {
    {UnitKind::battering_ram, RenderAction::idle, 686},
    {UnitKind::battering_ram, RenderAction::moving, 690},
    {UnitKind::battering_ram, RenderAction::attacking, 680},
    {UnitKind::battering_ram, RenderAction::dying, 683},
};

constexpr UnitDeathAnimationSet unit_death_animation_sets[] = {
    {UnitKind::villager, 1476, 15},
    {UnitKind::archer, 5, 10},
    {UnitKind::sheep, 3626, 13},
    {UnitKind::deer, 339, 14},
    {UnitKind::boar, 2556, 11},
    {UnitKind::trade_cart, 1119, 15},
    {UnitKind::fishing_ship, 441, 5},
    {UnitKind::galley, 2116, 6},
    {UnitKind::war_galley, 495, 6},
    {UnitKind::galleon, 1834, 6},
    {UnitKind::transport_ship, 2116, 6},
    {UnitKind::fire_ship, 2753, 6},
    {UnitKind::fast_fire_ship, 2778, 6},
    {UnitKind::trade_cog, 2116, 6},
    {UnitKind::demolition_ship, 4347, 7},
    {UnitKind::heavy_demolition_ship, 4338, 7},
    {UnitKind::cannon_galleon, 2116, 6},
    {UnitKind::elite_cannon_galleon, 2116, 6},
    {UnitKind::longboat, 692, 5},
    {UnitKind::elite_longboat, 692, 5},
    {UnitKind::turtle_ship, 5175, 6},
    {UnitKind::elite_turtle_ship, 5175, 6},
};

constexpr NavalCompositeSet naval_composite_sets[] = {
    {UnitKind::galley, RenderAction::attacking, {4048, 4045, 4047, 4046}},
    {UnitKind::galley, RenderAction::idle, {4052, 4049, 4051, 4050}},
    {UnitKind::galley, RenderAction::moving, {4056, 4053, 4055, 4054}},
    {UnitKind::war_galley, RenderAction::attacking, {4009, 4006, 4008, 4007}},
    {UnitKind::war_galley, RenderAction::idle, {4013, 4010, 4012, 4011}},
    {UnitKind::war_galley, RenderAction::moving, {4017, 4014, 4016, 4015}},
    {UnitKind::galleon, RenderAction::attacking, {3885, 3882, 3884, 3883}},
    {UnitKind::galleon, RenderAction::idle, {3889, 3886, 3888, 3887}},
    {UnitKind::galleon, RenderAction::moving, {3893, 3890, 3892, 3891}},
    {UnitKind::transport_ship, RenderAction::attacking, {4089, 4086, 4088, 4087}},
    {UnitKind::transport_ship, RenderAction::idle, {4093, 4090, 4092, 4091}},
    {UnitKind::transport_ship, RenderAction::moving, {4097, 4094, 4096, 4095}},
    {UnitKind::fire_ship, RenderAction::attacking, {4178, 4175, 4177, 4176}},
    {UnitKind::fire_ship, RenderAction::idle, {4182, 4179, 4181, 4180}},
    {UnitKind::fire_ship, RenderAction::moving, {4004, 4001, 4003, 4002}},
    {UnitKind::fast_fire_ship, RenderAction::attacking, {4022, 4019, 4021, 4020}},
    {UnitKind::fast_fire_ship, RenderAction::idle, {4026, 4023, 4025, 4024}},
    {UnitKind::fast_fire_ship, RenderAction::moving, {4030, 4027, 4029, 4028}},
    {UnitKind::demolition_ship, RenderAction::attacking, {4173, 4173, 4173, 4173}},
    {UnitKind::demolition_ship, RenderAction::idle, {4039, 4036, 4038, 4037}},
    {UnitKind::demolition_ship, RenderAction::moving, {4043, 4040, 4042, 4041}},
    {UnitKind::heavy_demolition_ship, RenderAction::attacking, {4155, 4152, 4154, 4153}},
    {UnitKind::heavy_demolition_ship, RenderAction::idle, {3988, 3985, 3987, 3986}},
    {UnitKind::heavy_demolition_ship, RenderAction::moving, {3992, 3989, 3991, 3990}},
    {UnitKind::cannon_galleon, RenderAction::attacking, {3957, 3954, 3956, 3955}},
    {UnitKind::cannon_galleon, RenderAction::idle, {3961, 3958, 3960, 3959}},
    {UnitKind::cannon_galleon, RenderAction::moving, {3965, 3962, 3964, 3963}},
    {UnitKind::elite_cannon_galleon, RenderAction::attacking, {3957, 3954, 3956, 3955}},
    {UnitKind::elite_cannon_galleon, RenderAction::idle, {3961, 3958, 3960, 3959}},
    {UnitKind::elite_cannon_galleon, RenderAction::moving, {3965, 3962, 3964, 3963}},
    {UnitKind::longboat, RenderAction::attacking, {953, 953, 953, 953}, false},
    {UnitKind::longboat, RenderAction::idle, {959, 959, 959, 959}, false},
    {UnitKind::longboat, RenderAction::moving, {963, 963, 963, 963}, false},
    {UnitKind::elite_longboat, RenderAction::attacking, {953, 953, 953, 953}, false},
    {UnitKind::elite_longboat, RenderAction::idle, {959, 959, 959, 959}, false},
    {UnitKind::elite_longboat, RenderAction::moving, {963, 963, 963, 963}, false},
    {UnitKind::turtle_ship, RenderAction::attacking, {7257, 7257, 7257, 7257}},
    {UnitKind::turtle_ship, RenderAction::idle, {7258, 7258, 7258, 7258}},
    {UnitKind::turtle_ship, RenderAction::moving, {7259, 7259, 7259, 7259}},
    {UnitKind::elite_turtle_ship, RenderAction::attacking, {7257, 7257, 7257, 7257}},
    {UnitKind::elite_turtle_ship, RenderAction::idle, {7258, 7258, 7258, 7258}},
    {UnitKind::elite_turtle_ship, RenderAction::moving, {7259, 7259, 7259, 7259}},
    {UnitKind::trade_cog, RenderAction::attacking, {3971, 3971, 3971, 3971}},
    {UnitKind::trade_cog, RenderAction::idle, {3975, 3975, 3975, 3975}},
    {UnitKind::trade_cog, RenderAction::moving, {3979, 3979, 3979, 3979}},
};

constexpr BuildingCompositeSet building_composite_sets[] = {
    {BuildingKind::town_center, {{
        {{3241, 3241, 3241, 3241, 3241}},
        {{3253, 3250, 3252, 3251, 6986}},
        {{3265, 3262, 3264, 3263, 7002}},
        {{3041, 3038, 3040, 3039, 7018}},
    }}},
    {BuildingKind::barracks, {{
        {{2575, 2575, 2575, 2575, 2575}},
        {{93, 90, 92, 91, 6698}},
        {{105, 102, 104, 103, 6706}},
        {{105, 102, 104, 103, 6706}},
    }}},
    {BuildingKind::mill, {{
        {{3124, 3124, 3124, 3124, 3124}},
        {{368, 365, 367, 366, 6923}},
        {{380, 377, 379, 378, 6930}},
        {{380, 377, 379, 378, 6930}},
    }}},
    {BuildingKind::archery_range, {{
        {{-1, -1, -1, -1, -1}},
        {{12, 9, 11, 10, 6658}},
        {{24, 21, 23, 22, 6665}},
        {{24, 21, 23, 22, 6665}},
    }}},
    {BuildingKind::watch_tower, {{
        {{4202, 4199, 4201, 4200, 7116}},
        {{2532, 2529, 2531, 2530, 7123}},
        {{2407, 2404, 2406, 2405, 7131}},
        {{-1, -1, -1, -1, -1}},
    }}},
    {BuildingKind::stable, {{
        {{-1, -1, -1, -1, -1}},
        {{513, 510, 512, 511, 7061}},
        {{525, 522, 524, 523, 7068}},
        {{525, 522, 524, 523, 7068}},
    }}},
    {BuildingKind::castle, {{
        {{-1, -1, -1, -1, -1}},
        {{-1, -1, -1, -1, -1}},
        {{174, 171, 173, 172, 6747}},
        {{174, 171, 173, 172, 6747}},
    }}},
    {BuildingKind::siege_workshop, {{
        {{-1, -1, -1, -1, -1}},
        {{-1, -1, -1, -1, -1}},
        {{489, 486, 488, 487, 7042}},
        {{489, 486, 488, 487, 7042}},
    }}},
    {BuildingKind::dock, {{
        {{215, 215, 215, 215, 215}},
        {{215, 215, 215, 215, 215}},
        {{215, 215, 215, 215, 215}},
        {{215, 215, 215, 215, 215}},
    }}, CompositePolicy::delta_graph},
    {BuildingKind::outpost, {{
        {{3223, 3223, 3223, 3223, 3223}},
        {{3223, 3223, 3223, 3223, 3223}},
        {{3223, 3223, 3223, 3223, 3223}},
        {{3223, 3223, 3223, 3223, 3223}},
    }}},
    {BuildingKind::monastery, {{
        {{-1, -1, -1, -1, -1}},
        {{-1, -1, -1, -1, -1}},
        {{150, 147, 149, 148, -1}},
        {{150, 147, 149, 148, -1}},
    }}},
    {BuildingKind::market, {{
        {{2268, 2265, 2267, 2266, -1}},
        {{2268, 2265, 2267, 2266, -1}},
        {{411, 408, 410, 409, -1}},
        {{3434, 3431, 3433, 3432, -1}},
    }}},
    {BuildingKind::stone_gate_x, {{
        {{-1, -1, -1, -1, -1}},
        {{-1, -1, -1, -1, -1}},
        {{6497, 6497, 6497, 6497, -1}},
        {{6497, 6497, 6497, 6497, -1}},
    }}},
    {BuildingKind::stone_gate_y, {{
        {{-1, -1, -1, -1, -1}},
        {{-1, -1, -1, -1, -1}},
        {{6518, 6518, 6518, 6518, -1}},
        {{6518, 6518, 6518, 6518, -1}},
    }}},
    {BuildingKind::palisade_gate_x, {{
        {{6512, 6512, 6512, 6512, 6512}},
        {{6512, 6512, 6512, 6512, 6512}},
        {{6512, 6512, 6512, 6512, 6512}},
        {{6512, 6512, 6512, 6512, 6512}},
    }}},
    {BuildingKind::palisade_gate_y, {{
        {{6533, 6533, 6533, 6533, 6533}},
        {{6533, 6533, 6533, 6533, 6533}},
        {{6533, 6533, 6533, 6533, 6533}},
        {{6533, 6533, 6533, 6533, 6533}},
    }}},
};

constexpr BuildingStateRoot building_state_roots[] = {
    {BuildingKind::palisade_wall, RenderBuildingState::construction, 118, true},
    {BuildingKind::watch_tower, RenderBuildingState::construction, 118, true},
    {BuildingKind::stone_gate_x, RenderBuildingState::construction, 118, true},
    {BuildingKind::stone_gate_y, RenderBuildingState::construction, 118, true},
    {BuildingKind::outpost, RenderBuildingState::construction, 118, true},
    {BuildingKind::bombard_tower, RenderBuildingState::construction, 118, true},
    {BuildingKind::house, RenderBuildingState::construction, 119, true},
    {BuildingKind::mill, RenderBuildingState::construction, 119, true},
    {BuildingKind::lumber_camp, RenderBuildingState::construction, 119, true},
    {BuildingKind::mining_camp, RenderBuildingState::construction, 119, true},
    {BuildingKind::barracks, RenderBuildingState::construction, 120, true},
    {BuildingKind::archery_range, RenderBuildingState::construction, 120, true},
    {BuildingKind::stable, RenderBuildingState::construction, 120, true},
    {BuildingKind::blacksmith, RenderBuildingState::construction, 120, true},
    {BuildingKind::monastery, RenderBuildingState::construction, 120, true},
    {BuildingKind::town_center, RenderBuildingState::construction, 121, true},
    {BuildingKind::castle, RenderBuildingState::construction, 121, true},
    {BuildingKind::university, RenderBuildingState::construction, 121, true},
    {BuildingKind::siege_workshop, RenderBuildingState::construction, 121, true},
    {BuildingKind::market, RenderBuildingState::construction, 121, true},
    {BuildingKind::wonder, RenderBuildingState::construction, 123, true},
    {BuildingKind::dock, RenderBuildingState::construction, 4248, true},
    {BuildingKind::palisade_wall, RenderBuildingState::destroyed, 37},
    {BuildingKind::stone_wall, RenderBuildingState::destroyed, 37},
    {BuildingKind::outpost, RenderBuildingState::destroyed, 37},
    {BuildingKind::house, RenderBuildingState::destroyed, 38},
    {BuildingKind::mill, RenderBuildingState::destroyed, 38},
    {BuildingKind::lumber_camp, RenderBuildingState::destroyed, 38},
    {BuildingKind::mining_camp, RenderBuildingState::destroyed, 38},
    {BuildingKind::watch_tower, RenderBuildingState::destroyed, 38},
    {BuildingKind::palisade_gate_x, RenderBuildingState::destroyed, 38},
    {BuildingKind::palisade_gate_y, RenderBuildingState::destroyed, 38},
    {BuildingKind::stone_gate_x, RenderBuildingState::destroyed, 38},
    {BuildingKind::stone_gate_y, RenderBuildingState::destroyed, 38},
    {BuildingKind::bombard_tower, RenderBuildingState::destroyed, 38},
    {BuildingKind::barracks, RenderBuildingState::destroyed, 39},
    {BuildingKind::archery_range, RenderBuildingState::destroyed, 39},
    {BuildingKind::stable, RenderBuildingState::destroyed, 39},
    {BuildingKind::blacksmith, RenderBuildingState::destroyed, 39},
    {BuildingKind::monastery, RenderBuildingState::destroyed, 39},
    {BuildingKind::town_center, RenderBuildingState::destroyed, 40},
    {BuildingKind::castle, RenderBuildingState::destroyed, 40},
    {BuildingKind::university, RenderBuildingState::destroyed, 40},
    {BuildingKind::siege_workshop, RenderBuildingState::destroyed, 40},
    {BuildingKind::market, RenderBuildingState::destroyed, 40},
    {BuildingKind::wonder, RenderBuildingState::destroyed, 42},
    {BuildingKind::dock, RenderBuildingState::destroyed, 5452},
};

// Generic CNST roots are footprint-sized scaffold/shadow layers, not full
// building bodies. Original construction combines them with the selected
// civilization/Age completed body, revealed continuously from ground upward.
// Four represented kinds instead have complete dedicated construction art.
constexpr BuildingConstructionContract building_construction_contracts[] = {
    {BuildingKind::town_center, ConstructionBodyMode::progressive_completed_body, 121},
    {BuildingKind::barracks, ConstructionBodyMode::progressive_completed_body, 120},
    {BuildingKind::archery_range, ConstructionBodyMode::progressive_completed_body, 120},
    {BuildingKind::house, ConstructionBodyMode::progressive_completed_body, 119},
    {BuildingKind::mill, ConstructionBodyMode::progressive_completed_body, 119},
    {BuildingKind::lumber_camp, ConstructionBodyMode::progressive_completed_body, 119},
    {BuildingKind::mining_camp, ConstructionBodyMode::progressive_completed_body, 119},
    {BuildingKind::farm, ConstructionBodyMode::progressive_completed_body, -1},
    {BuildingKind::stable, ConstructionBodyMode::progressive_completed_body, 120},
    {BuildingKind::blacksmith, ConstructionBodyMode::progressive_completed_body, 120},
    {BuildingKind::castle, ConstructionBodyMode::progressive_completed_body, 121},
    {BuildingKind::university, ConstructionBodyMode::progressive_completed_body, 121},
    {BuildingKind::siege_workshop, ConstructionBodyMode::progressive_completed_body, 121},
    {BuildingKind::palisade_wall, ConstructionBodyMode::progressive_completed_body, 118},
    {BuildingKind::watch_tower, ConstructionBodyMode::progressive_completed_body, 118},
    {BuildingKind::stone_wall, ConstructionBodyMode::dedicated_construction_body, -1},
    {BuildingKind::palisade_gate_x, ConstructionBodyMode::dedicated_construction_body, -1},
    {BuildingKind::palisade_gate_y, ConstructionBodyMode::dedicated_construction_body, -1},
    {BuildingKind::stone_gate_x, ConstructionBodyMode::progressive_completed_body, 118},
    {BuildingKind::stone_gate_y, ConstructionBodyMode::progressive_completed_body, 118},
    {BuildingKind::monastery, ConstructionBodyMode::progressive_completed_body, 120},
    {BuildingKind::market, ConstructionBodyMode::progressive_completed_body, 121},
    {BuildingKind::dock, ConstructionBodyMode::progressive_completed_body, 4248},
    {BuildingKind::bombard_tower, ConstructionBodyMode::progressive_completed_body, 118},
    {BuildingKind::fish_trap, ConstructionBodyMode::dedicated_construction_body, -1},
    {BuildingKind::outpost, ConstructionBodyMode::progressive_completed_body, 118},
    {BuildingKind::wonder, ConstructionBodyMode::progressive_completed_body, 123},
};
static_assert(std::size(building_construction_contracts) == 27);

constexpr BuildingDirectSlpSet building_direct_slp_sets[] = {
    {BuildingKind::bombard_tower, {{
        {{2549, 2549, 2549, 2549, 2549}},
        {{2549, 2549, 2549, 2549, 2549}},
        {{2549, 2549, 2549, 2549, 2549}},
        {{2549, 2549, 2549, 2549, 2549}},
    }}, true},
    {BuildingKind::house, {{
        {{2223, 2223, 2223, 2223, 2223}},
        {{2235, 2232, 2234, 2233, 5038}},
        {{2247, 2244, 2246, 2245, 5041}},
        {{2247, 2244, 2246, 2245, 5041}},
    }}, true},
    {BuildingKind::blacksmith, {{
        {{93, 90, 92, 91, 4931}},
        {{93, 90, 92, 91, 4931}},
        {{105, 102, 104, 103, 4934}},
        {{105, 102, 104, 103, 4934}},
    }}, true},
    {BuildingKind::lumber_camp, {{
        {{3507, 3504, 3506, 3505, 5106}},
        {{3507, 3504, 3506, 3505, 5106}},
        {{3507, 3504, 3506, 3505, 5106}},
        {{3507, 3504, 3506, 3505, 5106}},
    }}, true},
    {BuildingKind::mining_camp, {{
        {{3495, 3492, 3494, 3493, 5055}},
        {{3495, 3492, 3494, 3493, 5055}},
        {{3495, 3492, 3494, 3493, 5055}},
        {{3495, 3492, 3494, 3493, 5055}},
    }}, true},
    {BuildingKind::university, {{
        {{3835, 3832, 3834, 3833, 5119}},
        {{3835, 3832, 3834, 3833, 5119}},
        {{3835, 3832, 3834, 3833, 5119}},
        {{3839, 3836, 3838, 3837, 5122}},
    }}, true},
};

constexpr BuildingAnimatedSlpSet building_animated_slp_sets[] = {
    {BuildingKind::fish_trap, RenderBuildingState::foundation, 4585, 1},
    {BuildingKind::fish_trap, RenderBuildingState::construction, 4585, 1},
    {BuildingKind::fish_trap, RenderBuildingState::completed, 3593, 6},
};

constexpr WonderCompositeSet wonder_composite_sets[] = {
    {Civilization::generic, 3068},
    {Civilization::britons, 3068},
    {Civilization::franks, 3076},
    {Civilization::goths, 3093},
    {Civilization::teutons, 3097},
    {Civilization::japanese, 3072},
    {Civilization::chinese, 3080},
    {Civilization::byzantines, 3098},
    {Civilization::persians, 3075},
    {Civilization::saracens, 3077},
    {Civilization::turks, 3078},
    {Civilization::vikings, 3094},
    {Civilization::mongols, 3096},
    {Civilization::celts, 3095},
    {Civilization::spanish, 6322},
    {Civilization::aztecs, 6631},
    {Civilization::mayans, 6324},
    {Civilization::huns, 6323},
    {Civilization::koreans, 7249},
};

constexpr BuildingTopologySlpSet building_topology_slp_sets[] = {
    {
        BuildingKind::stone_wall,
        {2101, 2098, 2100, 2099, 5124},
        {3321, 3318, 3320, 3319, 7107},
        {},
        {},
        0,
        5,
        3,
    },
    {
        BuildingKind::fortified_wall,
        {2113, 2110, 2112, 2111, 5126},
        {3321, 3318, 3320, 3319, 7107},
        {},
        {},
        0,
        5,
        3,
    },
    {
        BuildingKind::palisade_wall,
        {1828, 1828, 1828, 1828, 1828},
        {-1, -1, -1, -1, -1},
        4682,
        4534,
        9,
        5,
        3,
    },
};

constexpr GateConstructionSet gate_construction_sets[] = {
    {
        BuildingKind::palisade_gate_x,
        {3289, 3286, 3288, 3287, 6798},
    },
    {
        BuildingKind::palisade_gate_y,
        {3305, 3302, 3304, 3303, 6830},
    },
};

std::string json_escape(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    for (const char character : value) {
        switch (character) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\b': result += "\\b"; break;
            case '\f': result += "\\f"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:
                if (static_cast<unsigned char>(character) < 0x20U) {
                    constexpr char digits[] = "0123456789abcdef";
                    result += "\\u00";
                    result += digits[
                        (static_cast<unsigned char>(character) >> 4U) & 0x0fU
                    ];
                    result += digits[
                        static_cast<unsigned char>(character) & 0x0fU
                    ];
                } else {
                    result += character;
                }
        }
    }
    return result;
}

std::string optional_number(const std::optional<std::int32_t>& value) {
    return value ? std::to_string(*value) : "null";
}

std::string category_name(RenderObjectCategory category) {
    switch (category) {
        case RenderObjectCategory::unit: return "unit";
        case RenderObjectCategory::building: return "building";
        case RenderObjectCategory::projectile: return "projectile";
        case RenderObjectCategory::impact: return "impact";
        case RenderObjectCategory::resource: return "resource";
        case RenderObjectCategory::unit_death: return "unit_death";
        case RenderObjectCategory::building_rubble: return "building_rubble";
    }
    throw std::logic_error{"unhandled render object category"};
}

std::string action_name(RenderAction action) {
    switch (action) {
        case RenderAction::idle: return "idle";
        case RenderAction::moving: return "moving";
        case RenderAction::attacking: return "attacking";
        case RenderAction::gathering: return "gathering";
        case RenderAction::working: return "working";
        case RenderAction::healing: return "healing";
        case RenderAction::converting: return "converting";
        case RenderAction::transforming: return "transforming";
        case RenderAction::carrying_relic: return "carrying_relic";
        case RenderAction::dying: return "dying";
        case RenderAction::destroyed: return "destroyed";
    }
    throw std::logic_error{"unhandled render action"};
}

std::string action_detail_name(RenderActionDetail detail) {
    switch (detail) {
        case RenderActionDetail::none: return "none";
        case RenderActionDetail::animal_resource:
            return "animal_resource";
        case RenderActionDetail::terrain_resource:
            return "terrain_resource";
        case RenderActionDetail::farm: return "farm";
        case RenderActionDetail::fish: return "fish";
        case RenderActionDetail::fish_trap: return "fish_trap";
        case RenderActionDetail::repair: return "repair";
        case RenderActionDetail::construction: return "construction";
    }
    throw std::logic_error{"unhandled render action detail"};
}

std::string building_state_name(RenderBuildingState state) {
    switch (state) {
        case RenderBuildingState::foundation: return "foundation";
        case RenderBuildingState::construction: return "construction";
        case RenderBuildingState::completed: return "completed";
        case RenderBuildingState::damaged: return "damaged";
        case RenderBuildingState::dying: return "dying";
        case RenderBuildingState::destroyed: return "destroyed";
    }
    throw std::logic_error{"unhandled building render state"};
}

std::string civilization_name(Civilization civilization) {
    static constexpr std::string_view names[] = {
        "generic", "britons", "franks", "teutons", "goths", "celts",
        "vikings", "byzantines", "japanese", "chinese", "persians",
        "saracens", "turks", "mongols", "spanish", "huns", "koreans",
        "aztecs", "mayans",
    };
    const auto index = static_cast<std::size_t>(civilization);
    if (index >= std::size(names)) {
        throw std::invalid_argument{"invalid civilization"};
    }
    return std::string{names[index]};
}

std::string age_name(Age age) {
    static constexpr std::string_view names[] = {
        "dark", "feudal", "castle", "imperial",
    };
    const auto index = static_cast<std::size_t>(age);
    if (index >= std::size(names)) {
        throw std::invalid_argument{"invalid age"};
    }
    return std::string{names[index]};
}

void write_request(std::ostream& output, const AssetRequest& request) {
    output << "\"requested_asset\":{"
           << "\"graphic_id\":" << optional_number(request.graphic_id) << ','
           << "\"slp_id\":" << optional_number(request.slp_id) << ','
           << "\"composite_slp_ids\":[";
    for (std::size_t index = 0;
         index < request.composite_slp_ids.size();
         ++index) {
        if (index != 0) output << ',';
        output << request.composite_slp_ids[index];
    }
    output << "],\"overlay_graphic_ids\":[";
    for (std::size_t index = 0;
         index < request.overlay_graphic_ids.size();
         ++index) {
        if (index != 0) output << ',';
        output << request.overlay_graphic_ids[index];
    }
    output << "],\"shadow_slp_id\":"
           << optional_number(request.shadow_slp_id)
           << ",\"construction_body_graphic_id\":"
           << optional_number(request.construction_body_graphic_id)
           << ",\"construction_body_slp_id\":"
           << optional_number(request.construction_body_slp_id)
           << ",\"required_frame_count\":" << request.required_frame_count
           << ",\"required_direction_count\":"
           << request.required_direction_count
           << ",\"source_mapping\":\""
           << json_escape(request.source_mapping) << "\"}";
}

void write_state(std::ostream& output, const RenderStateKey& state) {
    output << "\"render_state\":{"
           << "\"category\":\"" << category_name(state.category) << "\","
           << "\"object_kind\":\"" << json_escape(state.object_kind) << "\","
           << "\"action\":\"" << action_name(state.action) << "\","
           << "\"action_detail\":\""
           << action_detail_name(state.action_detail) << "\","
           << "\"building_state\":\""
           << building_state_name(state.building_state) << "\","
           << "\"owner\":" << static_cast<unsigned>(state.owner) << ','
           << "\"civilization\":\""
           << civilization_name(state.civilization) << "\","
           << "\"age\":\"" << age_name(state.age) << "\","
           << "\"architecture_family\":" << state.architecture_family << ','
           << "\"damage_stage\":" << state.damage_stage << ','
           << "\"construction_stage\":" << state.construction_stage << ','
           << "\"upgrade_variant\":" << state.upgrade_variant << ','
           << "\"direction\":" << state.direction << ','
           << "\"display_angle\":" << state.display_angle << ','
           << "\"animation_frame\":" << state.animation_frame << ','
           << "\"moving\":" << (state.moving ? "true" : "false") << ','
           << "\"composite\":" << (state.composite ? "true" : "false") << ','
           << "\"shadow\":" << (state.shadow ? "true" : "false") << '}';
}

}  // namespace

std::string RenderStateKey::stable_key() const {
    std::ostringstream output;
    output << category_name(category) << '|' << object_kind << '|'
           << action_name(action) << '|'
           << action_detail_name(action_detail) << '|'
           << building_state_name(building_state)
           << '|' << static_cast<unsigned>(owner) << '|'
           << civilization_name(civilization) << '|' << age_name(age) << '|'
           << architecture_family << '|' << damage_stage << '|'
           << construction_stage << '|' << direction << '|' << display_angle
           << '|' << upgrade_variant
           << '|' << animation_frame << '|' << moving << '|' << composite
           << '|' << shadow;
    return output.str();
}

std::span<const UnitAnimationSet> canonical_unit_animation_sets() {
    return unit_animation_sets;
}

std::optional<UnitAnimationSet> unit_animation_set(UnitKind kind) {
    const auto found = std::ranges::find(
        unit_animation_sets, kind, &UnitAnimationSet::kind
    );
    return found == std::end(unit_animation_sets)
        ? std::nullopt
        : std::optional<UnitAnimationSet>{*found};
}

std::span<const UnitActionCompositeSet>
canonical_unit_action_composite_sets() {
    return unit_action_composite_sets;
}

const UnitActionCompositeSet* unit_action_composite_set(
    UnitKind kind,
    RenderAction action
) {
    const auto found = std::ranges::find_if(
        unit_action_composite_sets,
        [kind, action](const UnitActionCompositeSet& set) {
            return set.kind == kind && set.action == action;
        }
    );
    return found == std::end(unit_action_composite_sets)
        ? nullptr : &*found;
}

std::span<const UnitDeathAnimationSet>
canonical_unit_death_animation_sets() {
    return unit_death_animation_sets;
}

const UnitDeathAnimationSet* unit_death_animation_set(UnitKind kind) {
    const auto found = std::ranges::find(
        unit_death_animation_sets, kind, &UnitDeathAnimationSet::kind
    );
    return found == std::end(unit_death_animation_sets)
        ? nullptr
        : &*found;
}

std::size_t unit_death_animation_frame(
    UnitKind kind,
    int elapsed_simulation_ticks
) {
    const UnitDeathAnimationSet* animation =
        unit_death_animation_set(kind);
    if (animation == nullptr || animation->frames <= 0) return 0;
    const std::size_t elapsed_frames = static_cast<std::size_t>(
        std::max(elapsed_simulation_ticks, 0)
    ) * 2U;
    return std::min(
        elapsed_frames,
        static_cast<std::size_t>(animation->frames - 1)
    );
}

std::span<const NavalCompositeSet> canonical_naval_composite_sets() {
    return naval_composite_sets;
}

const NavalCompositeSet* naval_composite_set(
    UnitKind kind,
    RenderAction action
) {
    const auto found = std::ranges::find_if(
        naval_composite_sets,
        [kind, action](const NavalCompositeSet& mapping) {
            return mapping.kind == kind && mapping.action == action;
        }
    );
    return found == std::end(naval_composite_sets)
        ? nullptr
        : &*found;
}

std::span<const BuildingCompositeSet>
canonical_building_composite_sets() {
    return building_composite_sets;
}

const BuildingCompositeSet* building_composite_set(BuildingKind kind) {
    const auto found = std::ranges::find(
        building_composite_sets, kind, &BuildingCompositeSet::kind
    );
    return found == std::end(building_composite_sets)
        ? nullptr
        : &*found;
}

std::span<const BuildingStateRoot> canonical_building_state_roots() {
    return building_state_roots;
}

const BuildingStateRoot* building_state_root(
    BuildingKind kind,
    RenderBuildingState state
) {
    const auto found = std::ranges::find_if(
        building_state_roots,
        [kind, state](const BuildingStateRoot& mapping) {
            return mapping.kind == kind && mapping.state == state;
        }
    );
    return found == std::end(building_state_roots)
        ? nullptr
        : &*found;
}

std::span<const BuildingDirectSlpSet>
canonical_building_direct_slp_sets() {
    return building_direct_slp_sets;
}

const BuildingDirectSlpSet* building_direct_slp_set(BuildingKind kind) {
    const auto found = std::ranges::find(
        building_direct_slp_sets, kind, &BuildingDirectSlpSet::kind
    );
    return found == std::end(building_direct_slp_sets)
        ? nullptr
        : &*found;
}

std::span<const BuildingAnimatedSlpSet>
canonical_building_animated_slp_sets() {
    return building_animated_slp_sets;
}

const BuildingAnimatedSlpSet* building_animated_slp_set(
    BuildingKind kind,
    RenderBuildingState state
) {
    const auto found = std::ranges::find_if(
        building_animated_slp_sets,
        [kind, state](const BuildingAnimatedSlpSet& mapping) {
            return mapping.kind == kind && mapping.state == state;
        }
    );
    return found == std::end(building_animated_slp_sets)
        ? nullptr
        : &*found;
}

std::span<const WonderCompositeSet>
canonical_wonder_composite_sets() {
    return wonder_composite_sets;
}

const WonderCompositeSet* wonder_composite_set(
    Civilization civilization
) {
    const auto found = std::ranges::find(
        wonder_composite_sets,
        civilization,
        &WonderCompositeSet::civilization
    );
    return found == std::end(wonder_composite_sets)
        ? nullptr
        : &*found;
}

std::span<const BuildingTopologySlpSet>
canonical_building_topology_slp_sets() {
    return building_topology_slp_sets;
}

const BuildingTopologySlpSet* building_topology_slp_set(
    BuildingKind kind
) {
    const auto found = std::ranges::find(
        building_topology_slp_sets,
        kind,
        &BuildingTopologySlpSet::kind
    );
    return found == std::end(building_topology_slp_sets)
        ? nullptr
        : &*found;
}

std::span<const GateConstructionSet>
canonical_gate_construction_sets() {
    return gate_construction_sets;
}

const GateConstructionSet* gate_construction_set(
    BuildingKind kind
) {
    const auto found = std::ranges::find(
        gate_construction_sets,
        kind,
        &GateConstructionSet::kind
    );
    return found == std::end(gate_construction_sets)
        ? nullptr
        : &*found;
}

std::vector<UnitRenderStateVariant> canonical_unit_render_states(
    UnitKind kind
) {
    std::vector<UnitRenderStateVariant> states{
        {RenderAction::idle, RenderActionDetail::none, false},
    };
    if (kind != UnitKind::relic) {
        states.push_back({
            RenderAction::moving, RenderActionDetail::none, true
        });
        states.push_back({
            RenderAction::dying, RenderActionDetail::none, false
        });
    }
    if (rules_for(kind).attack > 0 || is_animal(kind)) {
        states.push_back({
            RenderAction::attacking, RenderActionDetail::none, false
        });
    }
    const auto add_both_motion_states =
        [&states](RenderAction action, RenderActionDetail detail) {
            states.push_back({action, detail, false});
            states.push_back({action, detail, true});
        };
    if (kind == UnitKind::villager) {
        add_both_motion_states(
            RenderAction::gathering,
            RenderActionDetail::animal_resource
        );
        add_both_motion_states(
            RenderAction::gathering,
            RenderActionDetail::terrain_resource
        );
        add_both_motion_states(
            RenderAction::gathering,
            RenderActionDetail::farm
        );
        add_both_motion_states(
            RenderAction::working,
            RenderActionDetail::repair
        );
        add_both_motion_states(
            RenderAction::working,
            RenderActionDetail::construction
        );
    } else if (kind == UnitKind::fishing_ship) {
        add_both_motion_states(
            RenderAction::gathering,
            RenderActionDetail::fish
        );
        add_both_motion_states(
            RenderAction::gathering,
            RenderActionDetail::fish_trap
        );
    } else if (kind == UnitKind::monk) {
        states.push_back({
            RenderAction::healing, RenderActionDetail::none, false
        });
        states.push_back({
            RenderAction::converting, RenderActionDetail::none, false
        });
        add_both_motion_states(
            RenderAction::carrying_relic,
            RenderActionDetail::none
        );
    } else if (kind == UnitKind::missionary) {
        states.push_back({
            RenderAction::healing, RenderActionDetail::none, false
        });
        states.push_back({
            RenderAction::converting, RenderActionDetail::none, false
        });
    }
    if (kind == UnitKind::packed_trebuchet ||
        kind == UnitKind::trebuchet) {
        states.push_back({
            RenderAction::transforming, RenderActionDetail::none, false
        });
    }
    return states;
}

std::span<const ResourceAssetSet> canonical_resource_asset_sets() {
    return resource_asset_sets;
}

const ResourceAssetSet* resource_asset_set(ResourceRenderKind kind) {
    const auto found = std::ranges::find(
        resource_asset_sets, kind, &ResourceAssetSet::kind
    );
    return found == resource_asset_sets.end() ? nullptr : &*found;
}

std::optional<ResourceRenderKind> resource_render_kind_for(
    Terrain terrain
) {
    switch (terrain) {
        case Terrain::forest:
        case Terrain::pine_forest:
        case Terrain::oak_forest:
        case Terrain::bamboo_forest:
        case Terrain::palm_forest:
        case Terrain::jungle_forest:
            return ResourceRenderKind::forest;
        case Terrain::berry_bush: return ResourceRenderKind::berry_bush;
        case Terrain::gold_mine: return ResourceRenderKind::gold_mine;
        case Terrain::stone_mine: return ResourceRenderKind::stone_mine;
        case Terrain::fish:
        case Terrain::fish_shore:
        case Terrain::fish_deep:
            return ResourceRenderKind::fish;
        default: return std::nullopt;
    }
}

int render_resource_frame(ResourceRenderKind kind, int remaining) {
    const ResourceAssetSet* mapping = resource_asset_set(kind);
    if (mapping == nullptr || mapping->frame_count <= 1 ||
        mapping->initial_amount <= 0) {
        return 0;
    }
    return std::clamp(
        (mapping->initial_amount - remaining) *
            mapping->frame_count / mapping->initial_amount,
        0,
        mapping->frame_count - 1
    );
}

std::string_view resource_render_kind_name(ResourceRenderKind kind) {
    switch (kind) {
        case ResourceRenderKind::forest: return "forest";
        case ResourceRenderKind::berry_bush: return "berry_bush";
        case ResourceRenderKind::gold_mine: return "gold_mine";
        case ResourceRenderKind::stone_mine: return "stone_mine";
        case ResourceRenderKind::fish: return "fish";
    }
    return "unknown";
}

AssetResolution resolve_building_asset(
    const RenderStateKey& state,
    BuildingKind kind
) {
    BuildingKind damage_kind = kind;
    if (kind == BuildingKind::watch_tower) {
        damage_kind = state.upgrade_variant >= 2
            ? BuildingKind::keep
            : state.upgrade_variant == 1
                ? BuildingKind::guard_tower
                : BuildingKind::watch_tower;
    }
    kind = kind == BuildingKind::guard_tower || kind == BuildingKind::keep
        ? BuildingKind::watch_tower
        : kind == BuildingKind::fortified_gate_x
                ? BuildingKind::stone_gate_x
                : kind == BuildingKind::fortified_gate_y
                    ? BuildingKind::stone_gate_y
                    : kind;
    AssetResolution result;
    result.state = state;
    result.request.source_mapping = "canonical building catalog";
    result.evidence_sources = {
        "src/render_asset_coverage.cpp:building catalogs",
        "runtime building state selection",
    };
    if (state.category != RenderObjectCategory::building &&
        state.category != RenderObjectCategory::building_rubble) {
        result.status = AssetCoverageStatus::invalid_runtime_selection;
        result.reason = "building resolver received non-building category";
        return result;
    }
    if (state.owner > EntityOwner::neutral_stable_id) {
        result.status = AssetCoverageStatus::unsupported_owner_state;
        result.reason = "owner stable ID is outside supported roster";
        return result;
    }
    if (kind == BuildingKind::fish_trap &&
        (state.building_state == RenderBuildingState::damaged ||
         state.building_state == RenderBuildingState::dying ||
         state.building_state == RenderBuildingState::destroyed)) {
        result.status = AssetCoverageStatus::intentional_procedural;
        result.reason =
            state.building_state == RenderBuildingState::damaged
            ? "Fish Trap DAT damage roots 5357..5359 contain no drawable "
              "layer; reviewed procedural body retained"
            : "Fish Trap DAT unit 199 has no dying graphic; reviewed "
              "procedural rubble retained";
        result.intentional_procedural = true;
        result.request.source_mapping =
            "live DAT unit 199 + canonical procedural Fish Trap renderer";
        result.evidence_sources.push_back(
            "generated/building_body_state_catalog.json:fish_trap"
        );
        result.evidence_sources.push_back(
            "docs/assets/PROCEDURAL_BUILDING_ASSET_MAP.md:Fish Trap"
        );
        return result;
    }
    if (const BuildingAnimatedSlpSet* animated =
            building_animated_slp_set(kind, state.building_state);
        animated != nullptr) {
        result.request.slp_id = animated->slp_id;
        result.request.required_frame_count = animated->frame_count;
        result.request.required_direction_count = 1;
        result.request.source_mapping =
            "canonical_building_animated_slp_sets";
        result.status = AssetCoverageStatus::renderable;
        result.reason =
            "canonical animated building SLP selected";
        return result;
    }
    if (state.building_state == RenderBuildingState::foundation ||
        state.building_state == RenderBuildingState::dying) {
        RenderStateKey aliased_state = state;
        aliased_state.building_state =
            state.building_state == RenderBuildingState::foundation
            ? RenderBuildingState::construction
            : RenderBuildingState::destroyed;
        if (state.building_state == RenderBuildingState::dying) {
            aliased_state.category =
                RenderObjectCategory::building_rubble;
            aliased_state.action = RenderAction::destroyed;
        }
        result = resolve_building_asset(aliased_state, kind);
        result.state = state;
        result.reason =
            state.building_state == RenderBuildingState::foundation
            ? "foundation uses canonical construction selection: " +
                result.reason
            : "dying effect uses canonical destruction selection: " +
                result.reason;
        return result;
    }
    if (state.building_state == RenderBuildingState::damaged) {
        if (kind == BuildingKind::palisade_wall) {
            result.status =
                AssetCoverageStatus::intentional_procedural;
            result.reason =
                "Palisade Wall damage graphics have no drawable SLP; "
                "reviewed procedural contract in "
                "docs/assets/PROCEDURAL_BUILDING_ASSET_MAP.md";
            result.intentional_procedural = true;
            result.evidence_sources.push_back(
                "docs/assets/PROCEDURAL_BUILDING_ASSET_MAP.md:Palisade Wall"
            );
            return result;
        }
        RenderStateKey completed_state = state;
        completed_state.building_state = RenderBuildingState::completed;
        completed_state.damage_stage = 0;
        result = resolve_building_asset(completed_state, kind);
        result.state = state;
        if (state.damage_stage < 0 || state.damage_stage > 3) {
            result.status = AssetCoverageStatus::invalid_runtime_selection;
            result.reason = "building damage stage is outside 0..3";
            return result;
        }
        if (state.damage_stage == 0) {
            result.reason += "; damage below first overlay threshold";
            return result;
        }
        const auto records = canonical_building_damage_records(
            damage_kind, state.civilization
        );
        const BuildingDamageRecord& overlay = records[
            static_cast<std::size_t>(state.damage_stage - 1)
        ];
        if (overlay.flag == 2) {
            result.request = {};
            result.request.graphic_id = overlay.graphic_id;
            result.request.required_frame_count = 1;
            result.request.required_direction_count = 5;
            result.request.source_mapping =
                "canonical_building_damage_records:flag_2_replacement";
        } else {
            result.request.overlay_graphic_ids.push_back(overlay.graphic_id);
            result.request.source_mapping +=
                " + canonical_building_damage_records";
        }
        result.evidence_sources.push_back(
            "src/building_damage.cpp:"
            "canonical_building_damage_records"
        );
        if (overlay.flag != 0 && overlay.flag != 2) {
            result.status = AssetCoverageStatus::missing_mapping;
            result.reason =
                "selected damage record requires unsupported flagged "
                "overlay mode " + std::to_string(overlay.flag);
            result.intentional_procedural = false;
            return result;
        }
        result.reason += overlay.flag == 2
            ? "; canonical replacement damage body selected"
            : "; canonical damage overlay selected";
        return result;
    }
    if (kind == BuildingKind::farm) {
        result.status = AssetCoverageStatus::renderable;
        result.reason = state.building_state == RenderBuildingState::construction
            ? "farm texture body selected for progressive construction reveal"
            : "HD farm terrain textures selected";
        result.request.source_mapping =
            "Terrain/Textures/g_fm1_00_COLOR.png + "
            "Terrain/Textures/g_fm2_00_COLOR.png";
        return result;
    }
    if (state.building_state == RenderBuildingState::construction &&
        gate_construction_set(kind) != nullptr) {
        const GateConstructionSet* gate =
            gate_construction_set(kind);
        const int family =
            (state.civilization == Civilization::aztecs ||
             state.civilization == Civilization::mayans)
            ? 4
            : render_architecture_family(state.civilization);
        result.request.graphic_id = gate->family_graphic_roots[
            static_cast<std::size_t>(family)
        ];
        result.request.source_mapping =
            "canonical_gate_construction_sets";
        result.state.composite = true;
        result.status = AssetCoverageStatus::renderable;
        result.reason =
            "canonical family-specific gate construction root selected";
        return result;
    }
    if ((state.building_state == RenderBuildingState::foundation ||
         state.building_state == RenderBuildingState::construction) &&
        building_topology_slp_set(kind) != nullptr &&
        building_topology_slp_set(kind)
                ->construction_graphic_roots[0] >= 0) {
        const BuildingTopologySlpSet* topology =
            building_topology_slp_set(kind);
        if (state.architecture_family < 0 ||
            state.architecture_family >=
                static_cast<int>(
                    topology->construction_graphic_roots.size()
                )) {
            result.status =
                AssetCoverageStatus::invalid_runtime_selection;
            result.reason =
                "topology construction architecture family is invalid";
            return result;
        }
        result.request.graphic_id =
            topology->construction_graphic_roots[
                static_cast<std::size_t>(state.architecture_family)
            ];
        result.request.source_mapping =
            "canonical_building_topology_slp_sets";
        result.state.composite = true;
        result.status = AssetCoverageStatus::renderable;
        result.reason =
            "canonical topology-sensitive construction root selected";
        return result;
    }
    if (const BuildingStateRoot* state_root =
            building_state_root(kind, state.building_state);
        state_root != nullptr) {
        result.request.graphic_id = state_root->graphic_root;
        const BuildingConstructionContract* construction =
            building_construction_contract(kind);
        if (state.building_state == RenderBuildingState::construction &&
            construction != nullptr &&
            construction->body_mode ==
                ConstructionBodyMode::progressive_completed_body) {
            RenderStateKey completed = state;
            completed.building_state = RenderBuildingState::completed;
            completed.construction_stage = 4;
            const AssetResolution body = resolve_building_asset(completed, kind);
            result.request.construction_body_graphic_id =
                body.request.graphic_id;
            result.request.construction_body_slp_id = body.request.slp_id;
            result.request.source_mapping +=
                " + selected civilization/Age completed body";
            result.evidence_sources.push_back(
                "DAT construction progress + completed standing root"
            );
            result.reason =
                "exact completed body reveal plus footprint scaffold selected";
        }
        result.state.composite = true;
        result.status = AssetCoverageStatus::renderable;
        if (result.reason.empty()) {
            result.reason = "canonical building-state graphic root selected";
        }
        return result;
    }
    if (state.building_state == RenderBuildingState::completed) {
        if (const BuildingTopologySlpSet* topology =
                building_topology_slp_set(kind);
            topology != nullptr) {
            if (state.architecture_family < 0 ||
                state.architecture_family >=
                    static_cast<int>(topology->family_slps.size())) {
                result.status =
                    AssetCoverageStatus::invalid_runtime_selection;
                result.reason =
                    "topology building architecture family is invalid";
                return result;
            }
            if (state.animation_frame < 0 ||
                state.animation_frame >=
                    topology->reachable_frame_count) {
                result.status = AssetCoverageStatus::missing_frame;
                result.reason =
                    "topology building frame is outside reachable set";
                return result;
            }
            result.request.slp_id = topology->family_slps[
                static_cast<std::size_t>(state.architecture_family)
            ];
            result.request.required_frame_count =
                topology->asset_frame_count;
            result.request.required_direction_count = 1;
            result.request.shadow_slp_id =
                topology->explicit_shadow_slp_id;
            if (state.animation_frame == 2 &&
                topology->junction_overlay_slp_id) {
                result.request.composite_slp_ids.push_back(
                    *topology->junction_overlay_slp_id
                );
            }
            result.request.source_mapping =
                "canonical_building_topology_slp_sets";
            result.status = AssetCoverageStatus::renderable;
            result.reason =
                "canonical topology-sensitive building SLP selected";
            return result;
        }
        if (kind == BuildingKind::wonder) {
            const WonderCompositeSet* mapping =
                wonder_composite_set(state.civilization);
            if (mapping == nullptr) {
                result.status = AssetCoverageStatus::missing_mapping;
                result.reason =
                    "civilization has no canonical Wonder root";
                return result;
            }
            result.request.graphic_id = mapping->graphic_root;
            result.request.source_mapping =
                "canonical_wonder_composite_sets";
            result.state.composite = true;
            result.status = AssetCoverageStatus::renderable;
            result.reason =
                "canonical civilization-specific Wonder root selected";
            return result;
        }
        if (const BuildingDirectSlpSet* direct =
                building_direct_slp_set(kind);
            direct != nullptr) {
            const auto age = static_cast<std::size_t>(
                render_building_visual_age(kind, state.age)
            );
            if (age >= direct->slps.size() ||
                state.architecture_family < 0 ||
                state.architecture_family >=
                    static_cast<int>(direct->slps[age].size())) {
                result.status =
                    AssetCoverageStatus::invalid_runtime_selection;
                result.reason =
                    "direct building age or architecture family is invalid";
                return result;
            }
            result.request.slp_id = direct->slps[age][
                static_cast<std::size_t>(state.architecture_family)
            ];
            if (*result.request.slp_id < 0) {
                result.request.slp_id.reset();
                result.status = AssetCoverageStatus::missing_mapping;
                result.reason =
                    "building has no direct SLP for selected age/family";
                return result;
            }
            result.request.required_frame_count = 1;
            result.request.required_direction_count = 1;
            result.request.source_mapping =
                "canonical_building_direct_slp_sets";
            result.state.shadow = direct->static_shadow;
            result.status = AssetCoverageStatus::renderable;
            result.reason = "canonical direct building SLP selected";
            return result;
        }
        const BuildingCompositeSet* mapping =
            building_composite_set(kind);
        if (mapping != nullptr) {
            const auto age = static_cast<std::size_t>(
                render_building_composite_variant(
                    kind, state.age, state.upgrade_variant
                )
            );
            if (age >= mapping->graphic_roots.size() ||
                state.architecture_family < 0 ||
                state.architecture_family >=
                    static_cast<int>(
                        mapping->graphic_roots[age].size()
                    )) {
                result.status =
                    AssetCoverageStatus::invalid_runtime_selection;
                result.reason =
                    "building age or architecture family is invalid";
                return result;
            }
            const std::int16_t root = mapping->graphic_roots[age][
                static_cast<std::size_t>(state.architecture_family)
            ];
            if (root < 0) {
                result.status = AssetCoverageStatus::missing_mapping;
                result.reason =
                    "building has no completed root for selected age/family";
                return result;
            }
            result.request.graphic_id = root;
            result.request.source_mapping =
                "canonical_building_composite_sets";
            result.state.composite = true;
            result.status = AssetCoverageStatus::renderable;
            result.reason = mapping->composition_policy ==
                    CompositePolicy::complete_root
                ? "canonical complete root SLP selected without DAT deltas"
                : "canonical delta-only graphic graph selected";
            return result;
        }
    }
    result.status = AssetCoverageStatus::missing_mapping;
    result.reason = "building state has no canonical asset mapping";
    return result;
}

AssetResolution resolve_unit_asset(
    const RenderStateKey& state,
    UnitKind kind
) {
    AssetResolution result;
    result.state = state;
    result.request.source_mapping =
        "canonical_unit_animation_sets";
    result.evidence_sources = {
        "src/render_asset_coverage.cpp:canonical_unit_animation_sets",
        "runtime render action selection",
    };
    if (state.category != RenderObjectCategory::unit &&
        state.category != RenderObjectCategory::unit_death) {
        result.status = AssetCoverageStatus::invalid_runtime_selection;
        result.reason = "unit resolver received non-unit category";
        return result;
    }
    if (state.owner > EntityOwner::neutral_stable_id) {
        result.status = AssetCoverageStatus::unsupported_owner_state;
        result.reason = "owner stable ID is outside supported roster";
        return result;
    }
    if (state.action == RenderAction::dying ||
        state.action == RenderAction::destroyed) {
        if (const UnitDeathAnimationSet* death =
                unit_death_animation_set(kind);
            death != nullptr) {
            result.request.slp_id = death->slp;
            result.request.required_frame_count = death->frames;
            result.request.required_direction_count = 8;
            result.request.source_mapping =
                "canonical_unit_death_animation_sets";
            result.status = AssetCoverageStatus::renderable;
            result.reason =
                "canonical dedicated unit death animation selected";
            result.evidence_sources = {
                "src/render_asset_coverage.cpp:"
                "canonical_unit_death_animation_sets",
                "runtime unit death-effect selection",
            };
            return result;
        }
    }
    if (const NavalCompositeSet* naval =
            naval_composite_set(kind, state.action);
        naval != nullptr) {
        if (state.architecture_family < 0 ||
            state.architecture_family >=
                static_cast<int>(naval->graphic_roots.size())) {
            result.status = AssetCoverageStatus::invalid_runtime_selection;
            result.reason = "naval architecture family is outside 0..3";
            return result;
        }
        result.request.graphic_id = naval->graphic_roots[
            static_cast<std::size_t>(state.architecture_family)
        ];
        result.request.source_mapping =
            "canonical_naval_composite_sets";
        result.state.composite = true;
        result.status = AssetCoverageStatus::renderable;
        result.reason = "canonical naval composite root selected";
        result.evidence_sources = {
            "src/render_asset_coverage.cpp:canonical_naval_composite_sets",
            "DAT graphic composition",
        };
        return result;
    }
    const auto mapping = unit_animation_set(kind);
    const auto select_special = [&result, &state, kind]() {
        std::int32_t slp = -1;
        int frames = 0;
        if (kind == UnitKind::villager &&
            state.action == RenderAction::gathering &&
            !state.moving) {
            slp = 1528;
            frames = 15;
        } else if (kind == UnitKind::monk &&
                   state.action == RenderAction::converting) {
            slp = 768;
            frames = 10;
        } else if (kind == UnitKind::missionary &&
                   state.action == RenderAction::healing) {
            slp = 4869;
            frames = 14;
        } else if ((kind == UnitKind::packed_trebuchet ||
                    kind == UnitKind::trebuchet) &&
                   state.action == RenderAction::transforming) {
            slp = kind == UnitKind::packed_trebuchet ? 4573 : 1246;
            frames = 5;
        } else if (kind == UnitKind::monk &&
                   state.action == RenderAction::carrying_relic) {
            slp = state.moving ? 3831 : 3827;
            frames = 1;
        }
        if (slp < 0) return false;
        result.request.slp_id = slp;
        result.request.required_frame_count = frames;
        result.request.required_direction_count = frames == 1 ? 1 : 8;
        result.request.source_mapping = "canonical special unit action";
        result.status = AssetCoverageStatus::renderable;
        result.reason = "canonical special unit action mapping selected";
        return true;
    };
    if (select_special()) return result;
    if (!mapping) {
        result.status = AssetCoverageStatus::missing_mapping;
        result.reason = "unit kind has no canonical animation mapping";
        return result;
    }

    std::int32_t selected = -1;
    int frames = 0;
    switch (state.action) {
        case RenderAction::idle:
        case RenderAction::carrying_relic:
            selected = mapping->idle_slp;
            frames = mapping->idle_frames;
            break;
        case RenderAction::moving:
            selected = mapping->move_slp;
            frames = mapping->move_frames;
            break;
        case RenderAction::attacking:
            selected = mapping->attack_slp;
            frames = mapping->attack_frames;
            break;
        case RenderAction::dying:
        case RenderAction::destroyed:
            selected = mapping->death_slp;
            frames = mapping->death_frames;
            break;
        case RenderAction::gathering:
            if (kind == UnitKind::villager ||
                kind == UnitKind::fishing_ship) {
                selected = state.moving
                    ? mapping->move_slp
                    : mapping->idle_slp;
                frames = state.moving
                    ? mapping->move_frames
                    : mapping->idle_frames;
                result.request.source_mapping =
                    "canonical runtime gather fallback";
                break;
            }
            result.status = AssetCoverageStatus::missing_mapping;
            result.reason =
                "gathering state is unsupported for unit kind";
            return result;
        case RenderAction::working:
            if (kind == UnitKind::villager) {
                selected = state.moving
                    ? mapping->move_slp
                    : mapping->idle_slp;
                frames = state.moving
                    ? mapping->move_frames
                    : mapping->idle_frames;
                result.request.source_mapping =
                    "canonical runtime work fallback";
                break;
            }
            result.status = AssetCoverageStatus::missing_mapping;
            result.reason = "working state is unsupported for unit kind";
            return result;
        case RenderAction::healing:
            if (kind == UnitKind::monk) {
                selected = mapping->idle_slp;
                frames = mapping->idle_frames;
                result.request.source_mapping =
                    "canonical monk healing fallback";
                break;
            }
            result.status = AssetCoverageStatus::missing_mapping;
            result.reason = "healing state requires dedicated mapping";
            return result;
        case RenderAction::converting:
            if (kind == UnitKind::missionary) {
                selected = mapping->attack_slp;
                frames = mapping->attack_frames;
                result.request.source_mapping =
                    "canonical missionary conversion fallback";
                break;
            }
            result.status = AssetCoverageStatus::missing_mapping;
            result.reason = "conversion state requires dedicated mapping";
            return result;
        case RenderAction::transforming:
            result.status = AssetCoverageStatus::missing_mapping;
            result.reason =
                "special action requires dedicated canonical mapping";
            return result;
    }
    if (selected < 0 || frames <= 0) {
        result.status = AssetCoverageStatus::missing_mapping;
        result.reason =
            "canonical unit record has no asset for selected action";
        return result;
    }
    result.request.slp_id = selected;
    result.request.required_frame_count = frames;
    result.request.required_direction_count = 8;
    result.status = AssetCoverageStatus::renderable;
    result.reason = "canonical unit animation mapping selected";
    return result;
}

AssetResolution resolve_projectile_asset(
    const RenderStateKey& state,
    ProjectileAssetKind kind
) {
    AssetResolution result;
    result.state = state;
    result.request.source_mapping =
        "canonical_projectile_asset_bindings";
    result.evidence_sources = {
        "src/projectile_catalog.cpp:"
        "canonical_projectile_asset_bindings",
        "runtime projectile and impact selection",
    };
    const auto bindings = canonical_projectile_asset_bindings();
    const auto found = std::ranges::find(
        bindings, kind, &ProjectileAssetBinding::kind
    );
    if (found == bindings.end()) {
        result.status = AssetCoverageStatus::missing_mapping;
        result.reason = "projectile kind has no canonical asset binding";
        return result;
    }
    const ProjectileAssetBinding& binding = *found;
    if (state.category == RenderObjectCategory::impact) {
        if (!binding.impact_graphic || !binding.impact_slp_id ||
            !binding.impact_frame_count) {
            result.status = AssetCoverageStatus::missing_mapping;
            result.reason =
                "projectile kind has no canonical impact animation";
            return result;
        }
        result.request.graphic_id = *binding.impact_graphic;
        result.request.slp_id = *binding.impact_slp_id;
        result.request.required_frame_count =
            *binding.impact_frame_count;
        result.request.required_direction_count = 1;
        result.status = AssetCoverageStatus::renderable;
        result.reason = "canonical impact animation selected";
        return result;
    }
    if (state.category != RenderObjectCategory::projectile) {
        result.status = AssetCoverageStatus::invalid_runtime_selection;
        result.reason =
            "projectile resolver received unsupported object category";
        return result;
    }
    if (state.shadow) {
        if (!binding.shadow_graphic || !binding.shadow_slp_id) {
            result.status = AssetCoverageStatus::missing_shadow;
            result.reason =
                "projectile binding has no exact linked shadow";
            return result;
        }
        result.request.graphic_id = *binding.shadow_graphic;
        result.request.slp_id = *binding.shadow_slp_id;
        result.request.shadow_slp_id = *binding.shadow_slp_id;
        result.status = AssetCoverageStatus::renderable;
        result.reason = "canonical linked projectile shadow selected";
        return result;
    }
    result.request.graphic_id = binding.root_graphic;
    result.request.slp_id = binding.slp_id;
    result.request.shadow_slp_id = binding.shadow_slp_id;
    result.request.required_frame_count = binding.frame_count;
    result.request.required_direction_count = binding.angle_count;
    if (!binding.direction_mapping_proved) {
        result.status = AssetCoverageStatus::intentional_procedural;
        result.reason =
            "projectile directional transform is explicitly unproved";
        result.intentional_procedural = true;
        return result;
    }
    result.status = AssetCoverageStatus::renderable;
    result.reason = "canonical projectile animation selected";
    return result;
}

AssetResolution resolve_resource_asset(
    const RenderStateKey& state,
    ResourceRenderKind kind
) {
    AssetResolution result;
    result.state = state;
    result.request.source_mapping = "canonical_resource_asset_sets";
    result.evidence_sources = {
        "src/render_asset_coverage.cpp:canonical_resource_asset_sets",
        "runtime terrain-resource selection",
    };
    if (state.category != RenderObjectCategory::resource) {
        result.status = AssetCoverageStatus::invalid_runtime_selection;
        result.reason = "resource resolver received non-resource category";
        return result;
    }
    const ResourceAssetSet* mapping = resource_asset_set(kind);
    if (mapping == nullptr) {
        result.status = AssetCoverageStatus::missing_mapping;
        result.reason = "resource kind has no canonical asset mapping";
        return result;
    }
    if (state.animation_frame < 0 ||
        state.animation_frame >= mapping->frame_count) {
        result.status = AssetCoverageStatus::missing_frame;
        result.reason = "resource depletion frame is outside mapping";
        return result;
    }
    result.request.slp_id = mapping->slp_id;
    result.request.required_frame_count = mapping->frame_count;
    result.request.required_direction_count = 1;
    result.status = AssetCoverageStatus::renderable;
    result.reason = "canonical resource depletion frame selected";
    return result;
}

RenderAction render_action_for(const Unit& unit) {
    if (unit.trebuchet_transform_ticks_remaining > 0) {
        return RenderAction::transforming;
    }
    if (unit.carrying_relic) return RenderAction::carrying_relic;
    if (unit.conversion_target_id != 0) return RenderAction::converting;
    if (unit.healing_target_id != 0) return RenderAction::healing;
    if (unit.attack_target_id != 0 || unit.attacking_ground) {
        return RenderAction::attacking;
    }
    if (unit.resource_unit_id != 0 || unit.has_resource_target) {
        return RenderAction::gathering;
    }
    if (unit.repair_target_id != 0 || unit.resource_building_id != 0) {
        return RenderAction::working;
    }
    return RenderAction::idle;
}

int render_unit_world_depth(
    const Unit& unit,
    std::span<const Building> buildings
) {
    const int unit_depth = unit.position.x + unit.position.y;
    if (unit.attack_target_id == 0 ||
        !unit.attack_target_is_building) {
        return unit_depth;
    }
    const auto target = std::ranges::find(
        buildings, unit.attack_target_id, &Building::id
    );
    if (target == buildings.end()) return unit_depth;

    const BuildingRules& rules = rules_for(target->kind);
    const int maximum_x =
        target->position.x + rules.footprint_width - 1;
    const int maximum_y =
        target->position.y + rules.footprint_height - 1;
    const int x_distance = unit.position.x < target->position.x
        ? target->position.x - unit.position.x
        : unit.position.x > maximum_x
            ? unit.position.x - maximum_x
            : 0;
    const int y_distance = unit.position.y < target->position.y
        ? target->position.y - unit.position.y
        : unit.position.y > maximum_y
            ? unit.position.y - maximum_y
            : 0;
    if (std::max(x_distance, y_distance) > 1) return unit_depth;

    return std::max(
        unit_depth,
        target->position.x + target->position.y
    );
}

bool render_unit_is_interpolating(
    const Simulation& simulation,
    const Unit& unit
) {
    return simulation.tick_number() > 0 &&
        unit.render_subtile_initialized &&
        unit.render_previous_subtile != unit.render_current_subtile;
}

RenderUnitElevationEndpoints render_unit_elevation_endpoints(
    const Simulation& simulation,
    const Unit& unit
) {
    return {
        simulation.render_previous_elevation_position(unit),
        simulation.render_current_elevation_position(unit),
    };
}

RenderAction render_action_for(
    const Simulation& simulation,
    const Unit& unit
) {
    RenderAction direct = render_action_for(unit);
    if (direct == RenderAction::idle &&
        render_unit_is_interpolating(simulation, unit)) {
        direct = RenderAction::moving;
    }
    if (direct != RenderAction::idle &&
        direct != RenderAction::moving) {
        return direct;
    }
    const bool constructing = std::ranges::any_of(
        simulation.buildings(),
        [&unit](const Building& building) {
            return std::ranges::find(
                building.builder_ids, unit.id
            ) != building.builder_ids.end();
        }
    );
    return constructing ? RenderAction::working : direct;
}

RenderActionDetail render_action_detail_for(
    const Simulation& simulation,
    const Unit& unit
) {
    const RenderAction action = render_action_for(simulation, unit);
    if (action == RenderAction::working) {
        if (unit.repair_target_id != 0) {
            return RenderActionDetail::repair;
        }
        return RenderActionDetail::construction;
    }
    if (action != RenderAction::gathering) {
        return RenderActionDetail::none;
    }
    if (unit.resource_unit_id != 0) {
        const auto target = std::ranges::find_if(
            simulation.units(),
            [&unit](const Unit& candidate) {
                return candidate.id == unit.resource_unit_id;
            }
        );
        if (target != simulation.units().end() &&
            is_animal(target->kind)) {
            return RenderActionDetail::animal_resource;
        }
    }
    if (unit.resource_building_id != 0) {
        const auto target = std::ranges::find_if(
            simulation.buildings(),
            [&unit](const Building& candidate) {
                return candidate.id == unit.resource_building_id;
            }
        );
        if (target != simulation.buildings().end()) {
            return target->kind == BuildingKind::fish_trap
                ? RenderActionDetail::fish_trap
                : RenderActionDetail::farm;
        }
    }
    return unit.kind == UnitKind::fishing_ship
        ? RenderActionDetail::fish
        : RenderActionDetail::terrain_resource;
}

RenderBuildingState render_state_for(
    const Building& building,
    int maximum_hit_points
) {
    if (!building.completed()) {
        return building.construction_ticks_remaining > 0
            ? RenderBuildingState::construction
            : RenderBuildingState::foundation;
    }
    if (building.hit_points <= 0) return RenderBuildingState::destroyed;
    if (building.hit_points < maximum_hit_points) {
        return RenderBuildingState::damaged;
    }
    return RenderBuildingState::completed;
}

int render_construction_stage(
    const Building& building,
    int construction_ticks
) {
    if (building.completed() || construction_ticks <= 0) return 4;
    const int elapsed = std::clamp(
        construction_ticks - building.construction_ticks_remaining,
        0,
        construction_ticks
    );
    return std::clamp((elapsed * 4) / construction_ticks, 0, 3);
}

int render_construction_progress_basis_points(
    const Building& building,
    int construction_ticks
) {
    if (building.completed()) return 10000;
    if (construction_ticks <= 0) return 0;
    const int elapsed = std::clamp(
        construction_ticks - building.construction_ticks_remaining,
        0,
        construction_ticks
    );
    return static_cast<int>(
        static_cast<std::int64_t>(elapsed) * 10000 / construction_ticks
    );
}

int render_building_damage_reference_hit_points(
    const Building& building,
    int construction_ticks,
    int maximum_hit_points
) {
    maximum_hit_points = std::max(maximum_hit_points, 1);
    if (building.completed()) return maximum_hit_points;
    return std::max(
        1,
        maximum_hit_points * render_construction_progress_basis_points(
            building, construction_ticks
        ) / 10000
    );
}

std::span<const BuildingConstructionContract>
canonical_building_construction_contracts() {
    return building_construction_contracts;
}

const BuildingConstructionContract* building_construction_contract(
    BuildingKind kind
) {
    if (kind == BuildingKind::guard_tower || kind == BuildingKind::keep) {
        kind = BuildingKind::watch_tower;
    } else if (kind == BuildingKind::fortified_wall) {
        kind = BuildingKind::stone_wall;
    } else if (kind == BuildingKind::fortified_gate_x) {
        kind = BuildingKind::stone_gate_x;
    } else if (kind == BuildingKind::fortified_gate_y) {
        kind = BuildingKind::stone_gate_y;
    }
    const auto found = std::ranges::find(
        building_construction_contracts, kind,
        &BuildingConstructionContract::kind
    );
    return found == std::end(building_construction_contracts)
        ? nullptr : &*found;
}

int render_damage_stage(int hit_points, int maximum_hit_points) {
    if (maximum_hit_points <= 0 || hit_points <= 0) return 3;
    const int damage_percent =
        100 - hit_points * 100 / maximum_hit_points;
    if (damage_percent > 75) return 3;
    if (damage_percent > 50) return 2;
    if (damage_percent > 25) return 1;
    return 0;
}

int render_architecture_family(Civilization civilization) {
    if (civilization == Civilization::teutons ||
        civilization == Civilization::goths ||
        civilization == Civilization::vikings) {
        return 1;
    }
    if (civilization == Civilization::byzantines ||
        civilization == Civilization::persians ||
        civilization == Civilization::saracens) {
        return 2;
    }
    if (civilization == Civilization::japanese ||
        civilization == Civilization::chinese) {
        return 3;
    }
    return 0;
}

int render_building_architecture_family(
    BuildingKind kind,
    Civilization civilization
) {
    if ((building_composite_set(kind) != nullptr ||
         building_direct_slp_set(kind) != nullptr ||
         kind == BuildingKind::stone_wall) &&
        (civilization == Civilization::aztecs ||
         civilization == Civilization::mayans)) {
        return 4;
    }
    return render_architecture_family(civilization);
}

int render_building_topology_frame(
    const Simulation& simulation,
    const Building& building
) {
    if (building_topology_slp_set(building.kind) == nullptr) return 0;
    const auto adjacent = [&simulation, &building](
        int offset_x,
        int offset_y
    ) {
        return std::ranges::any_of(
            simulation.buildings(),
            [&building, offset_x, offset_y](
                const Building& candidate
            ) {
                return candidate.kind == building.kind &&
                    candidate.position.x ==
                        building.position.x + offset_x &&
                    candidate.position.y ==
                        building.position.y + offset_y;
            }
        );
    };
    const bool connected_x = adjacent(-1, 0) || adjacent(1, 0);
    const bool connected_y = adjacent(0, -1) || adjacent(0, 1);
    return connected_x && !connected_y
        ? 0
        : connected_y && !connected_x
            ? 1
            : 2;
}

Age render_building_visual_age(
    BuildingKind kind,
    Age current_age
) {
    // Town Centers exist from Dark Age even though constructing additional
    // ones has a later gameplay prerequisite. Visual selection follows
    // current player Age, not construction availability.
    if (kind == BuildingKind::town_center) {
        return current_age;
    }
    // Scenario placement can bypass build prerequisites. These completed
    // objects have no earlier replacement record; select their documented
    // minimum visual variant explicitly rather than scanning later slots.
    if ((kind == BuildingKind::stone_gate_x ||
         kind == BuildingKind::stone_gate_y) &&
        current_age < Age::castle) {
        return Age::castle;
    }
    return static_cast<int>(current_age) <
            static_cast<int>(rules_for(kind).minimum_age)
        ? rules_for(kind).minimum_age
        : current_age;
}

int render_building_upgrade_variant(
    const Simulation&,
    const Building& building
) {
    if (building.kind == BuildingKind::keep) return 2;
    if (building.kind == BuildingKind::guard_tower) return 1;
    return 0;
}

int render_building_composite_variant(
    BuildingKind kind,
    Age current_age,
    int upgrade_variant
) {
    if (kind == BuildingKind::watch_tower ||
        kind == BuildingKind::guard_tower ||
        kind == BuildingKind::keep) {
        return std::clamp(upgrade_variant, 0, 2);
    }
    int age = static_cast<int>(
        render_building_visual_age(kind, current_age)
    );
    return std::clamp(age, 0, 3);
}

std::optional<std::size_t> render_component_animation_frame(
    std::size_t frames_per_angle,
    std::uint64_t animation_tick,
    bool active
) {
    if (frames_per_angle == 0) return std::nullopt;
    return active && frames_per_angle > 1
        ? std::optional<std::size_t>{
              static_cast<std::size_t>(animation_tick / 2) %
                  frames_per_angle}
        : std::optional<std::size_t>{0};
}

std::optional<std::size_t> render_component_animation_frame_at_time(
    std::size_t frames_per_angle,
    std::uint64_t elapsed_milliseconds,
    float frame_rate_seconds,
    float replay_delay_seconds,
    bool active
) {
    if (frames_per_angle == 0) return std::nullopt;
    if (!active || frames_per_angle == 1) return 0;
    // FUN_004eb870 clamps graphic +0x68 to this exact float value.
    constexpr float minimum_frame_rate = 0.001F;
    const double frame_milliseconds =
        static_cast<double>(std::max(frame_rate_seconds, minimum_frame_rate)) *
        1000.0;
    const double replay_milliseconds =
        static_cast<double>(std::max(replay_delay_seconds, 0.0F)) * 1000.0;
    const double cycle_milliseconds =
        frame_milliseconds * static_cast<double>(frames_per_angle) +
        replay_milliseconds;
    const double cycle_position = std::fmod(
        static_cast<double>(elapsed_milliseconds), cycle_milliseconds
    );
    return std::min(
        static_cast<std::size_t>(cycle_position / frame_milliseconds),
        frames_per_angle - 1
    );
}

std::string render_unit_kind_name(UnitKind kind) {
    static constexpr std::string_view names[] = {
        "villager", "knight", "archer", "scout_cavalry", "militia",
        "spearman", "battering_ram", "skirmisher", "mangonel",
        "man_at_arms", "crossbowman", "pikeman", "long_swordsman",
        "cavalier", "paladin", "light_cavalry", "hussar",
        "two_handed_swordsman", "champion", "arbalester",
        "elite_skirmisher", "sheep", "deer", "boar", "monk", "relic",
        "trade_cart", "fishing_ship", "galley", "war_galley", "galleon",
        "transport_ship", "fire_ship", "fast_fire_ship", "demolition_ship",
        "heavy_demolition_ship", "cannon_galleon", "elite_cannon_galleon",
        "longboat", "elite_longboat", "turtle_ship", "elite_turtle_ship",
        "longbowman", "elite_longbowman", "throwing_axeman",
        "elite_throwing_axeman", "huskarl", "elite_huskarl",
        "teutonic_knight", "elite_teutonic_knight", "samurai",
        "elite_samurai", "chu_ko_nu", "elite_chu_ko_nu", "cataphract",
        "elite_cataphract", "war_elephant", "elite_war_elephant",
        "mameluke", "elite_mameluke", "janissary", "elite_janissary",
        "berserk", "elite_berserk", "mangudai", "elite_mangudai",
        "jaguar_warrior", "elite_jaguar_warrior", "plumed_archer",
        "elite_plumed_archer", "conquistador", "elite_conquistador",
        "tarkan", "elite_tarkan", "eagle_warrior", "elite_eagle_warrior",
        "scorpion", "heavy_scorpion", "onager", "siege_onager",
        "packed_trebuchet", "trebuchet", "cavalry_archer",
        "heavy_cavalry_archer", "camel_rider", "heavy_camel", "capped_ram",
        "siege_ram", "halberdier", "hand_cannoneer", "bombard_cannon",
        "petard", "missionary", "trade_cog", "woad_raider",
        "elite_woad_raider",
        "king",
    };
    const auto index = static_cast<std::size_t>(kind);
    if (index >= std::size(names)) {
        throw std::invalid_argument{"invalid unit kind"};
    }
    return std::string{names[index]};
}

std::string render_building_kind_name(BuildingKind kind) {
    static constexpr std::string_view names[] = {
        "town_center", "barracks", "archery_range", "house", "mill",
        "lumber_camp", "mining_camp", "farm", "stable", "blacksmith",
        "castle", "university", "siege_workshop", "palisade_wall",
        "watch_tower", "stone_wall", "palisade_gate_x", "palisade_gate_y",
        "stone_gate_x", "stone_gate_y", "monastery", "market", "dock",
        "bombard_tower", "fish_trap", "outpost", "wonder",
        "guard_tower", "keep", "fortified_wall", "fortified_gate_x",
        "fortified_gate_y",
    };
    const auto index = static_cast<std::size_t>(kind);
    if (index >= std::size(names)) {
        throw std::invalid_argument{"invalid building kind"};
    }
    return std::string{names[index]};
}

std::string asset_coverage_status_name(AssetCoverageStatus status) {
    switch (status) {
        case AssetCoverageStatus::renderable: return "renderable";
        case AssetCoverageStatus::intentional_procedural:
            return "intentional_procedural";
        case AssetCoverageStatus::missing_mapping: return "missing_mapping";
        case AssetCoverageStatus::missing_archive_resource:
            return "missing_archive_resource";
        case AssetCoverageStatus::invalid_dat_reference:
            return "invalid_dat_reference";
        case AssetCoverageStatus::decode_failure: return "decode_failure";
        case AssetCoverageStatus::missing_frame: return "missing_frame";
        case AssetCoverageStatus::missing_player_variant:
            return "missing_player_variant";
        case AssetCoverageStatus::missing_composite_part:
            return "missing_composite_part";
        case AssetCoverageStatus::missing_shadow: return "missing_shadow";
        case AssetCoverageStatus::invalid_runtime_selection:
            return "invalid_runtime_selection";
        case AssetCoverageStatus::unsupported_owner_state:
            return "unsupported_owner_state";
        case AssetCoverageStatus::renderer_failure:
            return "renderer_failure";
    }
    throw std::logic_error{"unhandled asset coverage status"};
}

std::string render_action_name(RenderAction action) {
    return action_name(action);
}

std::string render_action_detail_name(RenderActionDetail detail) {
    return action_detail_name(detail);
}

RuntimeFallbackTelemetry::RuntimeFallbackTelemetry(
    std::optional<std::filesystem::path> output_path
) : output_path_(std::move(output_path)) {}

bool RuntimeFallbackTelemetry::enabled() const noexcept {
    return output_path_.has_value();
}

bool RuntimeFallbackTelemetry::record(RuntimeFallbackEvent event) {
    if (!enabled()) return false;
    const std::string key = event.state.stable_key();
    const bool inserted = events_.try_emplace(key, std::move(event)).second;
    if (inserted) write_report();
    return inserted;
}

void RuntimeFallbackTelemetry::write_report() const {
    if (!output_path_) return;
    const std::filesystem::path temporary =
        output_path_->string() + ".tmp";
    std::ofstream output{temporary, std::ios::binary | std::ios::trunc};
    if (!output) {
        throw std::runtime_error{
            "cannot write runtime render fallback report: " +
            temporary.string()
        };
    }
    output << "{\n  \"schema\":\"aoe-runtime-render-fallback-v1\",\n"
           << "  \"events\":[";
    bool first = true;
    for (const auto& [stable_key, event] : events_) {
        if (!first) output << ',';
        first = false;
        output << "\n    {\"stable_render_state_key\":\""
               << json_escape(stable_key) << "\","
               << "\"entity_id\":" << event.entity_id << ',';
        write_state(output, event.state);
        output << ',';
        write_request(output, event.request);
        output << ",\"status\":\""
               << asset_coverage_status_name(event.status) << "\","
               << "\"reason\":\"" << json_escape(event.reason) << "\","
               << "\"simulation_tick\":" << event.simulation_tick << ','
               << "\"renderer_call_site\":\""
               << json_escape(event.renderer_call_site) << "\"}";
    }
    output << "\n  ]\n}\n";
    output.close();
    if (!output) {
        throw std::runtime_error{
            "cannot finish runtime render fallback report: " +
            temporary.string()
        };
    }
    std::filesystem::rename(temporary, *output_path_);
}

const std::map<std::string, RuntimeFallbackEvent>&
RuntimeFallbackTelemetry::events() const noexcept {
    return events_;
}

RuntimeFallbackTelemetry& runtime_fallback_telemetry() {
    static RuntimeFallbackTelemetry telemetry = [] {
        const char* value = std::getenv("AOE_RENDER_FALLBACK_REPORT");
        return value != nullptr && value[0] != '\0'
            ? RuntimeFallbackTelemetry{
                  std::filesystem::path{value}
              }
            : RuntimeFallbackTelemetry{};
    }();
    return telemetry;
}

}  // namespace aoe
