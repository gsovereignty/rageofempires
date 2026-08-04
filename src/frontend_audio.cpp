#include "aoe/frontend_audio.hpp"
#include "aoe/game_rules.hpp"

namespace aoe {

namespace {

bool common_land_voice(UnitKind kind) noexcept {
    return is_infantry(kind) ||
        kind == UnitKind::archer ||
        kind == UnitKind::crossbowman ||
        kind == UnitKind::arbalester ||
        kind == UnitKind::skirmisher ||
        kind == UnitKind::elite_skirmisher ||
        kind == UnitKind::battering_ram ||
        kind == UnitKind::capped_ram ||
        kind == UnitKind::siege_ram ||
        kind == UnitKind::longbowman ||
        kind == UnitKind::elite_longbowman ||
        kind == UnitKind::chu_ko_nu ||
        kind == UnitKind::elite_chu_ko_nu ||
        kind == UnitKind::mameluke ||
        kind == UnitKind::elite_mameluke ||
        kind == UnitKind::janissary ||
        kind == UnitKind::elite_janissary ||
        kind == UnitKind::berserk ||
        kind == UnitKind::elite_berserk ||
        kind == UnitKind::woad_raider ||
        kind == UnitKind::elite_woad_raider ||
        kind == UnitKind::plumed_archer ||
        kind == UnitKind::elite_plumed_archer ||
        kind == UnitKind::conquistador ||
        kind == UnitKind::elite_conquistador ||
        kind == UnitKind::eagle_warrior ||
        kind == UnitKind::elite_eagle_warrior ||
        kind == UnitKind::hand_cannoneer ||
        kind == UnitKind::bombard_cannon ||
        kind == UnitKind::petard;
}

bool common_cavalry_voice(UnitKind kind) noexcept {
    return kind == UnitKind::knight ||
        kind == UnitKind::cavalier ||
        kind == UnitKind::paladin ||
        kind == UnitKind::light_cavalry ||
        kind == UnitKind::hussar ||
        kind == UnitKind::cataphract ||
        kind == UnitKind::elite_cataphract ||
        kind == UnitKind::mangudai ||
        kind == UnitKind::elite_mangudai ||
        kind == UnitKind::tarkan ||
        kind == UnitKind::elite_tarkan ||
        kind == UnitKind::cavalry_archer ||
        kind == UnitKind::heavy_cavalry_archer;
}

}  // namespace

int accepted_command_sound(UnitKind kind) noexcept {
    if (common_land_voice(kind) && kind != UnitKind::petard) return 422;
    if (kind == UnitKind::villager) return 301;
    if (kind == UnitKind::scout_cavalry) return 474;
    if (common_cavalry_voice(kind)) {
        if (kind == UnitKind::cavalry_archer ||
            kind == UnitKind::heavy_cavalry_archer) return 415;
        if (kind == UnitKind::elite_mangudai) return 467;
        return 326;
    }
    if (kind == UnitKind::camel_rider ||
        kind == UnitKind::heavy_camel) return 326;
    if (kind == UnitKind::war_elephant ||
        kind == UnitKind::elite_war_elephant) return 483;
    if (kind == UnitKind::monk || kind == UnitKind::missionary) return 424;
    if (is_ship(kind)) return 340;
    if (kind == UnitKind::mangonel || kind == UnitKind::onager ||
        kind == UnitKind::siege_onager ||
        kind == UnitKind::scorpion ||
        kind == UnitKind::heavy_scorpion) return 476;
    if (kind == UnitKind::packed_trebuchet) return 484;
    if (kind == UnitKind::trebuchet) return 291;
    if (kind == UnitKind::trade_cart) return 306;
    if (kind == UnitKind::relic) return 30;
    if (kind == UnitKind::petard) return 421;
    return -1;
}

int selected_sound(UnitKind kind) noexcept {
    if (common_land_voice(kind)) return 420;
    if (kind == UnitKind::villager) return 303;
    if (kind == UnitKind::scout_cavalry ||
        common_cavalry_voice(kind)) return 325;
    if (kind == UnitKind::camel_rider ||
        kind == UnitKind::heavy_camel) return 430;
    if (kind == UnitKind::war_elephant ||
        kind == UnitKind::elite_war_elephant) return 477;
    if (kind == UnitKind::monk || kind == UnitKind::missionary) return 423;
    if (is_ship(kind)) return 339;
    if (kind == UnitKind::mangonel || kind == UnitKind::onager ||
        kind == UnitKind::siege_onager) return 489;
    if (kind == UnitKind::scorpion ||
        kind == UnitKind::heavy_scorpion) return 490;
    if (kind == UnitKind::packed_trebuchet ||
        kind == UnitKind::trebuchet) return 291;
    if (kind == UnitKind::relic) return 488;
    if (kind == UnitKind::trade_cart) return 305;
    if (kind == UnitKind::sheep) return 458;
    if (kind == UnitKind::deer) return 56;
    return -1;
}

int movement_sound(UnitKind kind) noexcept {
    if (common_land_voice(kind)) return 421;
    if (kind == UnitKind::villager) return 301;
    if (kind == UnitKind::scout_cavalry ||
        kind == UnitKind::elite_mangudai) return 467;
    if (common_cavalry_voice(kind)) {
        return (kind == UnitKind::cavalry_archer ||
                kind == UnitKind::heavy_cavalry_archer) ? 415 : 326;
    }
    if (kind == UnitKind::camel_rider ||
        kind == UnitKind::heavy_camel) return 326;
    if (kind == UnitKind::war_elephant ||
        kind == UnitKind::elite_war_elephant) return 483;
    if (kind == UnitKind::monk || kind == UnitKind::missionary) return 424;
    if (is_ship(kind)) return 340;
    if (kind == UnitKind::mangonel || kind == UnitKind::onager ||
        kind == UnitKind::siege_onager ||
        kind == UnitKind::scorpion ||
        kind == UnitKind::heavy_scorpion) return 476;
    if (kind == UnitKind::packed_trebuchet ||
        kind == UnitKind::trebuchet) return kind == UnitKind::packed_trebuchet
            ? 484 : 291;
    if (kind == UnitKind::trade_cart) return 306;
    if (kind == UnitKind::sheep) return 457;
    return -1;
}

int trained_sound(UnitKind kind) noexcept {
    if (kind == UnitKind::villager) return 317;
    if (kind == UnitKind::monk || kind == UnitKind::missionary) return 469;
    if (is_ship(kind)) return 338;
    if (kind == UnitKind::packed_trebuchet ||
        kind == UnitKind::trebuchet) return 291;
    if (kind == UnitKind::camel_rider ||
        kind == UnitKind::heavy_camel) return 430;
    if (kind == UnitKind::sheep || kind == UnitKind::deer ||
        kind == UnitKind::boar || kind == UnitKind::relic) return -1;
    if (kind == UnitKind::trade_cart) return 305;
    return 337;
}

int selected_sound(BuildingKind kind) noexcept {
    switch (kind) {
        case BuildingKind::town_center: return 17;
        case BuildingKind::barracks: return 2;
        case BuildingKind::archery_range: return 322;
        case BuildingKind::house: return 403;
        case BuildingKind::mill: return 15;
        case BuildingKind::lumber_camp: return 446;
        case BuildingKind::mining_camp: return 447;
        case BuildingKind::farm: return 416;
        case BuildingKind::stable: return 19;
        case BuildingKind::blacksmith: return 1;
        case BuildingKind::castle: return 4;
        case BuildingKind::university: return 21;
        case BuildingKind::siege_workshop: return 18;
        case BuildingKind::palisade_wall:
        case BuildingKind::stone_wall:
        case BuildingKind::stone_gate_x:
        case BuildingKind::stone_gate_y:
        case BuildingKind::fortified_wall:
        case BuildingKind::fortified_gate_x:
        case BuildingKind::fortified_gate_y:
            return 412;
        case BuildingKind::watch_tower:
        case BuildingKind::guard_tower:
        case BuildingKind::keep:
        case BuildingKind::bombard_tower:
        case BuildingKind::outpost:
            return 23;
        case BuildingKind::palisade_gate_x:
        case BuildingKind::palisade_gate_y:
            return 9;
        case BuildingKind::monastery: return 3;
        case BuildingKind::market: return 16;
        case BuildingKind::dock: return 5;
        case BuildingKind::fish_trap: return 460;
        case BuildingKind::wonder: return 383;
    }
    return -1;
}

}  // namespace aoe
