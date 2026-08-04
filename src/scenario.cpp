#include "aoe/scenario.hpp"
#include "aoe/format_versions.hpp"

#include "aoe/game_rules.hpp"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>

namespace aoe {
namespace {

std::runtime_error line_error(int line, const std::string& message) {
    return std::runtime_error(
        "scenario line " + std::to_string(line) + ": " + message
    );
}

bool valid_scenario_text(const std::string& text) {
    return !text.empty() && text.size() <= 4096 &&
        text.find_first_of("\r\n") == std::string::npos;
}

Terrain parse_terrain(const std::string& value, int line) {
    if (value == "grass") return Terrain::grass;
    if (value == "water") return Terrain::water;
    if (value == "beach") return Terrain::beach;
    if (value == "shallows") return Terrain::shallows;
    if (value == "forest") return Terrain::forest;
    if (value == "berries") return Terrain::berry_bush;
    if (value == "gold") return Terrain::gold_mine;
    if (value == "stone") return Terrain::stone_mine;
    if (value == "fish") return Terrain::fish;
    throw line_error(line, "unknown terrain " + value);
}

Player parse_player(const std::string& value, int line) {
    if (value == "blue") return Player::blue;
    if (value == "red") return Player::red;
    if (value == "neutral") return Player::neutral;
    throw line_error(line, "unknown player " + value);
}

PlayerSlotId parse_player_slot(int value, int line, bool allow_neutral) {
    const auto slot = decode_player_slot_id(value);
    if (!slot || (!allow_neutral && slot->is_neutral())) {
        throw line_error(line, "invalid player slot");
    }
    return *slot;
}

EntityOwner parse_entity_owner(
    const std::string& value,
    int line,
    int scenario_version
) {
    if (scenario_version < 65) {
        return EntityOwner{parse_player(value, line)};
    }
    if (value == "blue" || value == "red" || value == "neutral") {
        return EntityOwner{parse_player(value, line)};
    }
    int stable_id{};
    std::size_t consumed{};
    try {
        stable_id = std::stoi(value, &consumed);
    } catch (const std::exception&) {
        throw line_error(line, "invalid entity owner");
    }
    const auto owner = EntityOwner::from_stable_id(stable_id);
    if (consumed != value.size() || !owner) {
        throw line_error(line, "invalid entity owner");
    }
    return *owner;
}

Age parse_age(const std::string& value, int line) {
    if (value == "dark") return Age::dark;
    if (value == "feudal") return Age::feudal;
    if (value == "castle") return Age::castle;
    if (value == "imperial") return Age::imperial;
    throw line_error(line, "unknown age " + value);
}

Diplomacy parse_diplomacy(const std::string& value, int line) {
    if (value == "ally") return Diplomacy::ally;
    if (value == "neutral") return Diplomacy::neutral;
    if (value == "enemy") return Diplomacy::enemy;
    throw line_error(line, "unknown diplomacy " + value);
}

Civilization parse_civilization(const std::string& value, int line) {
    if (value == "generic") return Civilization::generic;
    if (value == "britons") return Civilization::britons;
    if (value == "franks") return Civilization::franks;
    if (value == "teutons") return Civilization::teutons;
    if (value == "goths") return Civilization::goths;
    if (value == "celts") return Civilization::celts;
    if (value == "vikings") return Civilization::vikings;
    if (value == "byzantines") return Civilization::byzantines;
    if (value == "japanese") return Civilization::japanese;
    if (value == "chinese") return Civilization::chinese;
    if (value == "persians") return Civilization::persians;
    if (value == "saracens") return Civilization::saracens;
    if (value == "turks") return Civilization::turks;
    if (value == "mongols") return Civilization::mongols;
    if (value == "spanish") return Civilization::spanish;
    if (value == "huns") return Civilization::huns;
    if (value == "koreans") return Civilization::koreans;
    if (value == "aztecs") return Civilization::aztecs;
    if (value == "mayans") return Civilization::mayans;
    throw line_error(line, "unknown civilization " + value);
}

std::string age_name(Age age) {
    switch (age) {
        case Age::dark: return "dark";
        case Age::feudal: return "feudal";
        case Age::castle: return "castle";
        case Age::imperial: return "imperial";
    }
    return "dark";
}

Technology parse_technology(const std::string& value, int line) {
    if (value == "wheelbarrow") return Technology::wheelbarrow;
    if (value == "fletching") return Technology::fletching;
    if (value == "forging") return Technology::forging;
    if (value == "murder_holes") return Technology::murder_holes;
    if (value == "man_at_arms") return Technology::man_at_arms;
    if (value == "crossbowman") return Technology::crossbowman;
    if (value == "pikeman") return Technology::pikeman;
    if (value == "long_swordsman") return Technology::long_swordsman;
    if (value == "loom") return Technology::loom;
    if (value == "double_bit_axe") return Technology::double_bit_axe;
    if (value == "horse_collar") return Technology::horse_collar;
    if (value == "fortified_wall") return Technology::fortified_wall;
    if (value == "guard_tower") return Technology::guard_tower;
    if (value == "keep") return Technology::keep;
    if (value == "bodkin_arrow") return Technology::bodkin_arrow;
    if (value == "bracer") return Technology::bracer;
    if (value == "iron_casting") return Technology::iron_casting;
    if (value == "blast_furnace") return Technology::blast_furnace;
    if (value == "scale_mail_armor") return Technology::scale_mail_armor;
    if (value == "chain_mail_armor") return Technology::chain_mail_armor;
    if (value == "plate_mail_armor") return Technology::plate_mail_armor;
    if (value == "scale_barding_armor") {
        return Technology::scale_barding_armor;
    }
    if (value == "chain_barding_armor") {
        return Technology::chain_barding_armor;
    }
    if (value == "plate_barding_armor") {
        return Technology::plate_barding_armor;
    }
    if (value == "padded_archer_armor") {
        return Technology::padded_archer_armor;
    }
    if (value == "leather_archer_armor") {
        return Technology::leather_archer_armor;
    }
    if (value == "ring_archer_armor") {
        return Technology::ring_archer_armor;
    }
    if (value == "bloodlines") return Technology::bloodlines;
    if (value == "husbandry") return Technology::husbandry;
    if (value == "cavalier") return Technology::cavalier;
    if (value == "paladin") return Technology::paladin;
    if (value == "light_cavalry") return Technology::light_cavalry;
    if (value == "hussar") return Technology::hussar;
    if (value == "two_handed_swordsman") {
        return Technology::two_handed_swordsman;
    }
    if (value == "champion") return Technology::champion;
    if (value == "arbalester") return Technology::arbalester;
    if (value == "elite_skirmisher") return Technology::elite_skirmisher;
    if (value == "war_galley") return Technology::war_galley;
    if (value == "galleon") return Technology::galleon;
    if (value == "fast_fire_ship") return Technology::fast_fire_ship;
    if (value == "heavy_demolition_ship") {
        return Technology::heavy_demolition_ship;
    }
    if (value == "cannon_galleon") return Technology::cannon_galleon;
    if (value == "elite_cannon_galleon") {
        return Technology::elite_cannon_galleon;
    }
    if (value == "careening") return Technology::careening;
    if (value == "dry_dock") return Technology::dry_dock;
    if (value == "shipwright") return Technology::shipwright;
    if (value == "longboat") return Technology::longboat;
    if (value == "elite_longboat") return Technology::elite_longboat;
    if (value == "turtle_ship") return Technology::turtle_ship;
    if (value == "elite_turtle_ship") {
        return Technology::elite_turtle_ship;
    }
    if (value == "longbowman") return Technology::longbowman;
    if (value == "elite_longbowman") {
        return Technology::elite_longbowman;
    }
    if (value == "throwing_axeman") return Technology::throwing_axeman;
    if (value == "elite_throwing_axeman") {
        return Technology::elite_throwing_axeman;
    }
    if (value == "huskarl") return Technology::huskarl;
    if (value == "elite_huskarl") return Technology::elite_huskarl;
    if (value == "teutonic_knight") return Technology::teutonic_knight;
    if (value == "elite_teutonic_knight") {
        return Technology::elite_teutonic_knight;
    }
    if (value == "samurai") return Technology::samurai;
    if (value == "elite_samurai") return Technology::elite_samurai;
    if (value == "chu_ko_nu") return Technology::chu_ko_nu;
    if (value == "elite_chu_ko_nu") return Technology::elite_chu_ko_nu;
    if (value == "cataphract") return Technology::cataphract;
    if (value == "elite_cataphract") return Technology::elite_cataphract;
    if (value == "war_elephant") return Technology::war_elephant;
    if (value == "elite_war_elephant") {
        return Technology::elite_war_elephant;
    }
    if (value == "mameluke") return Technology::mameluke;
    if (value == "elite_mameluke") return Technology::elite_mameluke;
    if (value == "janissary") return Technology::janissary;
    if (value == "elite_janissary") return Technology::elite_janissary;
    if (value == "berserk") return Technology::berserk;
    if (value == "elite_berserk") return Technology::elite_berserk;
    if (value == "mangudai") return Technology::mangudai;
    if (value == "elite_mangudai") return Technology::elite_mangudai;
    if (value == "berserkergang") return Technology::berserkergang;
    if (value == "jaguar_warrior") return Technology::jaguar_warrior;
    if (value == "elite_jaguar_warrior") {
        return Technology::elite_jaguar_warrior;
    }
    if (value == "plumed_archer") return Technology::plumed_archer;
    if (value == "elite_plumed_archer") {
        return Technology::elite_plumed_archer;
    }
    if (value == "conquistador") return Technology::conquistador;
    if (value == "elite_conquistador") {
        return Technology::elite_conquistador;
    }
    if (value == "tarkan") return Technology::tarkan;
    if (value == "elite_tarkan") return Technology::elite_tarkan;
    if (value == "woad_raider") return Technology::woad_raider;
    if (value == "elite_woad_raider") {
        return Technology::elite_woad_raider;
    }
    if (value == "yeomen") return Technology::yeomen;
    if (value == "bearded_axe") return Technology::bearded_axe;
    if (value == "anarchy") return Technology::anarchy;
    if (value == "crenellations") return Technology::crenellations;
    if (value == "kataparuto") return Technology::kataparuto;
    if (value == "rocketry") return Technology::rocketry;
    if (value == "logistica") return Technology::logistica;
    if (value == "mahouts") return Technology::mahouts;
    if (value == "zealotry") return Technology::zealotry;
    if (value == "artillery") return Technology::artillery;
    if (value == "drill") return Technology::drill;
    if (value == "supremacy") return Technology::supremacy;
    if (value == "atheism") return Technology::atheism;
    if (value == "shinkichon") return Technology::shinkichon;
    if (value == "el_dorado") return Technology::el_dorado;
    if (value == "elite_eagle_warrior") {
        return Technology::elite_eagle_warrior;
    }
    if (value == "heavy_scorpion") return Technology::heavy_scorpion;
    if (value == "onager") return Technology::onager;
    if (value == "siege_onager") return Technology::siege_onager;
    if (value == "heavy_cavalry_archer") {
        return Technology::heavy_cavalry_archer;
    }
    if (value == "heavy_camel") return Technology::heavy_camel;
    if (value == "capped_ram") return Technology::capped_ram;
    if (value == "siege_ram") return Technology::siege_ram;
    if (value == "halberdier") return Technology::halberdier;
    if (value == "chemistry") return Technology::chemistry;
    if (value == "hand_cannoneer_gate") {
        return Technology::hand_cannoneer_gate;
    }
    if (value == "bombard_cannon_gate") {
        return Technology::bombard_cannon_gate;
    }
    if (value == "siege_engineers") return Technology::siege_engineers;
    if (value == "conscription") return Technology::conscription;
    if (value == "petard_gate") return Technology::petard_gate;
    if (value == "bombard_tower") return Technology::bombard_tower;
    if (value == "sanctity") return Technology::sanctity;
    if (value == "fervor") return Technology::fervor;
    if (value == "redemption") return Technology::redemption;
    if (value == "atonement") return Technology::atonement;
    if (value == "illumination") return Technology::illumination;
    if (value == "block_printing") return Technology::block_printing;
    if (value == "faith") return Technology::faith;
    if (value == "theocracy") return Technology::theocracy;
    if (value == "heresy") return Technology::heresy;
    if (value == "heavy_plow") return Technology::heavy_plow;
    if (value == "crop_rotation") return Technology::crop_rotation;
    if (value == "bow_saw") return Technology::bow_saw;
    if (value == "two_man_saw") return Technology::two_man_saw;
    if (value == "gold_mining") return Technology::gold_mining;
    if (value == "gold_shaft_mining")
        return Technology::gold_shaft_mining;
    if (value == "stone_mining") return Technology::stone_mining;
    if (value == "stone_shaft_mining")
        return Technology::stone_shaft_mining;
    if (value == "hand_cart") return Technology::hand_cart;
    if (value == "fish_trap_gate") return Technology::fish_trap_gate;
    if (value == "coinage") return Technology::coinage;
    if (value == "banking") return Technology::banking;
    if (value == "cartography") return Technology::cartography;
    if (value == "caravan") return Technology::caravan;
    if (value == "guilds") return Technology::guilds;
    if (value == "trade_cog_gate") return Technology::trade_cog_gate;
    if (value == "outpost_gate") return Technology::outpost_gate;
    if (value == "town_watch") return Technology::town_watch;
    if (value == "town_patrol") return Technology::town_patrol;
    if (value == "masonry") return Technology::masonry;
    if (value == "architecture") return Technology::architecture;
    if (value == "ballistics") return Technology::ballistics;
    if (value == "heated_shot") return Technology::heated_shot;
    if (value == "hoardings") return Technology::hoardings;
    if (value == "sappers") return Technology::sappers;
    if (value == "wonder_plans") return Technology::wonder_plans;
    if (value == "thumb_ring") return Technology::thumb_ring;
    if (value == "parthian_tactics") return Technology::parthian_tactics;
    if (value == "squires") return Technology::squires;
    if (value == "tracking") return Technology::tracking;
    if (value == "herbal_medicine") return Technology::herbal_medicine;
    if (value == "stone_cutting") return Technology::stone_cutting;
    if (value == "spy_technology") return Technology::spy_technology;
    throw line_error(line, "unknown technology " + value);
}

std::string technology_name(Technology technology) {
    switch (technology) {
        case Technology::wheelbarrow: return "wheelbarrow";
        case Technology::fletching: return "fletching";
        case Technology::forging: return "forging";
        case Technology::murder_holes: return "murder_holes";
        case Technology::man_at_arms: return "man_at_arms";
        case Technology::crossbowman: return "crossbowman";
        case Technology::pikeman: return "pikeman";
        case Technology::long_swordsman: return "long_swordsman";
        case Technology::loom: return "loom";
        case Technology::double_bit_axe: return "double_bit_axe";
        case Technology::horse_collar: return "horse_collar";
        case Technology::fortified_wall: return "fortified_wall";
        case Technology::guard_tower: return "guard_tower";
        case Technology::keep: return "keep";
        case Technology::bodkin_arrow: return "bodkin_arrow";
        case Technology::bracer: return "bracer";
        case Technology::iron_casting: return "iron_casting";
        case Technology::blast_furnace: return "blast_furnace";
        case Technology::scale_mail_armor: return "scale_mail_armor";
        case Technology::chain_mail_armor: return "chain_mail_armor";
        case Technology::plate_mail_armor: return "plate_mail_armor";
        case Technology::scale_barding_armor: return "scale_barding_armor";
        case Technology::chain_barding_armor: return "chain_barding_armor";
        case Technology::plate_barding_armor: return "plate_barding_armor";
        case Technology::padded_archer_armor: return "padded_archer_armor";
        case Technology::leather_archer_armor: return "leather_archer_armor";
        case Technology::ring_archer_armor: return "ring_archer_armor";
        case Technology::bloodlines: return "bloodlines";
        case Technology::husbandry: return "husbandry";
        case Technology::cavalier: return "cavalier";
        case Technology::paladin: return "paladin";
        case Technology::light_cavalry: return "light_cavalry";
        case Technology::hussar: return "hussar";
        case Technology::two_handed_swordsman:
            return "two_handed_swordsman";
        case Technology::champion: return "champion";
        case Technology::arbalester: return "arbalester";
        case Technology::elite_skirmisher: return "elite_skirmisher";
        case Technology::war_galley: return "war_galley";
        case Technology::galleon: return "galleon";
        case Technology::fast_fire_ship: return "fast_fire_ship";
        case Technology::heavy_demolition_ship:
            return "heavy_demolition_ship";
        case Technology::cannon_galleon: return "cannon_galleon";
        case Technology::elite_cannon_galleon:
            return "elite_cannon_galleon";
        case Technology::careening: return "careening";
        case Technology::dry_dock: return "dry_dock";
        case Technology::shipwright: return "shipwright";
        case Technology::longboat: return "longboat";
        case Technology::elite_longboat: return "elite_longboat";
        case Technology::turtle_ship: return "turtle_ship";
        case Technology::elite_turtle_ship:
            return "elite_turtle_ship";
        case Technology::longbowman: return "longbowman";
        case Technology::elite_longbowman: return "elite_longbowman";
        case Technology::throwing_axeman: return "throwing_axeman";
        case Technology::elite_throwing_axeman:
            return "elite_throwing_axeman";
        case Technology::huskarl: return "huskarl";
        case Technology::elite_huskarl: return "elite_huskarl";
        case Technology::teutonic_knight: return "teutonic_knight";
        case Technology::elite_teutonic_knight:
            return "elite_teutonic_knight";
        case Technology::samurai: return "samurai";
        case Technology::elite_samurai: return "elite_samurai";
        case Technology::chu_ko_nu: return "chu_ko_nu";
        case Technology::elite_chu_ko_nu: return "elite_chu_ko_nu";
        case Technology::cataphract: return "cataphract";
        case Technology::elite_cataphract: return "elite_cataphract";
        case Technology::war_elephant: return "war_elephant";
        case Technology::elite_war_elephant:
            return "elite_war_elephant";
        case Technology::mameluke: return "mameluke";
        case Technology::elite_mameluke: return "elite_mameluke";
        case Technology::janissary: return "janissary";
        case Technology::elite_janissary: return "elite_janissary";
        case Technology::berserk: return "berserk";
        case Technology::elite_berserk: return "elite_berserk";
        case Technology::mangudai: return "mangudai";
        case Technology::elite_mangudai: return "elite_mangudai";
        case Technology::berserkergang: return "berserkergang";
        case Technology::jaguar_warrior: return "jaguar_warrior";
        case Technology::elite_jaguar_warrior:
            return "elite_jaguar_warrior";
        case Technology::plumed_archer: return "plumed_archer";
        case Technology::elite_plumed_archer:
            return "elite_plumed_archer";
        case Technology::conquistador: return "conquistador";
        case Technology::elite_conquistador:
            return "elite_conquistador";
        case Technology::tarkan: return "tarkan";
        case Technology::elite_tarkan: return "elite_tarkan";
        case Technology::woad_raider: return "woad_raider";
        case Technology::elite_woad_raider: return "elite_woad_raider";
        case Technology::yeomen: return "yeomen";
        case Technology::bearded_axe: return "bearded_axe";
        case Technology::anarchy: return "anarchy";
        case Technology::crenellations: return "crenellations";
        case Technology::kataparuto: return "kataparuto";
        case Technology::rocketry: return "rocketry";
        case Technology::logistica: return "logistica";
        case Technology::mahouts: return "mahouts";
        case Technology::zealotry: return "zealotry";
        case Technology::artillery: return "artillery";
        case Technology::drill: return "drill";
        case Technology::supremacy: return "supremacy";
        case Technology::atheism: return "atheism";
        case Technology::shinkichon: return "shinkichon";
        case Technology::el_dorado: return "el_dorado";
        case Technology::elite_eagle_warrior:
            return "elite_eagle_warrior";
        case Technology::heavy_scorpion: return "heavy_scorpion";
        case Technology::onager: return "onager";
        case Technology::siege_onager: return "siege_onager";
        case Technology::heavy_cavalry_archer:
            return "heavy_cavalry_archer";
        case Technology::heavy_camel: return "heavy_camel";
        case Technology::capped_ram: return "capped_ram";
        case Technology::siege_ram: return "siege_ram";
        case Technology::halberdier: return "halberdier";
        case Technology::chemistry: return "chemistry";
        case Technology::hand_cannoneer_gate:
            return "hand_cannoneer_gate";
        case Technology::bombard_cannon_gate:
            return "bombard_cannon_gate";
        case Technology::siege_engineers: return "siege_engineers";
        case Technology::conscription: return "conscription";
        case Technology::petard_gate: return "petard_gate";
        case Technology::bombard_tower: return "bombard_tower";
        case Technology::sanctity: return "sanctity";
        case Technology::fervor: return "fervor";
        case Technology::redemption: return "redemption";
        case Technology::atonement: return "atonement";
        case Technology::illumination: return "illumination";
        case Technology::block_printing: return "block_printing";
        case Technology::faith: return "faith";
        case Technology::theocracy: return "theocracy";
        case Technology::heresy: return "heresy";
        case Technology::heavy_plow: return "heavy_plow";
        case Technology::crop_rotation: return "crop_rotation";
        case Technology::bow_saw: return "bow_saw";
        case Technology::two_man_saw: return "two_man_saw";
        case Technology::gold_mining: return "gold_mining";
        case Technology::gold_shaft_mining: return "gold_shaft_mining";
        case Technology::stone_mining: return "stone_mining";
        case Technology::stone_shaft_mining: return "stone_shaft_mining";
        case Technology::hand_cart: return "hand_cart";
        case Technology::fish_trap_gate: return "fish_trap_gate";
        case Technology::coinage: return "coinage";
        case Technology::banking: return "banking";
        case Technology::cartography: return "cartography";
        case Technology::caravan: return "caravan";
        case Technology::guilds: return "guilds";
        case Technology::trade_cog_gate: return "trade_cog_gate";
        case Technology::outpost_gate: return "outpost_gate";
        case Technology::town_watch: return "town_watch";
        case Technology::town_patrol: return "town_patrol";
        case Technology::masonry: return "masonry";
        case Technology::architecture: return "architecture";
        case Technology::ballistics: return "ballistics";
        case Technology::heated_shot: return "heated_shot";
        case Technology::hoardings: return "hoardings";
        case Technology::sappers: return "sappers";
        case Technology::wonder_plans: return "wonder_plans";
        case Technology::thumb_ring: return "thumb_ring";
        case Technology::parthian_tactics: return "parthian_tactics";
        case Technology::squires: return "squires";
        case Technology::tracking: return "tracking";
        case Technology::herbal_medicine: return "herbal_medicine";
        case Technology::stone_cutting: return "stone_cutting";
        case Technology::spy_technology: return "spy_technology";
    }
    return "wheelbarrow";
}

UnitStance parse_stance(const std::string& value, int line) {
    if (value == "aggressive") return UnitStance::aggressive;
    if (value == "defensive") return UnitStance::defensive;
    if (value == "stand_ground") return UnitStance::stand_ground;
    if (value == "passive") return UnitStance::passive;
    throw line_error(line, "unknown stance " + value);
}

std::string stance_name(UnitStance stance) {
    switch (stance) {
        case UnitStance::aggressive: return "aggressive";
        case UnitStance::defensive: return "defensive";
        case UnitStance::stand_ground: return "stand_ground";
        case UnitStance::passive: return "passive";
    }
    return "aggressive";
}

UnitKind parse_unit(const std::string& value, int line) {
    if (value == "villager") return UnitKind::villager;
    if (value == "knight") return UnitKind::knight;
    if (value == "archer") return UnitKind::archer;
    if (value == "scout_cavalry") return UnitKind::scout_cavalry;
    if (value == "militia") return UnitKind::militia;
    if (value == "spearman") return UnitKind::spearman;
    if (value == "battering_ram") return UnitKind::battering_ram;
    if (value == "skirmisher") return UnitKind::skirmisher;
    if (value == "mangonel") return UnitKind::mangonel;
    if (value == "man_at_arms") return UnitKind::man_at_arms;
    if (value == "crossbowman") return UnitKind::crossbowman;
    if (value == "pikeman") return UnitKind::pikeman;
    if (value == "long_swordsman") return UnitKind::long_swordsman;
    if (value == "cavalier") return UnitKind::cavalier;
    if (value == "paladin") return UnitKind::paladin;
    if (value == "light_cavalry") return UnitKind::light_cavalry;
    if (value == "hussar") return UnitKind::hussar;
    if (value == "two_handed_swordsman") {
        return UnitKind::two_handed_swordsman;
    }
    if (value == "champion") return UnitKind::champion;
    if (value == "arbalester") return UnitKind::arbalester;
    if (value == "elite_skirmisher") return UnitKind::elite_skirmisher;
    if (value == "sheep") return UnitKind::sheep;
    if (value == "deer") return UnitKind::deer;
    if (value == "boar") return UnitKind::boar;
    if (value == "monk") return UnitKind::monk;
    if (value == "relic") return UnitKind::relic;
    if (value == "trade_cart") return UnitKind::trade_cart;
    if (value == "fishing_ship") return UnitKind::fishing_ship;
    if (value == "galley") return UnitKind::galley;
    if (value == "war_galley") return UnitKind::war_galley;
    if (value == "galleon") return UnitKind::galleon;
    if (value == "transport_ship") return UnitKind::transport_ship;
    if (value == "fire_ship") return UnitKind::fire_ship;
    if (value == "fast_fire_ship") return UnitKind::fast_fire_ship;
    if (value == "demolition_ship") return UnitKind::demolition_ship;
    if (value == "heavy_demolition_ship") {
        return UnitKind::heavy_demolition_ship;
    }
    if (value == "cannon_galleon") return UnitKind::cannon_galleon;
    if (value == "elite_cannon_galleon") {
        return UnitKind::elite_cannon_galleon;
    }
    if (value == "longboat") return UnitKind::longboat;
    if (value == "elite_longboat") return UnitKind::elite_longboat;
    if (value == "turtle_ship") return UnitKind::turtle_ship;
    if (value == "elite_turtle_ship") {
        return UnitKind::elite_turtle_ship;
    }
    if (value == "longbowman") return UnitKind::longbowman;
    if (value == "elite_longbowman") return UnitKind::elite_longbowman;
    if (value == "throwing_axeman") return UnitKind::throwing_axeman;
    if (value == "elite_throwing_axeman") {
        return UnitKind::elite_throwing_axeman;
    }
    if (value == "huskarl") return UnitKind::huskarl;
    if (value == "elite_huskarl") return UnitKind::elite_huskarl;
    if (value == "teutonic_knight") return UnitKind::teutonic_knight;
    if (value == "elite_teutonic_knight") {
        return UnitKind::elite_teutonic_knight;
    }
    if (value == "samurai") return UnitKind::samurai;
    if (value == "elite_samurai") return UnitKind::elite_samurai;
    if (value == "chu_ko_nu") return UnitKind::chu_ko_nu;
    if (value == "elite_chu_ko_nu") return UnitKind::elite_chu_ko_nu;
    if (value == "cataphract") return UnitKind::cataphract;
    if (value == "elite_cataphract") return UnitKind::elite_cataphract;
    if (value == "war_elephant") return UnitKind::war_elephant;
    if (value == "elite_war_elephant") {
        return UnitKind::elite_war_elephant;
    }
    if (value == "mameluke") return UnitKind::mameluke;
    if (value == "elite_mameluke") return UnitKind::elite_mameluke;
    if (value == "janissary") return UnitKind::janissary;
    if (value == "elite_janissary") return UnitKind::elite_janissary;
    if (value == "berserk") return UnitKind::berserk;
    if (value == "elite_berserk") return UnitKind::elite_berserk;
    if (value == "mangudai") return UnitKind::mangudai;
    if (value == "elite_mangudai") return UnitKind::elite_mangudai;
    if (value == "jaguar_warrior") return UnitKind::jaguar_warrior;
    if (value == "elite_jaguar_warrior") {
        return UnitKind::elite_jaguar_warrior;
    }
    if (value == "plumed_archer") return UnitKind::plumed_archer;
    if (value == "elite_plumed_archer") {
        return UnitKind::elite_plumed_archer;
    }
    if (value == "conquistador") return UnitKind::conquistador;
    if (value == "elite_conquistador") {
        return UnitKind::elite_conquistador;
    }
    if (value == "tarkan") return UnitKind::tarkan;
    if (value == "elite_tarkan") return UnitKind::elite_tarkan;
    if (value == "eagle_warrior") return UnitKind::eagle_warrior;
    if (value == "elite_eagle_warrior") {
        return UnitKind::elite_eagle_warrior;
    }
    if (value == "scorpion") return UnitKind::scorpion;
    if (value == "heavy_scorpion") return UnitKind::heavy_scorpion;
    if (value == "onager") return UnitKind::onager;
    if (value == "siege_onager") return UnitKind::siege_onager;
    if (value == "packed_trebuchet") return UnitKind::packed_trebuchet;
    if (value == "trebuchet") return UnitKind::trebuchet;
    if (value == "cavalry_archer") return UnitKind::cavalry_archer;
    if (value == "heavy_cavalry_archer") {
        return UnitKind::heavy_cavalry_archer;
    }
    if (value == "camel_rider") return UnitKind::camel_rider;
    if (value == "woad_raider") return UnitKind::woad_raider;
    if (value == "elite_woad_raider") return UnitKind::elite_woad_raider;
    if (value == "king") return UnitKind::king;
    if (value == "heavy_camel") return UnitKind::heavy_camel;
    if (value == "capped_ram") return UnitKind::capped_ram;
    if (value == "siege_ram") return UnitKind::siege_ram;
    if (value == "halberdier") return UnitKind::halberdier;
    if (value == "hand_cannoneer") return UnitKind::hand_cannoneer;
    if (value == "bombard_cannon") return UnitKind::bombard_cannon;
    if (value == "petard") return UnitKind::petard;
    if (value == "missionary") return UnitKind::missionary;
    if (value == "trade_cog") return UnitKind::trade_cog;
    throw line_error(line, "unknown unit " + value);
}

BuildingKind parse_building(const std::string& value, int line) {
    if (value == "town_center") return BuildingKind::town_center;
    if (value == "barracks") return BuildingKind::barracks;
    if (value == "archery_range") return BuildingKind::archery_range;
    if (value == "house") return BuildingKind::house;
    if (value == "mill") return BuildingKind::mill;
    if (value == "lumber_camp") return BuildingKind::lumber_camp;
    if (value == "mining_camp") return BuildingKind::mining_camp;
    if (value == "farm") return BuildingKind::farm;
    if (value == "stable") return BuildingKind::stable;
    if (value == "blacksmith") return BuildingKind::blacksmith;
    if (value == "castle") return BuildingKind::castle;
    if (value == "university") return BuildingKind::university;
    if (value == "siege_workshop") return BuildingKind::siege_workshop;
    if (value == "palisade_wall") return BuildingKind::palisade_wall;
    if (value == "watch_tower") return BuildingKind::watch_tower;
    if (value == "stone_wall") return BuildingKind::stone_wall;
    if (value == "palisade_gate_x") return BuildingKind::palisade_gate_x;
    if (value == "palisade_gate_y") return BuildingKind::palisade_gate_y;
    if (value == "stone_gate_x") return BuildingKind::stone_gate_x;
    if (value == "stone_gate_y") return BuildingKind::stone_gate_y;
    if (value == "guard_tower") return BuildingKind::guard_tower;
    if (value == "keep") return BuildingKind::keep;
    if (value == "fortified_wall") return BuildingKind::fortified_wall;
    if (value == "fortified_gate_x") return BuildingKind::fortified_gate_x;
    if (value == "fortified_gate_y") return BuildingKind::fortified_gate_y;
    if (value == "monastery") return BuildingKind::monastery;
    if (value == "market") return BuildingKind::market;
    if (value == "dock") return BuildingKind::dock;
    if (value == "bombard_tower") return BuildingKind::bombard_tower;
    if (value == "fish_trap") return BuildingKind::fish_trap;
    if (value == "outpost") return BuildingKind::outpost;
    if (value == "wonder") return BuildingKind::wonder;
    throw line_error(line, "unknown building " + value);
}

ResourceKind parse_trigger_resource(const std::string& value, int line) {
    if (value == "food") return ResourceKind::food;
    if (value == "wood") return ResourceKind::wood;
    if (value == "gold") return ResourceKind::gold;
    if (value == "stone") return ResourceKind::stone;
    throw line_error(line, "unknown trigger resource " + value);
}

bool at_end(std::istringstream& input) {
    input >> std::ws;
    return input.peek() == std::char_traits<char>::eof();
}

std::optional<TriggerCondition> parse_trigger_condition(
    const std::string& expression,
    int line
) {
    std::istringstream input(expression);
    std::string keyword;
    input >> keyword;
    TriggerCondition condition;
    if (keyword == "elapsed_ticks") {
        std::string comparison;
        input >> comparison >> condition.amount;
        condition.kind = TriggerConditionKind::elapsed_ticks;
        if (!input || comparison != ">=" || condition.amount < 0 ||
            !at_end(input)) {
            throw line_error(line, "invalid elapsed_ticks condition");
        }
    } else if (keyword == "unit_exists" ||
               keyword == "unit_destroyed" ||
               keyword == "building_exists" ||
               keyword == "building_destroyed") {
        input >> condition.entity;
        if (!input || condition.entity <= 0 || !at_end(input)) {
            throw line_error(line, "invalid entity trigger condition");
        }
        condition.kind =
            keyword == "unit_exists" ? TriggerConditionKind::unit_exists :
            keyword == "unit_destroyed" ?
                TriggerConditionKind::unit_destroyed :
            keyword == "building_exists" ?
                TriggerConditionKind::building_exists :
                TriggerConditionKind::building_destroyed;
    } else if (keyword == "resource") {
        std::string player;
        std::string resource;
        std::string comparison;
        input >> player >> resource >> comparison >> condition.amount;
        if (!input || comparison != ">=" || condition.amount < 0 ||
            !at_end(input)) {
            throw line_error(line, "invalid resource trigger condition");
        }
        condition.kind = TriggerConditionKind::resource_at_least;
        condition.player = parse_player(player, line);
        condition.resource = parse_trigger_resource(resource, line);
    } else if (keyword == "area_presence") {
        std::string player;
        std::string comparison;
        input >> player >> condition.first.x >> condition.first.y >>
            condition.second.x >> condition.second.y >> comparison >>
            condition.amount;
        if (!input || comparison != ">=" || condition.amount < 0 ||
            condition.first.x > condition.second.x ||
            condition.first.y > condition.second.y || !at_end(input)) {
            throw line_error(line, "invalid area_presence condition");
        }
        condition.kind = TriggerConditionKind::area_presence;
        condition.player = parse_player(player, line);
    } else if (keyword == "object_hp") {
        std::string comparison;
        input >> condition.entity >> comparison >> condition.amount;
        if (!input || condition.entity <= 0 || comparison != ">=" ||
            condition.amount < 0 || !at_end(input)) {
            throw line_error(line, "invalid object_hp condition");
        }
        condition.kind = TriggerConditionKind::object_hit_points_at_least;
    } else {
        return std::nullopt;
    }
    return condition;
}

std::optional<TriggerEffect> parse_trigger_effect(
    const std::string& expression,
    int line,
    bool strict = false
) {
    std::istringstream input(expression);
    std::string keyword;
    input >> keyword;
    TriggerEffect effect;
    if (keyword == "message" && strict) {
        std::string player_field;
        std::string ticks_field;
        std::string remainder;
        input >> player_field >> ticks_field;
        std::getline(input >> std::ws, remainder);
        if (!input || !player_field.starts_with("player=") ||
            !ticks_field.starts_with("ticks=") ||
            (!remainder.starts_with("text=") &&
             !remainder.starts_with("audio="))) {
            throw line_error(line, "invalid message trigger effect");
        }
        effect.player = parse_player(player_field.substr(7), line);
        if (effect.player == Player::neutral) {
            throw line_error(line, "message requires blue or red player");
        }
        try {
            std::size_t used{};
            effect.amount = std::stoi(ticks_field.substr(6), &used);
            if (used != ticks_field.size() - 6 || effect.amount <= 0) {
                throw std::invalid_argument("ticks");
            }
        } catch (const std::exception&) {
            throw line_error(line, "invalid message duration");
        }
        if (remainder.starts_with("audio=")) {
            std::istringstream audio_input(remainder.substr(6));
            audio_input >> std::quoted(effect.audio_file);
            std::getline(audio_input >> std::ws, remainder);
            if (!audio_input || !remainder.starts_with("text=")) {
                throw line_error(line, "invalid message audio field");
            }
        }
        std::istringstream text_input(remainder.substr(5));
        text_input >> std::quoted(effect.text);
        if (!text_input || !at_end(text_input)) {
            throw line_error(line, "invalid quoted message text");
        }
        effect.kind = TriggerEffectKind::message;
        if (!valid_scenario_text(effect.text) ||
            (!effect.audio_file.empty() &&
             !valid_scenario_text(effect.audio_file))) {
            throw line_error(line, "invalid message trigger effect");
        }
    } else if (keyword == "message" || keyword == "show_message") {
        std::getline(input >> std::ws, effect.text);
        effect.amount = 30;
        effect.kind = TriggerEffectKind::message;
        if (!valid_scenario_text(effect.text)) {
            throw line_error(line, "invalid message trigger effect");
        }
    } else if (keyword == "complete_objective") {
        input >> effect.amount;
        effect.kind = TriggerEffectKind::complete_objective;
        if (!input || effect.amount <= 0 || !at_end(input)) {
            throw line_error(line, "invalid complete_objective effect");
        }
    } else if (keyword == "add_resource") {
        std::string player;
        std::string resource;
        input >> player >> resource >> effect.amount;
        if (!input || !at_end(input)) {
            throw line_error(line, "invalid add_resource effect");
        }
        effect.kind = TriggerEffectKind::add_resource;
        effect.player = parse_player(player, line);
        effect.resource = parse_trigger_resource(resource, line);
    } else if (keyword == "create_unit") {
        std::string kind;
        std::string player;
        input >> kind >> player >> effect.position.x >> effect.position.y;
        if (!input || !at_end(input)) {
            throw line_error(line, "invalid create_unit effect");
        }
        effect.kind = TriggerEffectKind::create_unit;
        effect.unit = parse_unit(kind, line);
        effect.player = parse_player(player, line);
    } else if (keyword == "create_building") {
        std::string kind;
        std::string player;
        input >> kind >> player >> effect.position.x >> effect.position.y;
        if (!input || !at_end(input)) {
            throw line_error(line, "invalid create_building effect");
        }
        effect.kind = TriggerEffectKind::create_building;
        effect.building = parse_building(kind, line);
        effect.player = parse_player(player, line);
    } else if (keyword == "diplomacy") {
        std::string relation;
        input >> relation;
        effect.kind = TriggerEffectKind::diplomacy;
        if (!input || !at_end(input)) {
            throw line_error(line, "invalid diplomacy effect");
        }
        effect.diplomacy = parse_diplomacy(relation, line);
    } else if (keyword == "research") {
        std::string player;
        std::string technology;
        input >> player >> technology;
        if (!input || !at_end(input)) {
            throw line_error(line, "invalid research trigger effect");
        }
        effect.kind = TriggerEffectKind::research;
        effect.player = parse_player(player, line);
        effect.technology = parse_technology(technology, line);
    } else if (keyword == "tribute") {
        std::string source;
        std::string target;
        std::string resource;
        input >> source >> target >> resource >> effect.amount;
        if (!input || effect.amount < 0 || !at_end(input)) {
            throw line_error(line, "invalid tribute trigger effect");
        }
        effect.kind = TriggerEffectKind::tribute;
        effect.player = parse_player(source, line);
        effect.target_player = parse_player(target, line);
        effect.resource = parse_trigger_resource(resource, line);
    } else if (keyword == "remove_object") {
        input >> effect.entity;
        if (!input || effect.entity <= 0 || !at_end(input)) {
            throw line_error(line, "invalid remove_object trigger effect");
        }
        effect.kind = TriggerEffectKind::remove_object;
    } else if (keyword == "objective") {
        std::string state;
        input >> effect.objective_id >> state;
        if (!input || effect.objective_id <= 0 || !at_end(input) ||
            (state != "completed" && state != "incomplete" &&
             state != "shown" && state != "hidden")) {
            throw line_error(line, "invalid objective trigger effect");
        }
        effect.kind = TriggerEffectKind::set_objective_state;
        effect.state = state == "completed" || state == "shown";
        effect.amount = state == "completed" || state == "incomplete" ? 0 : 1;
    } else if (keyword == "activate_trigger" ||
               keyword == "deactivate_trigger") {
        input >> effect.trigger_id;
        if (!input || effect.trigger_id <= 0 || !at_end(input)) {
            throw line_error(line, "invalid trigger-state effect");
        }
        effect.kind = keyword == "activate_trigger"
            ? TriggerEffectKind::activate_trigger
            : TriggerEffectKind::deactivate_trigger;
    } else if (keyword == "victory" || keyword == "defeat") {
        std::string player;
        input >> player;
        effect.kind = keyword == "victory"
            ? TriggerEffectKind::victory
            : TriggerEffectKind::defeat;
        if (!input || !at_end(input)) {
            throw line_error(line, "invalid terminal trigger effect");
        }
        effect.player = parse_player(player, line);
    } else {
        return std::nullopt;
    }
    return effect;
}

std::string terrain_name(Terrain terrain) {
    switch (terrain) {
        case Terrain::grass: return "grass";
        case Terrain::water: return "water";
        case Terrain::beach: return "beach";
        case Terrain::shallows: return "shallows";
        case Terrain::forest: return "forest";
        case Terrain::berry_bush: return "berries";
        case Terrain::gold_mine: return "gold";
        case Terrain::stone_mine: return "stone";
        case Terrain::fish: return "fish";
    }
    return "grass";
}

std::string unit_name(UnitKind kind) {
    switch (kind) {
        case UnitKind::villager: return "villager";
        case UnitKind::knight: return "knight";
        case UnitKind::archer: return "archer";
        case UnitKind::scout_cavalry: return "scout_cavalry";
        case UnitKind::militia: return "militia";
        case UnitKind::spearman: return "spearman";
        case UnitKind::battering_ram: return "battering_ram";
        case UnitKind::skirmisher: return "skirmisher";
        case UnitKind::mangonel: return "mangonel";
        case UnitKind::man_at_arms: return "man_at_arms";
        case UnitKind::crossbowman: return "crossbowman";
        case UnitKind::pikeman: return "pikeman";
        case UnitKind::long_swordsman: return "long_swordsman";
        case UnitKind::cavalier: return "cavalier";
        case UnitKind::paladin: return "paladin";
        case UnitKind::light_cavalry: return "light_cavalry";
        case UnitKind::hussar: return "hussar";
        case UnitKind::two_handed_swordsman:
            return "two_handed_swordsman";
        case UnitKind::champion: return "champion";
        case UnitKind::arbalester: return "arbalester";
        case UnitKind::elite_skirmisher: return "elite_skirmisher";
        case UnitKind::sheep: return "sheep";
        case UnitKind::deer: return "deer";
        case UnitKind::boar: return "boar";
        case UnitKind::monk: return "monk";
        case UnitKind::relic: return "relic";
        case UnitKind::trade_cart: return "trade_cart";
        case UnitKind::fishing_ship: return "fishing_ship";
        case UnitKind::galley: return "galley";
        case UnitKind::war_galley: return "war_galley";
        case UnitKind::galleon: return "galleon";
        case UnitKind::transport_ship: return "transport_ship";
        case UnitKind::fire_ship: return "fire_ship";
        case UnitKind::fast_fire_ship: return "fast_fire_ship";
        case UnitKind::demolition_ship: return "demolition_ship";
        case UnitKind::heavy_demolition_ship:
            return "heavy_demolition_ship";
        case UnitKind::cannon_galleon: return "cannon_galleon";
        case UnitKind::elite_cannon_galleon:
            return "elite_cannon_galleon";
        case UnitKind::longboat: return "longboat";
        case UnitKind::elite_longboat: return "elite_longboat";
        case UnitKind::turtle_ship: return "turtle_ship";
        case UnitKind::elite_turtle_ship:
            return "elite_turtle_ship";
        case UnitKind::longbowman: return "longbowman";
        case UnitKind::elite_longbowman: return "elite_longbowman";
        case UnitKind::throwing_axeman: return "throwing_axeman";
        case UnitKind::elite_throwing_axeman:
            return "elite_throwing_axeman";
        case UnitKind::huskarl: return "huskarl";
        case UnitKind::elite_huskarl: return "elite_huskarl";
        case UnitKind::teutonic_knight: return "teutonic_knight";
        case UnitKind::elite_teutonic_knight:
            return "elite_teutonic_knight";
        case UnitKind::samurai: return "samurai";
        case UnitKind::elite_samurai: return "elite_samurai";
        case UnitKind::chu_ko_nu: return "chu_ko_nu";
        case UnitKind::elite_chu_ko_nu: return "elite_chu_ko_nu";
        case UnitKind::cataphract: return "cataphract";
        case UnitKind::elite_cataphract: return "elite_cataphract";
        case UnitKind::war_elephant: return "war_elephant";
        case UnitKind::elite_war_elephant:
            return "elite_war_elephant";
        case UnitKind::mameluke: return "mameluke";
        case UnitKind::elite_mameluke: return "elite_mameluke";
        case UnitKind::janissary: return "janissary";
        case UnitKind::elite_janissary: return "elite_janissary";
        case UnitKind::berserk: return "berserk";
        case UnitKind::elite_berserk: return "elite_berserk";
        case UnitKind::mangudai: return "mangudai";
        case UnitKind::elite_mangudai: return "elite_mangudai";
        case UnitKind::jaguar_warrior: return "jaguar_warrior";
        case UnitKind::elite_jaguar_warrior:
            return "elite_jaguar_warrior";
        case UnitKind::plumed_archer: return "plumed_archer";
        case UnitKind::elite_plumed_archer: return "elite_plumed_archer";
        case UnitKind::conquistador: return "conquistador";
        case UnitKind::elite_conquistador: return "elite_conquistador";
        case UnitKind::tarkan: return "tarkan";
        case UnitKind::elite_tarkan: return "elite_tarkan";
        case UnitKind::eagle_warrior: return "eagle_warrior";
        case UnitKind::elite_eagle_warrior:
            return "elite_eagle_warrior";
        case UnitKind::scorpion: return "scorpion";
        case UnitKind::heavy_scorpion: return "heavy_scorpion";
        case UnitKind::onager: return "onager";
        case UnitKind::siege_onager: return "siege_onager";
        case UnitKind::packed_trebuchet: return "packed_trebuchet";
        case UnitKind::trebuchet: return "trebuchet";
        case UnitKind::cavalry_archer: return "cavalry_archer";
        case UnitKind::heavy_cavalry_archer:
            return "heavy_cavalry_archer";
        case UnitKind::camel_rider: return "camel_rider";
        case UnitKind::heavy_camel: return "heavy_camel";
        case UnitKind::capped_ram: return "capped_ram";
        case UnitKind::siege_ram: return "siege_ram";
        case UnitKind::halberdier: return "halberdier";
        case UnitKind::hand_cannoneer: return "hand_cannoneer";
        case UnitKind::bombard_cannon: return "bombard_cannon";
        case UnitKind::petard: return "petard";
        case UnitKind::missionary: return "missionary";
        case UnitKind::trade_cog: return "trade_cog";
        case UnitKind::woad_raider: return "woad_raider";
        case UnitKind::elite_woad_raider: return "elite_woad_raider";
        case UnitKind::king: return "king";
    }
    return "villager";
}

std::string building_name(BuildingKind kind) {
    switch (kind) {
        case BuildingKind::town_center: return "town_center";
        case BuildingKind::barracks: return "barracks";
        case BuildingKind::archery_range: return "archery_range";
        case BuildingKind::house: return "house";
        case BuildingKind::mill: return "mill";
        case BuildingKind::lumber_camp: return "lumber_camp";
        case BuildingKind::mining_camp: return "mining_camp";
        case BuildingKind::farm: return "farm";
        case BuildingKind::stable: return "stable";
        case BuildingKind::blacksmith: return "blacksmith";
        case BuildingKind::castle: return "castle";
        case BuildingKind::university: return "university";
        case BuildingKind::siege_workshop: return "siege_workshop";
        case BuildingKind::palisade_wall: return "palisade_wall";
        case BuildingKind::watch_tower: return "watch_tower";
        case BuildingKind::stone_wall: return "stone_wall";
        case BuildingKind::palisade_gate_x: return "palisade_gate_x";
        case BuildingKind::palisade_gate_y: return "palisade_gate_y";
        case BuildingKind::stone_gate_x: return "stone_gate_x";
        case BuildingKind::stone_gate_y: return "stone_gate_y";
        case BuildingKind::monastery: return "monastery";
        case BuildingKind::market: return "market";
        case BuildingKind::dock: return "dock";
        case BuildingKind::bombard_tower: return "bombard_tower";
        case BuildingKind::fish_trap: return "fish_trap";
        case BuildingKind::outpost: return "outpost";
        case BuildingKind::wonder: return "wonder";
        case BuildingKind::guard_tower: return "guard_tower";
        case BuildingKind::keep: return "keep";
        case BuildingKind::fortified_wall: return "fortified_wall";
        case BuildingKind::fortified_gate_x: return "fortified_gate_x";
        case BuildingKind::fortified_gate_y: return "fortified_gate_y";
    }
    return "town_center";
}

}  // namespace

Scenario load_scenario(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("could not open scenario: " + path.string());
    }

    std::optional<Scenario> scenario;
    bool header_seen = false;
    int scenario_version = 1;
    std::array<bool, 8> team_seen{};
    std::string line;
    int line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        const auto content_start = line.find_first_not_of(" \t");
        if (content_start == std::string::npos || line[content_start] == '#') {
            continue;
        }
        std::istringstream record(line);
        std::string type;
        record >> type;

        if (!header_seen) {
            int version{};
            record >> version;
            if (type != "AOE-ARCHAEOLOGY-SCENARIO" ||
                version < 1 ||
                version > reconstruction_scenario_version) {
                throw line_error(line_number, "unsupported header");
            }
            scenario_version = version;
            header_seen = true;
            continue;
        }
        if (type == "map") {
            int width{};
            int height{};
            record >> width >> height;
            if (!record || scenario) {
                throw line_error(line_number, "invalid or duplicate map");
            }
            scenario.emplace(width, height);
            scenario->strict_trigger_syntax = scenario_version >= 62;
            scenario->enforce_civilization_availability =
                scenario_version >= 41;
            continue;
        }
        if (!scenario) {
            throw line_error(line_number, "map must be declared first");
        }

        if (type == "player-slot" && scenario_version >= 65) {
            int slot_id{};
            std::string controller_kind;
            std::string controller_id;
            ScenarioRosterEntry entry;
            std::string age;
            std::string civilization;
            int formation{};
            int technology_count{};
            record >> slot_id >> controller_kind >>
                std::quoted(controller_id) >>
                entry.economy.wood >> entry.economy.food >>
                entry.economy.gold >> entry.economy.stone >>
                age >> civilization >> formation >> technology_count;
            const PlayerSlotId slot =
                parse_player_slot(slot_id, line_number, false);
            if (!record || controller_id.empty() ||
                (controller_kind != "human" &&
                 controller_kind != "computer") ||
                formation < static_cast<int>(FormationKind::compact) ||
                formation > static_cast<int>(FormationKind::flank) ||
                technology_count < 0 || technology_count > 256 ||
                std::ranges::any_of(
                    scenario->roster_entries,
                    [slot](const ScenarioRosterEntry& existing) {
                        return existing.roster.slot == slot;
                    }
                )) {
                throw line_error(line_number, "invalid player-slot");
            }
            entry.roster.slot = slot;
            entry.roster.occupied = true;
            entry.roster.controllers.push_back({
                controller_id,
                controller_kind == "human"
                    ? RosterControllerKind::human
                    : RosterControllerKind::computer,
            });
            entry.age = parse_age(age, line_number);
            entry.civilization =
                parse_civilization(civilization, line_number);
            entry.formation = static_cast<FormationKind>(formation);
            for (int index = 0; index < technology_count; ++index) {
                std::string technology;
                record >> technology;
                entry.technologies.push_back(
                    parse_technology(technology, line_number)
                );
            }
            scenario->roster_schema = true;
            scenario->roster_entries.push_back(std::move(entry));
        } else if (type == "team" && scenario_version >= 65) {
            int slot_id{};
            int team_number{};
            record >> slot_id >> team_number;
            const PlayerSlotId slot =
                parse_player_slot(slot_id, line_number, false);
            const auto entry = std::ranges::find(
                scenario->roster_entries, slot,
                [](const ScenarioRosterEntry& value) {
                    return value.roster.slot;
                }
            );
            const auto team = team_number == 0
                ? std::optional<TeamId>{TeamId::none()}
                : TeamId::numbered(team_number);
            const auto slot_index = *slot.index();
            if (!record || entry == scenario->roster_entries.end() ||
                !team || team_seen[slot_index]) {
                throw line_error(line_number, "invalid team");
            }
            team_seen[slot_index] = true;
            entry->roster.team = *team;
        } else if (type == "diplomacy" && scenario_version >= 65) {
            std::string first;
            int to_id{};
            std::string relation;
            record >> first;
            if (first == "ally" || first == "neutral" ||
                first == "enemy") {
                scenario->blue_red_diplomacy =
                    parse_diplomacy(first, line_number);
                continue;
            }
            int from_id{};
            try {
                std::size_t consumed{};
                from_id = std::stoi(first, &consumed);
                if (consumed != first.size()) {
                    throw std::invalid_argument("trailing");
                }
            } catch (const std::exception&) {
                throw line_error(line_number, "invalid diplomacy");
            }
            record >> to_id >> relation;
            const PlayerSlotId from =
                parse_player_slot(from_id, line_number, false);
            const PlayerSlotId to =
                parse_player_slot(to_id, line_number, false);
            if (!record || from == to ||
                std::ranges::any_of(
                    scenario->directed_diplomacy,
                    [from, to](const DirectedDiplomacyRecord& value) {
                        return value.from == from && value.to == to;
                    }
                )) {
                throw line_error(line_number, "invalid diplomacy");
            }
            scenario->directed_diplomacy.push_back({
                from, to, parse_diplomacy(relation, line_number)
            });
        } else if (type == "trigger_syntax" &&
            scenario_version >= 63) {
            std::string syntax;
            record >> syntax;
            if (syntax != "strict" && syntax != "legacy") {
                throw line_error(
                    line_number, "invalid trigger syntax mode"
                );
            }
            scenario->strict_trigger_syntax = syntax == "strict";
        } else if (type == "economy") {
            std::string player_name;
            Economy economy;
            record >> player_name >> economy.wood >> economy.food;
            if (scenario_version >= 2) {
                record >> economy.gold >> economy.stone;
            }
            const Player player = parse_player(player_name, line_number);
            (player == Player::blue
                ? scenario->blue_economy
                : scenario->red_economy) = economy;
        } else if (type == "formation" && scenario_version >= 60) {
            std::string player_name;
            int kind{};
            record >> player_name >> kind;
            if (kind < static_cast<int>(FormationKind::compact) ||
                kind > static_cast<int>(FormationKind::flank)) {
                throw line_error(line_number, "invalid formation kind");
            }
            const Player player = parse_player(player_name, line_number);
            (player == Player::blue
                ? scenario->blue_formation
                : scenario->red_formation) =
                    static_cast<FormationKind>(kind);
        } else if (type == "match" && scenario_version >= 59) {
            record >> scenario->match_rules.conquest_enabled >>
                scenario->match_rules.wonder_enabled >>
                scenario->match_rules.relic_enabled >>
                scenario->match_rules.wonder_countdown_ticks >>
                scenario->match_rules.relic_countdown_ticks >>
                scenario->match_rules.relics_required >>
                scenario->match_rules.score_limit >>
                scenario->match_rules.time_limit_ticks;
            if (scenario_version >= 67) {
                record >> scenario->match_rules.regicide_enabled >>
                    scenario->match_rules.blue_king >>
                    scenario->match_rules.red_king;
            }
        } else if (type == "age" && scenario_version >= 3) {
            std::string player_name;
            std::string age;
            record >> player_name >> age;
            const Player player = parse_player(player_name, line_number);
            (player == Player::blue
                ? scenario->blue_age
                : scenario->red_age) = parse_age(age, line_number);
        } else if (type == "diplomacy" && scenario_version >= 30 &&
                   scenario_version < 65) {
            std::string relation;
            record >> relation;
            scenario->blue_red_diplomacy =
                parse_diplomacy(relation, line_number);
        } else if (type == "civilization" && scenario_version >= 31) {
            std::string player_name;
            std::string civilization;
            record >> player_name >> civilization;
            const Player player = parse_player(player_name, line_number);
            (player == Player::blue
                ? scenario->blue_civilization
                : scenario->red_civilization) =
                    parse_civilization(civilization, line_number);
        } else if (type == "technology" && scenario_version >= 4) {
            std::string player_name;
            std::string technology;
            record >> player_name >> technology;
            const Player player = parse_player(player_name, line_number);
            (player == Player::blue
                ? scenario->blue_technologies
                : scenario->red_technologies)
                .push_back(parse_technology(technology, line_number));
        } else if (type == "objective" && scenario_version >= 61) {
            ScenarioObjective objective;
            std::string player_name;
            int required{};
            int hidden{};
            record >> objective.id >> player_name >> required >> hidden >>
                std::quoted(objective.description);
            if (!record || objective.id <= 0 ||
                (required != 0 && required != 1) ||
                (hidden != 0 && hidden != 1) ||
                !valid_scenario_text(objective.description) ||
                std::ranges::any_of(
                    scenario->objectives,
                    [id = objective.id](const ScenarioObjective& existing) {
                        return existing.id == id;
                    }
                )) {
                throw line_error(line_number, "invalid objective");
            }
            record >> std::ws;
            if (record.peek() != std::char_traits<char>::eof()) {
                throw line_error(line_number, "trailing objective data");
            }
            objective.player = parse_player(player_name, line_number);
            objective.required = required != 0;
            objective.hidden = hidden != 0;
            scenario->objectives.push_back(std::move(objective));
            record.clear();
        } else if (type == "trigger" && scenario_version >= 61) {
            ScenarioTrigger trigger;
            int enabled{};
            int looping{};
            record >> trigger.id >> trigger.priority >> enabled >> looping;
            if (scenario_version >= 64) {
                int condition_count{};
                int effect_count{};
                record >> condition_count;
                if (!record || condition_count <= 0 ||
                    condition_count > 256) {
                    throw line_error(line_number, "invalid trigger");
                }
                for (int index = 0; index < condition_count; ++index) {
                    std::string condition;
                    record >> std::quoted(condition);
                    trigger.conditions.push_back(std::move(condition));
                }
                record >> effect_count;
                if (!record || effect_count <= 0 || effect_count > 256) {
                    throw line_error(line_number, "invalid trigger");
                }
                for (int index = 0; index < effect_count; ++index) {
                    std::string effect;
                    record >> std::quoted(effect);
                    trigger.effects.push_back(std::move(effect));
                }
            } else {
                std::string condition;
                std::string effect;
                record >> std::quoted(condition) >> std::quoted(effect);
                trigger.conditions.push_back(std::move(condition));
                trigger.effects.push_back(std::move(effect));
            }
            if (!record || trigger.id <= 0 ||
                trigger.priority < -100000 ||
                trigger.priority > 100000 ||
                (enabled != 0 && enabled != 1) ||
                (looping != 0 && looping != 1) ||
                std::ranges::any_of(
                    trigger.conditions,
                    [](const std::string& value) {
                        return !valid_scenario_text(value);
                    }
                ) ||
                std::ranges::any_of(
                    trigger.effects,
                    [](const std::string& value) {
                        return !valid_scenario_text(value);
                    }
                ) ||
                std::ranges::any_of(
                    scenario->triggers,
                    [id = trigger.id](const ScenarioTrigger& existing) {
                        return existing.id == id;
                    }
                )) {
                throw line_error(line_number, "invalid trigger");
            }
            record >> std::ws;
            if (record.peek() != std::char_traits<char>::eof()) {
                throw line_error(line_number, "trailing trigger data");
            }
            trigger.enabled = enabled != 0;
            trigger.looping = looping != 0;
            if (scenario->strict_trigger_syntax) {
                const bool invalid_condition = std::ranges::any_of(
                    trigger.conditions,
                    [line_number](const std::string& condition) {
                        return !parse_trigger_condition(
                            condition, line_number
                        );
                    }
                );
                const bool invalid_effect = std::ranges::any_of(
                    trigger.effects,
                    [line_number](const std::string& effect) {
                        return !parse_trigger_effect(
                            effect, line_number, true
                        );
                    }
                );
                if (invalid_condition || invalid_effect) {
                    throw line_error(
                        line_number,
                        "unknown executable trigger syntax"
                    );
                }
            }
            scenario->triggers.push_back(std::move(trigger));
            record.clear();
        } else if (type == "terrain") {
            std::string terrain;
            TilePosition position;
            record >> terrain >> position.x >> position.y;
            scenario->map.set_terrain(
                position,
                parse_terrain(terrain, line_number)
            );
        } else if (type == "terrain_rect") {
            std::string terrain;
            int left{};
            int top{};
            int right{};
            int bottom{};
            record >> terrain >> left >> top >> right >> bottom;
            const Terrain value = parse_terrain(terrain, line_number);
            for (int y = top; y <= bottom; ++y) {
                for (int x = left; x <= right; ++x) {
                    scenario->map.set_terrain({x, y}, value);
                }
            }
        } else if (type == "elevation" &&
                   scenario_version >= 63) {
            TilePosition position;
            int elevation{};
            record >> position.x >> position.y >> elevation;
            scenario->map.set_elevation(position, elevation);
        } else if (type == "unit") {
            std::string kind;
            std::string player;
            TilePosition position;
            record >> kind >> player >> position.x >> position.y;
            if (!record) {
                throw line_error(line_number, "missing or invalid value");
            }
            UnitPlacement placement{
                parse_unit(kind, line_number),
                parse_entity_owner(player, line_number, scenario_version),
                position,
                std::nullopt,
                std::nullopt,
                std::nullopt,
                std::nullopt,
                false,
                {},
                UnitStance::aggressive,
                std::nullopt,
            };
            if (scenario_version >= 20) {
                record >> std::ws;
                while (record.peek() != std::char_traits<char>::eof()) {
                    std::string marker;
                    record >> marker;
                    if (marker == "stance" &&
                        scenario_version >= 26) {
                        std::string stance;
                        record >> stance;
                        if (!record) {
                            throw line_error(
                                line_number,
                                "invalid stance"
                            );
                        }
                        placement.stance =
                            parse_stance(stance, line_number);
                        record >> std::ws;
                        continue;
                    }
                    if (marker == "food" &&
                        scenario_version >= 29) {
                        int amount{};
                        record >> amount;
                        if (!record || amount < 0 ||
                            !is_animal(placement.kind)) {
                            throw line_error(
                                line_number,
                                "invalid herdable food"
                            );
                        }
                        placement.food_remaining = amount;
                        record >> std::ws;
                        continue;
                    }
                    TilePosition target;
                    record >> target.x >> target.y;
                    if (!record) {
                        throw line_error(
                            line_number,
                            "invalid unit order"
                        );
                    }
                    if (marker == "garrison") {
                        placement.garrisoned_in = target;
                    } else if (
                        marker == "attack_move" &&
                        scenario_version >= 22
                    ) {
                        if (!scenario->map.contains(target) ||
                            !scenario->map.walkable(target)) {
                            throw line_error(
                                line_number,
                                "invalid attack-move destination"
                            );
                        }
                        placement.attack_move_destination = target;
                    } else if (
                        marker == "patrol" &&
                        scenario_version >= 23
                    ) {
                        if (!scenario->map.contains(target) ||
                            !scenario->map.walkable(target)) {
                            throw line_error(
                                line_number,
                                "invalid patrol destination"
                            );
                        }
                        placement.patrol_destination = target;
                    } else if (
                        (marker == "guard_unit" ||
                         marker == "guard_building") &&
                        scenario_version >= 24
                    ) {
                        if (!scenario->map.contains(target)) {
                            throw line_error(
                                line_number,
                                "invalid guard target"
                            );
                        }
                        placement.guard_target = target;
                        placement.guard_target_is_building =
                            marker == "guard_building";
                    } else if (
                        marker == "waypoint" &&
                        scenario_version >= 25
                    ) {
                        if (!scenario->map.contains(target) ||
                            !scenario->map.walkable(target)) {
                            throw line_error(
                                line_number,
                                "invalid waypoint"
                            );
                        }
                        placement.waypoints.push_back(target);
                    } else {
                        throw line_error(
                            line_number,
                            "unknown unit order marker"
                        );
                    }
                    record >> std::ws;
                }
                record.clear();
            }
            scenario->units.push_back(placement);
        } else if (type == "building") {
            std::string kind;
            std::string player;
            TilePosition position;
            record >> kind >> player >> position.x >> position.y;
            BuildingPlacement placement{
                parse_building(kind, line_number),
                parse_entity_owner(player, line_number, scenario_version),
                position,
                std::nullopt,
                std::nullopt,
                std::nullopt,
            };
            if (scenario_version >= 21) {
                record >> std::ws;
                while (record.peek() != std::char_traits<char>::eof()) {
                    std::string marker;
                    record >> marker;
                    if (marker == "rally") {
                        TilePosition rally;
                        record >> rally.x >> rally.y;
                        if (!scenario->map.contains(rally)) {
                            throw line_error(
                                line_number,
                                "rally point outside map"
                            );
                        }
                        placement.rally_point = rally;
                    } else if (
                        marker == "hit_points" &&
                        scenario_version >= 27
                    ) {
                        int hit_points{};
                        record >> hit_points;
                        const int maximum =
                            rules_for(placement.kind).hit_points;
                        if (hit_points < 1 || hit_points > maximum) {
                            throw line_error(
                                line_number,
                                "building hit points outside valid range"
                            );
                        }
                        placement.hit_points = hit_points;
                    } else if (
                        marker == "resource_amount" &&
                        scenario_version >= 28
                    ) {
                        int resource_amount{};
                        record >> resource_amount;
                        if (!record ||
                            placement.kind != BuildingKind::farm ||
                            resource_amount < 0 ||
                            resource_amount > 250) {
                            throw line_error(
                                line_number,
                                "farm resource amount outside valid range"
                            );
                        }
                        placement.resource_amount = resource_amount;
                    } else {
                        throw line_error(
                            line_number,
                            "unknown building marker"
                        );
                    }
                    record >> std::ws;
                }
                record.clear();
            }
            scenario->buildings.push_back(placement);
        } else {
            throw line_error(line_number, "unknown record " + type);
        }
        if (!record) {
            throw line_error(line_number, "missing or invalid value");
        }
    }

    if (!header_seen || !scenario) {
        throw std::runtime_error("scenario missing header or map");
    }
    if (!scenario->roster_schema) {
        scenario->roster_schema = true;
        scenario->roster_entries = {
            {
                {
                    *PlayerSlotId::from_index(0), true, TeamId::none(),
                    false, {{"blue", RosterControllerKind::human}},
                },
                scenario->blue_economy, scenario->blue_age,
                scenario->blue_civilization,
                scenario->blue_technologies,
                scenario->blue_formation,
            },
            {
                {
                    *PlayerSlotId::from_index(1), true, TeamId::none(),
                    false, {{"red", RosterControllerKind::human}},
                },
                scenario->red_economy, scenario->red_age,
                scenario->red_civilization,
                scenario->red_technologies,
                scenario->red_formation,
            },
        };
        scenario->directed_diplomacy = {
            {
                *PlayerSlotId::from_index(0),
                *PlayerSlotId::from_index(1),
                scenario->blue_red_diplomacy,
            },
            {
                *PlayerSlotId::from_index(1),
                *PlayerSlotId::from_index(0),
                scenario->blue_red_diplomacy,
            },
        };
    }
    if (scenario->roster_schema) {
        std::vector<MatchRosterSlot> slots;
        for (const ScenarioRosterEntry& entry : scenario->roster_entries) {
            slots.push_back(entry.roster);
        }
        const auto roster = MatchRoster::create(std::move(slots));
        if (!roster) {
            throw std::runtime_error("invalid scenario roster");
        }
        auto diplomacy = RosterDiplomacy::create(*roster);
        if (!diplomacy) {
            throw std::runtime_error("invalid scenario diplomacy");
        }
        std::size_t occupied{};
        for (const ScenarioRosterEntry& entry : scenario->roster_entries) {
            if (entry.roster.occupied) ++occupied;
        }
        if (scenario->directed_diplomacy.size() !=
            occupied * (occupied - 1)) {
            throw std::runtime_error(
                "scenario diplomacy matrix is incomplete"
            );
        }
        for (const DirectedDiplomacyRecord& relation :
             scenario->directed_diplomacy) {
            if (!diplomacy->set_stance(
                    relation.from, relation.to, relation.stance
                )) {
                throw std::runtime_error("invalid scenario diplomacy");
            }
        }
        const auto owner_is_valid = [&roster](EntityOwner owner) {
            const auto slot = entity_owner_slot(owner);
            return slot && (slot->is_neutral() ||
                roster->slot(*slot).occupied);
        };
        if (std::ranges::any_of(
                scenario->units,
                [&owner_is_valid](const UnitPlacement& unit) {
                    return !owner_is_valid(unit.owner);
                }
            ) ||
            std::ranges::any_of(
                scenario->buildings,
                [&owner_is_valid](const BuildingPlacement& building) {
                    return !owner_is_valid(building.owner);
                }
            )) {
            throw std::runtime_error(
                "scenario entity owner is not occupied"
            );
        }
        for (const ScenarioRosterEntry& entry :
             scenario->roster_entries) {
            const auto index = *entry.roster.slot.index();
            if (index == 0 || index == 1) {
                Economy& economy = index == 0
                    ? scenario->blue_economy : scenario->red_economy;
                Age& age = index == 0
                    ? scenario->blue_age : scenario->red_age;
                Civilization& civilization = index == 0
                    ? scenario->blue_civilization
                    : scenario->red_civilization;
                FormationKind& formation = index == 0
                    ? scenario->blue_formation
                    : scenario->red_formation;
                std::vector<Technology>& technologies = index == 0
                    ? scenario->blue_technologies
                    : scenario->red_technologies;
                economy = entry.economy;
                age = entry.age;
                civilization = entry.civilization;
                formation = entry.formation;
                technologies = entry.technologies;
            }
        }
        const auto blue = PlayerSlotId::from_index(0);
        const auto red = PlayerSlotId::from_index(1);
        if (roster->slot(*blue).occupied && roster->slot(*red).occupied) {
            scenario->blue_red_diplomacy =
                diplomacy->stance(*blue, *red);
        }
    }
    (void)create_simulation(*scenario);
    return std::move(*scenario);
}

void save_scenario(
    const Scenario& scenario,
    const std::filesystem::path& path
) {
    (void)create_simulation(scenario);
    std::ofstream output(path);
    if (!output) {
        throw std::runtime_error("could not write scenario: " + path.string());
    }
    output << "AOE-ARCHAEOLOGY-SCENARIO "
           << reconstruction_scenario_version << '\n';
    output << "map " << scenario.map.width() << ' '
           << scenario.map.height() << '\n';
    output << "trigger_syntax "
           << (scenario.strict_trigger_syntax ? "strict" : "legacy")
           << '\n';
    output << "match " << scenario.match_rules.conquest_enabled << ' '
           << scenario.match_rules.wonder_enabled << ' '
           << scenario.match_rules.relic_enabled << ' '
           << scenario.match_rules.wonder_countdown_ticks << ' '
           << scenario.match_rules.relic_countdown_ticks << ' '
           << scenario.match_rules.relics_required << ' '
           << scenario.match_rules.score_limit << ' '
           << scenario.match_rules.time_limit_ticks << ' '
           << scenario.match_rules.regicide_enabled << ' '
           << scenario.match_rules.blue_king << ' '
           << scenario.match_rules.red_king << '\n';
    std::vector<ScenarioRosterEntry> roster_entries =
        scenario.roster_entries;
    std::vector<DirectedDiplomacyRecord> diplomacy_records =
        scenario.directed_diplomacy;
    if (!scenario.roster_schema) {
        roster_entries = {
            {
                {
                    *PlayerSlotId::from_index(0), true, TeamId::none(),
                    false, {{"blue", RosterControllerKind::human}},
                },
                scenario.blue_economy, scenario.blue_age,
                scenario.blue_civilization, scenario.blue_technologies,
                scenario.blue_formation,
            },
            {
                {
                    *PlayerSlotId::from_index(1), true, TeamId::none(),
                    false, {{"red", RosterControllerKind::human}},
                },
                scenario.red_economy, scenario.red_age,
                scenario.red_civilization, scenario.red_technologies,
                scenario.red_formation,
            },
        };
        diplomacy_records = {
            {
                *PlayerSlotId::from_index(0),
                *PlayerSlotId::from_index(1),
                scenario.blue_red_diplomacy,
            },
            {
                *PlayerSlotId::from_index(1),
                *PlayerSlotId::from_index(0),
                scenario.blue_red_diplomacy,
            },
        };
    } else if (roster_entries.size() == 2) {
        for (ScenarioRosterEntry& entry : roster_entries) {
            const auto index = entry.roster.slot.index();
            if (!index || *index > 1) continue;
            entry.economy = *index == 0
                ? scenario.blue_economy : scenario.red_economy;
            entry.age = *index == 0
                ? scenario.blue_age : scenario.red_age;
            entry.civilization = *index == 0
                ? scenario.blue_civilization
                : scenario.red_civilization;
            entry.technologies = *index == 0
                ? scenario.blue_technologies
                : scenario.red_technologies;
            entry.formation = *index == 0
                ? scenario.blue_formation : scenario.red_formation;
        }
        for (DirectedDiplomacyRecord& relation : diplomacy_records) {
            relation.stance = scenario.blue_red_diplomacy;
        }
    }
    std::ranges::sort(
        roster_entries,
        [](const ScenarioRosterEntry& left,
           const ScenarioRosterEntry& right) {
            return left.roster.slot.stable_id() <
                right.roster.slot.stable_id();
        }
    );
    for (const ScenarioRosterEntry& entry : roster_entries) {
        if (!entry.roster.occupied ||
            entry.roster.controllers.size() != 1) {
            throw std::runtime_error("invalid scenario roster");
        }
        const RosterController& controller =
            entry.roster.controllers.front();
        output << "player-slot "
               << encode_player_slot_id(entry.roster.slot) << ' '
               << (controller.kind == RosterControllerKind::human
                    ? "human" : "computer")
               << ' ' << std::quoted(controller.id) << ' '
               << entry.economy.wood << ' ' << entry.economy.food << ' '
               << entry.economy.gold << ' ' << entry.economy.stone << ' '
               << age_name(entry.age) << ' '
               << name(entry.civilization) << ' '
               << static_cast<int>(entry.formation) << ' '
               << entry.technologies.size();
        for (Technology technology : entry.technologies) {
            output << ' ' << technology_name(technology);
        }
        output << '\n';
        output << "team " << encode_player_slot_id(entry.roster.slot)
               << ' ' << entry.roster.team.number() << '\n';
    }
    std::ranges::sort(
        diplomacy_records,
        [](const DirectedDiplomacyRecord& left,
           const DirectedDiplomacyRecord& right) {
            return std::pair{
                left.from.stable_id(), left.to.stable_id()
            } < std::pair{
                right.from.stable_id(), right.to.stable_id()
            };
        }
    );
    for (const DirectedDiplomacyRecord& relation : diplomacy_records) {
        output << "diplomacy "
               << static_cast<int>(relation.from.stable_id()) << ' '
               << static_cast<int>(relation.to.stable_id()) << ' '
               << (relation.stance == Diplomacy::ally ? "ally" :
                   relation.stance == Diplomacy::neutral ? "neutral" :
                   "enemy") << '\n';
    }
    std::vector<ScenarioObjective> objectives = scenario.objectives;
    std::ranges::sort(
        objectives,
        [](const ScenarioObjective& left, const ScenarioObjective& right) {
            return left.id < right.id;
        }
    );
    int previous_objective_id{};
    for (const ScenarioObjective& objective : objectives) {
        if (objective.id <= 0 || objective.id == previous_objective_id ||
            (objective.player != Player::blue &&
             objective.player != Player::red) ||
            !valid_scenario_text(objective.description)) {
            throw std::runtime_error("invalid scenario objective");
        }
        previous_objective_id = objective.id;
        output << "objective " << objective.id << ' '
               << name(objective.player) << ' ' << objective.required << ' '
               << objective.hidden << ' '
               << std::quoted(objective.description) << '\n';
    }
    std::vector<ScenarioTrigger> triggers = scenario.triggers;
    std::ranges::sort(
        triggers,
        [](const ScenarioTrigger& left, const ScenarioTrigger& right) {
            if (left.priority != right.priority) {
                return left.priority > right.priority;
            }
            return left.id < right.id;
        }
    );
    std::vector<int> trigger_ids;
    for (const ScenarioTrigger& trigger : triggers) {
        if (trigger.id <= 0 ||
            std::ranges::find(trigger_ids, trigger.id) !=
                trigger_ids.end() ||
            trigger.priority < -100000 || trigger.priority > 100000 ||
            trigger.conditions.empty() || trigger.conditions.size() > 256 ||
            trigger.effects.empty() || trigger.effects.size() > 256 ||
            std::ranges::any_of(
                trigger.conditions,
                [](const std::string& value) {
                    return !valid_scenario_text(value);
                }
            ) ||
            std::ranges::any_of(
                trigger.effects,
                [](const std::string& value) {
                    return !valid_scenario_text(value);
                }
            )) {
            throw std::runtime_error("invalid scenario trigger");
        }
        trigger_ids.push_back(trigger.id);
        output << "trigger " << trigger.id << ' ' << trigger.priority << ' '
               << trigger.enabled << ' ' << trigger.looping << ' '
               << trigger.conditions.size();
        for (const std::string& condition : trigger.conditions) {
            output << ' ' << std::quoted(condition);
        }
        output << ' ' << trigger.effects.size();
        for (const std::string& effect : trigger.effects) {
            output << ' ' << std::quoted(effect);
        }
        output << '\n';
    }
    for (int y = 0; y < scenario.map.height(); ++y) {
        for (int x = 0; x < scenario.map.width(); ++x) {
            const Terrain terrain = scenario.map.terrain_at({x, y});
            if (terrain != Terrain::grass) {
                output << "terrain " << terrain_name(terrain) << ' '
                       << x << ' ' << y << '\n';
            }
            const int elevation =
                scenario.map.elevation_at({x, y});
            if (elevation != 0) {
                output << "elevation " << x << ' ' << y << ' '
                       << elevation << '\n';
            }
        }
    }
    for (const UnitPlacement& unit : scenario.units) {
        output << "unit " << unit_name(unit.kind) << ' '
               << static_cast<int>(unit.owner.stable_id()) << ' '
               << unit.position.x << ' '
               << unit.position.y;
        if (unit.food_remaining) {
            output << " food " << *unit.food_remaining;
        }
        if (unit.garrisoned_in) {
            output << " garrison " << unit.garrisoned_in->x << ' '
                   << unit.garrisoned_in->y;
        } else if (unit.attack_move_destination) {
            output << " attack_move "
                   << unit.attack_move_destination->x << ' '
                   << unit.attack_move_destination->y;
        } else if (unit.patrol_destination) {
            output << " patrol " << unit.patrol_destination->x << ' '
                   << unit.patrol_destination->y;
        } else if (unit.guard_target) {
            output << (
                unit.guard_target_is_building
                    ? " guard_building "
                    : " guard_unit "
            ) << unit.guard_target->x << ' ' << unit.guard_target->y;
        }
        for (TilePosition waypoint : unit.waypoints) {
            output << " waypoint " << waypoint.x << ' ' << waypoint.y;
        }
        if (unit.stance != UnitStance::aggressive) {
            output << " stance " << stance_name(unit.stance);
        }
        output << '\n';
    }
    for (const BuildingPlacement& building : scenario.buildings) {
        output << "building " << building_name(building.kind) << ' '
               << static_cast<int>(building.owner.stable_id()) << ' '
               << building.position.x << ' '
               << building.position.y;
        if (building.rally_point) {
            output << " rally " << building.rally_point->x << ' '
                   << building.rally_point->y;
        }
        if (building.hit_points) {
            output << " hit_points " << *building.hit_points;
        }
        if (building.resource_amount) {
            output << " resource_amount " << *building.resource_amount;
        }
        output << '\n';
    }
}

Simulation create_simulation(const Scenario& scenario) {
    const auto owner_civilization = [&scenario](EntityOwner owner) {
        if (scenario.roster_schema) {
            const auto slot = entity_owner_slot(owner);
            if (!slot || slot->is_neutral()) {
                return Civilization::generic;
            }
            const auto entry = std::ranges::find(
                scenario.roster_entries, *slot,
                [](const ScenarioRosterEntry& value) {
                    return value.roster.slot;
                }
            );
            return entry == scenario.roster_entries.end()
                ? Civilization::generic : entry->civilization;
        }
        const auto player = owner.legacy_player();
        return player == Player::blue
            ? scenario.blue_civilization
            : player == Player::red
                ? scenario.red_civilization
                : Civilization::generic;
    };
    if (scenario.enforce_civilization_availability) {
        for (Technology technology : scenario.blue_technologies) {
            if (!civilization_has_technology(
                    scenario.blue_civilization, technology
                )) {
                throw std::runtime_error(
                    "scenario contains unavailable blue technology"
                );
            }
        }
        for (Technology technology : scenario.red_technologies) {
            if (!civilization_has_technology(
                    scenario.red_civilization, technology
                )) {
                throw std::runtime_error(
                    "scenario contains unavailable red technology"
                );
            }
        }
        for (const UnitPlacement& unit : scenario.units) {
            if (unit.owner != Player::neutral &&
                !civilization_has_unit(
                    owner_civilization(unit.owner), unit.kind
                )) {
                throw std::runtime_error(
                    "scenario contains unavailable unit"
                );
            }
        }
        for (const BuildingPlacement& building : scenario.buildings) {
            if (building.owner != Player::neutral &&
                !civilization_has_building(
                    owner_civilization(building.owner), building.kind
                )) {
                throw std::runtime_error(
                    "scenario contains unavailable building"
                );
            }
        }
    }
    Simulation simulation(scenario.map);
    if (scenario.roster_schema) {
        std::vector<MatchRosterSlot> slots;
        for (const ScenarioRosterEntry& entry : scenario.roster_entries) {
            slots.push_back(entry.roster);
        }
        const auto roster = MatchRoster::create(std::move(slots));
        if (!roster) throw std::runtime_error("invalid scenario roster");
        auto diplomacy = RosterDiplomacy::create(*roster);
        if (!diplomacy) {
            throw std::runtime_error("invalid scenario diplomacy");
        }
        for (const DirectedDiplomacyRecord& relation :
             scenario.directed_diplomacy) {
            if (!diplomacy->set_stance(
                    relation.from, relation.to, relation.stance
                )) {
                throw std::runtime_error("invalid scenario diplomacy");
            }
        }
        simulation.replace_roster(*roster, *diplomacy);
        for (const ScenarioRosterEntry& entry :
             scenario.roster_entries) {
            Simulation::PlayerState state =
                simulation.player_state(entry.roster.slot);
            state.economy = entry.economy;
            state.age = entry.age;
            state.civilization = entry.civilization;
            state.formation = entry.formation;
            state.controller =
                entry.roster.controllers.front().kind ==
                        RosterControllerKind::human
                    ? PlayerControllerState::active
                    : PlayerControllerState::active;
            state.technologies.fill(false);
            for (Technology technology : entry.technologies) {
                state.technologies.at(
                    static_cast<std::size_t>(technology)
                ) = true;
            }
            simulation.replace_player_state(
                entry.roster.slot, std::move(state)
            );
        }
    }
    simulation.set_match_rules(scenario.match_rules);
    simulation.replace_diplomacy(scenario.blue_red_diplomacy);
    simulation.replace_civilizations(
        scenario.blue_civilization,
        scenario.red_civilization
    );
    simulation.replace_ages(scenario.blue_age, scenario.red_age);
    simulation.replace_technologies(
        Player::blue,
        scenario.blue_technologies
    );
    simulation.replace_technologies(
        Player::red,
        scenario.red_technologies
    );
    std::vector<EntityId> unit_ids;
    for (const UnitPlacement& unit : scenario.units) {
        unit_ids.push_back(
            simulation.add_unit(unit.kind, unit.owner, unit.position)
        );
    }
    std::vector<EntityId> building_ids;
    for (const BuildingPlacement& building : scenario.buildings) {
        building_ids.push_back(simulation.add_building(
            building.kind,
            building.owner,
            building.position
        ));
    }
    std::vector<Unit> units = simulation.units();
    std::vector<std::pair<EntityId, EntityId>> garrison_requests;
    for (std::size_t unit_index = 0;
         unit_index < scenario.units.size();
         ++unit_index) {
        const UnitPlacement& placement = scenario.units[unit_index];
        units[unit_index].stance = placement.stance;
        units[unit_index].stance_anchor = placement.position;
        if (placement.food_remaining) {
            units[unit_index].food_remaining =
                *placement.food_remaining;
        }
        if (placement.attack_move_destination) {
            units[unit_index].destination =
                *placement.attack_move_destination;
            units[unit_index].attack_move_destination =
                *placement.attack_move_destination;
            units[unit_index].attack_moving = true;
            units[unit_index].moving = true;
        } else if (placement.patrol_destination) {
            units[unit_index].destination =
                *placement.patrol_destination;
            units[unit_index].attack_move_destination =
                *placement.patrol_destination;
            units[unit_index].attack_moving = true;
            units[unit_index].patrol_origin = placement.position;
            units[unit_index].patrol_destination =
                *placement.patrol_destination;
            units[unit_index].patrolling = true;
            units[unit_index].moving = true;
        }
        if (placement.guard_target) {
            if (placement.guard_target_is_building) {
                const auto target = std::ranges::find_if(
                    scenario.buildings,
                    [&placement](const BuildingPlacement& building) {
                        return building.position ==
                                   *placement.guard_target &&
                               building.owner == placement.owner;
                    }
                );
                if (target == scenario.buildings.end()) {
                    throw std::invalid_argument(
                        "scenario guard requires friendly building"
                    );
                }
                const std::size_t target_index =
                    static_cast<std::size_t>(std::distance(
                        scenario.buildings.begin(),
                        target
                    ));
                units[unit_index].guard_target_id =
                    building_ids[target_index];
                units[unit_index].guard_target_is_building = true;
                units[unit_index].destination = target->position;
                units[unit_index].moving = true;
            } else {
                std::optional<std::size_t> target_index;
                for (std::size_t candidate = 0;
                     candidate < scenario.units.size();
                     ++candidate) {
                    if (candidate != unit_index &&
                        scenario.units[candidate].position ==
                            *placement.guard_target &&
                        scenario.units[candidate].owner ==
                            placement.owner) {
                        target_index = candidate;
                        break;
                    }
                }
                if (!target_index) {
                    throw std::invalid_argument(
                        "scenario guard requires friendly unit"
                    );
                }
                units[unit_index].guard_target_id =
                    unit_ids[*target_index];
                units[unit_index].guard_target_is_building = false;
                units[unit_index].destination =
                    scenario.units[*target_index].position;
                units[unit_index].moving = true;
            }
        }
        if (!placement.waypoints.empty()) {
            std::size_t first_queued = 0;
            const bool has_primary_order =
                placement.attack_move_destination.has_value() ||
                placement.patrol_destination.has_value() ||
                placement.guard_target.has_value();
            if (!has_primary_order) {
                units[unit_index].destination =
                    placement.waypoints.front();
                units[unit_index].moving = true;
                first_queued = 1;
            }
            units[unit_index].waypoints.assign(
                placement.waypoints.begin() + first_queued,
                placement.waypoints.end()
            );
        }
        if (!placement.garrisoned_in) {
            continue;
        }
        const auto shelter = std::ranges::find_if(
            scenario.buildings,
            [&placement](const BuildingPlacement& building) {
                return building.position == *placement.garrisoned_in &&
                    building.owner == placement.owner;
            }
        );
        if (shelter == scenario.buildings.end()) {
            throw std::invalid_argument(
                "garrisoned scenario unit requires friendly building"
            );
        }
        const std::size_t building_index = static_cast<std::size_t>(
            std::distance(scenario.buildings.begin(), shelter)
        );
        garrison_requests.emplace_back(
            unit_ids[unit_index], building_ids[building_index]
        );
    }
    std::vector<Building> buildings = simulation.buildings();
    for (std::size_t index = 0; index < scenario.buildings.size(); ++index) {
        if (scenario.buildings[index].rally_point) {
            buildings[index].rally_point =
                *scenario.buildings[index].rally_point;
            buildings[index].has_rally_point = true;
        }
        if (scenario.buildings[index].hit_points) {
            buildings[index].hit_points =
                *scenario.buildings[index].hit_points;
        }
        if (scenario.buildings[index].resource_amount) {
            buildings[index].resource_amount =
                *scenario.buildings[index].resource_amount;
        }
    }
    simulation.replace_state(
        std::move(units),
        std::move(buildings),
        scenario.blue_economy,
        scenario.red_economy,
        0
    );
    simulation.replace_technologies(
        Player::blue,
        scenario.blue_technologies
    );
    simulation.replace_technologies(
        Player::red,
        scenario.red_technologies
    );
    for (const auto [unit_id, building_id] : garrison_requests) {
        if (!simulation.restore_garrison(unit_id, building_id)) {
            throw std::invalid_argument(
                "scenario contains invalid garrison assignment"
            );
        }
    }
    simulation.set_formation_kind(Player::blue, scenario.blue_formation);
    simulation.set_formation_kind(Player::red, scenario.red_formation);
    std::vector<ObjectiveState> objectives;
    objectives.reserve(scenario.objectives.size());
    for (const ScenarioObjective& source : scenario.objectives) {
        objectives.push_back({
            source.id,
            source.player,
            source.required,
            source.hidden,
            false,
            source.description,
        });
    }
    std::vector<TriggerState> triggers;
    triggers.reserve(scenario.triggers.size());
    for (const ScenarioTrigger& source : scenario.triggers) {
        TriggerState trigger;
        trigger.id = source.id;
        trigger.priority = source.priority;
        trigger.enabled = source.enabled;
        trigger.looping = source.looping;
        for (const std::string& expression : source.conditions) {
            const auto parsed = parse_trigger_condition(expression, 0);
            if (parsed) trigger.conditions.push_back(*parsed);
        }
        for (const std::string& expression : source.effects) {
            const auto parsed = parse_trigger_effect(
                expression, 0, scenario.strict_trigger_syntax
            );
            if (parsed) trigger.effects.push_back(*parsed);
        }
        trigger.executable =
            trigger.conditions.size() == source.conditions.size() &&
            trigger.effects.size() == source.effects.size();
        if (scenario.strict_trigger_syntax && !trigger.executable) {
            throw std::invalid_argument(
                "unknown executable trigger syntax"
            );
        }
        const auto condition = trigger.conditions.empty()
            ? std::optional<TriggerCondition>{}
            : std::optional<TriggerCondition>{trigger.conditions.front()};
        const auto effect = trigger.effects.empty()
            ? std::optional<TriggerEffect>{}
            : std::optional<TriggerEffect>{trigger.effects.front()};
        if (condition) trigger.condition = *condition;
        if (effect) {
            trigger.effect = *effect;
            if (effect->kind == TriggerEffectKind::complete_objective &&
                std::ranges::none_of(
                    objectives,
                    [id = effect->amount](const ObjectiveState& objective) {
                        return objective.id == id;
                    }
                )) {
                throw std::invalid_argument(
                    "trigger references missing objective"
                );
            }
            if ((effect->kind == TriggerEffectKind::create_unit ||
                 effect->kind == TriggerEffectKind::create_building) &&
                !scenario.map.contains(effect->position)) {
                throw std::invalid_argument(
                    "trigger creates entity outside map"
                );
            }
            if ((effect->kind == TriggerEffectKind::victory ||
                 effect->kind == TriggerEffectKind::defeat ||
                 effect->kind == TriggerEffectKind::add_resource) &&
                effect->player == Player::neutral) {
                throw std::invalid_argument(
                    "trigger effect requires blue or red player"
                );
            }
        }
        if (condition &&
            condition->kind == TriggerConditionKind::resource_at_least &&
            condition->player == Player::neutral) {
            throw std::invalid_argument(
                "resource condition requires blue or red player"
            );
        }
        if (condition &&
            (condition->kind == TriggerConditionKind::unit_exists ||
             condition->kind == TriggerConditionKind::unit_destroyed) &&
            std::ranges::find(unit_ids, condition->entity) ==
                unit_ids.end()) {
            throw std::invalid_argument(
                "trigger references missing unit"
            );
        }
        if (condition &&
            (condition->kind == TriggerConditionKind::building_exists ||
             condition->kind == TriggerConditionKind::building_destroyed) &&
            std::ranges::find(building_ids, condition->entity) ==
                building_ids.end()) {
            throw std::invalid_argument(
                "trigger references missing building"
            );
        }
        if (condition &&
            condition->kind == TriggerConditionKind::area_presence &&
            (!scenario.map.contains(condition->first) ||
             !scenario.map.contains(condition->second))) {
            throw std::invalid_argument(
                "trigger area lies outside map"
            );
        }
        for (const TriggerCondition& value : trigger.conditions) {
            if (value.kind ==
                    TriggerConditionKind::object_hit_points_at_least &&
                std::ranges::none_of(
                    unit_ids, [id = value.entity](EntityId candidate) {
                        return candidate == id;
                    }
                ) &&
                std::ranges::none_of(
                    building_ids, [id = value.entity](EntityId candidate) {
                        return candidate == id;
                    }
                )) {
                throw std::invalid_argument(
                    "object_hp references missing object"
                );
            }
        }
        for (const TriggerEffect& value : trigger.effects) {
            if ((value.kind == TriggerEffectKind::set_objective_state ||
                 value.kind == TriggerEffectKind::complete_objective) &&
                std::ranges::none_of(
                    objectives,
                    [id = value.kind ==
                            TriggerEffectKind::complete_objective
                         ? value.amount : value.objective_id](
                        const ObjectiveState& objective
                    ) { return objective.id == id; }
                )) {
                throw std::invalid_argument(
                    "trigger references missing objective"
                );
            }
            if ((value.kind == TriggerEffectKind::activate_trigger ||
                 value.kind == TriggerEffectKind::deactivate_trigger) &&
                std::ranges::none_of(
                    scenario.triggers,
                    [id = value.trigger_id](const ScenarioTrigger& candidate) {
                        return candidate.id == id;
                    }
                )) {
                throw std::invalid_argument(
                    "trigger references missing trigger"
                );
            }
            if (value.kind == TriggerEffectKind::remove_object &&
                std::ranges::find(unit_ids, value.entity) == unit_ids.end() &&
                std::ranges::find(building_ids, value.entity) ==
                    building_ids.end()) {
                throw std::invalid_argument(
                    "remove_object references missing object"
                );
            }
            if ((value.kind == TriggerEffectKind::tribute &&
                 (value.player == Player::neutral ||
                  value.target_player == Player::neutral ||
                  value.player == value.target_player ||
                  value.resource == ResourceKind::none)) ||
                (value.kind == TriggerEffectKind::research &&
                 value.player == Player::neutral)) {
                throw std::invalid_argument(
                    "trigger effect has invalid player/resource"
                );
            }
        }
        triggers.push_back(std::move(trigger));
    }
    validate_trigger_runtime_semantics(
        simulation.map(),
        simulation.units(),
        simulation.buildings(),
        objectives,
        triggers,
        0,
        true
    );
    simulation.replace_scenario_runtime(
        std::move(objectives), std::move(triggers)
    );
    return simulation;
}

}  // namespace aoe
