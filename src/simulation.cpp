#include "aoe/simulation.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

#include "aoe/game_rules.hpp"
#include "aoe/pathfinding.hpp"

namespace aoe {

namespace {

bool represented_player(Player player) {
    return player == Player::blue || player == Player::red;
}

bool is_wall(BuildingKind kind);

bool commercial_has_ability(
    const Unit& unit, CommercialTaskAbility ability
) {
    if (!unit.commercial_identity) return false;
    const auto* record = commercial_content_catalog().object(
        unit.commercial_identity->civilization_id,
        unit.commercial_identity->object_id
    );
    return record && std::ranges::any_of(
        record->tasks, [ability](const CommercialTask& task) {
            return commercial_task_ability(task.action_type) == ability;
        }
    );
}

bool gather_trace_enabled() {
    static const bool enabled = std::getenv("AOE_GATHER_TRACE") != nullptr;
    return enabled;
}

}

void validate_trigger_runtime_semantics(
    const GameMap& map,
    const std::vector<Unit>& units,
    const std::vector<Building>& buildings,
    const std::vector<ObjectiveState>& objectives,
    const std::vector<TriggerState>& triggers,
    std::uint64_t current_tick,
    bool require_initial_entity_references
) {
    const auto has_unit = [&](EntityId id) {
        return std::ranges::any_of(
            units, [id](const Unit& unit) { return unit.id == id; }
        );
    };
    const auto has_building = [&](EntityId id) {
        return std::ranges::any_of(
            buildings,
            [id](const Building& building) { return building.id == id; }
        );
    };
    const auto has_object = [&](EntityId id) {
        return has_unit(id) || has_building(id);
    };
    const auto has_objective = [&](int id) {
        return std::ranges::any_of(
            objectives,
            [id](const ObjectiveState& objective) {
                return objective.id == id;
            }
        );
    };
    const auto has_trigger = [&](int id) {
        return std::ranges::any_of(
            triggers,
            [id](const TriggerState& trigger) { return trigger.id == id; }
        );
    };
    for (const ObjectiveState& objective : objectives) {
        if (objective.id <= 0 || !represented_player(objective.player) ||
            objective.description.empty()) {
            throw std::invalid_argument("invalid scenario objective");
        }
    }
    for (std::size_t index = 0; index < objectives.size(); ++index) {
        if (std::ranges::any_of(
                objectives.begin() + static_cast<std::ptrdiff_t>(index + 1),
                objectives.end(),
                [id = objectives[index].id](const ObjectiveState& value) {
                    return value.id == id;
                }
            )) {
            throw std::invalid_argument("duplicate scenario objective");
        }
    }
    for (const TriggerState& trigger : triggers) {
        if (trigger.id <= 0 || trigger.priority < -100000 ||
            trigger.priority > 100000 ||
            trigger.activation_tick > current_tick ||
            trigger.last_fired_tick > current_tick ||
            (trigger.fired_count == 0 && trigger.last_fired_tick != 0) ||
            (trigger.executable &&
             (trigger.conditions.empty() || trigger.effects.empty()))) {
            throw std::invalid_argument("invalid scenario trigger state");
        }
        if (std::ranges::count(
                triggers, trigger.id, &TriggerState::id
            ) != 1) {
            throw std::invalid_argument("duplicate scenario trigger");
        }
        for (const TriggerCondition& condition : trigger.conditions) {
            if (condition.amount < 0) {
                throw std::invalid_argument(
                    "negative scenario trigger condition amount"
                );
            }
            switch (condition.kind) {
                case TriggerConditionKind::elapsed_ticks:
                    break;
                case TriggerConditionKind::unit_exists:
                case TriggerConditionKind::unit_destroyed:
                    if (condition.entity <= 0 ||
                        (require_initial_entity_references &&
                         !has_unit(condition.entity))) {
                        throw std::invalid_argument(
                            "trigger references missing unit"
                        );
                    }
                    break;
                case TriggerConditionKind::building_exists:
                case TriggerConditionKind::building_destroyed:
                    if (condition.entity <= 0 ||
                        (require_initial_entity_references &&
                         !has_building(condition.entity))) {
                        throw std::invalid_argument(
                            "trigger references missing building"
                        );
                    }
                    break;
                case TriggerConditionKind::resource_at_least:
                    if (!represented_player(condition.player) ||
                        condition.resource == ResourceKind::none) {
                        throw std::invalid_argument(
                            "resource condition has invalid player/resource"
                        );
                    }
                    break;
                case TriggerConditionKind::area_presence:
                    if (!represented_player(condition.player) ||
                        condition.first.x > condition.second.x ||
                        condition.first.y > condition.second.y ||
                        !map.contains(condition.first) ||
                        !map.contains(condition.second)) {
                        throw std::invalid_argument(
                            "invalid area-presence condition"
                        );
                    }
                    break;
                case TriggerConditionKind::object_hit_points_at_least:
                    if (condition.entity <= 0 ||
                        (require_initial_entity_references &&
                         !has_object(condition.entity))) {
                        throw std::invalid_argument(
                            "object_hp references missing object"
                        );
                    }
                    break;
            }
        }
        for (const TriggerEffect& effect : trigger.effects) {
            if (effect.amount < 0) {
                throw std::invalid_argument(
                    "negative scenario trigger effect amount"
                );
            }
            switch (effect.kind) {
                case TriggerEffectKind::message:
                    if (!represented_player(effect.player) ||
                        effect.amount <= 0 || effect.text.empty() ||
                        effect.audio_file.size() > 4096) {
                        throw std::invalid_argument(
                            "invalid message trigger effect"
                        );
                    }
                    break;
                case TriggerEffectKind::complete_objective:
                    if (!represented_player(effect.player) ||
                        effect.amount <= 0 ||
                        !has_objective(effect.amount)) {
                        throw std::invalid_argument(
                            "trigger references missing objective"
                        );
                    }
                    break;
                case TriggerEffectKind::add_resource:
                    if (!represented_player(effect.player) ||
                        effect.resource == ResourceKind::none ||
                        effect.amount < 0) {
                        throw std::invalid_argument(
                            "invalid add-resource trigger effect"
                        );
                    }
                    break;
                case TriggerEffectKind::create_unit:
                case TriggerEffectKind::create_building:
                    if (!map.contains(effect.position)) {
                        throw std::invalid_argument(
                            "trigger creates entity outside map"
                        );
                    }
                    break;
                case TriggerEffectKind::diplomacy:
                    break;
                case TriggerEffectKind::victory:
                case TriggerEffectKind::defeat:
                    if (!represented_player(effect.player)) {
                        throw std::invalid_argument(
                            "terminal trigger requires blue or red player"
                        );
                    }
                    break;
                case TriggerEffectKind::research:
                    if (!represented_player(effect.player)) {
                        throw std::invalid_argument(
                            "research trigger requires blue or red player"
                        );
                    }
                    break;
                case TriggerEffectKind::tribute:
                    if (!represented_player(effect.player) ||
                        !represented_player(effect.target_player) ||
                        effect.player == effect.target_player ||
                        effect.resource == ResourceKind::none ||
                        effect.amount < 0) {
                        throw std::invalid_argument(
                            "invalid tribute trigger effect"
                        );
                    }
                    break;
                case TriggerEffectKind::remove_object:
                    if (effect.entity <= 0 ||
                        (require_initial_entity_references &&
                         !has_object(effect.entity))) {
                        throw std::invalid_argument(
                            "remove_object references missing object"
                        );
                    }
                    break;
                case TriggerEffectKind::set_objective_state:
                    if (!represented_player(effect.player) ||
                        effect.objective_id <= 0 ||
                        !has_objective(effect.objective_id) ||
                        (effect.amount != 0 && effect.amount != 1)) {
                        throw std::invalid_argument(
                            "invalid objective trigger effect"
                        );
                    }
                    break;
                case TriggerEffectKind::activate_trigger:
                case TriggerEffectKind::deactivate_trigger:
                    if (effect.trigger_id <= 0 ||
                        !has_trigger(effect.trigger_id)) {
                        throw std::invalid_argument(
                            "trigger references missing trigger"
                        );
                    }
                    break;
            }
        }
    }
}

std::string_view name(UnitKind kind) {
    switch (kind) {
        case UnitKind::villager:
            return "villager";
        case UnitKind::knight:
            return "knight";
        case UnitKind::archer:
            return "archer";
        case UnitKind::scout_cavalry:
            return "scout cavalry";
        case UnitKind::militia:
            return "militia";
        case UnitKind::spearman:
            return "spearman";
        case UnitKind::battering_ram:
            return "battering ram";
        case UnitKind::skirmisher:
            return "skirmisher";
        case UnitKind::mangonel:
            return "mangonel";
        case UnitKind::man_at_arms:
            return "man-at-arms";
        case UnitKind::crossbowman:
            return "crossbowman";
        case UnitKind::pikeman:
            return "pikeman";
        case UnitKind::halberdier: return "halberdier";
        case UnitKind::hand_cannoneer: return "hand cannoneer";
        case UnitKind::bombard_cannon: return "bombard cannon";
        case UnitKind::petard: return "petard";
        case UnitKind::long_swordsman:
            return "long swordsman";
        case UnitKind::cavalier:
            return "cavalier";
        case UnitKind::paladin:
            return "paladin";
        case UnitKind::light_cavalry:
            return "light cavalry";
        case UnitKind::hussar:
            return "hussar";
        case UnitKind::two_handed_swordsman:
            return "two-handed swordsman";
        case UnitKind::champion:
            return "champion";
        case UnitKind::arbalester:
            return "arbalester";
        case UnitKind::elite_skirmisher:
            return "elite skirmisher";
        case UnitKind::sheep:
            return "sheep";
        case UnitKind::deer:
            return "deer";
        case UnitKind::boar:
            return "boar";
        case UnitKind::monk:
            return "monk";
        case UnitKind::relic:
            return "relic";
        case UnitKind::missionary:
            return "missionary";
        case UnitKind::trade_cog:
            return "trade cog";
        case UnitKind::trade_cart:
            return "trade cart";
        case UnitKind::fishing_ship:
            return "fishing ship";
        case UnitKind::galley: return "galley";
        case UnitKind::war_galley: return "war galley";
        case UnitKind::galleon: return "galleon";
        case UnitKind::transport_ship: return "transport ship";
        case UnitKind::fire_ship: return "fire ship";
        case UnitKind::fast_fire_ship: return "fast fire ship";
        case UnitKind::demolition_ship: return "demolition ship";
        case UnitKind::heavy_demolition_ship:
            return "heavy demolition ship";
        case UnitKind::cannon_galleon: return "cannon galleon";
        case UnitKind::elite_cannon_galleon:
            return "elite cannon galleon";
        case UnitKind::longboat: return "longboat";
        case UnitKind::elite_longboat: return "elite longboat";
        case UnitKind::turtle_ship: return "turtle ship";
        case UnitKind::elite_turtle_ship: return "elite turtle ship";
        case UnitKind::longbowman: return "longbowman";
        case UnitKind::elite_longbowman: return "elite longbowman";
        case UnitKind::throwing_axeman: return "throwing axeman";
        case UnitKind::elite_throwing_axeman:
            return "elite throwing axeman";
        case UnitKind::huskarl: return "huskarl";
        case UnitKind::elite_huskarl: return "elite huskarl";
        case UnitKind::teutonic_knight: return "teutonic knight";
        case UnitKind::elite_teutonic_knight:
            return "elite teutonic knight";
        case UnitKind::samurai: return "samurai";
        case UnitKind::elite_samurai: return "elite samurai";
        case UnitKind::chu_ko_nu: return "chu ko nu";
        case UnitKind::elite_chu_ko_nu: return "elite chu ko nu";
        case UnitKind::cataphract: return "cataphract";
        case UnitKind::elite_cataphract: return "elite cataphract";
        case UnitKind::war_elephant: return "war elephant";
        case UnitKind::elite_war_elephant: return "elite war elephant";
        case UnitKind::mameluke: return "mameluke";
        case UnitKind::elite_mameluke: return "elite mameluke";
        case UnitKind::janissary: return "janissary";
        case UnitKind::elite_janissary: return "elite janissary";
        case UnitKind::berserk: return "berserk";
        case UnitKind::elite_berserk: return "elite berserk";
        case UnitKind::mangudai: return "mangudai";
        case UnitKind::elite_mangudai: return "elite mangudai";
        case UnitKind::jaguar_warrior: return "jaguar warrior";
        case UnitKind::elite_jaguar_warrior:
            return "elite jaguar warrior";
        case UnitKind::plumed_archer: return "plumed archer";
        case UnitKind::elite_plumed_archer: return "elite plumed archer";
        case UnitKind::conquistador: return "conquistador";
        case UnitKind::elite_conquistador: return "elite conquistador";
        case UnitKind::tarkan: return "tarkan";
        case UnitKind::elite_tarkan: return "elite tarkan";
        case UnitKind::woad_raider: return "woad raider";
        case UnitKind::elite_woad_raider: return "elite woad raider";
        case UnitKind::king: return "king";
        case UnitKind::eagle_warrior: return "eagle warrior";
        case UnitKind::elite_eagle_warrior: return "elite eagle warrior";
        case UnitKind::scorpion: return "scorpion";
        case UnitKind::heavy_scorpion: return "heavy scorpion";
        case UnitKind::onager: return "onager";
        case UnitKind::siege_onager: return "siege onager";
        case UnitKind::packed_trebuchet: return "packed trebuchet";
        case UnitKind::trebuchet: return "trebuchet";
        case UnitKind::cavalry_archer: return "cavalry archer";
        case UnitKind::heavy_cavalry_archer:
            return "heavy cavalry archer";
        case UnitKind::camel_rider: return "camel rider";
        case UnitKind::heavy_camel: return "heavy camel";
        case UnitKind::capped_ram: return "capped ram";
        case UnitKind::siege_ram: return "siege ram";
    }
    return "unknown unit";
}

std::string_view name(BuildingKind kind) {
    switch (kind) {
        case BuildingKind::town_center:
            return "town center";
        case BuildingKind::barracks:
            return "barracks";
        case BuildingKind::archery_range:
            return "archery range";
        case BuildingKind::house:
            return "house";
        case BuildingKind::mill:
            return "mill";
        case BuildingKind::lumber_camp:
            return "lumber camp";
        case BuildingKind::mining_camp:
            return "mining camp";
        case BuildingKind::farm:
            return "farm";
        case BuildingKind::stable:
            return "stable";
        case BuildingKind::blacksmith:
            return "blacksmith";
        case BuildingKind::castle:
            return "castle";
        case BuildingKind::university:
            return "university";
        case BuildingKind::siege_workshop:
            return "siege workshop";
        case BuildingKind::palisade_wall:
            return "palisade wall";
        case BuildingKind::watch_tower:
            return "watch tower";
        case BuildingKind::guard_tower:
            return "guard tower";
        case BuildingKind::keep:
            return "keep";
        case BuildingKind::stone_wall:
            return "stone wall";
        case BuildingKind::fortified_wall:
            return "fortified wall";
        case BuildingKind::palisade_gate_x:
            return "palisade gate (NW-SE)";
        case BuildingKind::palisade_gate_y:
            return "palisade gate (NE-SW)";
        case BuildingKind::stone_gate_x:
            return "stone gate (NW-SE)";
        case BuildingKind::stone_gate_y:
            return "stone gate (NE-SW)";
        case BuildingKind::fortified_gate_x:
            return "fortified gate (NW-SE)";
        case BuildingKind::fortified_gate_y:
            return "fortified gate (NE-SW)";
        case BuildingKind::monastery:
            return "monastery";
        case BuildingKind::market:
            return "market";
        case BuildingKind::dock:
            return "dock";
        case BuildingKind::bombard_tower:
            return "bombard tower";
        case BuildingKind::fish_trap:
            return "fish trap";
        case BuildingKind::outpost:
            return "outpost";
        case BuildingKind::wonder:
            return "wonder";
    }
    return "unknown building";
}

std::string_view name(Player player) {
    return player == Player::blue ? "blue" :
        player == Player::red ? "red" : "neutral";
}

std::string_view name(MatchOutcome outcome) {
    switch (outcome) {
        case MatchOutcome::ongoing:
            return "Ongoing";
        case MatchOutcome::blue_victory:
            return "Blue victory";
        case MatchOutcome::red_victory:
            return "Red victory";
        case MatchOutcome::allied_victory:
            return "Allied victory";
        case MatchOutcome::draw:
            return "Draw";
    }
    return "Unknown";
}

std::string_view name(Civilization civilization) {
    switch (civilization) {
        case Civilization::generic: return "generic";
        case Civilization::britons: return "britons";
        case Civilization::franks: return "franks";
        case Civilization::teutons: return "teutons";
        case Civilization::goths: return "goths";
        case Civilization::celts: return "celts";
        case Civilization::vikings: return "vikings";
        case Civilization::byzantines: return "byzantines";
        case Civilization::japanese: return "japanese";
        case Civilization::chinese: return "chinese";
        case Civilization::persians: return "persians";
        case Civilization::saracens: return "saracens";
        case Civilization::turks: return "turks";
        case Civilization::mongols: return "mongols";
        case Civilization::spanish: return "spanish";
        case Civilization::huns: return "huns";
        case Civilization::koreans: return "koreans";
        case Civilization::aztecs: return "aztecs";
        case Civilization::mayans: return "mayans";
    }
    return "generic";
}

std::string_view name(UnitStance stance) {
    switch (stance) {
        case UnitStance::aggressive: return "aggressive";
        case UnitStance::defensive: return "defensive";
        case UnitStance::stand_ground: return "stand ground";
        case UnitStance::passive: return "passive";
    }
    return "aggressive";
}

std::string_view name(ResourceKind resource) {
    switch (resource) {
        case ResourceKind::none:
            return "nothing";
        case ResourceKind::wood:
            return "wood";
        case ResourceKind::food:
            return "food";
        case ResourceKind::gold:
            return "gold";
        case ResourceKind::stone:
            return "stone";
    }
    return "resource";
}

std::string_view name(Age age) {
    switch (age) {
        case Age::dark:
            return "Dark Age";
        case Age::feudal:
            return "Feudal Age";
        case Age::castle:
            return "Castle Age";
        case Age::imperial:
            return "Imperial Age";
    }
    return "Unknown Age";
}

std::string_view name(Technology technology) {
    switch (technology) {
        case Technology::wheelbarrow:
            return "Wheelbarrow";
        case Technology::fletching:
            return "Fletching";
        case Technology::forging:
            return "Forging";
        case Technology::murder_holes:
            return "Murder Holes";
        case Technology::man_at_arms:
            return "Man-at-Arms";
        case Technology::crossbowman:
            return "Crossbowman";
        case Technology::pikeman:
            return "Pikeman";
        case Technology::halberdier: return "Halberdier";
        case Technology::chemistry: return "Chemistry";
        case Technology::hand_cannoneer_gate: return "Hand Cannoneer";
        case Technology::bombard_cannon_gate: return "Bombard Cannon";
        case Technology::siege_engineers: return "Siege Engineers";
        case Technology::conscription: return "Conscription";
        case Technology::petard_gate: return "Petard";
        case Technology::bombard_tower: return "Bombard Tower";
        case Technology::sanctity: return "Sanctity";
        case Technology::fervor: return "Fervor";
        case Technology::redemption: return "Redemption";
        case Technology::atonement: return "Atonement";
        case Technology::illumination: return "Illumination";
        case Technology::block_printing: return "Block Printing";
        case Technology::faith: return "Faith";
        case Technology::theocracy: return "Theocracy";
        case Technology::heresy: return "Heresy";
        case Technology::heavy_plow: return "Heavy Plow";
        case Technology::crop_rotation: return "Crop Rotation";
        case Technology::bow_saw: return "Bow Saw";
        case Technology::two_man_saw: return "Two-Man Saw";
        case Technology::gold_mining: return "Gold Mining";
        case Technology::gold_shaft_mining: return "Gold Shaft Mining";
        case Technology::stone_mining: return "Stone Mining";
        case Technology::stone_shaft_mining: return "Stone Shaft Mining";
        case Technology::hand_cart: return "Hand Cart";
        case Technology::fish_trap_gate: return "Fish Trap";
        case Technology::coinage: return "Coinage";
        case Technology::banking: return "Banking";
        case Technology::cartography: return "Cartography";
        case Technology::caravan: return "Caravan";
        case Technology::guilds: return "Guilds";
        case Technology::trade_cog_gate: return "Trade Cog";
        case Technology::outpost_gate: return "Outpost";
        case Technology::town_watch: return "Town Watch";
        case Technology::town_patrol: return "Town Patrol";
        case Technology::masonry: return "Masonry";
        case Technology::architecture: return "Architecture";
        case Technology::ballistics: return "Ballistics";
        case Technology::heated_shot: return "Heated Shot";
        case Technology::hoardings: return "Hoardings";
        case Technology::sappers: return "Sappers";
        case Technology::wonder_plans: return "Wonder Plans";
        case Technology::thumb_ring: return "Thumb Ring";
        case Technology::parthian_tactics: return "Parthian Tactics";
        case Technology::squires: return "Squires";
        case Technology::tracking: return "Tracking";
        case Technology::herbal_medicine: return "Herbal Medicine";
        case Technology::stone_cutting: return "Treadmill Crane";
        case Technology::spy_technology: return "Spies";
        case Technology::long_swordsman:
            return "Long Swordsman";
        case Technology::loom:
            return "Loom";
        case Technology::double_bit_axe:
            return "Double-Bit Axe";
        case Technology::horse_collar:
            return "Horse Collar";
        case Technology::fortified_wall:
            return "Fortified Wall";
        case Technology::guard_tower:
            return "Guard Tower";
        case Technology::keep:
            return "Keep";
        case Technology::bodkin_arrow:
            return "Bodkin Arrow";
        case Technology::bracer:
            return "Bracer";
        case Technology::iron_casting:
            return "Iron Casting";
        case Technology::blast_furnace:
            return "Blast Furnace";
        case Technology::scale_mail_armor:
            return "Scale Mail Armor";
        case Technology::chain_mail_armor:
            return "Chain Mail Armor";
        case Technology::plate_mail_armor:
            return "Plate Mail Armor";
        case Technology::scale_barding_armor:
            return "Scale Barding Armor";
        case Technology::chain_barding_armor:
            return "Chain Barding Armor";
        case Technology::plate_barding_armor:
            return "Plate Barding Armor";
        case Technology::padded_archer_armor:
            return "Padded Archer Armor";
        case Technology::leather_archer_armor:
            return "Leather Archer Armor";
        case Technology::ring_archer_armor:
            return "Ring Archer Armor";
        case Technology::bloodlines:
            return "Bloodlines";
        case Technology::husbandry:
            return "Husbandry";
        case Technology::cavalier:
            return "Cavalier";
        case Technology::paladin:
            return "Paladin";
        case Technology::light_cavalry:
            return "Light Cavalry";
        case Technology::hussar:
            return "Hussar";
        case Technology::two_handed_swordsman:
            return "Two-Handed Swordsman";
        case Technology::champion:
            return "Champion";
        case Technology::arbalester:
            return "Arbalester";
        case Technology::elite_skirmisher:
            return "Elite Skirmisher";
        case Technology::war_galley: return "War Galley";
        case Technology::galleon: return "Galleon";
        case Technology::fast_fire_ship: return "Fast Fire Ship";
        case Technology::heavy_demolition_ship:
            return "Heavy Demolition Ship";
        case Technology::cannon_galleon: return "Cannon Galleon";
        case Technology::elite_cannon_galleon:
            return "Elite Cannon Galleon";
        case Technology::careening: return "Careening";
        case Technology::dry_dock: return "Dry Dock";
        case Technology::shipwright: return "Shipwright";
        case Technology::longboat: return "Longboat";
        case Technology::elite_longboat: return "Elite Longboat";
        case Technology::turtle_ship: return "Turtle Ship";
        case Technology::elite_turtle_ship: return "Elite Turtle Ship";
        case Technology::longbowman: return "Longbowman";
        case Technology::elite_longbowman: return "Elite Longbowman";
        case Technology::throwing_axeman: return "Throwing Axeman";
        case Technology::elite_throwing_axeman:
            return "Elite Throwing Axeman";
        case Technology::huskarl: return "Huskarl";
        case Technology::elite_huskarl: return "Elite Huskarl";
        case Technology::teutonic_knight: return "Teutonic Knight";
        case Technology::elite_teutonic_knight:
            return "Elite Teutonic Knight";
        case Technology::samurai: return "Samurai";
        case Technology::elite_samurai: return "Elite Samurai";
        case Technology::chu_ko_nu: return "Chu Ko Nu";
        case Technology::elite_chu_ko_nu: return "Elite Chu Ko Nu";
        case Technology::cataphract: return "Cataphract";
        case Technology::elite_cataphract: return "Elite Cataphract";
        case Technology::war_elephant: return "War Elephant";
        case Technology::elite_war_elephant: return "Elite War Elephant";
        case Technology::mameluke: return "Mameluke";
        case Technology::elite_mameluke: return "Elite Mameluke";
        case Technology::janissary: return "Janissary";
        case Technology::elite_janissary: return "Elite Janissary";
        case Technology::berserk: return "Berserk";
        case Technology::elite_berserk: return "Elite Berserk";
        case Technology::mangudai: return "Mangudai";
        case Technology::elite_mangudai: return "Elite Mangudai";
        case Technology::berserkergang: return "Berserkergang";
        case Technology::jaguar_warrior: return "Jaguar Warrior";
        case Technology::elite_jaguar_warrior:
            return "Elite Jaguar Warrior";
        case Technology::plumed_archer: return "Plumed Archer";
        case Technology::elite_plumed_archer: return "Elite Plumed Archer";
        case Technology::conquistador: return "Conquistador";
        case Technology::elite_conquistador: return "Elite Conquistador";
        case Technology::tarkan: return "Tarkan";
        case Technology::elite_tarkan: return "Elite Tarkan";
        case Technology::woad_raider: return "Woad Raider";
        case Technology::elite_woad_raider: return "Elite Woad Raider";
        case Technology::yeomen: return "Yeomen";
        case Technology::bearded_axe: return "Bearded Axe";
        case Technology::anarchy: return "Anarchy";
        case Technology::crenellations: return "Crenellations";
        case Technology::kataparuto: return "Kataparuto";
        case Technology::rocketry: return "Rocketry";
        case Technology::logistica: return "Logistica";
        case Technology::mahouts: return "Mahouts";
        case Technology::zealotry: return "Zealotry";
        case Technology::artillery: return "Artillery";
        case Technology::drill: return "Drill";
        case Technology::supremacy: return "Supremacy";
        case Technology::atheism: return "Atheism";
        case Technology::shinkichon: return "Shinkichon";
        case Technology::el_dorado: return "El Dorado";
        case Technology::elite_eagle_warrior: return "Elite Eagle Warrior";
        case Technology::heavy_scorpion: return "Heavy Scorpion";
        case Technology::onager: return "Onager";
        case Technology::siege_onager: return "Siege Onager";
        case Technology::heavy_cavalry_archer:
            return "Heavy Cavalry Archer";
        case Technology::heavy_camel: return "Heavy Camel";
        case Technology::capped_ram: return "Capped Ram";
        case Technology::siege_ram: return "Siege Ram";
    }
    return "Unknown Technology";
}

ResourceKind resource_for(Terrain terrain) {
    switch (terrain) {
        case Terrain::forest:
        case Terrain::pine_forest:
        case Terrain::oak_forest:
        case Terrain::bamboo_forest:
        case Terrain::palm_forest:
        case Terrain::jungle_forest:
            return ResourceKind::wood;
        case Terrain::berry_bush:
            return ResourceKind::food;
        case Terrain::gold_mine:
            return ResourceKind::gold;
        case Terrain::stone_mine:
            return ResourceKind::stone;
        case Terrain::fish:
        case Terrain::fish_shore:
        case Terrain::fish_deep:
            return ResourceKind::food;
        case Terrain::grass:
        case Terrain::grass2:
        case Terrain::dirt:
        case Terrain::dirt2:
        case Terrain::dirt3:
        case Terrain::road:
        case Terrain::snow:
        case Terrain::ice:
        case Terrain::water:
        case Terrain::deep_water:
        case Terrain::beach:
        case Terrain::shallows:
            return ResourceKind::none;
    }
    return ResourceKind::none;
}

bool accepts_resource(BuildingKind building, ResourceKind resource) {
    if (building == BuildingKind::town_center) {
        return resource != ResourceKind::none;
    }
    if (building == BuildingKind::lumber_camp) {
        return resource == ResourceKind::wood;
    }
    if (building == BuildingKind::mill) {
        return resource == ResourceKind::food;
    }
    if (building == BuildingKind::dock) {
        return resource == ResourceKind::food;
    }
    if (building == BuildingKind::mining_camp) {
        return resource == ResourceKind::gold ||
               resource == ResourceKind::stone;
    }
    return false;
}

int damage_after_armor(
    int attack,
    DamageClass damage_class,
    int melee_armor,
    int pierce_armor
) {
    const int armor = damage_class == DamageClass::melee
        ? melee_armor
        : pierce_armor;
    return std::max(1, attack - armor);
}

float commercial_conversion_class_resistance(UnitKind kind) {
    switch (kind) {
        // Live VER 5.7 unit classes recovered as 2, 20, 21, or 22.
        case UnitKind::fishing_ship:
        case UnitKind::galley:
        case UnitKind::war_galley:
        case UnitKind::galleon:
        case UnitKind::transport_ship:
        case UnitKind::fire_ship:
        case UnitKind::fast_fire_ship:
        case UnitKind::demolition_ship:
        case UnitKind::heavy_demolition_ship:
        case UnitKind::cannon_galleon:
        case UnitKind::elite_cannon_galleon:
        case UnitKind::longboat:
        case UnitKind::elite_longboat:
        case UnitKind::turtle_ship:
        case UnitKind::elite_turtle_ship:
        case UnitKind::trade_cog:
            return 3.0F;
        default:
            break;
    }
    switch (kind) {
        // Commercial IDs 448, 546, 441, 751, and 752 add eight.
        case UnitKind::scout_cavalry:
        case UnitKind::light_cavalry:
        case UnitKind::hussar:
        case UnitKind::eagle_warrior:
        case UnitKind::elite_eagle_warrior:
            return 8.0F;
        default:
            return 0.0F;
    }
}

Simulation::Simulation(GameMap map)
    : map_(std::move(map)) {
    const auto tile_count =
        static_cast<std::size_t>(map_.width() * map_.height());
    for (std::size_t index = 0; index < player_states_.size(); ++index) {
        PlayerState& state = player_states_[index];
        state.explored.assign(tile_count, false);
        if (index >= 2) {
            state.controller = PlayerControllerState::observer;
        }
    }
}

int Simulation::consume_commercial_random() {
    // Microsoft CRT used by AoK HD: state is shared by every rand() consumer.
    commercial_random_state_ =
        commercial_random_state_ * 214013U + 2531011U;
    return static_cast<int>((commercial_random_state_ >> 16U) & 0x7fffU);
}

Simulation Simulation::create_demo() {
    Simulation simulation(GameMap::create_demo_map());
    simulation.add_unit(UnitKind::villager, Player::blue, {2, 7});
    simulation.add_unit(UnitKind::scout_cavalry, Player::blue, {4, 9});
    simulation.add_unit(UnitKind::villager, Player::red, {21, 8});
    simulation.add_unit(UnitKind::scout_cavalry, Player::red, {19, 6});
    simulation.add_building(BuildingKind::town_center, Player::blue, {0, 10});
    simulation.add_building(BuildingKind::town_center, Player::red, {20, 0});
    return simulation;
}

RosterMatchOutcome Simulation::roster_outcome() const {
    struct SurvivingGroup {
        int key{};
        std::vector<std::uint8_t> slots;
    };
    std::vector<SurvivingGroup> groups;
    for (std::size_t index = 0; index < player_states_.size(); ++index) {
        const PlayerSlotId slot = *PlayerSlotId::from_index(index);
        const MatchRosterSlot& entry = roster_.slot(slot);
        if (!entry.occupied ||
            player_states_[index].controller ==
                PlayerControllerState::resigned) {
            continue;
        }
        const EntityOwner owner = entity_owner_from_slot(slot);
        const bool alive = std::ranges::any_of(
            units_, [owner](const Unit& unit) {
                return unit.owner == owner && unit.hit_points > 0 &&
                    !is_animal(unit.kind) && !is_relic(unit.kind);
            }
        ) || std::ranges::any_of(
            buildings_, [owner](const Building& building) {
                return building.owner == owner && building.hit_points > 0;
            }
        );
        if (!alive) continue;
        const int key = entry.team.has_team()
            ? entry.team.number()
            : -static_cast<int>(index) - 1;
        auto group = std::ranges::find(groups, key, &SurvivingGroup::key);
        if (group == groups.end()) {
            groups.push_back({key, {}});
            group = std::prev(groups.end());
        }
        group->slots.push_back(static_cast<std::uint8_t>(index));
    }
    if (groups.empty()) return {RosterOutcomeStatus::draw, {}, {}};
    bool hostile_groups = false;
    for (std::size_t left = 0; left < groups.size(); ++left) {
        for (std::size_t right = left + 1; right < groups.size(); ++right) {
            for (std::uint8_t first : groups[left].slots) {
                for (std::uint8_t second : groups[right].slots) {
                    const auto a = *PlayerSlotId::from_index(first);
                    const auto b = *PlayerSlotId::from_index(second);
                    hostile_groups = hostile_groups ||
                        roster_diplomacy_.stance(a, b) == Diplomacy::enemy ||
                        roster_diplomacy_.stance(b, a) == Diplomacy::enemy;
                }
            }
        }
    }
    if (groups.size() > 1 && hostile_groups) return {};
    RosterMatchOutcome result;
    result.status = RosterOutcomeStatus::victory;
    for (const SurvivingGroup& group : groups) {
        result.winning_slots.insert(
            result.winning_slots.end(),
            group.slots.begin(),
            group.slots.end()
        );
    }
    std::ranges::sort(result.winning_slots);
    if (groups.size() == 1 && groups.front().key > 0) {
        result.winning_team = groups.front().key;
    }
    return result;
}

std::optional<MatchOutcome> Simulation::legacy_roster_outcome() const {
    const RosterMatchOutcome value = roster_outcome();
    if (value.status == RosterOutcomeStatus::ongoing) {
        return MatchOutcome::ongoing;
    }
    if (value.status == RosterOutcomeStatus::draw) {
        return MatchOutcome::draw;
    }
    if (value.winning_slots == std::vector<std::uint8_t>{0}) {
        return MatchOutcome::blue_victory;
    }
    if (value.winning_slots == std::vector<std::uint8_t>{1}) {
        return MatchOutcome::red_victory;
    }
    if (value.winning_slots == std::vector<std::uint8_t>{0, 1}) {
        return MatchOutcome::allied_victory;
    }
    return std::nullopt;
}

const Economy& Simulation::economy(Player player) const {
    const auto slot = player_slot_from_legacy(player);
    if (!slot || slot->is_neutral()) {
        throw std::invalid_argument("neutral has no economy");
    }
    return economy(*slot);
}

const Economy& Simulation::economy(PlayerSlotId player) const {
    return player_state(player).economy;
}

const Economy& Simulation::economy(EntityOwner player) const {
    const auto slot = entity_owner_slot(player);
    if (!slot || slot->is_neutral()) {
        throw std::invalid_argument("neutral has no economy");
    }
    return economy(*slot);
}

PlayerStatistics& Simulation::mutable_statistics(Player player) {
    return match_statistics_.players[player == Player::red ? 1U : 0U];
}

PlayerStatistics& Simulation::mutable_statistics(EntityOwner player) {
    const auto slot = entity_owner_slot(player);
    if (!slot || slot->is_neutral()) {
        throw std::invalid_argument("neutral has no statistics");
    }
    return match_statistics_.players[*slot->index()];
}

MatchStatistics Simulation::match_statistics() const {
    MatchStatistics result = match_statistics_;
    result.current_score = {
        score(Player::blue), score(Player::red),
    };
    for (std::size_t index = 0; index < roster_.slots().size(); ++index) {
        result.active_slots[index] = roster_.slots()[index].occupied;
        result.team_numbers[index] =
            roster_.slots()[index].team.number();
    }
    return result;
}

LegacyMatchStatistics Simulation::legacy_match_statistics() const {
    const MatchStatistics source = match_statistics();
    LegacyMatchStatistics result;
    std::copy_n(source.players.begin(), 2, result.players.begin());
    std::copy_n(source.current_score.begin(), 2, result.current_score.begin());
    result.timeline.reserve(source.timeline.size());
    for (const StatisticsTimelineSample& sample : source.timeline) {
        LegacyStatisticsTimelineSample legacy;
        legacy.tick = sample.tick;
        std::copy_n(sample.score.begin(), 2, legacy.score.begin());
        std::copy_n(sample.population.begin(), 2, legacy.population.begin());
        std::copy_n(sample.gathered.begin(), 2, legacy.gathered.begin());
        result.timeline.push_back(std::move(legacy));
    }
    return result;
}

const PlayerStatistics& Simulation::player_statistics(
    PlayerSlotId player
) const {
    const auto index = player.index();
    if (!index) {
        throw std::invalid_argument("neutral has no statistics");
    }
    return match_statistics_.players[*index];
}

void Simulation::replace_match_statistics(MatchStatistics statistics) {
    match_statistics_ = std::move(statistics);
}

void Simulation::credit_gathered(
    Player player, ResourceKind resource, int amount
) {
    if (player == Player::neutral || amount <= 0) return;
    ResourceStatistics& gathered = mutable_statistics(player).gathered;
    switch (resource) {
        case ResourceKind::food: gathered.food += amount; break;
        case ResourceKind::wood: gathered.wood += amount; break;
        case ResourceKind::gold: gathered.gold += amount; break;
        case ResourceKind::stone: gathered.stone += amount; break;
        case ResourceKind::none: break;
    }
}

void Simulation::sample_match_statistics() {
    match_statistics_.timeline.push_back({
        tick_number_,
        {score(Player::blue), score(Player::red)},
        {population(Player::blue), population(Player::red)},
        {
            mutable_statistics(Player::blue).gathered,
            mutable_statistics(Player::red).gathered,
        },
    });
}

int Simulation::population(Player player) const {
    return static_cast<int>(std::ranges::count_if(
        units_,
        [player](const Unit& unit) {
            return unit.owner == player &&
                !is_animal(unit.kind) && !is_relic(unit.kind);
        }
    ));
}

int Simulation::population_capacity(Player player) const {
    int capacity = 0;
    for (const Building& building : buildings_) {
        if (building.owner == player && building.completed()) {
            capacity += rules_for(building.kind).population_support;
        }
    }
    if (civilization(player) == Civilization::goths &&
        age(player) == Age::imperial) {
        capacity += 10;
    }
    if (civilization(player) == Civilization::huns) {
        capacity = std::max(capacity, 200);
    }
    return capacity;
}

int Simulation::farm_capacity(Player player) const {
    const int base =
        has_technology(player, Technology::crop_rotation) ? 550 :
        has_technology(player, Technology::heavy_plow) ? 375 :
        has_technology(player, Technology::horse_collar) ? 250 : 175;
    return base +
        (team_has_civilization(player, Civilization::chinese) ? 45 : 0);
}

int Simulation::effective_building_wood_cost(
    Player player,
    BuildingKind kind
) const {
    int numerator = 100;
    if (kind == BuildingKind::farm &&
        civilization(player) == Civilization::teutons) {
        numerator = 60;
    } else if (kind == BuildingKind::dock &&
               team_has_civilization(player, Civilization::vikings)) {
        numerator = 85;
    } else if (is_wall(kind) &&
               team_has_civilization(player, Civilization::mayans)) {
        numerator = 50;
    }
    return rules_for(kind).wood_cost * numerator / 100;
}

int Simulation::effective_building_stone_cost(
    Player player,
    BuildingKind kind
) const {
    return rules_for(kind).stone_cost *
        (is_wall(kind) &&
         team_has_civilization(player, Civilization::mayans) ? 50 : 100) /
        100;
}

int Simulation::effective_carry_capacity(const Unit& unit) const {
    return carry_capacity(unit);
}

int Simulation::effective_ship_movement_numerator(
    const Unit& unit
) const {
    return ship_movement_numerator(unit);
}

Age Simulation::age(Player player) const {
    const auto slot = player_slot_from_legacy(player);
    if (!slot || slot->is_neutral()) {
        throw std::invalid_argument("neutral has no age");
    }
    return age(*slot);
}

Age Simulation::age(PlayerSlotId player) const {
    return player_state(player).age;
}

Age Simulation::age(EntityOwner player) const {
    const auto slot = entity_owner_slot(player);
    // Gaia has no player-state age, but shared unit calculations still need a
    // stable baseline for neutral animals and relics.
    if (!slot || slot->is_neutral()) return Age::dark;
    return age(*slot);
}

bool Simulation::has_technology(
    Player player,
    Technology technology
) const {
    const auto slot = player_slot_from_legacy(player);
    if (!slot || slot->is_neutral()) return false;
    return has_technology(*slot, technology);
}

bool Simulation::has_technology(
    PlayerSlotId player,
    Technology technology
) const {
    if (technology == Technology::petard_gate) {
        return age(player) >= Age::castle;
    }
    if (technology == Technology::fish_trap_gate) {
        return age(player) >= Age::feudal;
    }
    if (technology == Technology::trade_cog_gate) {
        return age(player) >= Age::feudal;
    }
    if (technology == Technology::outpost_gate) {
        return true;
    }
    if (technology == Technology::wonder_plans) {
        return age(player) >= Age::imperial;
    }
    if (civilization(player) == Civilization::vikings) {
        if (technology == Technology::wheelbarrow &&
            age(player) >= Age::feudal) {
            return true;
        }
        if (technology == Technology::hand_cart &&
            age(player) >= Age::castle) {
            return true;
        }
    }
    if (civilization(player) == Civilization::koreans) {
        if (technology == Technology::guard_tower &&
            age(player) >= Age::castle) {
            return true;
        }
        if (technology == Technology::keep &&
            age(player) >= Age::imperial) {
            return true;
        }
    }
    const auto& technologies = player_state(player).technologies;
    return technologies.at(static_cast<std::size_t>(technology));
}

bool Simulation::has_technology(
    EntityOwner player,
    Technology technology
) const {
    const auto slot = entity_owner_slot(player);
    return slot && !slot->is_neutral() &&
        has_technology(*slot, technology);
}

float Simulation::effective_commercial_attribute(
    EntityOwner owner,
    CommercialObjectIdentity identity,
    std::int16_t attribute,
    float base
) const {
    const auto slot = owner.slot_index();
    const auto* object = commercial_content_catalog().object(
        identity.civilization_id, identity.object_id
    );
    if (!slot || object == nullptr) return base;
    float value = base;
    const auto apply_effect = [&](CommercialEffectId effect_id) {
        const auto* effect = commercial_content_catalog().effect(effect_id);
        if (!effect) return;
        for (const auto& command : effect->commands) {
            if (command.attribute_id != attribute ||
                (command.object_id >= 0 &&
                 command.object_id != identity.object_id) ||
                (command.unit_class >= 0 &&
                 command.unit_class != object->unit_class) ||
                (command.packed_class && (attribute == 8 || attribute == 9))) {
                continue;
            }
            if (command.type == 0) value = command.amount;
            else if (command.type == 4) value += command.amount;
            else if (command.type == 5) value *= command.amount;
        }
    };
    const PlayerState& player = player_states_[*slot];
    if (player.commercial_civilization) {
        if (const auto bonus = commercial_content_catalog()
                .civilization_bonus_effect(*player.commercial_civilization)) {
            apply_effect(*bonus);
        }
    }
    for (std::size_t id = 0; id < player.commercial_technologies.size(); ++id) {
        if (!player.commercial_technologies[id]) continue;
        if (const auto* technology = commercial_content_catalog().technology(
                static_cast<CommercialTechnologyId>(id))) {
            apply_effect(technology->effect_id);
        }
    }
    return value;
}

int Simulation::effective_commercial_class_amount(
    EntityOwner owner,
    CommercialObjectIdentity identity,
    std::int16_t attribute,
    std::int16_t class_id,
    int base
) const {
    const auto slot = owner.slot_index();
    const auto* object = commercial_content_catalog().object(
        identity.civilization_id, identity.object_id
    );
    if (!slot || !object) return base;
    int value = base;
    const auto apply_effect = [&](CommercialEffectId effect_id) {
        const auto* effect = commercial_content_catalog().effect(effect_id);
        if (!effect) return;
        for (const auto& command : effect->commands) {
            if (command.attribute_id != attribute ||
                command.packed_class != class_id || !command.packed_amount ||
                (command.object_id >= 0 && command.object_id != identity.object_id) ||
                (command.unit_class >= 0 && command.unit_class != object->unit_class)) {
                continue;
            }
            if (command.type == 0) value = *command.packed_amount;
            else if (command.type == 4) value += *command.packed_amount;
            else if (command.type == 5) {
                value = value * *command.packed_amount / 100;
            }
        }
    };
    const PlayerState& player = player_states_[*slot];
    if (player.commercial_civilization) {
        if (const auto bonus = commercial_content_catalog()
                .civilization_bonus_effect(*player.commercial_civilization)) {
            apply_effect(*bonus);
        }
    }
    for (std::size_t id = 0; id < player.commercial_technologies.size(); ++id) {
        if (!player.commercial_technologies[id]) continue;
        if (const auto* technology = commercial_content_catalog().technology(
                static_cast<CommercialTechnologyId>(id))) {
            apply_effect(technology->effect_id);
        }
    }
    return value;
}

int Simulation::commercial_damage(
    const Unit& attacker, const Unit& defender
) const {
    const auto* record = commercial_content_catalog().object(
        attacker.commercial_identity->civilization_id,
        attacker.commercial_identity->object_id
    );
    if (!record) return 0;
    const auto target_armor = [&](std::int16_t class_id) {
        if (!defender.commercial_identity) {
            if (class_id == 3) return pierce_armor(defender);
            if (class_id == 4) return melee_armor(defender);
            return 0;
        }
        const auto* target = commercial_content_catalog().object(
            defender.commercial_identity->civilization_id,
            defender.commercial_identity->object_id
        );
        if (!target) return 0;
        const auto found = std::ranges::find_if(
            target->armors, [class_id](const auto& value) {
                return value.class_id == class_id;
            }
        );
        return effective_commercial_class_amount(
            defender.owner, *defender.commercial_identity, 8, class_id,
            found == target->armors.end() ? 0 : found->amount
        );
    };
    int total{};
    for (const auto& attack : record->attacks) {
        const int amount = effective_commercial_class_amount(
            attacker.owner, *attacker.commercial_identity, 9,
            attack.class_id, attack.amount
        );
        total += std::max(
            0, amount - target_armor(attack.class_id)
        );
    }
    return record->attacks.empty() ? std::max(1, attacker.attack)
                                   : std::max(1, total);
}

int Simulation::commercial_damage(
    const Unit& attacker, const Building& defender
) const {
    const auto* record = commercial_content_catalog().object(
        attacker.commercial_identity->civilization_id,
        attacker.commercial_identity->object_id
    );
    if (!record) return 0;
    const auto target_armor = [&](std::int16_t class_id) {
        if (!defender.commercial_identity) {
            if (class_id == 3) return pierce_armor(defender);
            if (class_id == 4) return melee_armor(defender);
            return 0;
        }
        const auto* target = commercial_content_catalog().object(
            defender.commercial_identity->civilization_id,
            defender.commercial_identity->object_id
        );
        if (!target) return 0;
        const auto found = std::ranges::find_if(
            target->armors, [class_id](const auto& value) {
                return value.class_id == class_id;
            }
        );
        return effective_commercial_class_amount(
            defender.owner, *defender.commercial_identity, 8, class_id,
            found == target->armors.end() ? 0 : found->amount
        );
    };
    int total{};
    for (const auto& attack : record->attacks) {
        const int amount = effective_commercial_class_amount(
            attacker.owner, *attacker.commercial_identity, 9,
            attack.class_id, attack.amount
        );
        total += std::max(
            0, amount - target_armor(attack.class_id)
        );
    }
    return record->attacks.empty() ? std::max(1, attacker.attack)
                                   : std::max(1, total);
}

int Simulation::maximum_hit_points(const Unit& unit) const {
    if (unit.commercial_identity) {
        const auto* record = commercial_content_catalog().object(
            unit.commercial_identity->civilization_id,
            unit.commercial_identity->object_id
        );
        if (record) return std::max(1, static_cast<int>(std::lround(
            effective_commercial_attribute(
                unit.owner, *unit.commercial_identity, 0, record->hit_points
            )
        )));
    }
    const int base = rules_for(unit.kind).hit_points;
    const int frank_bonus =
        civilization(unit.owner) == Civilization::franks &&
        is_cavalry(unit.kind) ? base / 5 : 0;
    const bool infantry =
        unit.kind == UnitKind::militia ||
        unit.kind == UnitKind::man_at_arms ||
        unit.kind == UnitKind::long_swordsman ||
        unit.kind == UnitKind::two_handed_swordsman ||
        unit.kind == UnitKind::champion ||
        unit.kind == UnitKind::spearman ||
        unit.kind == UnitKind::pikeman ||
        unit.kind == UnitKind::halberdier ||
        unit.kind == UnitKind::throwing_axeman ||
        unit.kind == UnitKind::elite_throwing_axeman ||
        unit.kind == UnitKind::huskarl ||
        unit.kind == UnitKind::elite_huskarl ||
        unit.kind == UnitKind::teutonic_knight ||
        unit.kind == UnitKind::elite_teutonic_knight ||
        unit.kind == UnitKind::samurai ||
        unit.kind == UnitKind::elite_samurai ||
        unit.kind == UnitKind::berserk ||
        unit.kind == UnitKind::elite_berserk ||
        unit.kind == UnitKind::jaguar_warrior ||
        unit.kind == UnitKind::elite_jaguar_warrior;
    const int viking_percent =
        civilization(unit.owner) == Civilization::vikings && infantry &&
        age(unit.owner) >= Age::feudal ? 20 : 0;
    const bool scout_line =
        unit.kind == UnitKind::scout_cavalry ||
        unit.kind == UnitKind::light_cavalry ||
        unit.kind == UnitKind::hussar;
    const int mongol_bonus =
        civilization(unit.owner) == Civilization::mongols && scout_line
            ? base * 30 / 100 : 0;
    int aztec_monk_bonus{};
    if (civilization(unit.owner) == Civilization::aztecs &&
        unit.kind == UnitKind::monk) {
        for (Technology technology : {
                 Technology::sanctity,
                 Technology::fervor,
                 Technology::redemption,
                 Technology::atonement,
                 Technology::illumination,
                 Technology::block_printing,
                 Technology::faith,
                 Technology::theocracy,
                 Technology::heresy,
                 Technology::herbal_medicine,
             }) {
            aztec_monk_bonus +=
                has_technology(unit.owner, technology) ? 5 : 0;
        }
    }
    return base + frank_bonus + base * viking_percent / 100 +
        mongol_bonus + aztec_monk_bonus +
        (unit.kind == UnitKind::villager &&
         has_technology(unit.owner, Technology::loom)
            ? 15
            : 0) +
        (is_cavalry(unit.kind) &&
         has_technology(unit.owner, Technology::bloodlines)
            ? 20
            : 0) +
        ((unit.kind == UnitKind::mameluke ||
          unit.kind == UnitKind::elite_mameluke ||
          unit.kind == UnitKind::camel_rider ||
          unit.kind == UnitKind::heavy_camel) &&
         has_technology(unit.owner, Technology::zealotry)
            ? 30
            : 0) +
        ((unit.kind == UnitKind::eagle_warrior ||
          unit.kind == UnitKind::elite_eagle_warrior) &&
         has_technology(unit.owner, Technology::el_dorado)
            ? 40
            : 0) +
        (unit.kind == UnitKind::villager &&
         has_technology(unit.owner, Technology::supremacy)
            ? 40
            : 0) +
        ((unit.kind == UnitKind::monk ||
          unit.kind == UnitKind::missionary) &&
         has_technology(unit.owner, Technology::sanctity)
            ? 15
            : 0);
}

namespace {

bool is_ram(UnitKind kind) {
    return kind == UnitKind::battering_ram ||
           kind == UnitKind::capped_ram ||
           kind == UnitKind::siege_ram;
}

bool is_wall(BuildingKind kind) {
    return kind == BuildingKind::palisade_wall ||
           kind == BuildingKind::stone_wall ||
           kind == BuildingKind::palisade_gate_x ||
           kind == BuildingKind::palisade_gate_y ||
           kind == BuildingKind::stone_gate_x ||
           kind == BuildingKind::stone_gate_y ||
           kind == BuildingKind::fortified_wall ||
           kind == BuildingKind::fortified_gate_x ||
           kind == BuildingKind::fortified_gate_y;
}

bool receives_class_3_or_52_effects(BuildingKind kind) {
    return !is_wall(kind) &&
        kind != BuildingKind::farm &&
        kind != BuildingKind::fish_trap;
}

bool has_stone_defense_class_13(BuildingKind kind) {
    return kind == BuildingKind::watch_tower ||
        kind == BuildingKind::guard_tower ||
        kind == BuildingKind::keep ||
        kind == BuildingKind::bombard_tower ||
        kind == BuildingKind::stone_wall ||
        kind == BuildingKind::fortified_wall;
}

bool ballistics_tracks(UnitKind kind) {
    return kind == UnitKind::archer ||
        kind == UnitKind::crossbowman ||
        kind == UnitKind::arbalester ||
        kind == UnitKind::skirmisher ||
        kind == UnitKind::elite_skirmisher ||
        kind == UnitKind::cavalry_archer ||
        kind == UnitKind::heavy_cavalry_archer ||
        kind == UnitKind::longbowman ||
        kind == UnitKind::elite_longbowman ||
        kind == UnitKind::chu_ko_nu ||
        kind == UnitKind::elite_chu_ko_nu ||
        kind == UnitKind::mangudai ||
        kind == UnitKind::elite_mangudai ||
        kind == UnitKind::plumed_archer ||
        kind == UnitKind::elite_plumed_archer ||
        kind == UnitKind::galley ||
        kind == UnitKind::war_galley ||
        kind == UnitKind::galleon ||
        kind == UnitKind::longboat ||
        kind == UnitKind::elite_longboat ||
        kind == UnitKind::turtle_ship ||
        kind == UnitKind::elite_turtle_ship;
}

bool ballistics_tracks(BuildingKind kind) {
    return kind == BuildingKind::town_center ||
        kind == BuildingKind::castle ||
        kind == BuildingKind::watch_tower ||
        kind == BuildingKind::guard_tower ||
        kind == BuildingKind::keep ||
        kind == BuildingKind::bombard_tower;
}

int class_11_building_armor(
    const Simulation& simulation,
    const Building& building
) {
    if (!receives_class_3_or_52_effects(building.kind)) return 0;
    return
        (simulation.has_technology(
            building.owner, Technology::masonry
        ) ? 3 : 0) +
        (simulation.has_technology(
            building.owner, Technology::architecture
        ) ? 3 : 0);
}

bool receives_siege_engineers(UnitKind kind) {
    return is_ram(kind) ||
           kind == UnitKind::mangonel ||
           kind == UnitKind::onager ||
           kind == UnitKind::siege_onager ||
           kind == UnitKind::scorpion ||
           kind == UnitKind::heavy_scorpion ||
           kind == UnitKind::packed_trebuchet ||
           kind == UnitKind::trebuchet ||
           kind == UnitKind::bombard_cannon;
}

bool receives_infantry_armor(UnitKind kind) {
    return kind == UnitKind::militia ||
           kind == UnitKind::man_at_arms ||
           kind == UnitKind::long_swordsman ||
           kind == UnitKind::two_handed_swordsman ||
           kind == UnitKind::champion ||
           kind == UnitKind::spearman ||
           kind == UnitKind::pikeman ||
           kind == UnitKind::halberdier ||
           kind == UnitKind::throwing_axeman ||
           kind == UnitKind::elite_throwing_axeman ||
           kind == UnitKind::huskarl ||
           kind == UnitKind::elite_huskarl ||
           kind == UnitKind::teutonic_knight ||
           kind == UnitKind::elite_teutonic_knight ||
           kind == UnitKind::samurai ||
           kind == UnitKind::elite_samurai ||
           kind == UnitKind::berserk ||
           kind == UnitKind::elite_berserk ||
           kind == UnitKind::jaguar_warrior ||
           kind == UnitKind::elite_jaguar_warrior;
}

bool receives_cavalry_armor(UnitKind kind) {
    return kind == UnitKind::knight ||
           kind == UnitKind::scout_cavalry ||
           kind == UnitKind::cavalier ||
           kind == UnitKind::paladin ||
           kind == UnitKind::light_cavalry ||
           kind == UnitKind::hussar ||
           kind == UnitKind::cataphract ||
           kind == UnitKind::elite_cataphract ||
           kind == UnitKind::war_elephant ||
           kind == UnitKind::elite_war_elephant ||
           kind == UnitKind::mameluke ||
           kind == UnitKind::elite_mameluke ||
           kind == UnitKind::tarkan ||
           kind == UnitKind::elite_tarkan ||
           kind == UnitKind::camel_rider ||
           kind == UnitKind::heavy_camel;
}

bool receives_archer_armor(UnitKind kind) {
    return kind == UnitKind::archer ||
           kind == UnitKind::crossbowman ||
           kind == UnitKind::arbalester ||
           kind == UnitKind::skirmisher ||
           kind == UnitKind::elite_skirmisher ||
           kind == UnitKind::longbowman ||
           kind == UnitKind::elite_longbowman ||
           kind == UnitKind::chu_ko_nu ||
           kind == UnitKind::elite_chu_ko_nu ||
           kind == UnitKind::janissary ||
           kind == UnitKind::elite_janissary ||
           kind == UnitKind::mangudai ||
           kind == UnitKind::elite_mangudai ||
           kind == UnitKind::plumed_archer ||
           kind == UnitKind::elite_plumed_archer ||
           kind == UnitKind::conquistador ||
           kind == UnitKind::elite_conquistador ||
           kind == UnitKind::cavalry_archer ||
           kind == UnitKind::heavy_cavalry_archer ||
           kind == UnitKind::hand_cannoneer;
}

}  // namespace

int Simulation::maximum_hit_points(const Building& building) const {
    if (building.commercial_identity) {
        const auto* record = commercial_content_catalog().object(
            building.commercial_identity->civilization_id,
            building.commercial_identity->object_id
        );
        if (record) return std::max(1, static_cast<int>(std::lround(
            effective_commercial_attribute(
                building.owner, *building.commercial_identity, 0,
                record->hit_points
            )
        )));
    }
    int maximum = rules_for(building.kind).hit_points;
    if (building.kind == BuildingKind::watch_tower &&
        has_technology(building.owner, Technology::keep)) {
        maximum = 2250;
    }
    else if (building.kind == BuildingKind::watch_tower &&
        has_technology(building.owner, Technology::guard_tower)) {
        maximum = 1500;
    }
    if (has_technology(building.owner, Technology::fortified_wall)) {
        if (building.kind == BuildingKind::stone_wall) {
            maximum = 3000;
        }
        if (building.kind == BuildingKind::stone_gate_x ||
            building.kind == BuildingKind::stone_gate_y) {
            maximum = 4000;
        }
    }
    if (civilization(building.owner) == Civilization::byzantines) {
        const int percent =
            age(building.owner) >= Age::imperial ? 40 :
            age(building.owner) >= Age::castle ? 30 :
            age(building.owner) >= Age::feudal ? 20 : 10;
        maximum += maximum * percent / 100;
    }
    if (civilization(building.owner) == Civilization::persians &&
        building.kind == BuildingKind::town_center) {
        maximum *= 2;
    }
    if (receives_class_3_or_52_effects(building.kind) &&
        has_technology(building.owner, Technology::masonry)) {
        maximum = maximum * 110 / 100;
    }
    if (receives_class_3_or_52_effects(building.kind) &&
        has_technology(building.owner, Technology::architecture)) {
        maximum = maximum * 110 / 100;
    }
    if (building.kind == BuildingKind::castle &&
        has_technology(building.owner, Technology::hoardings)) {
        maximum = maximum * 121 / 100;
    }
    return maximum;
}

int Simulation::melee_armor(const Unit& unit) const {
    if (unit.commercial_identity) {
        const auto* record = commercial_content_catalog().object(
            unit.commercial_identity->civilization_id,
            unit.commercial_identity->object_id
        );
        if (record) {
            const auto found = std::ranges::find_if(
                record->armors,
                [](const CommercialClassAmount& value) {
                    return value.class_id == 4;
                }
            );
            return effective_commercial_class_amount(
                unit.owner, *unit.commercial_identity, 8, 4,
                found == record->armors.end() ? 0 : found->amount
            );
        }
    }
    return rules_for(unit.kind).melee_armor +
        (unit.kind == UnitKind::villager &&
         has_technology(unit.owner, Technology::loom)
            ? 1
            : 0) +
        (receives_infantry_armor(unit.kind) &&
         has_technology(unit.owner, Technology::scale_mail_armor)
            ? 1
            : 0) +
        (receives_infantry_armor(unit.kind) &&
         has_technology(unit.owner, Technology::chain_mail_armor)
            ? 1
            : 0) +
        (receives_infantry_armor(unit.kind) &&
         has_technology(unit.owner, Technology::plate_mail_armor)
            ? 1
            : 0) +
        (receives_cavalry_armor(unit.kind) &&
         has_technology(unit.owner, Technology::scale_barding_armor)
            ? 1
            : 0) +
        (receives_cavalry_armor(unit.kind) &&
         has_technology(unit.owner, Technology::chain_barding_armor)
            ? 1
            : 0) +
        (receives_cavalry_armor(unit.kind) &&
         has_technology(unit.owner, Technology::plate_barding_armor)
            ? 1
            : 0) +
        (receives_archer_armor(unit.kind) &&
         has_technology(unit.owner, Technology::padded_archer_armor)
            ? 1
            : 0) +
        (receives_archer_armor(unit.kind) &&
         has_technology(unit.owner, Technology::leather_archer_armor)
            ? 1
            : 0) +
        (receives_archer_armor(unit.kind) &&
         has_technology(unit.owner, Technology::ring_archer_armor)
            ? 1
            : 0) +
        (unit.kind == UnitKind::villager &&
         has_technology(unit.owner, Technology::supremacy)
            ? 2
            : 0) +
        ((unit.kind == UnitKind::cavalry_archer ||
          unit.kind == UnitKind::heavy_cavalry_archer ||
          unit.kind == UnitKind::mangudai ||
          unit.kind == UnitKind::elite_mangudai) &&
         has_technology(unit.owner, Technology::parthian_tactics)
            ? 1
            : 0);
}

int Simulation::melee_armor(const Building& building) const {
    if (building.commercial_identity) {
        const auto* record = commercial_content_catalog().object(
            building.commercial_identity->civilization_id,
            building.commercial_identity->object_id
        );
        if (record) {
            const auto found = std::ranges::find_if(
                record->armors,
                [](const CommercialClassAmount& value) {
                    return value.class_id == 4;
                }
            );
            return effective_commercial_class_amount(
                building.owner, *building.commercial_identity, 8, 4,
                found == record->armors.end() ? 0 : found->amount
            );
        }
    }
    int base = rules_for(building.kind).melee_armor;
    if (building.kind == BuildingKind::watch_tower &&
        has_technology(building.owner, Technology::keep)) {
        base = 3;
    }
    else if (building.kind == BuildingKind::watch_tower &&
        has_technology(building.owner, Technology::guard_tower)) {
        base = 2;
    }
    if (building.kind == BuildingKind::stone_wall &&
        has_technology(building.owner, Technology::fortified_wall)) {
        base = 12;
    }
    return base + (receives_class_3_or_52_effects(building.kind)
        ? (has_technology(building.owner, Technology::masonry) ? 1 : 0) +
          (has_technology(building.owner, Technology::architecture) ? 1 : 0)
        : 0);
}

int Simulation::pierce_armor(const Unit& unit) const {
    if (unit.commercial_identity) {
        const auto* record = commercial_content_catalog().object(
            unit.commercial_identity->civilization_id,
            unit.commercial_identity->object_id
        );
        if (record) {
            const auto found = std::ranges::find_if(
                record->armors,
                [](const CommercialClassAmount& value) {
                    return value.class_id == 3;
                }
            );
            return effective_commercial_class_amount(
                unit.owner, *unit.commercial_identity, 8, 3,
                found == record->armors.end() ? 0 : found->amount
            );
        }
    }
    const bool turk_scout =
        civilization(unit.owner) == Civilization::turks &&
        (unit.kind == UnitKind::scout_cavalry ||
         unit.kind == UnitKind::light_cavalry ||
         unit.kind == UnitKind::hussar);
    return rules_for(unit.kind).pierce_armor + (turk_scout ? 1 : 0) +
        (is_ship(unit.kind) &&
         has_technology(unit.owner, Technology::careening) ? 1 : 0) +
        (unit.kind == UnitKind::villager &&
         has_technology(unit.owner, Technology::loom)
            ? 2
            : 0) +
        (unit.kind == UnitKind::villager &&
         has_technology(unit.owner, Technology::supremacy)
            ? 2
            : 0) +
        (receives_infantry_armor(unit.kind) &&
         has_technology(unit.owner, Technology::scale_mail_armor)
            ? 1
            : 0) +
        (receives_infantry_armor(unit.kind) &&
         has_technology(unit.owner, Technology::chain_mail_armor)
            ? 1
            : 0) +
        (receives_infantry_armor(unit.kind) &&
         has_technology(unit.owner, Technology::plate_mail_armor)
            ? 2
            : 0) +
        (receives_cavalry_armor(unit.kind) &&
         has_technology(unit.owner, Technology::scale_barding_armor)
            ? 1
            : 0) +
        (receives_cavalry_armor(unit.kind) &&
         has_technology(unit.owner, Technology::chain_barding_armor)
            ? 1
            : 0) +
        (receives_cavalry_armor(unit.kind) &&
         has_technology(unit.owner, Technology::plate_barding_armor)
            ? 2
            : 0) +
        (receives_archer_armor(unit.kind) &&
         has_technology(unit.owner, Technology::padded_archer_armor)
            ? 1
            : 0) +
        (receives_archer_armor(unit.kind) &&
         has_technology(unit.owner, Technology::leather_archer_armor)
            ? 1
            : 0) +
        (receives_archer_armor(unit.kind) &&
         has_technology(unit.owner, Technology::ring_archer_armor)
            ? 2
            : 0) +
        ((unit.kind == UnitKind::cavalry_archer ||
          unit.kind == UnitKind::heavy_cavalry_archer ||
          unit.kind == UnitKind::mangudai ||
          unit.kind == UnitKind::elite_mangudai) &&
         has_technology(unit.owner, Technology::parthian_tactics)
            ? 2
            : 0);
}

int Simulation::pierce_armor(const Building& building) const {
    if (building.commercial_identity) {
        const auto* record = commercial_content_catalog().object(
            building.commercial_identity->civilization_id,
            building.commercial_identity->object_id
        );
        if (record) {
            const auto found = std::ranges::find_if(
                record->armors,
                [](const CommercialClassAmount& value) {
                    return value.class_id == 3;
                }
            );
            return effective_commercial_class_amount(
                building.owner, *building.commercial_identity, 8, 3,
                found == record->armors.end() ? 0 : found->amount
            );
        }
    }
    int base = rules_for(building.kind).pierce_armor;
    if (building.kind == BuildingKind::watch_tower &&
        has_technology(building.owner, Technology::keep)) {
        base = 9;
    }
    else if (building.kind == BuildingKind::watch_tower &&
        has_technology(building.owner, Technology::guard_tower)) {
        base = 8;
    }
    if (building.kind == BuildingKind::stone_wall &&
        has_technology(building.owner, Technology::fortified_wall)) {
        base = 12;
    }
    return base + (receives_class_3_or_52_effects(building.kind)
        ? (has_technology(building.owner, Technology::masonry) ? 1 : 0) +
          (has_technology(building.owner, Technology::architecture) ? 1 : 0)
        : 0);
}

int Simulation::building_class_11_armor(
    const Building& building
) const {
    return class_11_building_armor(*this, building);
}

int Simulation::sappers_attack_bonus(
    Player player,
    BuildingKind target
) const {
    if (!has_technology(player, Technology::sappers)) return 0;
    int bonus = target == BuildingKind::fish_trap ? 0 : 15;
    if (has_stone_defense_class_13(target)) bonus += 15;
    return bonus;
}

int Simulation::defensive_ship_bonus(
    Player player,
    BuildingKind source
) const {
    int bonus = rules_for(source).bonus_vs_ships;
    if (!has_technology(player, Technology::heated_shot)) return bonus;
    if (source == BuildingKind::watch_tower ||
        source == BuildingKind::guard_tower ||
        source == BuildingKind::keep ||
        source == BuildingKind::bombard_tower) {
        bonus = bonus * 225 / 100;
    }
    if (source == BuildingKind::castle) bonus += 4;
    return bonus;
}

bool Simulation::is_visible(
    Player player,
    TilePosition position
) const {
    return is_visible(EntityOwner{player}, position);
}

bool Simulation::is_visible(
    PlayerSlotId player,
    TilePosition position
) const {
    return is_visible(entity_owner_from_slot(player), position);
}

bool Simulation::is_visible(
    EntityOwner player,
    TilePosition position
) const {
    if (!map_.contains(position)) {
        return false;
    }
    if (has_technology(player, Technology::spy_technology)) {
        if (std::ranges::any_of(
                units_,
                [this, player, position](const Unit& unit) {
                    return unit.hit_points > 0 &&
                        unit.garrisoned_in == 0 &&
                        is_enemy(player, unit.owner) &&
                        unit.position == position;
                }
            )) {
            return true;
        }
        if (std::ranges::any_of(
                buildings_,
                [this, player, position](const Building& building) {
                    if (building.hit_points <= 0 ||
                        !is_enemy(player, building.owner)) {
                        return false;
                    }
                    const auto& rules = rules_for(building.kind);
                    return position.x >= building.position.x &&
                        position.y >= building.position.y &&
                        position.x <
                            building.position.x + rules.footprint_width &&
                        position.y <
                            building.position.y + rules.footprint_height;
                }
            )) {
            return true;
        }
    }
    const auto within_range = [position](TilePosition source, int range) {
        const int x = source.x - position.x;
        const int y = source.y - position.y;
        return x * x + y * y <= range * range;
    };
    return std::ranges::any_of(
               units_,
               [this, player, &within_range](const Unit& unit) {
                   return unit.garrisoned_in == 0 &&
                          (unit.owner == player ||
                           (is_ally(unit.owner, player) &&
                            has_technology(
                                player, Technology::cartography
                            ))) &&
                          within_range(
                              unit.position,
                              effective_unit_vision_range(unit)
                          );
               }
           ) ||
           std::ranges::any_of(
               buildings_,
               [this, player, position](const Building& building) {
                   const int range =
                       effective_building_vision_range(building);
                   return (building.owner == player ||
                           (is_ally(building.owner, player) &&
                            has_technology(
                                player, Technology::cartography
                            ))) &&
                          combat_distance_squared(position, building) <=
                              range * range;
               }
           );
}

PlayerControllerState Simulation::controller_state(Player player) const {
    const auto slot = player_slot_from_legacy(player);
    return !slot || slot->is_neutral()
        ? PlayerControllerState::observer : controller_state(*slot);
}

PlayerControllerState Simulation::controller_state(
    PlayerSlotId player
) const {
    return player_state(player).controller;
}

bool Simulation::is_visible_to_controller(
    Player player, TilePosition position
) const {
    return observer_perspective(player)
        ? map_.contains(position)
        : is_visible(player, position);
}

bool Simulation::has_attack_reveal(
    EntityOwner viewer, EntityId attacker
) const {
    const auto slot = entity_owner_slot(viewer);
    if (!slot || slot->is_neutral()) return false;
    const auto active = [this, attacker](PlayerSlotId source) {
        const auto& reveals = player_state(source).attack_reveal_expiries;
        const auto found = reveals.find(attacker);
        return found != reveals.end() && tick_number_ < found->second;
    };
    if (active(*slot)) return true;
    if (!has_technology(viewer, Technology::cartography)) return false;
    for (std::size_t index = 0; index < player_states_.size(); ++index) {
        const PlayerSlotId ally = *PlayerSlotId::from_index(index);
        if (ally == *slot || !roster_.slot(ally).occupied ||
            !is_ally(entity_owner_from_slot(ally), viewer)) continue;
        if (active(ally)) return true;
    }
    return false;
}

bool Simulation::is_unit_visible(
    Player player, const Unit& unit
) const {
    return is_unit_visible(EntityOwner{player}, unit);
}

bool Simulation::is_unit_visible(
    EntityOwner player, const Unit& unit
) const {
    return unit.owner == player || is_visible(player, unit.position) ||
        (is_enemy(player, unit.owner) && has_attack_reveal(player, unit.id));
}

bool Simulation::is_unit_visible_to_controller(
    Player player, const Unit& unit
) const {
    return observer_perspective(player) || is_unit_visible(player, unit);
}

bool Simulation::is_explored_to_controller(
    Player player, TilePosition position
) const {
    return observer_perspective(player)
        ? map_.contains(position)
        : is_explored(player, position);
}

bool Simulation::is_building_visible(
    Player player,
    const Building& building
) const {
    return is_building_visible(EntityOwner{player}, building);
}

bool Simulation::is_building_visible(
    EntityOwner player,
    const Building& building
) const {
    const BuildingRules& rules = rules_for(building.kind);
    for (int y = 0; y < rules.footprint_height; ++y) {
        for (int x = 0; x < rules.footprint_width; ++x) {
            if (is_visible(
                    player,
                    {
                        building.position.x + x,
                        building.position.y + y,
                    }
                )) {
                return true;
            }
        }
    }
    return false;
}

bool Simulation::is_explored(
    Player player,
    TilePosition position
) const {
    const auto slot = player_slot_from_legacy(player);
    return slot && !slot->is_neutral() && is_explored(*slot, position);
}

bool Simulation::is_explored(
    PlayerSlotId player,
    TilePosition position
) const {
    if (!map_.contains(position)) {
        return false;
    }
    const auto& explored = player_state(player).explored;
    return explored.at(map_index(position));
}

std::vector<TilePosition> Simulation::explored_tiles(Player player) const {
    std::vector<TilePosition> result;
    for (int y = 0; y < map_.height(); ++y) {
        for (int x = 0; x < map_.width(); ++x) {
            if (is_explored(player, {x, y})) {
                result.push_back({x, y});
            }
        }
    }
    return result;
}

const std::map<EntityId, Simulation::BuildingMemory>&
Simulation::remembered_buildings(Player player) const {
    const auto slot = player_slot_from_legacy(player);
    if (!slot || slot->is_neutral()) {
        static const std::map<EntityId, BuildingMemory> empty;
        return empty;
    }
    return player_state(*slot).remembered_buildings;
}

std::vector<EntityId> Simulation::idle_villagers(Player player) const {
    std::vector<EntityId> idle;
    for (const Unit& unit : units_) {
        if (unit.owner != player ||
            unit.kind != UnitKind::villager ||
            unit.garrisoned_in != 0) {
            continue;
        }
        const bool constructing = std::ranges::any_of(
            buildings_,
            [&unit](const Building& building) {
                return !building.completed() &&
                    std::ranges::find(
                        building.builder_ids,
                        unit.id
                    ) != building.builder_ids.end();
            }
        );
        const bool ordered =
            unit.moving ||
            !unit.path.empty() ||
            !unit.waypoints.empty() ||
            unit.attack_target_id != 0 ||
            unit.repair_target_id != 0 ||
            unit.has_resource_target ||
            unit.returning_resource ||
            unit.resource_building_id != 0 ||
            unit.garrison_target_id != 0 ||
            unit.attack_moving ||
            unit.patrolling ||
            unit.guard_target_id != 0 ||
            constructing;
        if (!ordered) {
            idle.push_back(unit.id);
        }
    }
    return idle;
}

std::vector<EntityId> Simulation::idle_military(Player player) const {
    std::vector<EntityId> idle;
    for (const Unit& unit : units_) {
        if (unit.owner != player ||
            unit.kind == UnitKind::villager ||
            is_animal(unit.kind) ||
            unit.garrisoned_in != 0) {
            continue;
        }
        const bool ordered =
            unit.moving ||
            !unit.path.empty() ||
            !unit.waypoints.empty() ||
            unit.attack_target_id != 0 ||
            unit.repair_target_id != 0 ||
            unit.has_resource_target ||
            unit.returning_resource ||
            unit.garrison_target_id != 0 ||
            unit.attack_moving ||
            unit.patrolling ||
            unit.attacking_ground ||
            unit.guard_target_id != 0 ||
            unit.returning_to_stance;
        if (!ordered) {
            idle.push_back(unit.id);
        }
    }
    return idle;
}

EntityId Simulation::add_unit(
    UnitKind kind,
    Player owner,
    TilePosition position
) {
    return add_unit(kind, EntityOwner{owner}, position);
}

EntityId Simulation::add_unit(
    UnitKind kind,
    PlayerSlotId owner,
    TilePosition position
) {
    return add_unit(kind, entity_owner_from_slot(owner), position);
}

EntityId Simulation::add_unit(
    UnitKind kind,
    EntityOwner owner,
    TilePosition position
) {
    const bool fishing_ship = is_ship(kind);
    const bool valid_terrain = fishing_ship
        ? map_.sailable(position)
        : map_.walkable(position);
    if (!valid_terrain || occupied(position, 0)) {
        throw std::invalid_argument("unit requires an empty walkable tile");
    }

    Unit unit;
    unit.id = next_id_++;
    unit.kind = kind;
    unit.owner = owner;
    unit.position = position;
    unit.previous_position = position;
    unit.render_previous_subtile = {
        position.x * 320, position.y * 320
    };
    unit.render_current_subtile = unit.render_previous_subtile;
    unit.render_subtile_initialized = true;
    unit.destination = position;
    unit.hit_points = maximum_hit_points(unit);
    unit.attack = rules_for(kind).attack;
    unit.food_remaining =
        kind == UnitKind::sheep ? 100 :
        kind == UnitKind::deer ? 140 :
        kind == UnitKind::boar ? 340 :
        0;
    if (is_animal(kind) || is_relic(kind)) {
        unit.stance = UnitStance::passive;
    }
    if (kind == UnitKind::scout_cavalry ||
        kind == UnitKind::king ||
        kind == UnitKind::knight ||
        kind == UnitKind::cavalier ||
        kind == UnitKind::paladin ||
        kind == UnitKind::light_cavalry ||
        kind == UnitKind::hussar) {
        constexpr int movement_denominator = 320;
        unit.movement_speed_remainder = std::max(
            0,
            movement_denominator - cavalry_movement_numerator(unit)
        );
    }
    units_.push_back(unit);
    initialize_unit_render_elevation(units_.back());
    if (const auto legacy = owner.legacy_player();
        legacy && *legacy != Player::neutral &&
        !is_animal(kind) && !is_relic(kind)) {
        ++mutable_statistics(*legacy).units_created;
    }
    if (const auto legacy = owner.legacy_player();
        legacy && *legacy != Player::neutral) {
        refresh_unit_attacks(*legacy);
    }
    update_exploration();
    return unit.id;
}

EntityId Simulation::add_commercial_object(
    CommercialObjectIdentity identity,
    EntityOwner owner,
    TilePosition position
) {
    const CommercialObjectRecord* record =
        commercial_content_catalog().object(
            identity.civilization_id, identity.object_id
        );
    if (record == nullptr) {
        throw std::invalid_argument("unknown commercial object identity");
    }
    const auto owner_slot = owner.slot_index();
    if (!owner_slot) {
        throw std::invalid_argument("commercial object requires player owner");
    }
    PlayerState& player = player_states_[*owner_slot];
    if (player.commercial_civilization &&
        *player.commercial_civilization != identity.civilization_id) {
        throw std::invalid_argument("commercial civilization mismatch");
    }
    player.commercial_civilization = identity.civilization_id;
    if (!player.commercial_civilization_initialized) {
        if (const auto setup = commercial_content_catalog()
                .civilization_effect(identity.civilization_id)) {
            apply_commercial_effect(owner, *setup);
        }
        player.commercial_civilization_initialized = true;
    }
    const int effective_hp = std::max(1, static_cast<int>(std::lround(
        effective_commercial_attribute(
            owner, identity, 0, static_cast<float>(record->hit_points)
        )
    )));
    const bool building =
        record->base_class == CommercialObjectBaseClass::building;
    if (!map_.walkable(position) || occupied(position, 0)) {
        throw std::invalid_argument("commercial object requires empty terrain");
    }
    if (building) {
        Building value;
        value.id = next_id_++;
        value.kind = BuildingKind::house;
        value.commercial_identity = identity;
        value.owner = owner;
        value.position = position;
        value.hit_points = effective_hp;
        buildings_.push_back(value);
        update_exploration();
        return value.id;
    }
    Unit value;
    value.id = next_id_++;
    value.kind = UnitKind::villager;
    value.commercial_identity = identity;
    value.owner = owner;
    value.position = position;
    value.previous_position = position;
    value.render_previous_subtile = {position.x * 320, position.y * 320};
    value.render_current_subtile = value.render_previous_subtile;
    value.render_subtile_initialized = true;
    value.destination = position;
    value.hit_points = effective_hp;
    value.attack = static_cast<int>(std::lround(
        effective_commercial_attribute(owner, identity, 9, record->attack)
    ));
    if (record->speed <= 0.0f) value.stance = UnitStance::passive;
    units_.push_back(value);
    initialize_unit_render_elevation(units_.back());
    update_exploration();
    return value.id;
}

bool Simulation::command_gather_unit(
    EntityId villager_id,
    EntityId herdable_id
) {
    if (outcome_ != MatchOutcome::ongoing ||
        villager_id == herdable_id) {
        return false;
    }
    Unit* villager = find_unit(villager_id);
    Unit* herdable = find_unit(herdable_id);
    if (villager == nullptr || herdable == nullptr ||
        (villager->kind != UnitKind::villager &&
         !commercial_has_ability(*villager, CommercialTaskAbility::gather) &&
         !commercial_has_ability(*villager, CommercialTaskAbility::hunt) &&
         !commercial_has_ability(*villager, CommercialTaskAbility::graze)) ||
        villager->garrisoned_in != 0 ||
        !is_animal(herdable->kind) ||
        (is_herdable(herdable->kind) &&
         herdable->owner != villager->owner &&
         !herdable->owner.is_neutral()) ||
        herdable->food_remaining <= 0) {
        return false;
    }
    if (is_huntable(herdable->kind) && herdable->hit_points > 0) {
        const TilePosition target_position = herdable->position;
        if (!command_unit(villager_id, target_position)) {
            return false;
        }
        villager = find_unit(villager_id);
        if (villager == nullptr) return false;
        villager->has_resource_target = true;
        villager->resource_target = target_position;
        villager->resource_building_id = 0;
        villager->resource_unit_id = herdable_id;
        villager->returning_resource = false;
        villager->carried_resource = ResourceKind::food;
        return true;
    }
    const std::array<TilePosition, 4> gathering_positions{{
        {herdable->position.x - 1, herdable->position.y},
        {herdable->position.x, herdable->position.y - 1},
        {herdable->position.x, herdable->position.y + 1},
        {herdable->position.x + 1, herdable->position.y},
    }};
    std::vector<TilePosition> reachable;
    for (TilePosition position : gathering_positions) {
        if (map_.walkable(position) &&
            !occupied(position, villager->id, villager->owner, true)) {
            reachable.push_back(position);
        }
    }
    std::ranges::sort(
        reachable,
        [villager](TilePosition left, TilePosition right) {
            const int left_distance =
                std::abs(left.x - villager->position.x) +
                std::abs(left.y - villager->position.y);
            const int right_distance =
                std::abs(right.x - villager->position.x) +
                std::abs(right.y - villager->position.y);
            if (left_distance != right_distance) {
                return left_distance < right_distance;
            }
            return left.y != right.y
                ? left.y < right.y
                : left.x < right.x;
        }
    );
    const bool routed = std::ranges::any_of(
        reachable,
        [this, villager](TilePosition position) {
            return route_unit(*villager, position);
        }
    );
    if (!routed) {
        return false;
    }
    if (is_herdable(herdable->kind)) {
        // Neutral herdables become controlled when a player interacts with
        // them. This keeps capture and the subsequent gather order atomic.
        herdable->owner = villager->owner;
        herdable->hit_points = 0;
    }
    villager->attack_target_id = 0;
    villager->attack_target_is_building = false;
    villager->repair_target_id = 0;
    villager->has_resource_target = true;
    villager->resource_target = herdable->position;
    villager->resource_building_id = 0;
    villager->resource_unit_id = herdable_id;
    villager->returning_resource = false;
    if (villager->carried_amount == 0) {
        villager->carried_resource = ResourceKind::food;
    }
    villager->garrison_target_id = 0;
    if (villager->carried_amount > 0 &&
        villager->carried_resource != ResourceKind::food) {
        Building* drop_off = nearest_drop_off(*villager);
        villager->returning_resource = true;
        if (drop_off != nullptr) {
            route_unit(*villager, drop_off->position);
        } else {
            villager->moving = false;
        }
    }
    detach_builder(villager->id);
    return true;
}

bool Simulation::command_commercial_task(
    EntityId unit_id,
    std::uint16_t task_id,
    EntityId target_id,
    bool target_is_building,
    TilePosition position
) {
    Unit* unit = find_unit(unit_id);
    if (outcome_ != MatchOutcome::ongoing || !unit ||
        !unit->commercial_identity || unit->garrisoned_in != 0) {
        return false;
    }
    const auto* object = commercial_content_catalog().object(
        unit->commercial_identity->civilization_id,
        unit->commercial_identity->object_id
    );
    if (!object) return false;
    const auto task = std::ranges::find_if(
        object->tasks, [task_id](const CommercialTask& candidate) {
            return candidate.id == task_id;
        }
    );
    if (task == object->tasks.end()) return false;
    Unit* target_unit = target_is_building ? nullptr : find_unit(target_id);
    Building* target_building = target_is_building
        ? find_building(target_id) : nullptr;
    const auto target_identity = target_unit
        ? target_unit->commercial_identity
        : target_building ? target_building->commercial_identity
                          : std::nullopt;
    if (task->object_id &&
        (!target_identity || target_identity->object_id != *task->object_id)) {
        return false;
    }
    if (task->object_class >= 0 && target_identity) {
        const auto* target = commercial_content_catalog().object(
            target_identity->civilization_id, target_identity->object_id
        );
        if (!target || target->unit_class != task->object_class) return false;
    }
    switch (commercial_task_ability(task->action_type)) {
        case CommercialTaskAbility::gather:
        case CommercialTaskAbility::graze:
        case CommercialTaskAbility::hunt:
        case CommercialTaskAbility::loot:
            return target_unit && command_gather_unit(unit_id, target_id);
        case CommercialTaskAbility::combat:
        case CommercialTaskAbility::unpack_attack: {
            if (!target_unit && !target_building) return false;
            stop_unit(unit_id);
            unit = find_unit(unit_id);
            unit->attack_target_id = target_id;
            unit->attack_target_is_building = target_is_building;
            unit->attack_target_auto = false;
            return route_unit(
                *unit, target_unit ? target_unit->position
                                   : target_building->position
            );
        }
        case CommercialTaskAbility::guard:
            return command_guard(unit_id, target_id, target_is_building);
        case CommercialTaskAbility::convert:
            return command_convert(unit_id, target_id);
        case CommercialTaskAbility::heal:
            return command_heal(unit_id, target_id);
        case CommercialTaskAbility::repair:
        case CommercialTaskAbility::build:
            if (!target_building || target_building->owner != unit->owner ||
                !route_unit(*unit, target_building->position)) return false;
            unit->repair_target_id = target_id;
            if (!target_building->completed() &&
                std::ranges::find(target_building->builder_ids, unit_id) ==
                    target_building->builder_ids.end()) {
                target_building->builder_ids.push_back(unit_id);
            }
            return true;
        case CommercialTaskAbility::trade:
        case CommercialTaskAbility::off_map_trade:
            return target_building && command_trade_route(unit_id, target_id);
        case CommercialTaskAbility::garrison:
            if (target_building && route_unit(*unit, target_building->position)) {
                unit->garrison_target_id = target_id;
                return true;
            }
            return target_unit && command_embark(unit_id, target_id);
        case CommercialTaskAbility::transport:
        case CommercialTaskAbility::pickup:
        case CommercialTaskAbility::kidnap:
        case CommercialTaskAbility::deposit:
            return target_unit && command_embark(target_id, unit_id);
        case CommercialTaskAbility::retreat:
            return command_unit(unit_id, position);
        case CommercialTaskAbility::deselect:
            std::erase(selected_units_, unit_id);
            if (selected_unit_ == unit_id) selected_unit_.reset();
            return true;
        case CommercialTaskAbility::make:
        case CommercialTaskAbility::bird:
        case CommercialTaskAbility::predator:
        case CommercialTaskAbility::auto_convert:
        case CommercialTaskAbility::wonder_victory:
            // These tasks are autonomous capabilities, not player orders.
            return target_id == 0;
    }
    return false;
}

EntityId Simulation::add_building(
    BuildingKind kind,
    Player owner,
    TilePosition position
) {
    return add_building(kind, EntityOwner{owner}, position);
}

EntityId Simulation::add_building(
    BuildingKind kind,
    PlayerSlotId owner,
    TilePosition position
) {
    return add_building(kind, entity_owner_from_slot(owner), position);
}

EntityId Simulation::add_building(
    BuildingKind kind,
    EntityOwner owner,
    TilePosition position
) {
    if (kind == BuildingKind::watch_tower) {
        kind = has_technology(owner, Technology::keep)
            ? BuildingKind::keep
            : has_technology(owner, Technology::guard_tower)
                ? BuildingKind::guard_tower
                : kind;
    } else if (has_technology(owner, Technology::fortified_wall)) {
        kind = kind == BuildingKind::stone_wall
            ? BuildingKind::fortified_wall
            : kind == BuildingKind::stone_gate_x
                ? BuildingKind::fortified_gate_x
                : kind == BuildingKind::stone_gate_y
                    ? BuildingKind::fortified_gate_y
                    : kind;
    }
    if (!footprint_available(kind, position, 0)) {
        throw std::invalid_argument(
            "building requires an empty walkable footprint"
        );
    }
    Building building;
    building.id = next_id_++;
    building.kind = kind;
    building.owner = owner;
    building.position = position;
    building.hit_points = maximum_hit_points(building);
    if (kind == BuildingKind::farm) {
        building.resource_amount = farm_capacity(owner);
    } else if (kind == BuildingKind::fish_trap) {
        building.resource_amount = 700;
    }
    buildings_.push_back(building);
    if (const auto legacy = owner.legacy_player();
        legacy && *legacy != Player::neutral) {
        PlayerStatistics& statistics = mutable_statistics(*legacy);
        ++statistics.buildings_built;
        if (kind == BuildingKind::wonder) ++statistics.wonders_built;
    }
    update_exploration();
    return building.id;
}

bool Simulation::select_unit_at(TilePosition position, Player player) {
    const auto found = std::find_if(
        units_.begin(),
        units_.end(),
        [this, position, player](const Unit& unit) {
            return unit.garrisoned_in == 0 &&
                unit.position == position &&
                (unit.owner == player ||
                 (unit.kind == UnitKind::sheep &&
                  unit.owner.is_neutral() &&
                  is_visible(player, unit.position)));
        }
    );
    if (found == units_.end()) {
        selected_unit_.reset();
        selected_units_.clear();
        return false;
    }
    selected_unit_ = found->id;
    selected_units_ = {found->id};
    selected_building_.reset();
    return true;
}

bool Simulation::select_units_in_area(
    TilePosition first,
    TilePosition second,
    Player player
) {
    const int left = std::min(first.x, second.x);
    const int right = std::max(first.x, second.x);
    const int top = std::min(first.y, second.y);
    const int bottom = std::max(first.y, second.y);

    selected_units_.clear();
    for (const Unit& unit : units_) {
        if (unit.owner == player &&
            unit.garrisoned_in == 0 &&
            unit.position.x >= left && unit.position.x <= right &&
            unit.position.y >= top && unit.position.y <= bottom) {
            selected_units_.push_back(unit.id);
        }
    }
    selected_unit_ = selected_units_.empty()
        ? std::nullopt
        : std::optional<EntityId>{selected_units_.front()};
    selected_building_.reset();
    return !selected_units_.empty();
}

bool Simulation::select_units(
    const std::vector<EntityId>& ids,
    Player player
) {
    selected_units_.clear();
    for (EntityId id : ids) {
        Unit* unit = find_unit(id);
        if (unit != nullptr && unit->garrisoned_in == 0 &&
            unit->owner == player &&
            std::ranges::find(selected_units_, id) == selected_units_.end()) {
            selected_units_.push_back(id);
        }
    }
    selected_unit_ = selected_units_.empty()
        ? std::nullopt
        : std::optional<EntityId>{selected_units_.front()};
    selected_building_.reset();
    return !selected_units_.empty();
}

std::vector<TilePosition> Simulation::formation_destinations(
    const std::vector<EntityId>& unit_ids,
    TilePosition center
) const {
    FormationKind kind = FormationKind::compact;
    if (!unit_ids.empty()) {
        const auto first = std::ranges::find_if(
            units_, [id = unit_ids.front()](const Unit& unit) {
                return unit.id == id;
            }
        );
        if (first != units_.end()) kind = formation_kind(first->owner);
    }
    return formation_destinations(unit_ids, center, kind);
}

std::vector<TilePosition> Simulation::formation_destinations(
    const std::vector<EntityId>& unit_ids,
    TilePosition center,
    FormationKind kind,
    std::optional<TilePosition> facing
) const {
    std::vector<TilePosition> destinations;
    destinations.reserve(unit_ids.size());
    for (EntityId id : unit_ids) {
        const auto unit = std::ranges::find_if(
            units_,
            [id](const Unit& candidate) {
                return candidate.id == id &&
                    candidate.garrisoned_in == 0;
            }
        );
        destinations.push_back(
            unit == units_.end() ? center : unit->position
        );
    }
    if (unit_ids.empty()) {
        return destinations;
    }

    const auto selected = [&unit_ids](EntityId id) {
        return std::ranges::find(unit_ids, id) != unit_ids.end();
    };
    const auto tile_index = [this](TilePosition position) {
        return static_cast<std::size_t>(position.y) *
                   static_cast<std::size_t>(map_.width()) +
               static_cast<std::size_t>(position.x);
    };
    std::vector<bool> blocked_tiles(
        static_cast<std::size_t>(map_.width()) *
            static_cast<std::size_t>(map_.height()),
        false
    );
    for (const Unit& unit : units_) {
        if (unit.garrisoned_in == 0 && !selected(unit.id) &&
            map_.contains(unit.position)) {
            blocked_tiles[tile_index(unit.position)] = true;
        }
    }
    for (const Building& building : buildings_) {
        const BuildingRules& rules = rules_for(building.kind);
        for (int y = building.position.y;
             y < building.position.y + rules.footprint_height;
             ++y) {
            for (int x = building.position.x;
                 x < building.position.x + rules.footprint_width;
                 ++x) {
                const TilePosition position{x, y};
                if (map_.contains(position)) {
                    blocked_tiles[tile_index(position)] = true;
                }
            }
        }
    }
    const auto blocked =
        [this, &blocked_tiles, &tile_index](TilePosition position) {
            return !map_.contains(position) ||
                blocked_tiles[tile_index(position)];
    };

    bool all_ships = true;
    bool any_ships = false;
    int centroid_x{};
    int centroid_y{};
    int live_count{};
    for (EntityId id : unit_ids) {
        const auto unit = std::ranges::find_if(
            units_, [id](const Unit& candidate) {
                return candidate.id == id;
            }
        );
        if (unit == units_.end() || unit->garrisoned_in != 0) continue;
        const bool ship = is_ship(unit->kind);
        all_ships = all_ships && ship;
        any_ships = any_ships || ship;
        centroid_x += unit->position.x;
        centroid_y += unit->position.y;
        ++live_count;
    }
    if (any_ships && !all_ships) return destinations;
    if (live_count > 0) {
        centroid_x /= live_count;
        centroid_y /= live_count;
    }
    const int dx = facing ? facing->x : center.x - centroid_x;
    const int dy = facing ? facing->y : center.y - centroid_y;
    const TilePosition forward =
        std::abs(dx) >= std::abs(dy)
            ? TilePosition{dx >= 0 ? 1 : -1, 0}
            : TilePosition{0, dy >= 0 ? 1 : -1};
    const TilePosition lateral{-forward.y, forward.x};
    const int spacing = all_ships ? 2 : 1;
    std::vector<TilePosition> ideals;
    ideals.reserve(unit_ids.size());
    const int count = static_cast<int>(unit_ids.size());
    const int columns = std::max(
        1, static_cast<int>(std::ceil(std::sqrt(count)))
    );
    for (int index = 0; index < count; ++index) {
        int row{};
        int column{};
        const auto symmetric_offset = [](int position, int width) {
            if (width % 2 != 0) {
                return position == 0 ? 0 :
                    ((position + 1) / 2) *
                        (position % 2 ? -1 : 1);
            }
            return (position / 2 + 1) *
                (position % 2 ? -1 : 1);
        };
        if (kind == FormationKind::line) {
            column = symmetric_offset(index, count);
        } else if (kind == FormationKind::box ||
                   kind == FormationKind::compact) {
            row = index / columns;
            const int row_start = row * columns;
            const int row_width = std::min(columns, count - row_start);
            column = symmetric_offset(index - row_start, row_width);
        } else if (kind == FormationKind::staggered) {
            row = index / columns;
            const int row_start = row * columns;
            const int row_width = std::min(columns, count - row_start);
            column = symmetric_offset(index - row_start, row_width);
            if (row % 2 != 0) column *= 2;
        } else {
            const int wing = index % 2 == 0 ? -1 : 1;
            const int depth = index / 2;
            row = depth / 2;
            column = wing * (1 + depth / 2);
        }
        ideals.push_back({
            center.x - forward.x * row * spacing +
                lateral.x * column * spacing,
            center.y - forward.y * row * spacing +
                lateral.y * column * spacing,
        });
    }

    std::vector<TilePosition> candidates;
    std::vector<bool> candidate_added(blocked_tiles.size(), false);
    for (TilePosition ideal : ideals) {
        if (!map_.contains(ideal) || blocked(ideal)) continue;
        const bool terrain_ok = all_ships
            ? map_.sailable(ideal)
            : map_.walkable(ideal);
        if (terrain_ok && !candidate_added[tile_index(ideal)]) {
            candidates.push_back(ideal);
            candidate_added[tile_index(ideal)] = true;
        }
    }
    const std::size_t ideal_candidate_count = candidates.size();
    for (int y = 0; y < map_.height(); ++y) {
        for (int x = 0; x < map_.width(); ++x) {
            const TilePosition position{x, y};
            const bool terrain_ok = all_ships
                ? map_.sailable(position)
                : map_.walkable(position);
            if (terrain_ok && !blocked(position) &&
                !candidate_added[tile_index(position)]) {
                candidates.push_back(position);
                candidate_added[tile_index(position)] = true;
            }
        }
    }
    std::ranges::sort(
        candidates.begin() +
            static_cast<std::ptrdiff_t>(
                ideal_candidate_count
            ),
        candidates.end(),
        [center](TilePosition first, TilePosition second) {
            const int first_distance =
                std::abs(first.x - center.x) +
                std::abs(first.y - center.y);
            const int second_distance =
                std::abs(second.x - center.x) +
                std::abs(second.y - center.y);
            if (first_distance != second_distance) {
                return first_distance < second_distance;
            }
            if (first.y != second.y) {
                return first.y < second.y;
            }
            return first.x < second.x;
        }
    );

    std::vector<bool> assigned(unit_ids.size(), false);
    std::size_t assigned_count{};
    for (std::size_t candidate_index = 0;
         candidate_index < candidates.size(); ++candidate_index) {
        const TilePosition candidate = candidates[candidate_index];
        std::optional<std::size_t> best_index;
        std::size_t best_distance{};
        EntityId best_id{};
        int best_role{};
        for (std::size_t index = 0; index < unit_ids.size(); ++index) {
            if (assigned[index]) {
                continue;
            }
            const auto unit = std::ranges::find_if(
                units_,
                [id = unit_ids[index]](const Unit& current) {
                    return current.id == id &&
                        current.garrisoned_in == 0;
                }
            );
            if (unit == units_.end()) {
                continue;
            }
            const std::vector<TilePosition> path =
                unit->position == candidate
                    ? std::vector<TilePosition>{}
                    : find_path(
                        map_,
                        unit->position,
                        candidate,
                        blocked
                    );
            if (unit->position != candidate && path.empty()) {
                continue;
            }
            const std::size_t distance = path.size();
            const bool cavalry = is_cavalry(unit->kind);
            const bool rear =
                unit->kind == UnitKind::monk ||
                unit->kind == UnitKind::missionary ||
                is_ram(unit->kind) ||
                unit->kind == UnitKind::mangonel ||
                unit->kind == UnitKind::onager ||
                unit->kind == UnitKind::siege_onager ||
                unit->kind == UnitKind::scorpion ||
                unit->kind == UnitKind::heavy_scorpion;
            const int role =
                kind == FormationKind::flank && cavalry ? 0 :
                rear ? 3 :
                rules_for(unit->kind).attack_range > 1 ? 2 : 1;
            const bool ideal_candidate =
                candidate_index < ideal_candidate_count;
            if (!best_index ||
                (ideal_candidate && role < best_role) ||
                ((!ideal_candidate || role == best_role) &&
                 distance < best_distance) ||
                ((!ideal_candidate || role == best_role) &&
                 distance == best_distance &&
                 unit->id < best_id)) {
                best_index = index;
                best_distance = distance;
                best_id = unit->id;
                best_role = role;
            }
        }
        if (!best_index) {
            continue;
        }
        destinations[*best_index] = candidate;
        assigned[*best_index] = true;
        ++assigned_count;
        if (assigned_count == unit_ids.size()) {
            break;
        }
    }
    const auto siege_unit = [](UnitKind unit_kind) {
        return is_ram(unit_kind) ||
            unit_kind == UnitKind::mangonel ||
            unit_kind == UnitKind::onager ||
            unit_kind == UnitKind::siege_onager ||
            unit_kind == UnitKind::scorpion ||
            unit_kind == UnitKind::heavy_scorpion ||
            unit_kind == UnitKind::packed_trebuchet ||
            unit_kind == UnitKind::trebuchet ||
            unit_kind == UnitKind::bombard_cannon;
    };
    if (!all_ships) {
        for (std::size_t index = 0; index < unit_ids.size(); ++index) {
            const auto unit = std::ranges::find_if(
                units_, [id = unit_ids[index]](const Unit& candidate) {
                    return candidate.id == id;
                }
            );
            if (unit == units_.end() || !siege_unit(unit->kind)) continue;
            const auto has_clearance = [&destinations, index](
                    TilePosition position) {
                for (std::size_t other = 0;
                     other < destinations.size(); ++other) {
                    if (other == index) continue;
                    const int distance = std::max(
                        std::abs(position.x - destinations[other].x),
                        std::abs(position.y - destinations[other].y)
                    );
                    if (distance < 2) return false;
                }
                return true;
            };
            if (has_clearance(destinations[index])) continue;
            const auto replacement = std::ranges::find_if(
                candidates, [&, unit](TilePosition candidate) {
                    if (std::ranges::find(destinations, candidate) !=
                        destinations.end() ||
                        !has_clearance(candidate)) return false;
                    const int candidate_depth =
                        (candidate.x - center.x) * forward.x +
                        (candidate.y - center.y) * forward.y;
                    const int assigned_depth =
                        (destinations[index].x - center.x) * forward.x +
                        (destinations[index].y - center.y) * forward.y;
                    if (candidate_depth > assigned_depth) return false;
                    if (unit->position == candidate) return true;
                    return !find_path(
                        map_, unit->position, candidate, blocked
                    ).empty();
                }
            );
            if (replacement != candidates.end()) {
                destinations[index] = *replacement;
            }
        }
    }
    return destinations;
}

bool Simulation::set_formation_kind(
    Player player,
    FormationKind kind
) {
    const auto slot = player_slot_from_legacy(player);
    return slot && !slot->is_neutral() && set_formation_kind(*slot, kind);
}

bool Simulation::set_formation_kind(
    PlayerSlotId player,
    FormationKind kind
) {
    if (outcome_ != MatchOutcome::ongoing ||
        kind < FormationKind::compact ||
        kind > FormationKind::flank) return false;
    player_states_.at(*player.index()).formation = kind;
    return true;
}

FormationKind Simulation::formation_kind(Player player) const {
    const auto slot = player_slot_from_legacy(player);
    if (!slot || slot->is_neutral()) {
        // Gaia units can be inspected and selected, but Gaia has no mutable
        // player state. Use the same compact fallback as formation placement.
        return FormationKind::compact;
    }
    return formation_kind(*slot);
}

FormationKind Simulation::formation_kind(PlayerSlotId player) const {
    return player_state(player).formation;
}

std::pair<int, int> Simulation::formation_pace(
    const std::vector<EntityId>& unit_ids
) const {
    int slowest_interval = 1;
    int slowest_speed_numerator = 32000;
    for (EntityId id : unit_ids) {
        const auto found = std::ranges::find_if(
            units_, [id](const Unit& unit) { return unit.id == id; }
        );
        if (found == units_.end()) continue;
        const Unit& unit = *found;
        slowest_interval = std::max(
            slowest_interval,
            effective_movement_interval(unit)
        );
        int interval = effective_movement_interval(unit);
        if (is_ram(unit.kind) ||
            unit.kind == UnitKind::mangonel ||
            unit.kind == UnitKind::onager ||
            unit.kind == UnitKind::siege_onager ||
            unit.kind == UnitKind::scorpion ||
            unit.kind == UnitKind::heavy_scorpion ||
            unit.kind == UnitKind::packed_trebuchet) {
            interval = 1;
        }
        int rate = 32000 / std::max(1, interval);
        if (unit.kind == UnitKind::scout_cavalry ||
            unit.kind == UnitKind::knight ||
            unit.kind == UnitKind::cavalier ||
            unit.kind == UnitKind::paladin ||
            unit.kind == UnitKind::light_cavalry ||
            unit.kind == UnitKind::hussar ||
            unit.kind == UnitKind::camel_rider ||
            unit.kind == UnitKind::heavy_camel) {
            rate = cavalry_movement_numerator(unit) * 100 /
                std::max(1, interval);
        } else if (is_ship(unit.kind)) {
            rate = 320 * ship_movement_numerator(unit) /
                std::max(1, interval);
        } else {
            rate = 320 * unique_unit_movement_numerator(unit) /
                std::max(1, interval);
        }
        slowest_speed_numerator =
            std::min(slowest_speed_numerator, rate);
    }
    return {slowest_interval, slowest_speed_numerator};
}

bool Simulation::command_formation(
    const std::vector<EntityId>& unit_ids,
    TilePosition center,
    FormationKind kind
) {
    if (outcome_ != MatchOutcome::ongoing || unit_ids.empty() ||
        kind < FormationKind::compact ||
        kind > FormationKind::flank) return false;
    std::vector<EntityId> unique_ids = unit_ids;
    std::ranges::sort(unique_ids);
    if (std::ranges::adjacent_find(unique_ids) != unique_ids.end()) {
        return false;
    }
    const Unit* first = find_unit(unit_ids.front());
    if (first == nullptr || first->owner == Player::neutral) return false;
    const Player owner = first->owner;
    bool any_ship = false;
    bool any_land = false;
    for (EntityId id : unit_ids) {
        const Unit* unit = find_unit(id);
        if (unit == nullptr || unit->owner != owner ||
            unit->garrisoned_in != 0) return false;
        any_ship = any_ship || is_ship(unit->kind);
        any_land = any_land || !is_ship(unit->kind);
    }
    if (any_ship && any_land) return false;
    const auto [slowest_interval, slowest_speed_numerator] =
        formation_pace(unit_ids);
    const auto destinations =
        formation_destinations(unit_ids, center, kind);
    const std::uint64_t group = next_formation_group_id_;
    const std::vector<Unit> original_units = units_;
    bool accepted = true;
    for (std::size_t index = 0; index < unit_ids.size(); ++index) {
        accepted =
            command_unit(unit_ids[index], destinations[index]) && accepted;
        Unit* unit = find_unit(unit_ids[index]);
        if (unit != nullptr) {
            unit->formation_move_interval = slowest_interval;
            unit->formation_speed_numerator = slowest_speed_numerator;
            unit->formation_group_id = group;
            unit->movement_speed_remainder = 0;
            unit->formation_anchor = center;
            unit->formation_slot = destinations[index];
        }
    }
    if (!accepted) {
        units_ = original_units;
        return false;
    }
    ++next_formation_group_id_;
    set_formation_kind(owner, kind);
    return accepted;
}

bool Simulation::command_formation_order(
    const std::vector<EntityId>& unit_ids,
    TilePosition center,
    FormationKind kind,
    FormationOrderKind order,
    EntityId guard_target,
    bool guard_target_is_building
) {
    if (unit_ids.empty() ||
        kind < FormationKind::compact ||
        kind > FormationKind::flank ||
        order < FormationOrderKind::move ||
        order > FormationOrderKind::queued_waypoint) {
        return false;
    }
    if (order == FormationOrderKind::guard) {
        const Unit* first = unit_ids.empty()
            ? nullptr : find_unit(unit_ids.front());
        const Unit* target_unit = guard_target_is_building
            ? nullptr : find_unit(guard_target);
        const Building* target_building = guard_target_is_building
            ? find_building(guard_target) : nullptr;
        const EntityOwner target_owner = target_unit != nullptr
            ? target_unit->owner
            : target_building != nullptr
                ? target_building->owner : EntityOwner{Player::neutral};
        const bool valid_target =
            target_unit != nullptr
                ? target_unit->garrisoned_in == 0
                : target_building != nullptr &&
                    target_building->completed();
        if (first == nullptr || !valid_target ||
            target_owner != first->owner ||
            std::ranges::find(unit_ids, guard_target) !=
                unit_ids.end()) return false;
    }
    if (order == FormationOrderKind::queued_waypoint) {
        std::vector<EntityId> unique = unit_ids;
        std::ranges::sort(unique);
        if (std::ranges::adjacent_find(unique) != unique.end()) return false;
        const Unit* first = find_unit(unit_ids.front());
        if (first == nullptr || first->garrisoned_in != 0) return false;
        const Player owner = first->owner;
        const bool ship_domain = is_ship(first->kind);
        if (std::ranges::any_of(unit_ids, [this](EntityId id) {
                const Unit* unit = find_unit(id);
                return unit == nullptr ||
                    unit->waypoints.size() +
                        unit->formation_waypoints.size() >= 20;
            })) return false;
        if (std::ranges::any_of(unit_ids, [this, owner, ship_domain](
                EntityId id
            ) {
                const Unit* unit = find_unit(id);
                return unit == nullptr || unit->owner != owner ||
                    unit->garrisoned_in != 0 ||
                    is_ship(unit->kind) != ship_domain;
            })) return false;
        const auto slots = formation_destinations(unit_ids, center, kind);
        const bool all_idle = std::ranges::all_of(
            unit_ids, [this](EntityId id) {
                const Unit* unit = find_unit(id);
                return unit != nullptr && !unit->moving &&
                    unit->attack_target_id == 0 &&
                    unit->repair_target_id == 0 &&
                    !unit->has_resource_target &&
                    unit->garrison_target_id == 0 &&
                    !unit->attack_moving &&
                    unit->guard_target_id == 0;
            }
        );
        if (all_idle) {
            return command_formation(unit_ids, center, kind);
        }
        std::uint64_t group = first->formation_group_id;
        const auto [interval, speed] = formation_pace(unit_ids);
        bool new_group = group == 0;
        if (new_group) {
            group = next_formation_group_id_;
        }
        for (std::size_t index = 0; index < unit_ids.size(); ++index) {
            Unit* unit = find_unit(unit_ids[index]);
            unit->formation_waypoints.push_back({
                slots[index], center, slots[index], kind,
                group, interval, speed
            });
        }
        if (new_group) ++next_formation_group_id_;
        set_formation_kind(owner, kind);
        return true;
    }
    std::vector<TilePosition> patrol_origins;
    if (order == FormationOrderKind::patrol) {
        TilePosition origin{};
        for (EntityId id : unit_ids) {
            const Unit* unit = find_unit(id);
            if (unit == nullptr) return false;
            origin.x += unit->position.x;
            origin.y += unit->position.y;
        }
        origin.x /= static_cast<int>(unit_ids.size());
        origin.y /= static_cast<int>(unit_ids.size());
        patrol_origins = formation_destinations(
            unit_ids, origin, kind,
            TilePosition{origin.x - center.x, origin.y - center.y}
        );
    }
    if (!command_formation(unit_ids, center, kind)) return false;
    for (std::size_t index = 0; index < unit_ids.size(); ++index) {
        Unit* unit = find_unit(unit_ids[index]);
        if (unit == nullptr) return false;
        if (order == FormationOrderKind::attack_move) {
            unit->attack_moving = true;
            unit->attack_move_destination = unit->formation_slot;
        } else if (order == FormationOrderKind::patrol) {
            unit->attack_moving = true;
            unit->patrolling = true;
            unit->patrol_origin = patrol_origins[index];
            unit->patrol_destination = unit->formation_slot;
            unit->attack_move_destination = unit->formation_slot;
        } else if (order == FormationOrderKind::guard) {
            unit->guard_target_id = guard_target;
            unit->guard_target_is_building = guard_target_is_building;
        }
    }
    return true;
}

bool Simulation::is_unit_selected(EntityId id) const {
    return std::ranges::find(selected_units_, id) != selected_units_.end();
}

bool Simulation::select_building_at(TilePosition position, Player player) {
    Building* found = building_at(position);
    if (found == nullptr || found->owner != player) {
        selected_building_.reset();
        return false;
    }
    selected_building_ = found->id;
    selected_unit_.reset();
    selected_units_.clear();
    return true;
}

bool Simulation::command_unit(
    EntityId unit_id,
    TilePosition destination
) {
    if (outcome_ != MatchOutcome::ongoing || !map_.contains(destination)) {
        return false;
    }
    Unit* unit = find_unit(unit_id);
    if (unit == nullptr) {
        return false;
    }
    if (is_huntable(unit->kind) ||
        unit->trebuchet_transform_ticks_remaining > 0) {
        return false;
    }
    if (unit->garrisoned_in != 0) {
        return false;
    }
    if (gather_trace_enabled() && unit->kind == UnitKind::villager) {
        std::cerr << "GATHER_COMMAND tick=" << tick_number_
                  << " id=" << unit->id
                  << " destination=" << destination.x << ','
                  << destination.y
                  << " active_before=" << unit->has_resource_target
                  << " returning_before=" << unit->returning_resource
                  << " carried_before=" << unit->carried_amount << '\n';
    }
    const bool leaving_formation_movement =
        unit->formation_move_interval > 0;
    unit->formation_move_interval = 0;
    unit->formation_speed_numerator = 0;
    unit->formation_group_id = 0;
    unit->formation_anchor = {-1, -1};
    unit->formation_slot = {-1, -1};
    if (leaving_formation_movement) {
        unit->movement_speed_remainder = 0;
    }
    const auto clear_attack_move = [unit] {
        unit->attack_moving = false;
        unit->attack_move_destination = {-1, -1};
        unit->patrolling = false;
        unit->patrol_origin = {-1, -1};
        unit->patrol_destination = {-1, -1};
        unit->attack_ground_target = {-1, -1};
        unit->attacking_ground = false;
        unit->guard_target_id = 0;
        unit->guard_target_is_building = false;
        unit->waypoints.clear();
        unit->formation_waypoints.clear();
        unit->attack_target_auto = false;
        unit->returning_to_stance = false;
    };
    Unit* target = unit_at(destination);
    Building* building_target = building_at(destination);
    if (unit->kind == UnitKind::trebuchet &&
        target == nullptr && building_target == nullptr) {
        return false;
    }
    if (unit->kind == UnitKind::fishing_ship &&
        map_.terrain_at(destination) == Terrain::fish &&
        map_.resource_amount_at(destination) > 0) {
        if (!route_unit(*unit, destination)) {
            return false;
        }
        unit->has_resource_target = true;
        unit->resource_target = destination;
        unit->carried_resource = ResourceKind::food;
        unit->returning_resource = false;
        return true;
    }
    if (unit->kind == UnitKind::fishing_ship &&
        building_target != nullptr &&
        building_target->kind == BuildingKind::fish_trap &&
        building_target->owner == unit->owner &&
        building_target->completed() &&
        building_target->resource_amount > 0) {
        if (!route_unit(*unit, destination)) {
            return false;
        }
        unit->has_resource_target = true;
        unit->resource_target = building_target->position;
        unit->resource_building_id = building_target->id;
        unit->carried_resource = ResourceKind::food;
        unit->returning_resource = false;
        return true;
    }
    if (target != nullptr && is_relic(target->kind)) {
        return false;
    }
    if (unit->kind == UnitKind::villager && target != nullptr) {
        if ((target->owner == unit->owner ||
             target->owner.is_neutral()) &&
            is_herdable(target->kind)) {
            return command_gather_unit(unit_id, target->id);
        }
        if (is_huntable(target->kind) &&
            target->hit_points <= 0) {
            return command_gather_unit(unit_id, target->id);
        }
    }
    if (is_ram(unit->kind) &&
        target != nullptr &&
        target->owner != unit->owner) {
        return false;
    }
    const ResourceKind target_resource =
        resource_for(map_.terrain_at(destination));
    const bool gathering =
        target_resource != ResourceKind::none &&
        unit->kind == UnitKind::villager;
    const bool fishing_destination =
        is_ship(unit->kind) && map_.sailable(destination);
    if (is_ship(unit->kind) &&
        !fishing_destination &&
        building_target == nullptr) {
        return false;
    }
    if ((!map_.walkable(destination) && !fishing_destination && !gathering) ||
        (target_resource != ResourceKind::none && !gathering)) {
        return false;
    }
    if (target != nullptr && !is_enemy(target->owner, unit->owner)) {
        return false;
    }
    if (building_target != nullptr &&
        building_target->owner == unit->owner) {
        const bool villager_repairs =
            unit->kind == UnitKind::villager &&
            building_target->completed() &&
            building_target->hit_points <
                maximum_hit_points(*building_target);
        if (!villager_repairs && can_garrison(*unit, *building_target)) {
            if (garrison_count(building_target->id) >=
                    garrison_capacity(building_target->kind) ||
                !route_unit(*unit, destination)) {
                return false;
            }
            unit->attack_target_id = 0;
            unit->attack_target_is_building = false;
            unit->repair_target_id = 0;
            unit->has_resource_target = false;
            unit->returning_resource = false;
            unit->garrison_target_id = building_target->id;
            detach_builder(unit->id);
            clear_attack_move();
            return true;
        }
        if (unit->kind == UnitKind::villager &&
            building_target->kind == BuildingKind::farm &&
            building_target->completed() &&
            building_target->resource_amount > 0) {
            route_unit(*unit, destination);
            unit->attack_target_id = 0;
            unit->attack_target_is_building = false;
            unit->repair_target_id = 0;
            unit->has_resource_target = true;
            unit->resource_target = destination;
            unit->resource_building_id = building_target->id;
            unit->resource_unit_id = 0;
            unit->returning_resource = false;
            if (unit->carried_amount == 0) {
                unit->carried_resource = ResourceKind::food;
            }
            if (unit->carried_amount > 0 &&
                unit->carried_resource != ResourceKind::food) {
                Building* drop_off = nearest_drop_off(*unit);
                unit->returning_resource = true;
                if (drop_off != nullptr) {
                    route_unit(*unit, drop_off->position);
                } else {
                    unit->moving = false;
                }
            }
            detach_builder(unit->id);
            clear_attack_move();
            return true;
        }
        if (unit->kind == UnitKind::villager &&
            building_target->completed() &&
            building_target->hit_points <
                maximum_hit_points(*building_target)) {
            if (!route_unit(*unit, destination)) {
                return false;
            }
            unit->attack_target_id = 0;
            unit->attack_target_is_building = false;
            if (unit->repair_target_id != building_target->id) {
                unit->repair_wood_remainder = 0;
                unit->repair_stone_remainder = 0;
            }
            unit->repair_target_id = building_target->id;
            unit->has_resource_target = false;
            unit->returning_resource = false;
            detach_builder(unit->id);
            clear_attack_move();
            return true;
        }
        if (unit->kind != UnitKind::villager ||
            building_target->completed()) {
            return false;
        }
        if (!route_unit(*unit, destination)) {
            return false;
        }
        unit->attack_target_id = 0;
        unit->attack_target_is_building = false;
        unit->repair_target_id = 0;
        detach_builder(unit->id);
        if (std::ranges::find(
                building_target->builder_ids,
                unit->id
            ) == building_target->builder_ids.end()) {
            building_target->builder_ids.push_back(unit->id);
        }
        if (building_target->builder_id == 0) {
            building_target->builder_id = unit->id;
        }
        unit->has_resource_target = false;
        unit->returning_resource = false;
        clear_attack_move();
        return true;
    }

    if (building_target != nullptr &&
        !is_enemy(building_target->owner, unit->owner)) {
        return false;
    }
    const bool routed = gathering
        ? route_to_resource_interaction(*unit, destination)
        : route_unit(*unit, destination);
    if (!routed && !gathering) {
        return false;
    }
    unit->attack_target_id =
        target != nullptr ? target->id :
        building_target != nullptr ? building_target->id : 0;
    unit->attack_target_is_building =
        building_target != nullptr;
    unit->repair_target_id = 0;
    unit->has_resource_target = gathering;
    unit->resource_target =
        gathering ? destination : TilePosition{-1, -1};
    unit->resource_building_id = 0;
    unit->resource_unit_id = 0;
    unit->returning_resource = false;
    unit->garrison_target_id = 0;
    if (gathering && unit->carried_amount == 0) {
        unit->carried_resource = target_resource;
    }
    if (gathering && unit->carried_amount > 0 &&
        unit->carried_resource != target_resource) {
        Building* drop_off = nearest_drop_off(*unit);
        unit->returning_resource = true;
        if (drop_off != nullptr) {
            route_unit(*unit, drop_off->position);
        } else {
            unit->moving = false;
        }
    }
    detach_builder(unit->id);
    clear_attack_move();
    return true;
}

bool Simulation::command_attack_move(
    EntityId unit_id,
    TilePosition destination
) {
    if (outcome_ != MatchOutcome::ongoing ||
        !map_.contains(destination) ||
        !map_.walkable(destination)) {
        return false;
    }
    Unit* unit = find_unit(unit_id);
    if (unit == nullptr || unit->garrisoned_in != 0 ||
        (rules_for(unit->kind).attack <= 0 &&
         !commercial_has_ability(*unit, CommercialTaskAbility::combat)) ||
        (is_animal(unit->kind) || is_relic(unit->kind)) ||
        !route_unit(*unit, destination)) {
        return false;
    }
    unit->attack_target_id = 0;
    unit->attack_target_is_building = false;
    unit->attack_target_auto = false;
    unit->repair_target_id = 0;
    unit->repair_wood_remainder = 0;
    unit->repair_stone_remainder = 0;
    unit->has_resource_target = false;
    unit->resource_target = {-1, -1};
    unit->resource_building_id = 0;
    unit->resource_unit_id = 0;
    unit->returning_resource = false;
    unit->garrison_target_id = 0;
    unit->attack_move_destination = destination;
    unit->attack_moving = true;
    unit->patrolling = false;
    unit->patrol_origin = {-1, -1};
    unit->patrol_destination = {-1, -1};
    unit->attack_ground_target = {-1, -1};
    unit->attacking_ground = false;
    unit->guard_target_id = 0;
    unit->guard_target_is_building = false;
    unit->waypoints.clear();
    unit->formation_waypoints.clear();
    detach_builder(unit->id);
    return true;
}

bool Simulation::command_attack_ground(
    EntityId unit_id,
    TilePosition destination
) {
    if (outcome_ != MatchOutcome::ongoing ||
        !map_.contains(destination)) {
        return false;
    }
    Unit* unit = find_unit(unit_id);
    if (unit == nullptr ||
        (unit->kind != UnitKind::mangonel &&
         unit->kind != UnitKind::onager &&
         unit->kind != UnitKind::siege_onager) ||
        unit->garrisoned_in != 0) {
        return false;
    }
    const int distance_squared =
        combat_distance_squared(unit->position, destination);
    const int minimum_range =
        effective_minimum_attack_range(*unit);
    const int attack_range = effective_attack_range(*unit);
    if (distance_squared <=
        minimum_range * minimum_range) {
        return false;
    }
    if (!stop_unit(unit_id)) {
        return false;
    }
    unit = find_unit(unit_id);
    if (distance_squared > attack_range * attack_range &&
        !route_unit(*unit, destination)) {
        return false;
    }
    unit->attack_ground_target = destination;
    unit->attacking_ground = true;
    return true;
}

bool Simulation::command_convert(EntityId monk_id, EntityId target_id) {
    if (outcome_ != MatchOutcome::ongoing || monk_id == target_id) {
        return false;
    }
    Unit* monk = find_unit(monk_id);
    Unit* target = find_unit(target_id);
    Building* building_target = find_building(target_id);
    if (monk != nullptr && target == nullptr &&
        building_target != nullptr) {
        if ((monk->kind != UnitKind::monk &&
             monk->kind != UnitKind::missionary &&
             !commercial_has_ability(*monk, CommercialTaskAbility::convert)) ||
            !has_technology(monk->owner, Technology::redemption) ||
            !is_enemy(monk->owner, building_target->owner) ||
            !building_target->completed() ||
            building_target->hit_points <= 0 ||
            monk->garrisoned_in != 0 ||
            monk->conversion_cooldown > 0 ||
            !is_building_visible(monk->owner, *building_target)) {
            return false;
        }
        const int range =
            effective_attack_range(*monk) +
            (has_technology(
                monk->owner, Technology::block_printing
            ) ? 3 : 0);
        if (combat_distance_squared(
                monk->position, *building_target
            ) > range * range) {
            return false;
        }
        stop_unit(monk_id);
        monk = find_unit(monk_id);
        monk->conversion_target_id = target_id;
        monk->conversion_progress = 0;
        return true;
    }
    const bool religious_target =
        target != nullptr &&
        (target->kind == UnitKind::monk ||
         target->kind == UnitKind::missionary);
    const bool siege_target =
        target != nullptr &&
        (rules_for(target->kind).trained_at ==
             BuildingKind::siege_workshop ||
         target->kind == UnitKind::packed_trebuchet ||
         target->kind == UnitKind::trebuchet);
    if (monk == nullptr || target == nullptr ||
        (monk->kind != UnitKind::monk &&
         monk->kind != UnitKind::missionary &&
         !commercial_has_ability(*monk, CommercialTaskAbility::convert)) ||
        !is_enemy(monk->owner, target->owner) ||
        monk->garrisoned_in != 0 ||
        target->garrisoned_in != 0 ||
        (is_animal(target->kind) || is_relic(target->kind)) ||
        target->hit_points <= 0 ||
        target->unconvertible ||
        monk->conversion_cooldown > 0 ||
        (religious_target &&
         !has_technology(monk->owner, Technology::atonement)) ||
        (siege_target &&
         !has_technology(monk->owner, Technology::redemption)) ||
        !is_unit_visible(monk->owner, *target)) {
        return false;
    }
    const int distance_squared =
        combat_distance_squared(monk->position, target->position);
    const int conversion_range =
        effective_attack_range(*monk) +
        (has_technology(monk->owner, Technology::block_printing) ? 3 : 0);
    if (distance_squared > conversion_range * conversion_range) {
        return false;
    }
    stop_unit(monk_id);
    monk = find_unit(monk_id);
    monk->conversion_target_id = target_id;
    monk->conversion_progress = 0;
    return true;
}

bool Simulation::command_heal(EntityId monk_id, EntityId target_id) {
    Unit* monk = find_unit(monk_id);
    Unit* target = find_unit(target_id);
    if (outcome_ != MatchOutcome::ongoing || monk_id == target_id ||
        monk == nullptr || target == nullptr ||
        (monk->kind != UnitKind::monk &&
         monk->kind != UnitKind::missionary &&
         !commercial_has_ability(*monk, CommercialTaskAbility::heal)) ||
        monk->owner != target->owner ||
        (!is_organic(target->kind) && !target->commercial_identity) ||
        target->garrisoned_in != 0 ||
        target->hit_points <= 0 ||
        target->hit_points >= maximum_hit_points(*target) ||
        combat_distance_squared(monk->position, target->position) >
            (civilization(monk->owner) == Civilization::teutons
                ? 64 : 16)) {
        return false;
    }
    stop_unit(monk_id);
    monk = find_unit(monk_id);
    monk->healing_target_id = target_id;
    return true;
}

bool Simulation::command_collect_relic(
    EntityId monk_id,
    EntityId relic_id
) {
    Unit* monk = find_unit(monk_id);
    Unit* relic = find_unit(relic_id);
    if (outcome_ != MatchOutcome::ongoing ||
        monk == nullptr || relic == nullptr ||
        monk->kind != UnitKind::monk || !is_relic(relic->kind) ||
        relic->owner != Player::neutral ||
        monk->carrying_relic) {
        return false;
    }
    if (std::abs(monk->position.x - relic->position.x) +
            std::abs(monk->position.y - relic->position.y) > 1) {
        for (TilePosition approach : {
                 TilePosition{relic->position.x + 1, relic->position.y},
                 TilePosition{relic->position.x - 1, relic->position.y},
                 TilePosition{relic->position.x, relic->position.y + 1},
                 TilePosition{relic->position.x, relic->position.y - 1},
             }) {
            if (map_.contains(approach) && map_.walkable(approach) &&
                route_unit(*monk, approach)) {
                monk->relic_target_id = relic_id;
                monk->relic_deposit_target_id = 0;
                return true;
            }
        }
        return false;
    }
    monk->carrying_relic = true;
    monk->relic_target_id = 0;
    std::erase_if(units_, [relic_id](const Unit& unit) {
        return unit.id == relic_id;
    });
    prune_unit_render_elevations();
    return true;
}

bool Simulation::command_deposit_relic(
    EntityId monk_id,
    EntityId monastery_id
) {
    Unit* monk = find_unit(monk_id);
    Building* monastery = find_building(monastery_id);
    if (outcome_ != MatchOutcome::ongoing ||
        monk == nullptr || monastery == nullptr ||
        monk->kind != UnitKind::monk || !monk->carrying_relic ||
        monastery->kind != BuildingKind::monastery ||
        monastery->owner != monk->owner || !monastery->completed()) {
        return false;
    }
    if (distance_to_building(monk->position, *monastery) > 1) {
        const auto approach = spawn_position(*monastery);
        if (!approach || !route_unit(*monk, *approach)) return false;
        monk->relic_target_id = 0;
        monk->relic_deposit_target_id = monastery_id;
        return true;
    }
    monk->carrying_relic = false;
    monk->relic_deposit_target_id = 0;
    ++monastery->relic_count;
    ++mutable_statistics(monk->owner).relics_collected;
    return true;
}

int Simulation::market_base_price(MarketResource resource) const {
    if (resource != MarketResource::food &&
        resource != MarketResource::wood &&
        resource != MarketResource::stone) {
        throw std::invalid_argument("invalid market resource");
    }
    return resource == MarketResource::food
        ? food_market_price_
        : resource == MarketResource::wood
            ? wood_market_price_
            : stone_market_price_;
}

int Simulation::market_buy_price(MarketResource resource) const {
    return market_buy_price(Player::blue, resource);
}

int Simulation::market_sell_price(MarketResource resource) const {
    return market_sell_price(Player::blue, resource);
}

int Simulation::market_buy_price(
    Player player,
    MarketResource resource
) const {
    const int fee =
        civilization(player) == Civilization::saracens ? 5 :
        has_technology(player, Technology::guilds) ? 15 : 30;
    return market_price_after_fee(
        market_base_price(resource), fee, true
    );
}

int Simulation::market_sell_price(
    Player player,
    MarketResource resource
) const {
    const int fee =
        civilization(player) == Civilization::saracens ? 5 :
        has_technology(player, Technology::guilds) ? 15 : 30;
    return market_price_after_fee(
        market_base_price(resource), fee, false
    );
}

bool Simulation::buy_resource(Player player, MarketResource resource) {
    if (outcome_ != MatchOutcome::ongoing || player == Player::neutral ||
        (resource != MarketResource::food &&
         resource != MarketResource::wood &&
         resource != MarketResource::stone) ||
        std::ranges::none_of(buildings_, [player](const Building& building) {
            return building.owner == player &&
                building.kind == BuildingKind::market &&
                building.completed() && building.hit_points > 0;
        })) {
        return false;
    }
    Economy& economy =
        player == Player::blue ? player_states_[0].economy : player_states_[1].economy;
    const int price = market_buy_price(player, resource);
    if (economy.gold < price) {
        return false;
    }
    economy.gold -= price;
    int* stock = resource == MarketResource::food
        ? &economy.food
        : resource == MarketResource::wood
            ? &economy.wood
            : &economy.stone;
    *stock += 100;
    int& base = resource == MarketResource::food
        ? food_market_price_
        : resource == MarketResource::wood
            ? wood_market_price_
            : stone_market_price_;
    base = std::min(999, base + 3);
    return true;
}

bool Simulation::sell_resource(Player player, MarketResource resource) {
    if (outcome_ != MatchOutcome::ongoing || player == Player::neutral ||
        (resource != MarketResource::food &&
         resource != MarketResource::wood &&
         resource != MarketResource::stone) ||
        std::ranges::none_of(buildings_, [player](const Building& building) {
            return building.owner == player &&
                building.kind == BuildingKind::market &&
                building.completed() && building.hit_points > 0;
        })) {
        return false;
    }
    Economy& economy =
        player == Player::blue ? player_states_[0].economy : player_states_[1].economy;
    int* stock = resource == MarketResource::food
        ? &economy.food
        : resource == MarketResource::wood
            ? &economy.wood
            : &economy.stone;
    if (*stock < 100) {
        return false;
    }
    *stock -= 100;
    economy.gold += market_sell_price(player, resource);
    int& base = resource == MarketResource::food
        ? food_market_price_
        : resource == MarketResource::wood
            ? wood_market_price_
            : stone_market_price_;
    base = std::max(20, base - 3);
    return true;
}

bool Simulation::tribute_resource(
    Player from,
    Player to,
    ResourceKind resource,
    int amount
) {
    if (outcome_ != MatchOutcome::ongoing ||
        from == Player::neutral || to == Player::neutral ||
        from == to || amount <= 0 ||
        resource == ResourceKind::none ||
        !is_ally(from, to) ||
        std::ranges::none_of(
            buildings_,
            [from](const Building& building) {
                return building.owner == from &&
                    building.kind == BuildingKind::market &&
                    building.completed() && building.hit_points > 0;
            }
        )) {
        return false;
    }
    Economy& sender =
        from == Player::blue ? player_states_[0].economy : player_states_[1].economy;
    Economy& recipient =
        to == Player::blue ? player_states_[0].economy : player_states_[1].economy;
    const int fee_percent =
        has_technology(from, Technology::banking) ? 0 :
        has_technology(from, Technology::coinage) ? 20 : 30;
    const int fee = percentage_fee_floor(amount, fee_percent);
    int* source =
        resource == ResourceKind::wood ? &sender.wood :
        resource == ResourceKind::food ? &sender.food :
        resource == ResourceKind::gold ? &sender.gold :
        resource == ResourceKind::stone ? &sender.stone : nullptr;
    int* destination =
        resource == ResourceKind::wood ? &recipient.wood :
        resource == ResourceKind::food ? &recipient.food :
        resource == ResourceKind::gold ? &recipient.gold :
        resource == ResourceKind::stone ? &recipient.stone : nullptr;
    const std::int64_t total_cost =
        static_cast<std::int64_t>(amount) + fee;
    if (source == nullptr || destination == nullptr ||
        total_cost > std::numeric_limits<int>::max() ||
        *source < total_cost ||
        static_cast<std::int64_t>(*destination) + amount >
            std::numeric_limits<int>::max()) {
        return false;
    }
    *source -= static_cast<int>(total_cost);
    *destination += amount;
    const auto add_resource = [resource, amount](
        ResourceStatistics& statistics
    ) {
        if (resource == ResourceKind::food) statistics.food += amount;
        else if (resource == ResourceKind::wood) statistics.wood += amount;
        else if (resource == ResourceKind::gold) statistics.gold += amount;
        else if (resource == ResourceKind::stone) statistics.stone += amount;
    };
    add_resource(mutable_statistics(from).tribute_sent);
    add_resource(mutable_statistics(to).tribute_received);
    return true;
}

Diplomacy Simulation::diplomacy(Player player, Player other) const {
    if (player == other) {
        return Diplomacy::ally;
    }
    if (player == Player::neutral || other == Player::neutral) {
        return Diplomacy::neutral;
    }
    return blue_red_diplomacy_;
}

Diplomacy Simulation::diplomacy(
    PlayerSlotId player,
    PlayerSlotId other
) const {
    if (player.is_neutral() || other.is_neutral()) {
        return player == other ? Diplomacy::ally : Diplomacy::neutral;
    }
    return roster_diplomacy_.stance(player, other);
}

Diplomacy Simulation::diplomacy(
    EntityOwner player,
    EntityOwner other
) const {
    const auto first = entity_owner_slot(player);
    const auto second = entity_owner_slot(other);
    if (!first || !second) return Diplomacy::neutral;
    return diplomacy(*first, *second);
}

Diplomacy Simulation::diplomacy(
    Player player,
    EntityOwner other
) const {
    return diplomacy(EntityOwner{player}, other);
}

Diplomacy Simulation::diplomacy(
    EntityOwner player,
    Player other
) const {
    return diplomacy(player, EntityOwner{other});
}

bool Simulation::is_enemy(Player player, Player other) const {
    return diplomacy(player, other) == Diplomacy::enemy;
}

bool Simulation::is_enemy(
    EntityOwner player,
    EntityOwner other
) const {
    return diplomacy(player, other) == Diplomacy::enemy;
}

bool Simulation::is_enemy(EntityOwner player, Player other) const {
    return is_enemy(player, EntityOwner{other});
}

bool Simulation::is_enemy(Player player, EntityOwner other) const {
    return is_enemy(EntityOwner{player}, other);
}

bool Simulation::is_enemy(
    PlayerSlotId player,
    EntityOwner other
) const {
    return is_enemy(entity_owner_from_slot(player), other);
}

bool Simulation::is_enemy(
    EntityOwner player,
    PlayerSlotId other
) const {
    return is_enemy(player, entity_owner_from_slot(other));
}

bool Simulation::is_ally(Player player, Player other) const {
    return diplomacy(player, other) == Diplomacy::ally;
}

bool Simulation::is_ally(
    EntityOwner player,
    EntityOwner other
) const {
    return diplomacy(player, other) == Diplomacy::ally;
}

bool Simulation::is_ally(EntityOwner player, Player other) const {
    return is_ally(player, EntityOwner{other});
}

bool Simulation::is_ally(Player player, EntityOwner other) const {
    return is_ally(EntityOwner{player}, other);
}

bool Simulation::is_ally(
    PlayerSlotId player,
    EntityOwner other
) const {
    return is_ally(entity_owner_from_slot(player), other);
}

bool Simulation::is_ally(
    EntityOwner player,
    PlayerSlotId other
) const {
    return is_ally(player, entity_owner_from_slot(other));
}

Civilization Simulation::civilization(Player player) const {
    const auto slot = player_slot_from_legacy(player);
    return !slot || slot->is_neutral()
        ? Civilization::generic : civilization(*slot);
}

Civilization Simulation::civilization(PlayerSlotId player) const {
    return player_state(player).civilization;
}

Civilization Simulation::civilization(EntityOwner player) const {
    const auto slot = entity_owner_slot(player);
    return !slot || slot->is_neutral()
        ? Civilization::generic : civilization(*slot);
}

bool Simulation::team_has_civilization(
    Player player,
    Civilization required
) const {
    if (civilization(player) == required) return true;
    const Player other =
        player == Player::blue ? Player::red : Player::blue;
    return diplomacy(player, other) == Diplomacy::ally &&
           diplomacy(other, player) == Diplomacy::ally &&
           civilization(other) == required;
}

bool Simulation::team_has_civilization(
    EntityOwner player,
    Civilization required
) const {
    const auto source = entity_owner_slot(player);
    if (!source || source->is_neutral()) return false;
    if (civilization(*source) == required) return true;
    for (std::size_t index = 0; index < player_states_.size(); ++index) {
        const auto other = *PlayerSlotId::from_index(index);
        if (other == *source || !roster_.slot(other).occupied) continue;
        if (roster_diplomacy_.stance(*source, other) == Diplomacy::ally &&
            roster_diplomacy_.stance(other, *source) == Diplomacy::ally &&
            civilization(other) == required) {
            return true;
        }
    }
    return false;
}

bool Simulation::set_civilization(
    Player player,
    Civilization selected
) {
    if (player == Player::neutral || tick_number_ != 0) {
        return false;
    }
    std::vector<int> old_maximums;
    std::vector<int> old_building_maximums;
    for (const Unit& unit : units_) {
        if (unit.owner == player) {
            old_maximums.push_back(maximum_hit_points(unit));
        }
    }
    for (const Building& building : buildings_) {
        if (building.owner == player) {
            old_building_maximums.push_back(
                maximum_hit_points(building)
            );
        }
    }
    (player == Player::blue
        ? player_states_[0].civilization
        : player_states_[1].civilization) = selected;
    std::size_t index{};
    for (Unit& unit : units_) {
        if (unit.owner != player) {
            continue;
        }
        const int maximum = maximum_hit_points(unit);
        unit.hit_points = std::clamp(
            unit.hit_points + maximum - old_maximums[index++],
            1,
            maximum
        );
    }
    index = 0;
    for (Building& building : buildings_) {
        if (building.owner != player) {
            continue;
        }
        const int maximum = maximum_hit_points(building);
        building.hit_points = std::clamp(
            building.hit_points + maximum -
                old_building_maximums[index++],
            1,
            maximum
        );
    }
    return true;
}

void Simulation::replace_player_state(
    PlayerSlotId player,
    PlayerState state
) {
    if (player.is_neutral()) {
        throw std::invalid_argument("neutral has no player state");
    }
    const auto expected_size =
        static_cast<std::size_t>(map_.width() * map_.height());
    if (state.explored.size() != expected_size) {
        throw std::invalid_argument("player exploration size mismatch");
    }
    player_states_.at(*player.index()) = std::move(state);
}

const Simulation::PlayerState& Simulation::player_state(
    PlayerSlotId player
) const {
    const auto index = player.index();
    if (!index) {
        throw std::invalid_argument("neutral has no player state");
    }
    return player_states_.at(*index);
}

void Simulation::replace_roster(
    MatchRoster roster,
    RosterDiplomacy diplomacy
) {
    roster_ = std::move(roster);
    roster_diplomacy_ = std::move(diplomacy);
}

bool Simulation::set_diplomacy(
    Player player,
    Player other,
    Diplomacy relation
) {
    if (outcome_ != MatchOutcome::ongoing ||
        player == other || player == Player::neutral ||
        other == Player::neutral) {
        return false;
    }
    blue_red_diplomacy_ = relation;
    const auto blue = *PlayerSlotId::from_index(0);
    const auto red = *PlayerSlotId::from_index(1);
    roster_diplomacy_.set_symmetric_stance(blue, red, relation);
    for (Unit& unit : units_) {
        if (unit.attack_target_id != 0) {
            Unit* target = find_unit(unit.attack_target_id);
            Building* building = find_building(unit.attack_target_id);
            const EntityOwner owner = target != nullptr
                ? target->owner
                : building != nullptr ? building->owner : unit.owner;
            if (!is_enemy(unit.owner, owner)) {
                unit.attack_target_id = 0;
                unit.attack_target_is_building = false;
                unit.moving = false;
            }
        }
    }
    update_match_outcome();
    update_exploration();
    return true;
}

bool Simulation::command_trade_route(
    EntityId cart_id,
    EntityId target_market_id
) {
    Unit* cart = find_unit(cart_id);
    Building* target = find_building(target_market_id);
    const bool naval = cart != nullptr &&
        cart->kind == UnitKind::trade_cog;
    const bool commercial_trade = cart != nullptr &&
        (commercial_has_ability(*cart, CommercialTaskAbility::trade) ||
         commercial_has_ability(*cart, CommercialTaskAbility::off_map_trade));
    const BuildingKind route_building =
        naval ? BuildingKind::dock : BuildingKind::market;
    if (outcome_ != MatchOutcome::ongoing || cart == nullptr ||
        target == nullptr ||
        (cart->kind != UnitKind::trade_cart && !naval && !commercial_trade) ||
        (!commercial_trade && target->kind != route_building) || !target->completed() ||
        target->hit_points <= 0 ||
        !is_ally(cart->owner, target->owner) ||
        target->owner == cart->owner) {
        return false;
    }
    Building* home{};
    int nearest = map_.width() + map_.height() + 1;
    for (Building& building : buildings_) {
        if ((!commercial_trade && building.kind != route_building) ||
            (commercial_trade && (!building.commercial_identity ||
             !target->commercial_identity ||
             building.commercial_identity->object_id !=
                 target->commercial_identity->object_id)) ||
            building.owner != cart->owner || !building.completed() ||
            building.hit_points <= 0) {
            continue;
        }
        const int distance = distance_to_building(cart->position, building);
        if (distance < nearest) {
            nearest = distance;
            home = &building;
        }
    }
    const auto trade_destination =
        [this, naval, cart](const Building& building)
            -> std::optional<TilePosition> {
            if (!naval) return spawn_position(building);
            const BuildingRules& rules = rules_for(building.kind);
            for (int y = building.position.y - 1;
                 y <= building.position.y + rules.footprint_height; ++y) {
                for (int x = building.position.x - 1;
                     x <= building.position.x + rules.footprint_width; ++x) {
                    const TilePosition tile{x, y};
                    if (map_.sailable(tile) &&
                        !occupied(tile, cart->id)) {
                        return tile;
                    }
                }
            }
            return std::nullopt;
        };
    const auto destination = trade_destination(*target);
    if (home == nullptr || !destination) {
        return false;
    }
    const Unit original_cart = *cart;
    if (!route_unit(*cart, *destination)) {
        *cart = original_cart;
        return false;
    }
    cart->trade_home_market_id = home->id;
    cart->trade_target_market_id = target->id;
    cart->trade_returning = false;
    cart->trade_waiting = false;
    cart->trade_work_ticks_remaining = 0;
    return true;
}

bool Simulation::command_embark(EntityId unit_id, EntityId transport_id) {
    Unit* unit = find_unit(unit_id);
    Unit* transport = find_unit(transport_id);
    if (outcome_ != MatchOutcome::ongoing || unit == nullptr ||
        transport == nullptr || unit_id == transport_id ||
        (transport->kind != UnitKind::transport_ship &&
         !commercial_has_ability(*transport, CommercialTaskAbility::transport)) ||
        unit->owner != transport->owner || is_ship(unit->kind) ||
        unit->garrisoned_in != 0 ||
        std::abs(unit->position.x - transport->position.x) +
            std::abs(unit->position.y - transport->position.y) > 1 ||
        std::ranges::count_if(units_, [transport_id](const Unit& passenger) {
            return passenger.garrisoned_in == transport_id;
        }) >= (transport->commercial_identity
            ? std::max(0, commercial_content_catalog().object(
                  transport->commercial_identity->civilization_id,
                  transport->commercial_identity->object_id
              )->garrison_capacity)
            : transport_capacity(transport->owner))) {
        return false;
    }
    stop_unit(unit_id);
    unit = find_unit(unit_id);
    unit->garrisoned_in = transport_id;
    unit->position = transport->position;
    unit->previous_position = transport->position;
    return true;
}

bool Simulation::command_disembark(
    EntityId transport_id,
    TilePosition shore
) {
    Unit* transport = find_unit(transport_id);
    if (outcome_ != MatchOutcome::ongoing || transport == nullptr ||
        transport->kind != UnitKind::transport_ship ||
        !map_.walkable(shore) ||
        std::abs(transport->position.x - shore.x) +
            std::abs(transport->position.y - shore.y) > 1) {
        return false;
    }
    std::vector<TilePosition> landing_tiles{shore};
    std::vector<bool> visited(
        static_cast<std::size_t>(map_.width() * map_.height()),
        false
    );
    const auto visit = [this, &visited](TilePosition position) {
        visited[static_cast<std::size_t>(
            position.y * map_.width() + position.x
        )] = true;
    };
    const auto was_visited = [this, &visited](TilePosition position) {
        return visited[static_cast<std::size_t>(
            position.y * map_.width() + position.x
        )];
    };
    visit(shore);
    for (std::size_t index = 0; index < landing_tiles.size(); ++index) {
        constexpr std::array<TilePosition, 4> offsets{{
            {1, 0}, {0, 1}, {-1, 0}, {0, -1},
        }};
        for (const TilePosition offset : offsets) {
            const TilePosition candidate{
                landing_tiles[index].x + offset.x,
                landing_tiles[index].y + offset.y,
            };
            if (!map_.contains(candidate) || was_visited(candidate)) {
                continue;
            }
            visit(candidate);
            if (map_.walkable(candidate)) {
                landing_tiles.push_back(candidate);
            }
        }
    }
    bool released = false;
    for (Unit& passenger : units_) {
        if (passenger.garrisoned_in != transport_id) continue;
        const auto position = std::ranges::find_if(
            landing_tiles,
            [this, &passenger](TilePosition candidate) {
                return !occupied(candidate, passenger.id);
            }
        );
        if (position == landing_tiles.end()) {
            continue;
        }
        passenger.garrisoned_in = 0;
        passenger.position = *position;
        passenger.previous_position = *position;
        passenger.destination = *position;
        released = true;
    }
    return released;
}

bool Simulation::restore_garrison(
    EntityId unit_id,
    EntityId building_id
) {
    Unit* unit = find_unit(unit_id);
    Building* building = find_building(building_id);
    if (unit == nullptr || building == nullptr ||
        unit->hit_points <= 0 || building->hit_points <= 0 ||
        unit->garrisoned_in != 0 || !can_garrison(*unit, *building) ||
        garrison_count(building_id) >= garrison_capacity(building->kind)) {
        return false;
    }
    unit->moving = false;
    unit->path.clear();
    unit->next_path_step = 0;
    unit->attack_target_id = 0;
    unit->attack_target_is_building = false;
    unit->attack_target_auto = false;
    unit->repair_target_id = 0;
    unit->has_resource_target = false;
    unit->returning_resource = false;
    unit->garrison_target_id = 0;
    unit->attack_moving = false;
    unit->patrolling = false;
    unit->attacking_ground = false;
    unit->guard_target_id = 0;
    unit->guard_target_is_building = false;
    unit->waypoints.clear();
    unit->formation_waypoints.clear();
    unit->returning_to_stance = false;
    unit->garrisoned_in = building_id;
    unit->position = building->position;
    unit->previous_position = building->position;
    unit->destination = building->position;
    unit->stance_anchor = building->position;
    return true;
}

void Simulation::validate_loaded_state() const {
    std::vector<EntityId> ids;
    ids.reserve(units_.size() + buildings_.size());
    const auto add_id = [&ids](EntityId id) {
        if (id == 0 || std::ranges::find(ids, id) != ids.end()) {
            throw std::runtime_error("duplicate or zero entity id in save");
        }
        ids.push_back(id);
    };
    for (const Building& building : buildings_) {
        add_id(building.id);
        const auto building_slot = entity_owner_slot(building.owner);
        if (!building_slot || building_slot->is_neutral() ||
            !roster_.slot(*building_slot).occupied) {
            throw std::runtime_error("invalid building owner in save");
        }
        if (static_cast<int>(building.kind) <
                static_cast<int>(BuildingKind::town_center) ||
            static_cast<int>(building.kind) >
                static_cast<int>(BuildingKind::fortified_gate_y)) {
            throw std::runtime_error("invalid building kind in save");
        }
        const BuildingRules& rules = rules_for(building.kind);
        if (building.kind == BuildingKind::farm &&
            (building.resource_amount < 0 ||
             building.resource_amount > 550)) {
            throw std::runtime_error("invalid Farm food in save");
        }
        for (int y = 0; y < rules.footprint_height; ++y) {
            for (int x = 0; x < rules.footprint_width; ++x) {
                const TilePosition tile{
                    building.position.x + x,
                    building.position.y + y,
                };
                const bool terrain_allowed =
                    building.kind == BuildingKind::fish_trap
                        ? map_.sailable(tile)
                        : map_.contains(tile) &&
                            map_.terrain_at(tile) == Terrain::grass;
                if (!terrain_allowed) {
                    throw std::runtime_error(
                        "invalid building footprint in save"
                    );
                }
            }
        }
    }
    for (std::size_t first = 0; first < buildings_.size(); ++first) {
        const BuildingRules& first_rules = rules_for(buildings_[first].kind);
        for (std::size_t second = first + 1;
             second < buildings_.size(); ++second) {
            const BuildingRules& second_rules =
                rules_for(buildings_[second].kind);
            const bool separated =
                buildings_[first].position.x + first_rules.footprint_width <=
                    buildings_[second].position.x ||
                buildings_[second].position.x +
                        second_rules.footprint_width <=
                    buildings_[first].position.x ||
                buildings_[first].position.y + first_rules.footprint_height <=
                    buildings_[second].position.y ||
                buildings_[second].position.y +
                        second_rules.footprint_height <=
                    buildings_[first].position.y;
            if (!separated) {
                throw std::runtime_error("overlapping buildings in save");
            }
        }
    }
    std::vector<TilePosition> occupied_units;
    for (const Unit& unit : units_) {
        add_id(unit.id);
        const auto unit_slot = entity_owner_slot(unit.owner);
        if (!unit_slot ||
            (!unit_slot->is_neutral() &&
             !roster_.slot(*unit_slot).occupied)) {
            throw std::runtime_error("invalid unit owner in save");
        }
        if (static_cast<int>(unit.kind) <
                static_cast<int>(UnitKind::villager) ||
            static_cast<int>(unit.kind) >
                static_cast<int>(UnitKind::king)) {
            throw std::runtime_error("invalid unit kind in save");
        }
        const bool trader =
            unit.kind == UnitKind::trade_cart ||
            unit.kind == UnitKind::trade_cog;
        const bool has_trade_home = unit.trade_home_market_id != 0;
        const bool has_trade_target = unit.trade_target_market_id != 0;
        if ((!trader &&
             (has_trade_home || has_trade_target ||
              unit.trade_returning || unit.trade_waiting ||
              unit.trade_work_ticks_remaining != 0)) ||
            has_trade_home != has_trade_target ||
            (!has_trade_home &&
             (unit.trade_returning || unit.trade_waiting ||
              unit.trade_work_ticks_remaining != 0)) ||
            unit.trade_work_ticks_remaining < 0 ||
            unit.trade_work_ticks_remaining > 3 ||
            (!unit.trade_waiting &&
             unit.trade_work_ticks_remaining != 0)) {
            throw std::runtime_error("invalid trade route state in save");
        }
        if (unit.food_decay_remainder < 0 ||
            unit.food_decay_remainder >= 500 ||
            (!is_animal(unit.kind) &&
             (unit.food_remaining != 0 ||
              unit.food_decay_remainder != 0))) {
            throw std::runtime_error("invalid animal food state in save");
        }
        const int maximum_animal_food =
            unit.kind == UnitKind::sheep ? 100 :
            unit.kind == UnitKind::deer ? 140 :
            unit.kind == UnitKind::boar ? 340 : 0;
        if (unit.food_remaining < 0 ||
            unit.food_remaining > maximum_animal_food) {
            throw std::runtime_error("invalid animal food in save");
        }
        if (unit.garrisoned_in != 0) {
            const auto shelter = std::ranges::find(
                buildings_, unit.garrisoned_in, &Building::id
            );
            if (shelter != buildings_.end()) {
                if (shelter->hit_points <= 0 ||
                    !can_garrison(unit, *shelter) ||
                    unit.position != shelter->position) {
                    throw std::runtime_error("invalid garrison in save");
                }
            } else {
                const auto transport = std::ranges::find(
                    units_, unit.garrisoned_in, &Unit::id
                );
                if (transport == units_.end() ||
                    transport->kind != UnitKind::transport_ship ||
                    transport->owner != unit.owner ||
                    transport->hit_points <= 0 || is_ship(unit.kind) ||
                    unit.position != transport->position) {
                    throw std::runtime_error(
                        "invalid transport garrison in save"
                    );
                }
            }
            continue;
        }
        const bool valid_terrain = is_ship(unit.kind)
            ? map_.sailable(unit.position) : map_.walkable(unit.position);
        if (!valid_terrain ||
            std::ranges::find(occupied_units, unit.position) !=
                occupied_units.end()) {
            throw std::runtime_error("invalid unit placement in save");
        }
        for (const Building& building : buildings_) {
            const BuildingRules& rules = rules_for(building.kind);
            if (unit.position.x >= building.position.x &&
                unit.position.x <
                    building.position.x + rules.footprint_width &&
                unit.position.y >= building.position.y &&
                unit.position.y <
                    building.position.y + rules.footprint_height) {
                throw std::runtime_error(
                    "unit overlaps building footprint in save"
                );
            }
        }
        occupied_units.push_back(unit.position);
    }
    for (const Building& building : buildings_) {
        if (garrison_count(building.id) >
            garrison_capacity(building.kind)) {
            throw std::runtime_error("garrison overflow in save");
        }
    }
    for (const Unit& transport : units_) {
        if (transport.kind != UnitKind::transport_ship) {
            continue;
        }
        const int passengers = static_cast<int>(std::ranges::count_if(
            units_, [&transport](const Unit& unit) {
                return unit.garrisoned_in == transport.id;
            }
        ));
        if (passengers > transport_capacity(transport.owner)) {
            throw std::runtime_error("transport overflow in save");
        }
    }
}

bool Simulation::command_patrol(
    EntityId unit_id,
    TilePosition destination
) {
    Unit* unit = find_unit(unit_id);
    if (unit == nullptr) {
        return false;
    }
    const TilePosition origin = unit->position;
    if (origin == destination ||
        !command_attack_move(unit_id, destination)) {
        return false;
    }
    unit = find_unit(unit_id);
    unit->patrol_origin = origin;
    unit->patrol_destination = destination;
    unit->patrolling = true;
    return true;
}

bool Simulation::command_guard(
    EntityId unit_id,
    EntityId target_id,
    bool target_is_building
) {
    if (unit_id == target_id || outcome_ != MatchOutcome::ongoing) {
        return false;
    }
    Unit* unit = find_unit(unit_id);
    if (unit == nullptr || unit->garrisoned_in != 0 ||
        (is_animal(unit->kind) || is_relic(unit->kind))) {
        return false;
    }
    Unit* guarded_unit =
        target_is_building ? nullptr : find_unit(target_id);
    Building* guarded_building =
        target_is_building ? find_building(target_id) : nullptr;
    const bool valid_unit =
        guarded_unit != nullptr &&
        guarded_unit->garrisoned_in == 0 &&
        guarded_unit->owner == unit->owner;
    const bool valid_building =
        guarded_building != nullptr &&
        guarded_building->owner == unit->owner &&
        guarded_building->completed();
    if (!valid_unit && !valid_building) {
        return false;
    }
    if (!stop_unit(unit_id)) {
        return false;
    }
    unit = find_unit(unit_id);
    const int distance = valid_unit
        ? std::abs(guarded_unit->position.x - unit->position.x) +
            std::abs(guarded_unit->position.y - unit->position.y)
        : distance_to_building(unit->position, *guarded_building);
    if (distance > 2) {
        route_unit(
            *unit,
            valid_unit
                ? guarded_unit->position
                : guarded_building->position
        );
    }
    unit->guard_target_id = target_id;
    unit->guard_target_is_building = target_is_building;
    return true;
}

bool Simulation::queue_waypoint(
    EntityId unit_id,
    TilePosition destination
) {
    if (outcome_ != MatchOutcome::ongoing ||
        !map_.contains(destination) ||
        !map_.walkable(destination)) {
        return false;
    }
    Unit* unit = find_unit(unit_id);
    if (unit == nullptr || unit->garrisoned_in != 0 ||
        (is_animal(unit->kind) || is_relic(unit->kind)) ||
        unit->waypoints.size() >= 20) {
        return false;
    }
    const bool has_active_order =
        unit->moving ||
        unit->attack_target_id != 0 ||
        unit->repair_target_id != 0 ||
        unit->has_resource_target ||
        unit->garrison_target_id != 0 ||
        unit->attack_moving ||
        unit->guard_target_id != 0;
    if (!has_active_order) {
        return command_unit(unit_id, destination);
    }
    unit->waypoints.push_back(destination);
    return true;
}

bool Simulation::set_unit_stance(
    EntityId unit_id,
    UnitStance stance
) {
    if (outcome_ != MatchOutcome::ongoing) {
        return false;
    }
    Unit* unit = find_unit(unit_id);
    if (unit == nullptr || unit->garrisoned_in != 0 ||
        is_huntable(unit->kind)) {
        return false;
    }
    unit->stance = stance;
    unit->stance_anchor = unit->position;
    unit->returning_to_stance = false;
    if ((stance == UnitStance::stand_ground ||
         stance == UnitStance::passive) &&
        unit->attack_target_auto) {
        unit->attack_target_id = 0;
        unit->attack_target_is_building = false;
        unit->attack_target_auto = false;
        unit->previous_position = unit->position;
        unit->destination = unit->position;
        unit->moving = false;
        unit->path.clear();
        unit->next_path_step = 0;
    }
    return true;
}

bool Simulation::delete_unit(EntityId unit_id) {
    if (outcome_ != MatchOutcome::ongoing) {
        return false;
    }
    const auto unit = std::ranges::find_if(
        units_,
        [unit_id](const Unit& candidate) {
            return candidate.id == unit_id;
        }
    );
    if (unit == units_.end()) {
        return false;
    }
    if (unit->kind == UnitKind::transport_ship) {
        std::erase_if(units_, [unit_id](const Unit& passenger) {
            return passenger.garrisoned_in == unit_id;
        });
    }
    detach_builder(unit_id);
    units_.erase(unit);
    prune_unit_render_elevations();
    std::erase(selected_units_, unit_id);
    if (selected_unit_ == unit_id) {
        selected_unit_.reset();
    }
    if (!selected_units_.empty()) {
        selected_unit_ = selected_units_.front();
    }
    update_match_outcome();
    return true;
}

bool Simulation::command_pack_trebuchet(EntityId unit_id, bool pack) {
    if (outcome_ != MatchOutcome::ongoing) return false;
    Unit* unit = find_unit(unit_id);
    if (unit == nullptr ||
        (pack && unit->kind != UnitKind::trebuchet) ||
        (!pack && unit->kind != UnitKind::packed_trebuchet)) {
        return false;
    }
    stop_unit(unit_id);
    unit = find_unit(unit_id);
    unit->trebuchet_transform_ticks_remaining = 2;
    unit->trebuchet_transform_to_packed = pack;
    return true;
}

bool Simulation::delete_building(EntityId building_id) {
    if (outcome_ != MatchOutcome::ongoing) {
        return false;
    }
    const auto building = std::ranges::find_if(
        buildings_,
        [building_id](const Building& candidate) {
            return candidate.id == building_id;
        }
    );
    if (building == buildings_.end()) {
        return false;
    }
    if (garrison_capacity(building->kind) > 0) {
        ungarrison_at(building_id);
    }
    if (!building->completed()) {
        const BuildingRules& rules = rules_for(building->kind);
        Economy& economy = building->owner == Player::blue
            ? player_states_[0].economy
            : player_states_[1].economy;
        economy.wood +=
            effective_building_wood_cost(
                building->owner, building->kind
            ) *
            building->construction_ticks_remaining /
            rules.construction_ticks;
        economy.stone +=
            effective_building_stone_cost(
                building->owner, building->kind
            ) *
            building->construction_ticks_remaining /
            rules.construction_ticks;
        economy.gold +=
            rules.gold_cost *
            building->construction_ticks_remaining /
            rules.construction_ticks;
    }
    buildings_.erase(building);
    if (selected_building_ == building_id) {
        selected_building_.reset();
    }
    update_match_outcome();
    return true;
}

bool Simulation::stop_unit(EntityId unit_id) {
    if (outcome_ != MatchOutcome::ongoing) {
        return false;
    }
    Unit* unit = find_unit(unit_id);
    if (unit == nullptr || unit->garrisoned_in != 0) {
        return false;
    }
    if (gather_trace_enabled() && unit->kind == UnitKind::villager) {
        std::cerr << "GATHER_STOP tick=" << tick_number_
                  << " id=" << unit->id
                  << " active_before=" << unit->has_resource_target
                  << " returning_before=" << unit->returning_resource
                  << " carried_before=" << unit->carried_amount << '\n';
    }
    const bool leaving_formation_movement =
        unit->formation_move_interval > 0;
    unit->previous_position = unit->position;
    unit->destination = unit->position;
    unit->moving = false;
    unit->path.clear();
    unit->next_path_step = 0;
    unit->movement_cooldown = 0;
    unit->attack_target_id = 0;
    unit->attack_target_is_building = false;
    unit->repair_target_id = 0;
    unit->repair_wood_remainder = 0;
    unit->repair_stone_remainder = 0;
    unit->has_resource_target = false;
    unit->resource_target = {-1, -1};
    unit->resource_building_id = 0;
    unit->resource_unit_id = 0;
    unit->returning_resource = false;
    unit->garrison_target_id = 0;
    unit->attack_move_destination = {-1, -1};
    unit->attack_moving = false;
    unit->patrol_origin = {-1, -1};
    unit->patrol_destination = {-1, -1};
    unit->patrolling = false;
    unit->attack_ground_target = {-1, -1};
    unit->attacking_ground = false;
    unit->guard_target_id = 0;
    unit->guard_target_is_building = false;
    unit->waypoints.clear();
    unit->formation_waypoints.clear();
    unit->attack_target_auto = false;
    unit->stance_anchor = unit->position;
    unit->returning_to_stance = false;
    unit->formation_move_interval = 0;
    unit->formation_speed_numerator = 0;
    unit->formation_group_id = 0;
    unit->formation_anchor = {-1, -1};
    unit->formation_slot = {-1, -1};
    if (leaving_formation_movement) {
        unit->movement_speed_remainder = 0;
    }
    detach_builder(unit_id);
    return true;
}

bool Simulation::command_selected(TilePosition destination) {
    if (!selected_unit_) {
        return false;
    }
    if (selected_units_.size() > 1) {
        const Unit* first = find_unit(selected_units_.front());
        return first != nullptr && command_formation(
            selected_units_, destination, formation_kind(first->owner)
        );
    }
    const bool accepted = command_unit(*selected_unit_, destination);
    if (!accepted && find_unit(*selected_unit_) == nullptr) {
        selected_unit_.reset();
    }
    return accepted;
}

bool Simulation::construct_building_at(
    EntityId builder_id,
    BuildingKind kind,
    TilePosition position
) {
    if (outcome_ != MatchOutcome::ongoing ||
        !footprint_available(kind, position, 0)) {
        return false;
    }
    if (kind == BuildingKind::dock) {
        const BuildingRules& dock_rules = rules_for(kind);
        bool beside_water = false;
        for (int y = position.y - 1;
             y <= position.y + dock_rules.footprint_height; ++y) {
            for (int x = position.x - 1;
                 x <= position.x + dock_rules.footprint_width; ++x) {
                const TilePosition candidate{x, y};
                if (map_.sailable(candidate)) {
                    beside_water = true;
                }
            }
        }
        if (!beside_water) {
            return false;
        }
    }
    Unit* builder = find_unit(builder_id);
    const bool fishing_ship_builder =
        kind == BuildingKind::fish_trap &&
        builder != nullptr &&
        builder->kind == UnitKind::fishing_ship;
    if (builder == nullptr ||
        (builder->kind != UnitKind::villager &&
         !fishing_ship_builder)) {
        return false;
    }
    if (!civilization_has_building(
            civilization(builder->owner), kind
        )) {
        return false;
    }
    const bool replacing_missing_town_center =
        kind == BuildingKind::town_center &&
        std::ranges::none_of(
            buildings_,
            [builder](const Building& building) {
                return building.owner == builder->owner &&
                    building.kind == BuildingKind::town_center &&
                    building.hit_points > 0;
            }
        );
    if (static_cast<int>(age(builder->owner)) <
            static_cast<int>(rules_for(kind).minimum_age) &&
        !replacing_missing_town_center) {
        return false;
    }
    if (kind == BuildingKind::siege_workshop &&
        !std::ranges::any_of(
            buildings_,
            [builder](const Building& building) {
                return building.owner == builder->owner &&
                    building.kind == BuildingKind::blacksmith &&
                    building.completed();
            }
        )) {
        return false;
    }
    if (kind == BuildingKind::bombard_tower &&
        !has_technology(builder->owner, Technology::bombard_tower)) {
        return false;
    }
    if (kind == BuildingKind::fish_trap &&
        !has_technology(builder->owner, Technology::fish_trap_gate)) {
        return false;
    }
    if (kind == BuildingKind::wonder &&
        !has_technology(builder->owner, Technology::wonder_plans)) {
        return false;
    }
    Economy& owner_economy =
        builder->owner == Player::blue ? player_states_[0].economy : player_states_[1].economy;
    const BuildingRules& building_rules = rules_for(kind);
    const int wood_cost =
        effective_building_wood_cost(builder->owner, kind);
    const int stone_cost =
        effective_building_stone_cost(builder->owner, kind);
    const int gold_cost = building_rules.gold_cost;
    if (owner_economy.wood < wood_cost ||
        owner_economy.stone < stone_cost ||
        owner_economy.gold < gold_cost) {
        return false;
    }
    // Building placement is an order, not a short-range action. Prove that
    // the builder can reach the future foundation before charging resources.
    if (!route_unit(*builder, position)) {
        return false;
    }
    owner_economy.wood -= wood_cost;
    owner_economy.stone -= stone_cost;
    owner_economy.gold -= gold_cost;
    detach_builder(builder->id);
    const EntityId building_id =
        add_building(kind, builder->owner, position);
    Building* building = find_building(building_id);
    building->construction_ticks_remaining =
        rules_for(kind).construction_ticks;
    building->builder_id = builder->id;
    building->builder_ids = {builder->id};
    building->construction_work_remainder = 0;
    building->hit_points = 1;
    builder->has_resource_target = false;
    builder->returning_resource = false;
    builder->attack_target_id = 0;
    builder->attack_target_is_building = false;
    builder->repair_target_id = 0;
    return true;
}

bool Simulation::construct_building(
    BuildingKind kind,
    TilePosition position
) {
    return selected_unit_ &&
           construct_building_at(*selected_unit_, kind, position);
}

bool Simulation::queue_unit_at(EntityId building_id, UnitKind kind) {
    constexpr std::size_t production_queue_limit = 15;
    if (outcome_ != MatchOutcome::ongoing) {
        return false;
    }
    Building* building = find_building(building_id);
    if (building == nullptr || !building->completed() ||
        building->production_queue.size() >= production_queue_limit) {
        return false;
    }
    if (!civilization_has_unit(civilization(building->owner), kind)) {
        return false;
    }
    if ((kind == UnitKind::elite_eagle_warrior &&
         !has_technology(
             building->owner, Technology::elite_eagle_warrior
         )) ||
        (kind == UnitKind::heavy_scorpion &&
         !has_technology(building->owner, Technology::heavy_scorpion)) ||
        (kind == UnitKind::onager &&
         !has_technology(building->owner, Technology::onager)) ||
        (kind == UnitKind::siege_onager &&
         !has_technology(building->owner, Technology::siege_onager)) ||
        (kind == UnitKind::heavy_cavalry_archer &&
         !has_technology(
             building->owner, Technology::heavy_cavalry_archer
         )) ||
        (kind == UnitKind::heavy_camel &&
         !has_technology(building->owner, Technology::heavy_camel)) ||
        (kind == UnitKind::capped_ram &&
         !has_technology(building->owner, Technology::capped_ram)) ||
        (kind == UnitKind::siege_ram &&
         !has_technology(building->owner, Technology::siege_ram)) ||
        kind == UnitKind::packed_trebuchet) {
        return false;
    }
    if (kind == UnitKind::trade_cog &&
        !has_technology(building->owner, Technology::trade_cog_gate)) {
        return false;
    }
    UnitKind effective_kind = kind;
    if (kind == UnitKind::eagle_warrior &&
        has_technology(building->owner, Technology::elite_eagle_warrior)) {
        effective_kind = UnitKind::elite_eagle_warrior;
    }
    if (kind == UnitKind::scorpion &&
        has_technology(building->owner, Technology::heavy_scorpion)) {
        effective_kind = UnitKind::heavy_scorpion;
    }
    if (kind == UnitKind::mangonel) {
        if (has_technology(building->owner, Technology::siege_onager)) {
            effective_kind = UnitKind::siege_onager;
        } else if (has_technology(building->owner, Technology::onager)) {
            effective_kind = UnitKind::onager;
        }
    }
    if (kind == UnitKind::trebuchet) {
        effective_kind = UnitKind::packed_trebuchet;
    }
    if (kind == UnitKind::cavalry_archer &&
        has_technology(
            building->owner, Technology::heavy_cavalry_archer
        )) {
        effective_kind = UnitKind::heavy_cavalry_archer;
    }
    if (kind == UnitKind::camel_rider &&
        has_technology(building->owner, Technology::heavy_camel)) {
        effective_kind = UnitKind::heavy_camel;
    }
    if (kind == UnitKind::battering_ram) {
        if (has_technology(building->owner, Technology::siege_ram)) {
            effective_kind = UnitKind::siege_ram;
        } else if (has_technology(building->owner, Technology::capped_ram)) {
            effective_kind = UnitKind::capped_ram;
        }
    }
    if ((kind == UnitKind::hand_cannoneer ||
         kind == UnitKind::bombard_cannon) &&
        !has_technology(building->owner, Technology::chemistry)) {
        return false;
    }
    if (kind == UnitKind::galley &&
        has_technology(building->owner, Technology::war_galley)) {
        effective_kind =
            has_technology(building->owner, Technology::galleon)
                ? UnitKind::galleon : UnitKind::war_galley;
    }
    if (kind == UnitKind::fire_ship &&
        has_technology(building->owner, Technology::fast_fire_ship)) {
        effective_kind = UnitKind::fast_fire_ship;
    }
    if (kind == UnitKind::demolition_ship &&
        has_technology(
            building->owner, Technology::heavy_demolition_ship
        )) {
        effective_kind = UnitKind::heavy_demolition_ship;
    }
    if (kind == UnitKind::cannon_galleon) {
        if (!has_technology(building->owner, Technology::cannon_galleon)) {
            return false;
        }
        if (has_technology(
                building->owner, Technology::elite_cannon_galleon
            )) {
            effective_kind = UnitKind::elite_cannon_galleon;
        }
    }
    if (kind == UnitKind::longboat) {
        if (civilization(building->owner) != Civilization::vikings) {
            return false;
        }
        if (has_technology(building->owner, Technology::elite_longboat)) {
            effective_kind = UnitKind::elite_longboat;
        }
    }
    if (kind == UnitKind::turtle_ship) {
        if (civilization(building->owner) != Civilization::koreans) {
            return false;
        }
        if (has_technology(
                building->owner, Technology::elite_turtle_ship
            )) {
            effective_kind = UnitKind::elite_turtle_ship;
        }
    }
    if ((kind == UnitKind::elite_longboat &&
         (civilization(building->owner) != Civilization::vikings ||
          !has_technology(
              building->owner, Technology::elite_longboat
          ))) ||
        (kind == UnitKind::elite_turtle_ship &&
         (civilization(building->owner) != Civilization::koreans ||
          !has_technology(
              building->owner, Technology::elite_turtle_ship
          )))) {
        return false;
    }
    const auto unique_line = [&](UnitKind base, UnitKind elite,
                                 Civilization required,
                                 Technology upgrade) {
        if (kind != base) return true;
        if (civilization(building->owner) != required) return false;
        if (has_technology(building->owner, upgrade)) {
            effective_kind = elite;
        }
        return true;
    };
    if (!unique_line(
            UnitKind::longbowman, UnitKind::elite_longbowman,
            Civilization::britons, Technology::elite_longbowman
        ) ||
        !unique_line(
            UnitKind::throwing_axeman,
            UnitKind::elite_throwing_axeman,
            Civilization::franks, Technology::elite_throwing_axeman
        ) ||
        !unique_line(
            UnitKind::huskarl, UnitKind::elite_huskarl,
            Civilization::goths, Technology::elite_huskarl
        ) ||
        !unique_line(
            UnitKind::teutonic_knight,
            UnitKind::elite_teutonic_knight,
            Civilization::teutons, Technology::elite_teutonic_knight
        ) ||
        !unique_line(
            UnitKind::samurai, UnitKind::elite_samurai,
            Civilization::japanese, Technology::elite_samurai
        ) ||
        !unique_line(
            UnitKind::chu_ko_nu, UnitKind::elite_chu_ko_nu,
            Civilization::chinese, Technology::elite_chu_ko_nu
        ) ||
        !unique_line(
            UnitKind::cataphract, UnitKind::elite_cataphract,
            Civilization::byzantines, Technology::elite_cataphract
        ) ||
        !unique_line(
            UnitKind::war_elephant, UnitKind::elite_war_elephant,
            Civilization::persians, Technology::elite_war_elephant
        ) ||
        !unique_line(
            UnitKind::mameluke, UnitKind::elite_mameluke,
            Civilization::saracens, Technology::elite_mameluke
        ) ||
        !unique_line(
            UnitKind::janissary, UnitKind::elite_janissary,
            Civilization::turks, Technology::elite_janissary
        ) ||
        !unique_line(
            UnitKind::berserk, UnitKind::elite_berserk,
            Civilization::vikings, Technology::elite_berserk
        ) ||
        !unique_line(
            UnitKind::mangudai, UnitKind::elite_mangudai,
            Civilization::mongols, Technology::elite_mangudai
        ) ||
        !unique_line(
            UnitKind::jaguar_warrior, UnitKind::elite_jaguar_warrior,
            Civilization::aztecs, Technology::elite_jaguar_warrior
        ) ||
        !unique_line(
            UnitKind::plumed_archer, UnitKind::elite_plumed_archer,
            Civilization::mayans, Technology::elite_plumed_archer
        ) ||
        !unique_line(
            UnitKind::conquistador, UnitKind::elite_conquistador,
            Civilization::spanish, Technology::elite_conquistador
        ) ||
        !unique_line(
            UnitKind::tarkan, UnitKind::elite_tarkan,
            Civilization::huns, Technology::elite_tarkan
        ) ||
        !unique_line(
            UnitKind::woad_raider, UnitKind::elite_woad_raider,
            Civilization::celts, Technology::elite_woad_raider
        )) {
        return false;
    }
    const bool direct_locked_elite =
        (kind == UnitKind::elite_longbowman &&
         (civilization(building->owner) != Civilization::britons ||
          !has_technology(
              building->owner, Technology::elite_longbowman
          ))) ||
        (kind == UnitKind::elite_throwing_axeman &&
         (civilization(building->owner) != Civilization::franks ||
          !has_technology(
              building->owner, Technology::elite_throwing_axeman
          ))) ||
        (kind == UnitKind::elite_huskarl &&
         (civilization(building->owner) != Civilization::goths ||
          !has_technology(
              building->owner, Technology::elite_huskarl
          ))) ||
        (kind == UnitKind::elite_teutonic_knight &&
         (civilization(building->owner) != Civilization::teutons ||
          !has_technology(
              building->owner, Technology::elite_teutonic_knight
          ))) ||
        (kind == UnitKind::elite_samurai &&
         (civilization(building->owner) != Civilization::japanese ||
          !has_technology(
              building->owner, Technology::elite_samurai
          ))) ||
        (kind == UnitKind::elite_chu_ko_nu &&
         (civilization(building->owner) != Civilization::chinese ||
          !has_technology(
              building->owner, Technology::elite_chu_ko_nu
          ))) ||
        (kind == UnitKind::elite_cataphract &&
         (civilization(building->owner) != Civilization::byzantines ||
          !has_technology(
              building->owner, Technology::elite_cataphract
          ))) ||
        (kind == UnitKind::elite_war_elephant &&
         (civilization(building->owner) != Civilization::persians ||
          !has_technology(
              building->owner, Technology::elite_war_elephant
          ))) ||
        (kind == UnitKind::elite_mameluke &&
         (civilization(building->owner) != Civilization::saracens ||
          !has_technology(building->owner, Technology::elite_mameluke))) ||
        (kind == UnitKind::elite_janissary &&
         (civilization(building->owner) != Civilization::turks ||
          !has_technology(building->owner, Technology::elite_janissary))) ||
        (kind == UnitKind::elite_berserk &&
         (civilization(building->owner) != Civilization::vikings ||
          !has_technology(building->owner, Technology::elite_berserk))) ||
        (kind == UnitKind::elite_mangudai &&
         (civilization(building->owner) != Civilization::mongols ||
          !has_technology(building->owner, Technology::elite_mangudai))) ||
        (kind == UnitKind::elite_jaguar_warrior &&
         (civilization(building->owner) != Civilization::aztecs ||
          !has_technology(
              building->owner, Technology::elite_jaguar_warrior
          ))) ||
        (kind == UnitKind::elite_plumed_archer &&
         (civilization(building->owner) != Civilization::mayans ||
          !has_technology(
              building->owner, Technology::elite_plumed_archer
          ))) ||
        (kind == UnitKind::elite_conquistador &&
         (civilization(building->owner) != Civilization::spanish ||
          !has_technology(
              building->owner, Technology::elite_conquistador
          ))) ||
        (kind == UnitKind::elite_tarkan &&
         (civilization(building->owner) != Civilization::huns ||
          !has_technology(building->owner, Technology::elite_tarkan))) ||
        (kind == UnitKind::elite_woad_raider &&
         (civilization(building->owner) != Civilization::celts ||
          !has_technology(
              building->owner, Technology::elite_woad_raider
          )));
    if (direct_locked_elite) return false;
    if (kind == UnitKind::militia &&
        has_technology(building->owner, Technology::man_at_arms)) {
        effective_kind =
            has_technology(building->owner, Technology::champion)
                ? UnitKind::champion
                : (has_technology(
                building->owner, Technology::two_handed_swordsman
            )
                ? UnitKind::two_handed_swordsman
                : (has_technology(
                       building->owner, Technology::long_swordsman
                   )
                    ? UnitKind::long_swordsman
                    : UnitKind::man_at_arms));
    }
    if (kind == UnitKind::archer &&
        has_technology(building->owner, Technology::crossbowman)) {
        effective_kind =
            has_technology(building->owner, Technology::arbalester)
                ? UnitKind::arbalester
                : UnitKind::crossbowman;
    }
    if (kind == UnitKind::spearman &&
        has_technology(building->owner, Technology::pikeman)) {
        effective_kind =
            has_technology(building->owner, Technology::halberdier)
                ? UnitKind::halberdier : UnitKind::pikeman;
    }
    if (kind == UnitKind::skirmisher &&
        has_technology(
            building->owner, Technology::elite_skirmisher
        )) {
        effective_kind = UnitKind::elite_skirmisher;
    }
    if (kind == UnitKind::knight &&
        has_technology(building->owner, Technology::cavalier)) {
        effective_kind =
            has_technology(building->owner, Technology::paladin)
                ? UnitKind::paladin
                : UnitKind::cavalier;
    }
    if (kind == UnitKind::scout_cavalry &&
        has_technology(building->owner, Technology::light_cavalry)) {
        effective_kind =
            has_technology(building->owner, Technology::hussar)
                ? UnitKind::hussar
                : UnitKind::light_cavalry;
    }
    if (effective_kind == UnitKind::man_at_arms &&
        !has_technology(building->owner, Technology::man_at_arms)) {
        return false;
    }
    if (effective_kind == UnitKind::crossbowman &&
        !has_technology(building->owner, Technology::crossbowman)) {
        return false;
    }
    if (effective_kind == UnitKind::arbalester &&
        !has_technology(building->owner, Technology::arbalester)) {
        return false;
    }
    if (effective_kind == UnitKind::elite_skirmisher &&
        !has_technology(
            building->owner, Technology::elite_skirmisher
        )) {
        return false;
    }
    if (effective_kind == UnitKind::pikeman &&
        !has_technology(building->owner, Technology::pikeman)) {
        return false;
    }
    if (effective_kind == UnitKind::halberdier &&
        !has_technology(building->owner, Technology::halberdier)) {
        return false;
    }
    if (effective_kind == UnitKind::long_swordsman &&
        !has_technology(building->owner, Technology::long_swordsman)) {
        return false;
    }
    if (effective_kind == UnitKind::two_handed_swordsman &&
        !has_technology(
            building->owner, Technology::two_handed_swordsman
        )) {
        return false;
    }
    if (effective_kind == UnitKind::champion &&
        !has_technology(building->owner, Technology::champion)) {
        return false;
    }
    if (effective_kind == UnitKind::cavalier &&
        !has_technology(building->owner, Technology::cavalier)) {
        return false;
    }
    if (effective_kind == UnitKind::paladin &&
        !has_technology(building->owner, Technology::paladin)) {
        return false;
    }
    if (effective_kind == UnitKind::light_cavalry &&
        !has_technology(building->owner, Technology::light_cavalry)) {
        return false;
    }
    if (effective_kind == UnitKind::hussar &&
        !has_technology(building->owner, Technology::hussar)) {
        return false;
    }
    const bool anarchy_huskarl =
        building->kind == BuildingKind::barracks &&
        civilization(building->owner) == Civilization::goths &&
        has_technology(building->owner, Technology::anarchy) &&
        (effective_kind == UnitKind::huskarl ||
         effective_kind == UnitKind::elite_huskarl);
    if (!can_train(building->kind, effective_kind) &&
        !anarchy_huskarl) {
        return false;
    }
    if (building->age_research_ticks_remaining > 0 ||
        building->technology_research_ticks_remaining > 0 ||
        static_cast<int>(age(building->owner)) <
            static_cast<int>(rules_for(effective_kind).minimum_age)) {
        return false;
    }
    if (committed_population(building->owner) >=
        population_capacity(building->owner)) {
        return false;
    }

    Economy& owner_economy =
        building->owner == Player::blue ? player_states_[0].economy : player_states_[1].economy;
    const UnitRules& unit_rules = rules_for(effective_kind);
    const bool infantry =
        effective_kind == UnitKind::militia ||
        effective_kind == UnitKind::man_at_arms ||
        effective_kind == UnitKind::long_swordsman ||
        effective_kind == UnitKind::two_handed_swordsman ||
        effective_kind == UnitKind::champion ||
        effective_kind == UnitKind::spearman ||
        effective_kind == UnitKind::pikeman ||
        effective_kind == UnitKind::halberdier ||
        effective_kind == UnitKind::throwing_axeman ||
        effective_kind == UnitKind::elite_throwing_axeman ||
        effective_kind == UnitKind::huskarl ||
        effective_kind == UnitKind::elite_huskarl ||
        effective_kind == UnitKind::teutonic_knight ||
        effective_kind == UnitKind::elite_teutonic_knight ||
        effective_kind == UnitKind::samurai ||
        effective_kind == UnitKind::elite_samurai ||
        effective_kind == UnitKind::berserk ||
        effective_kind == UnitKind::elite_berserk ||
        effective_kind == UnitKind::jaguar_warrior ||
        effective_kind == UnitKind::elite_jaguar_warrior;
    int discount{};
    if (civilization(building->owner) == Civilization::goths && infantry) {
        discount =
            age(building->owner) >= Age::imperial ? 35 :
            age(building->owner) >= Age::castle ? 30 :
            age(building->owner) >= Age::feudal ? 25 : 20;
    }
    if (civilization(building->owner) == Civilization::byzantines &&
        (effective_kind == UnitKind::spearman ||
         effective_kind == UnitKind::pikeman ||
         effective_kind == UnitKind::halberdier ||
         effective_kind == UnitKind::skirmisher ||
         effective_kind == UnitKind::elite_skirmisher)) {
        discount = 25;
    }
    if (civilization(building->owner) == Civilization::mayans &&
        is_archer(effective_kind) &&
        effective_kind != UnitKind::plumed_archer &&
        effective_kind != UnitKind::elite_plumed_archer) {
        discount =
            age(building->owner) >= Age::imperial ? 30 :
            age(building->owner) >= Age::castle ? 20 :
            age(building->owner) >= Age::feudal ? 10 : 0;
    }
    const int shipwright_percent =
        is_ship(effective_kind) &&
        has_technology(building->owner, Technology::shipwright)
        ? 80 : 100;
    const int wood_cost =
        unit_rules.wood_cost * (100 - discount) / 100 *
        shipwright_percent / 100;
    const int food_cost =
        unit_rules.food_cost * (100 - discount) / 100 *
        shipwright_percent / 100;
    const int gold_cost =
        unit_rules.gold_cost * (100 - discount) / 100 *
        shipwright_percent / 100;
    if (owner_economy.wood < wood_cost ||
        owner_economy.food < food_cost ||
        owner_economy.gold < gold_cost) {
        return false;
    }

    owner_economy.wood -= wood_cost;
    owner_economy.food -= food_cost;
    owner_economy.gold -= gold_cost;
    int training_ticks = unit_rules.training_ticks;
    if (is_ship(effective_kind) &&
        has_technology(building->owner, Technology::shipwright)) {
        training_ticks = std::max(1, training_ticks * 65 / 100);
    }
    if (civilization(building->owner) == Civilization::huns &&
        building->kind == BuildingKind::stable) {
        training_ticks = std::max(1, training_ticks * 5 / 6);
    }
    const bool aztec_military =
        effective_kind == UnitKind::monk ||
        rules_for(effective_kind).attack > 0;
    if (civilization(building->owner) == Civilization::aztecs &&
        aztec_military) {
        training_ticks = std::max(1, training_ticks * 100 / 111);
    }
    building->production_queue.push_back({
        effective_kind,
        training_ticks,
        wood_cost,
        food_cost,
        gold_cost,
        0,
        0,
        std::nullopt,
    });
    return true;
}

bool Simulation::queue_commercial_object_at(
    EntityId building_id,
    CommercialObjectIdentity identity
) {
    constexpr std::size_t production_queue_limit = 15;
    Building* building = find_building(building_id);
    const CommercialObjectRecord* object =
        commercial_content_catalog().object(
            identity.civilization_id, identity.object_id
        );
    if (outcome_ != MatchOutcome::ongoing || building == nullptr ||
        object == nullptr || !building->completed() ||
        building->production_queue.size() >= production_queue_limit ||
        object->base_class == CommercialObjectBaseClass::building ||
        !commercial_object_enabled(building->owner, identity) ||
        object->creation_location_object_id == std::nullopt) {
        return false;
    }
    if (!building->commercial_identity ||
        building->commercial_identity->civilization_id !=
            identity.civilization_id ||
        building->commercial_identity->object_id !=
            *object->creation_location_object_id) {
        return false;
    }
    Economy& economy = player_states_.at(building->owner.stable_id()).economy;
    int wood{};
    int food{};
    int gold{};
    int stone{};
    for (const CommercialResourceCost& cost : object->costs) {
        if (!cost.paid) continue;
        if (cost.resource_id == 0) food += cost.amount;
        else if (cost.resource_id == 1) wood += cost.amount;
        else if (cost.resource_id == 2) stone += cost.amount;
        else if (cost.resource_id == 3) gold += cost.amount;
    }
    const auto effective_cost = [&](int base, std::int16_t attribute) {
        const float all = effective_commercial_attribute(
            building->owner, identity, 100, static_cast<float>(base)
        );
        return std::max(0, static_cast<int>(std::lround(
            effective_commercial_attribute(
                building->owner, identity, attribute, all
            )
        )));
    };
    food = effective_cost(food, 103);
    wood = effective_cost(wood, 104);
    stone = effective_cost(stone, 105);
    gold = effective_cost(gold, 106);
    if (economy.wood < wood || economy.food < food ||
        economy.gold < gold || economy.stone < stone) {
        return false;
    }
    economy.wood -= wood;
    economy.food -= food;
    economy.gold -= gold;
    economy.stone -= stone;
    ProductionOrder order;
    order.kind = UnitKind::villager;
    order.ticks_remaining = std::max(1, static_cast<int>(std::lround(
        effective_commercial_attribute(
            building->owner, identity, 101,
            static_cast<float>(object->creation_time)
        )
    )));
    order.paid_wood = wood;
    order.paid_food = food;
    order.paid_gold = gold;
    order.paid_stone = stone;
    order.commercial_identity = identity;
    building->production_queue.push_back(order);
    return true;
}

bool Simulation::queue_unit(UnitKind kind) {
    return selected_building_ &&
           queue_unit_at(*selected_building_, kind);
}

bool Simulation::cancel_production_at(EntityId building_id) {
    if (outcome_ != MatchOutcome::ongoing) {
        return false;
    }
    Building* building = find_building(building_id);
    if (building == nullptr || building->production_queue.empty()) {
        return false;
    }
    const ProductionOrder& order = building->production_queue.back();
    Economy& economy = building->owner == Player::blue
        ? player_states_[0].economy
        : player_states_[1].economy;
    economy.wood += order.paid_wood;
    economy.food += order.paid_food;
    economy.gold += order.paid_gold;
    economy.stone += order.paid_stone;
    building->production_queue.pop_back();
    return true;
}

bool Simulation::set_rally_point(
    EntityId building_id,
    TilePosition position
) {
    if (outcome_ != MatchOutcome::ongoing || !map_.contains(position)) {
        return false;
    }
    Building* building = find_building(building_id);
    if (building == nullptr || !building->completed()) {
        return false;
    }
    building->rally_point = position;
    building->has_rally_point = true;
    return true;
}

bool Simulation::reseed_farm(EntityId building_id) {
    if (outcome_ != MatchOutcome::ongoing) {
        return false;
    }
    Building* mill = find_building(building_id);
    if (mill == nullptr || mill->kind != BuildingKind::mill ||
        !mill->completed()) {
        return false;
    }
    Economy& economy = mill->owner == Player::blue
        ? player_states_[0].economy
        : player_states_[1].economy;
    const int base_cost = rules_for(BuildingKind::farm).wood_cost;
    const int cost =
        civilization(mill->owner) == Civilization::teutons
            ? base_cost * 60 / 100
            : base_cost;
    int& queue = mill->owner == Player::blue
        ? player_states_[0].farm_reseed_queue
        : player_states_[1].farm_reseed_queue;
    if (economy.wood < cost ||
        queue >= maximum_farm_reseed_queue) {
        return false;
    }
    economy.wood -= cost;
    ++queue;
    return true;
}

bool Simulation::reseed_farm_immediately(EntityId building_id) {
    Building* farm = find_building(building_id);
    if (outcome_ != MatchOutcome::ongoing || farm == nullptr ||
        farm->kind != BuildingKind::farm || !farm->completed() ||
        farm->resource_amount != 0) {
        return false;
    }
    Economy& economy = farm->owner == Player::blue
        ? player_states_[0].economy : player_states_[1].economy;
    const int base_cost = rules_for(BuildingKind::farm).wood_cost;
    const int cost =
        civilization(farm->owner) == Civilization::teutons
            ? base_cost * 60 / 100 : base_cost;
    if (economy.wood < cost) return false;
    economy.wood -= cost;
    farm->resource_amount = farm_capacity(farm->owner);
    return true;
}

bool Simulation::consume_farm_reseed(EntityId building_id) {
    Building* farm = find_building(building_id);
    if (outcome_ != MatchOutcome::ongoing || farm == nullptr ||
        farm->kind != BuildingKind::farm || !farm->completed() ||
        farm->resource_amount != 0) {
        return false;
    }
    int& queue = farm->owner == Player::blue
        ? player_states_[0].farm_reseed_queue : player_states_[1].farm_reseed_queue;
    if (queue <= 0) return false;
    --queue;
    farm->resource_amount = farm_capacity(farm->owner);
    return true;
}

int Simulation::garrison_count(EntityId building_id) const {
    return static_cast<int>(std::ranges::count_if(
        units_,
        [building_id](const Unit& unit) {
            return unit.garrisoned_in == building_id;
        }
    ));
}

bool Simulation::ungarrison_at(EntityId building_id) {
    Building* building = find_building(building_id);
    if (outcome_ != MatchOutcome::ongoing || building == nullptr ||
        garrison_capacity(building->kind) == 0) {
        return false;
    }
    bool released = false;
    for (Unit& unit : units_) {
        if (unit.garrisoned_in != building_id) {
            continue;
        }
        const std::optional<TilePosition> position = spawn_position(*building);
        if (!position) {
            if (building->hit_points <= 0) {
                unit.hit_points = 0;
                unit.garrisoned_in = 0;
                unit.garrison_target_id = 0;
            }
            continue;
        }
        unit.position = *position;
        unit.previous_position = *position;
        unit.destination = *position;
        unit.garrisoned_in = 0;
        unit.garrison_target_id = 0;
        unit.moving = false;
        unit.path.clear();
        unit.next_path_step = 0;
        released = true;
    }
    return released;
}

bool Simulation::resign(Player player) {
    if (outcome_ != MatchOutcome::ongoing ||
        !player_commands_allowed(player)) {
        return false;
    }
    if (player == Player::blue) {
        player_states_[0].controller = PlayerControllerState::resigned;
    } else if (player == Player::red) {
        player_states_[1].controller = PlayerControllerState::resigned;
    } else {
        return false;
    }
    outcome_ = player == Player::blue
        ? MatchOutcome::red_victory
        : MatchOutcome::blue_victory;
    selected_unit_.reset();
    selected_units_.clear();
    selected_building_.reset();
    return true;
}

bool Simulation::resign(PlayerSlotId player) {
    const auto index = player.index();
    if (!index || !roster_.slot(player).occupied ||
        player_states_[*index].controller !=
            PlayerControllerState::active) {
        return false;
    }
    const bool legacy_roster = std::ranges::none_of(
        roster_.slots().begin() + 2,
        roster_.slots().end(),
        [](const MatchRosterSlot& entry) { return entry.occupied; }
    );
    if (*index < 2 && legacy_roster) {
        return resign(*player_slot_to_legacy(player));
    }
    player_states_[*index].controller = PlayerControllerState::resigned;
    return true;
}

void Simulation::replace_controller_states(
    PlayerControllerState blue,
    PlayerControllerState red
) {
    player_states_[0].controller = blue;
    player_states_[1].controller = red;
}

void Simulation::set_match_rules(MatchRules rules) {
    if (outcome_ != MatchOutcome::ongoing) return;
    rules.wonder_countdown_ticks =
        std::max(1, rules.wonder_countdown_ticks);
    rules.relic_countdown_ticks =
        std::max(1, rules.relic_countdown_ticks);
    rules.relics_required = std::max(1, rules.relics_required);
    rules.score_limit = std::max(0, rules.score_limit);
    match_rules_ = rules;
    player_states_[0].victory_countdown = 0;
    player_states_[1].victory_countdown = 0;
    player_states_[0].countdown_kind = VictoryCountdownKind::none;
    player_states_[1].countdown_kind = VictoryCountdownKind::none;
    player_states_[0].countdown_last_tick = tick_number_;
    player_states_[1].countdown_last_tick = tick_number_;
}

void Simulation::replace_match_state(
    MatchRules rules,
    MatchOutcome outcome,
    int blue_countdown,
    int red_countdown,
    VictoryCountdownKind blue_kind,
    VictoryCountdownKind red_kind
) {
    match_rules_ = rules;
    outcome_ = outcome;
    player_states_[0].victory_countdown = std::max(0, blue_countdown);
    player_states_[1].victory_countdown = std::max(0, red_countdown);
    player_states_[0].countdown_kind = blue_kind;
    player_states_[1].countdown_kind = red_kind;
    player_states_[0].countdown_last_tick = tick_number_;
    player_states_[1].countdown_last_tick = tick_number_;
}

int Simulation::victory_countdown(Player player) const {
    return player == Player::blue
        ? player_states_[0].victory_countdown
        : player == Player::red ? player_states_[1].victory_countdown : 0;
}

int Simulation::score(Player player) const {
    const Economy& resources = economy(player);
    int total = resources.food + resources.wood +
        resources.gold + resources.stone;
    for (const Unit& unit : units_) {
        if (unit.owner == player && unit.hit_points > 0) {
            total += unit.hit_points;
        }
    }
    for (const Building& building : buildings_) {
        if (building.owner == player && building.hit_points > 0) {
            total += building.hit_points;
        }
    }
    return total;
}

bool Simulation::advance_age_at(EntityId building_id) {
    if (outcome_ != MatchOutcome::ongoing) {
        return false;
    }
    Building* town_center = find_building(building_id);
    if (town_center == nullptr ||
        town_center->kind != BuildingKind::town_center ||
        !town_center->completed() ||
        !town_center->production_queue.empty() ||
        town_center->age_research_ticks_remaining > 0 ||
        town_center->technology_research_ticks_remaining > 0) {
        return false;
    }
    if (std::ranges::any_of(
            buildings_,
            [town_center](const Building& building) {
                return building.owner == town_center->owner &&
                       building.age_research_ticks_remaining > 0;
            }
        )) {
        return false;
    }
    const Age current = age(town_center->owner);
    if (current == Age::imperial) {
        return false;
    }
    const Age target = static_cast<Age>(
        static_cast<int>(current) + 1
    );
    if (!has_age_prerequisites(town_center->owner, target)) {
        return false;
    }
    Economy& economy = town_center->owner == Player::blue
        ? player_states_[0].economy
        : player_states_[1].economy;
    const AgeRules& rules = rules_for(target);
    if (economy.food < rules.food_cost ||
        economy.gold < rules.gold_cost) {
        return false;
    }
    economy.food -= rules.food_cost;
    economy.gold -= rules.gold_cost;
    town_center->age_research_target = target;
    town_center->age_research_ticks_remaining = rules.research_ticks;
    return true;
}

bool Simulation::research_technology_at(
    EntityId building_id,
    Technology technology
) {
    if (outcome_ != MatchOutcome::ongoing) {
        return false;
    }
    Building* building = find_building(building_id);
    const TechnologyRules& rules = rules_for(technology);
    if ((technology == Technology::siege_onager &&
         !has_technology(building->owner, Technology::onager)) ||
        (technology == Technology::siege_ram &&
         !has_technology(building->owner, Technology::capped_ram)) ||
        (technology == Technology::elite_eagle_warrior &&
         civilization(building->owner) != Civilization::aztecs &&
         civilization(building->owner) != Civilization::mayans)) {
        return false;
    }
    if (building == nullptr || !building->completed() ||
        !civilization_has_technology(
            civilization(building->owner), technology
        ) ||
        building->kind != rules.researched_at ||
        !building->production_queue.empty() ||
        building->age_research_ticks_remaining > 0 ||
        building->technology_research_ticks_remaining > 0 ||
        has_technology(building->owner, technology) ||
        static_cast<int>(age(building->owner)) <
            static_cast<int>(rules.minimum_age)) {
        return false;
    }
    if (technology == Technology::bombard_tower &&
        !has_technology(building->owner, Technology::chemistry) &&
        civilization(building->owner) != Civilization::turks) {
        return false;
    }
    if (technology == Technology::long_swordsman &&
        !has_technology(building->owner, Technology::man_at_arms)) {
        return false;
    }
    if (technology == Technology::two_handed_swordsman &&
        !has_technology(building->owner, Technology::long_swordsman)) {
        return false;
    }
    if (technology == Technology::champion &&
        !has_technology(
            building->owner, Technology::two_handed_swordsman
        )) {
        return false;
    }
    if (technology == Technology::paladin &&
        !has_technology(building->owner, Technology::cavalier)) {
        return false;
    }
    if (technology == Technology::hussar &&
        !has_technology(building->owner, Technology::light_cavalry)) {
        return false;
    }
    if (technology == Technology::arbalester &&
        !has_technology(building->owner, Technology::crossbowman)) {
        return false;
    }
    if (technology == Technology::halberdier &&
        !has_technology(building->owner, Technology::pikeman)) {
        return false;
    }
    if (technology == Technology::keep &&
        !has_technology(building->owner, Technology::guard_tower)) {
        return false;
    }
    if (technology == Technology::bodkin_arrow &&
        !has_technology(building->owner, Technology::fletching)) {
        return false;
    }
    if (technology == Technology::bracer &&
        !has_technology(building->owner, Technology::bodkin_arrow)) {
        return false;
    }
    if ((technology == Technology::heavy_plow &&
         !has_technology(building->owner, Technology::horse_collar)) ||
        (technology == Technology::crop_rotation &&
         !has_technology(building->owner, Technology::heavy_plow)) ||
        (technology == Technology::bow_saw &&
         !has_technology(building->owner, Technology::double_bit_axe)) ||
        (technology == Technology::two_man_saw &&
         !has_technology(building->owner, Technology::bow_saw)) ||
        (technology == Technology::gold_shaft_mining &&
         !has_technology(building->owner, Technology::gold_mining)) ||
        (technology == Technology::stone_shaft_mining &&
         !has_technology(building->owner, Technology::stone_mining)) ||
        (technology == Technology::hand_cart &&
         !has_technology(building->owner, Technology::wheelbarrow)) ||
        (technology == Technology::banking &&
         !has_technology(building->owner, Technology::coinage)) ||
        (technology == Technology::caravan &&
         !has_technology(building->owner, Technology::cartography)) ||
        (technology == Technology::town_patrol &&
         !has_technology(building->owner, Technology::town_watch)) ||
        (technology == Technology::architecture &&
         !has_technology(building->owner, Technology::masonry))) {
        return false;
    }
    if (technology == Technology::iron_casting &&
        !has_technology(building->owner, Technology::forging)) {
        return false;
    }
    if (technology == Technology::blast_furnace &&
        !has_technology(building->owner, Technology::iron_casting)) {
        return false;
    }
    if (technology == Technology::chain_mail_armor &&
        !has_technology(building->owner, Technology::scale_mail_armor)) {
        return false;
    }
    if (technology == Technology::plate_mail_armor &&
        !has_technology(building->owner, Technology::chain_mail_armor)) {
        return false;
    }
    if (technology == Technology::chain_barding_armor &&
        !has_technology(building->owner, Technology::scale_barding_armor)) {
        return false;
    }
    if (technology == Technology::plate_barding_armor &&
        !has_technology(building->owner, Technology::chain_barding_armor)) {
        return false;
    }
    if (technology == Technology::leather_archer_armor &&
        !has_technology(building->owner, Technology::padded_archer_armor)) {
        return false;
    }
    if (technology == Technology::ring_archer_armor &&
        !has_technology(building->owner, Technology::leather_archer_armor)) {
        return false;
    }
    if (technology == Technology::galleon &&
        !has_technology(building->owner, Technology::war_galley)) {
        return false;
    }
    if (technology == Technology::elite_cannon_galleon &&
        !has_technology(building->owner, Technology::cannon_galleon)) {
        return false;
    }
    if (technology == Technology::dry_dock &&
        !has_technology(building->owner, Technology::careening)) {
        return false;
    }
    if (technology == Technology::longboat ||
        technology == Technology::turtle_ship) {
        return false;
    }
    if (technology == Technology::hand_cannoneer_gate ||
        technology == Technology::bombard_cannon_gate ||
        technology == Technology::petard_gate) {
        return false;
    }
    if (technology == Technology::elite_longboat &&
        civilization(building->owner) != Civilization::vikings) {
        return false;
    }
    if (technology == Technology::elite_turtle_ship &&
        civilization(building->owner) != Civilization::koreans) {
        return false;
    }
    if (technology == Technology::longbowman ||
        technology == Technology::throwing_axeman ||
        technology == Technology::huskarl ||
        technology == Technology::teutonic_knight ||
        technology == Technology::samurai ||
        technology == Technology::chu_ko_nu ||
        technology == Technology::cataphract ||
        technology == Technology::war_elephant ||
        technology == Technology::mameluke ||
        technology == Technology::janissary ||
        technology == Technology::berserk ||
        technology == Technology::mangudai ||
        technology == Technology::jaguar_warrior ||
        technology == Technology::plumed_archer ||
        technology == Technology::conquistador ||
        technology == Technology::tarkan ||
        technology == Technology::woad_raider) {
        return false;
    }
    if ((technology == Technology::elite_longbowman &&
         civilization(building->owner) != Civilization::britons) ||
        (technology == Technology::elite_throwing_axeman &&
         civilization(building->owner) != Civilization::franks) ||
        (technology == Technology::elite_huskarl &&
         civilization(building->owner) != Civilization::goths) ||
        (technology == Technology::elite_teutonic_knight &&
         civilization(building->owner) != Civilization::teutons) ||
        (technology == Technology::elite_samurai &&
         civilization(building->owner) != Civilization::japanese) ||
        (technology == Technology::elite_chu_ko_nu &&
         civilization(building->owner) != Civilization::chinese) ||
        (technology == Technology::elite_cataphract &&
         civilization(building->owner) != Civilization::byzantines) ||
        (technology == Technology::elite_war_elephant &&
         civilization(building->owner) != Civilization::persians) ||
        (technology == Technology::elite_mameluke &&
         civilization(building->owner) != Civilization::saracens) ||
        (technology == Technology::elite_janissary &&
         civilization(building->owner) != Civilization::turks) ||
        (technology == Technology::elite_berserk &&
         civilization(building->owner) != Civilization::vikings) ||
        (technology == Technology::elite_mangudai &&
         civilization(building->owner) != Civilization::mongols) ||
        (technology == Technology::berserkergang &&
         civilization(building->owner) != Civilization::vikings) ||
        (technology == Technology::elite_jaguar_warrior &&
         civilization(building->owner) != Civilization::aztecs) ||
        (technology == Technology::elite_plumed_archer &&
         civilization(building->owner) != Civilization::mayans) ||
        (technology == Technology::elite_conquistador &&
         civilization(building->owner) != Civilization::spanish) ||
        (technology == Technology::elite_tarkan &&
         civilization(building->owner) != Civilization::huns) ||
        (technology == Technology::elite_woad_raider &&
         civilization(building->owner) != Civilization::celts)) {
        return false;
    }
    if (std::ranges::any_of(
            buildings_,
            [building, technology](const Building& candidate) {
                return candidate.owner == building->owner &&
                       candidate.technology_research_ticks_remaining > 0 &&
                       candidate.technology_research_target == technology;
            }
        )) {
        return false;
    }
    Economy& economy = building->owner == Player::blue
        ? player_states_[0].economy
        : player_states_[1].economy;
    int discount{};
    if (civilization(building->owner) == Civilization::chinese) {
        discount =
            age(building->owner) >= Age::imperial ? 20 :
            age(building->owner) >= Age::castle ? 15 :
            age(building->owner) >= Age::feudal ? 10 : 0;
    }
    const int wood_cost = rules.wood_cost * (100 - discount) / 100;
    const int food_cost = rules.food_cost * (100 - discount) / 100;
    const int spy_multiplier =
        technology == Technology::spy_technology
        ? static_cast<int>(std::ranges::count_if(
              units_,
              [this, building](const Unit& unit) {
                  return is_enemy(building->owner, unit.owner) &&
                      unit.kind == UnitKind::villager &&
                      unit.hit_points > 0;
              }
          ))
        : 1;
    const int gold_cost =
        civilization(building->owner) == Civilization::spanish &&
        building->kind == BuildingKind::blacksmith
            ? 0
            : rules.gold_cost * spy_multiplier *
                (100 - discount) / 100;
    const int stone_cost = rules.stone_cost * (100 - discount) / 100;
    if (economy.wood < wood_cost ||
        economy.food < food_cost ||
        economy.gold < gold_cost ||
        economy.stone < stone_cost) {
        return false;
    }
    economy.wood -= wood_cost;
    economy.food -= food_cost;
    economy.gold -= gold_cost;
    economy.stone -= stone_cost;
    building->technology_research_target = technology;
    building->technology_research_ticks_remaining =
        rules.research_ticks;
    return true;
}

bool Simulation::has_commercial_technology(
    EntityOwner owner,
    CommercialTechnologyId technology
) const {
    const auto slot = owner.slot_index();
    return slot && technology < 460 &&
        player_states_[*slot].commercial_technologies[technology];
}

bool Simulation::research_commercial_technology_at(
    EntityId building_id,
    CommercialTechnologyId technology_id
) {
    Building* building = find_building(building_id);
    const CommercialTechnologyRecord* technology =
        commercial_content_catalog().technology(technology_id);
    if (outcome_ != MatchOutcome::ongoing || building == nullptr ||
        technology == nullptr || !building->completed() ||
        !building->commercial_identity ||
        technology->research_location_object_id !=
            building->commercial_identity->object_id ||
        building->technology_research_ticks_remaining > 0 ||
        building->age_research_ticks_remaining > 0 ||
        !building->production_queue.empty() ||
        has_commercial_technology(building->owner, technology_id)) {
        return false;
    }
    const auto owner_slot = building->owner.slot_index();
    if (!owner_slot || player_states_[*owner_slot]
            .commercial_disabled_technologies[technology_id]) {
        return false;
    }
    if (technology->civilization_id &&
        *technology->civilization_id !=
            building->commercial_identity->civilization_id) {
        return false;
    }
    for (CommercialTechnologyId prerequisite : technology->prerequisites) {
        if (!has_commercial_technology(building->owner, prerequisite)) {
            return false;
        }
    }
    Economy& economy = player_states_.at(building->owner.stable_id()).economy;
    int food{};
    int wood{};
    int stone{};
    int gold{};
    const auto overridden = player_states_[*owner_slot]
        .commercial_technology_cost_overrides.find(technology_id);
    if (overridden != player_states_[*owner_slot]
            .commercial_technology_cost_overrides.end()) {
        food = overridden->second[0];
        wood = overridden->second[1];
        stone = overridden->second[2];
        gold = overridden->second[3];
    } else {
        for (const CommercialTechnologyCost& cost : technology->costs) {
            if (!cost.enabled) continue;
            if (cost.resource_id == 0) food += cost.amount;
            else if (cost.resource_id == 1) wood += cost.amount;
            else if (cost.resource_id == 2) stone += cost.amount;
            else if (cost.resource_id == 3) gold += cost.amount;
        }
    }
    if (economy.food < food || economy.wood < wood ||
        economy.stone < stone || economy.gold < gold) {
        return false;
    }
    economy.food -= food;
    economy.wood -= wood;
    economy.stone -= stone;
    economy.gold -= gold;
    building->commercial_research_target = technology_id;
    const auto time_override = player_states_[*owner_slot]
        .commercial_technology_time_overrides.find(technology_id);
    building->technology_research_ticks_remaining = std::max(
        1, time_override == player_states_[*owner_slot]
               .commercial_technology_time_overrides.end()
            ? technology->research_time : time_override->second
    );
    return true;
}

bool Simulation::commercial_object_enabled(
    EntityOwner owner,
    CommercialObjectIdentity identity
) const {
    const CommercialObjectRecord* record =
        commercial_content_catalog().object(
            identity.civilization_id, identity.object_id
        );
    if (record == nullptr) return false;
    bool enabled = record->enabled && !record->disabled;
    const auto slot = owner.slot_index();
    if (!slot) return enabled;
    const auto& technologies = player_states_[*slot].commercial_technologies;
    const auto& catalog = commercial_content_catalog();
    for (std::size_t id = 0; id < technologies.size(); ++id) {
        if (!technologies[id]) continue;
        const auto* technology = catalog.technology(
            static_cast<CommercialTechnologyId>(id)
        );
        if (technology == nullptr) continue;
        const auto* effect = catalog.effect(technology->effect_id);
        if (effect == nullptr) continue;
        for (const CommercialEffectCommand& command : effect->commands) {
            if (command.type == 2 && command.object_id == identity.object_id) {
                enabled = command.unit_class != 0;
            }
        }
    }
    return enabled;
}

void Simulation::apply_commercial_effect(
    EntityOwner owner,
    CommercialEffectId effect_id
) {
    const CommercialEffectRecord* effect =
        commercial_content_catalog().effect(effect_id);
    if (effect == nullptr) return;
    const auto owner_slot = owner.slot_index();
    if (!owner_slot) return;
    PlayerState& player = player_states_[*owner_slot];
    const auto applies = [](const CommercialEffectCommand& command,
                            const CommercialObjectRecord& record) {
        return (command.object_id < 0 || command.object_id == record.id) &&
            (command.unit_class < 0 || command.unit_class == record.unit_class);
    };
    const auto apply_attribute = [](const CommercialEffectCommand& command,
                                    Unit& unit) {
        const auto change = [&command](int value) {
            if (command.type == 0) {
                return static_cast<int>(command.amount);
            }
            if (command.type == 5) {
                return static_cast<int>(value * command.amount);
            }
            return value + static_cast<int>(command.amount);
        };
        if (command.attribute_id == 0) unit.hit_points = change(unit.hit_points);
        else if (command.attribute_id == 9) unit.attack = change(unit.attack);
    };
    for (const CommercialEffectCommand& command : effect->commands) {
        if (command.type == 1 || command.type == 6) {
            if (command.object_id >= 0 && command.object_id < 256) {
                float& resource = player.commercial_resources[
                    static_cast<std::size_t>(command.object_id)
                ];
                if (command.type == 6) resource *= command.amount;
                else if (command.unit_class == 0) resource = command.amount;
                else resource += command.amount;
            }
            continue;
        }
        if (command.type == 101) {
            if (command.object_id >= 0 && command.object_id < 460 &&
                command.unit_class >= 0 && command.unit_class < 4) {
                const auto technology = static_cast<CommercialTechnologyId>(
                    command.object_id
                );
                auto [it, inserted] =
                    player.commercial_technology_cost_overrides.try_emplace(
                        technology
                    );
                if (inserted) {
                    it->second.fill(0);
                    if (const auto* record =
                            commercial_content_catalog().technology(
                                technology
                            )) {
                        for (const auto& cost : record->costs) {
                            if (cost.enabled && cost.resource_id < 4) {
                                it->second[cost.resource_id] = cost.amount;
                            }
                        }
                    }
                }
                int& cost = it->second[command.unit_class];
                if (command.attribute_id == 0) {
                    cost = std::max(0, static_cast<int>(command.amount));
                } else {
                    cost = std::max(
                        0, cost + static_cast<int>(command.amount)
                    );
                }
            }
            continue;
        }
        if (command.type == 102) {
            const int technology = static_cast<int>(command.amount);
            if (technology >= 0 && technology < 460) {
                player.commercial_disabled_technologies[technology] = true;
            }
            continue;
        }
        if (command.type == 103) {
            if (command.object_id >= 0 && command.object_id < 460) {
                player.commercial_technology_time_overrides[
                    static_cast<CommercialTechnologyId>(command.object_id)
                ] = std::max(0, static_cast<int>(command.amount));
            }
            continue;
        }
        if (command.type == 255) continue;
        if (command.type == 3) {
            for (Unit& unit : units_) {
                if (unit.owner == owner && unit.commercial_identity &&
                    unit.commercial_identity->object_id == command.object_id) {
                    unit.commercial_identity->object_id =
                        static_cast<CommercialObjectId>(command.unit_class);
                    const auto* upgraded = commercial_content_catalog().object(
                        unit.commercial_identity->civilization_id,
                        unit.commercial_identity->object_id
                    );
                    if (upgraded) {
                        unit.hit_points = upgraded->hit_points;
                        unit.attack = upgraded->attack;
                    }
                }
            }
            for (Building& building : buildings_) {
                if (building.owner == owner && building.commercial_identity &&
                    building.commercial_identity->object_id ==
                        command.object_id) {
                    building.commercial_identity->object_id =
                        static_cast<CommercialObjectId>(command.unit_class);
                    const auto* upgraded = commercial_content_catalog().object(
                        building.commercial_identity->civilization_id,
                        building.commercial_identity->object_id
                    );
                    if (upgraded) building.hit_points = upgraded->hit_points;
                }
            }
            continue;
        }
        if (command.type != 0 && command.type != 4 && command.type != 5) {
            continue;
        }
        for (Unit& unit : units_) {
            if (unit.owner != owner || !unit.commercial_identity) continue;
            const auto* record = commercial_content_catalog().object(
                unit.commercial_identity->civilization_id,
                unit.commercial_identity->object_id
            );
            if (record && applies(command, *record)) {
                apply_attribute(command, unit);
            }
        }
        for (Building& building : buildings_) {
            if (building.owner != owner || !building.commercial_identity) {
                continue;
            }
            const auto* record = commercial_content_catalog().object(
                building.commercial_identity->civilization_id,
                building.commercial_identity->object_id
            );
            if (record && applies(command, *record) &&
                command.attribute_id == 0) {
                if (command.type == 0) {
                    building.hit_points = static_cast<int>(command.amount);
                } else if (command.type == 5) {
                    building.hit_points = static_cast<int>(
                        building.hit_points * command.amount
                    );
                } else {
                    building.hit_points += static_cast<int>(command.amount);
                }
            }
        }
    }
}

void Simulation::update() {
    if (outcome_ != MatchOutcome::ongoing) {
        return;
    }
    ++tick_number_;
    prune_attack_reveals();
    for (Unit& unit : units_) {
        if (!unit.render_subtile_initialized) {
            unit.render_current_subtile = {
                unit.position.x * 320, unit.position.y * 320
            };
            unit.render_subtile_initialized = true;
        }
        if (!unit_render_elevations_.contains(unit.id)) {
            initialize_unit_render_elevation(unit);
        }
        unit.render_previous_subtile = unit.render_current_subtile;
        unit_render_elevations_.at(unit.id).previous =
            unit_render_elevations_.at(unit.id).current;
    }
    if (evaluate_scenario_triggers()) {
        return;
    }
    const auto qualifying_herdable_captor = [](const Unit& candidate) {
        return !candidate.owner.is_neutral() &&
            candidate.hit_points > 0 && candidate.garrisoned_in == 0 &&
            !is_herdable(candidate.kind) && !is_relic(candidate.kind);
    };
    const auto capture_owner = [&](const Unit& herdable) {
        std::array<int, 8> nearest_distance_squared;
        nearest_distance_squared.fill(std::numeric_limits<int>::max());
        std::array<int, 8> nearby_counts{};
        bool current_owner_nearby = false;

        for (const Unit& candidate : units_) {
            if (!qualifying_herdable_captor(candidate)) continue;
            const int dx = candidate.position.x - herdable.position.x;
            const int dy = candidate.position.y - herdable.position.y;
            // Native get-auto-converted action scans the inclusive seven by
            // seven tile box centered on the herdable.
            if (std::abs(dx) > 3 || std::abs(dy) > 3) continue;
            const auto slot = entity_owner_slot(candidate.owner);
            if (!slot || slot->is_neutral() ||
                !roster_.slot(*slot).occupied) {
                continue;
            }
            const std::size_t index = *slot->index();
            ++nearby_counts[index];
            nearest_distance_squared[index] = std::min(
                nearest_distance_squared[index], dx * dx + dy * dy
            );
            current_owner_nearby = current_owner_nearby ||
                candidate.owner == herdable.owner;
        }
        if (current_owner_nearby) return herdable.owner;

        std::optional<std::size_t> selected;
        for (std::size_t index = 0; index < nearby_counts.size(); ++index) {
            if (nearby_counts[index] == 0) continue;
            if (!selected ||
                nearest_distance_squared[index] <
                    nearest_distance_squared[*selected]) {
                selected = index;
            }
        }
        return selected
            ? entity_owner_from_slot(*PlayerSlotId::from_index(*selected))
            : herdable.owner;
    };
    const auto capture_nearby_neutral_herdables =
        [&](Unit& captured, EntityOwner owner) {
            std::vector<Unit*> changed{&captured};
            for (std::size_t index = 0; index < changed.size(); ++index) {
                const Unit& source = *changed[index];
                for (Unit& candidate : units_) {
                    if (!is_herdable(candidate.kind) ||
                        !candidate.owner.is_neutral() ||
                        candidate.hit_points <= 0 ||
                        candidate.garrisoned_in != 0) {
                        continue;
                    }
                    if (std::abs(
                            candidate.position.x - source.position.x
                        ) <= 3 &&
                        std::abs(
                            candidate.position.y - source.position.y
                        ) <= 3) {
                        candidate.owner = owner;
                        candidate.stance_anchor = candidate.position;
                        changed.push_back(&candidate);
                    }
                }
            }
        };
    for (Unit& unit : units_) {
        if (!is_herdable(unit.kind) || unit.hit_points <= 0 ||
            unit.garrisoned_in != 0) {
            continue;
        }
        const EntityOwner owner = capture_owner(unit);
        if (owner == unit.owner) continue;
        unit.owner = owner;
        unit.stance_anchor = unit.position;
        capture_nearby_neutral_herdables(unit, owner);
    }
    for (Unit& unit : units_) {
        if (!is_animal(unit.kind) || unit.hit_points > 0 ||
            unit.food_remaining <= 0) {
            continue;
        }
        // Live DAT resource-decay values are 0.25 food/s for Sheep/Deer
        // and 0.4 food/s for Boar. The simulation represents five ticks
        // per second, so retain exact hundredths over denominator 500.
        unit.food_decay_remainder +=
            unit.kind == UnitKind::boar ? 40 : 25;
        const int decayed = unit.food_decay_remainder / 500;
        unit.food_decay_remainder %= 500;
        unit.food_remaining = std::max(
            0, unit.food_remaining - decayed
        );
    }
    for (Unit& unit : units_) {
        if (unit.garrisoned_in == 0 || unit.hit_points <= 0 ||
            unit.hit_points >= maximum_hit_points(unit)) {
            continue;
        }
        const Building* shelter = find_building(unit.garrisoned_in);
        if (shelter == nullptr || shelter->hit_points <= 0 ||
            !can_garrison(unit, *shelter)) {
            continue;
        }
        // Bounded contract: interpret raw DAT rates 0.1/0.2 as HP/s.
        // At five ticks per represented second, that is one whole HP
        // every 50/25 ticks without a fractional save-state accumulator.
        std::uint64_t healing_interval =
            shelter->kind == BuildingKind::castle ? 25 : 50;
        if (has_technology(unit.owner, Technology::herbal_medicine)) {
            healing_interval = std::max<std::uint64_t>(
                1, healing_interval / 4
            );
        }
        if (tick_number_ % healing_interval == 0) {
            ++unit.hit_points;
        }
    }
    std::vector<std::pair<EntityId, EntityId>> relic_collections;
    std::vector<std::pair<EntityId, EntityId>> relic_deposits;
    for (Unit& unit : units_) {
        if ((unit.relic_target_id != 0 ||
             unit.relic_deposit_target_id != 0) &&
            unit.moving && unit.movement_cooldown == 0 &&
            unit.next_path_step < unit.path.size()) {
            const TilePosition next = unit.path[unit.next_path_step];
            if (map_.traversable(unit.position, next) &&
                !occupied(next, unit.id, unit.owner)) {
                unit.previous_position = unit.position;
                unit.position = next;
                unit.last_move_tick = tick_number_;
                ++unit.next_path_step;
                unit.movement_cooldown = std::max(
                    0,
                    effective_movement_interval(unit) - 1
                );
                if (unit.next_path_step >= unit.path.size()) {
                    unit.moving = false;
                }
            }
        }
        if (unit.relic_target_id != 0) {
            relic_collections.push_back({
                unit.id, unit.relic_target_id
            });
        }
        if (unit.relic_deposit_target_id != 0) {
            relic_deposits.push_back({
                unit.id, unit.relic_deposit_target_id
            });
        }
    }
    for (const auto [monk, relic] : relic_collections) {
        const Unit* monk_unit = find_unit(monk);
        const Unit* relic_unit = find_unit(relic);
        if (monk_unit != nullptr && relic_unit != nullptr &&
            std::abs(monk_unit->position.x - relic_unit->position.x) +
                std::abs(
                    monk_unit->position.y - relic_unit->position.y
                ) <= 1) {
            command_collect_relic(monk, relic);
        }
    }
    for (const auto [monk, monastery] : relic_deposits) {
        const Unit* monk_unit = find_unit(monk);
        const Building* target = find_building(monastery);
        if (monk_unit != nullptr && target != nullptr &&
            distance_to_building(monk_unit->position, *target) <= 1) {
            command_deposit_relic(monk, monastery);
        }
    }
    for (Unit& unit : units_) {
        if (unit.trebuchet_transform_ticks_remaining <= 0) continue;
        --unit.trebuchet_transform_ticks_remaining;
        if (unit.trebuchet_transform_ticks_remaining == 0) {
            unit.kind = unit.trebuchet_transform_to_packed
                ? UnitKind::packed_trebuchet
                : UnitKind::trebuchet;
            unit.attack = rules_for(unit.kind).attack;
        }
    }
    if (tick_number_ % 3 == 0) {
        for (Unit& unit : units_) {
            if ((unit.kind == UnitKind::berserk ||
                 unit.kind == UnitKind::elite_berserk) &&
                unit.hit_points > 0) {
                const int regeneration =
                    berserk_regeneration_per_three_ticks(unit);
                unit.hit_points = std::min(
                    maximum_hit_points(unit),
                    unit.hit_points + regeneration
                );
            }
        }
    }
    update_gate_states();
        if (tick_number_ % 10 == 0) {
        for (const Building& building : buildings_) {
            if (building.kind == BuildingKind::monastery &&
                building.completed() && building.relic_count > 0) {
                Economy& economy = building.owner == Player::blue
                    ? player_states_[0].economy : player_states_[1].economy;
                int& remainder = building.owner == Player::blue
                    ? player_states_[0].aztec_relic_gold_remainder
                    : player_states_[1].aztec_relic_gold_remainder;
                if (team_has_civilization(
                        building.owner, Civilization::aztecs
                    )) {
                    const int hundredths =
                        remainder + building.relic_count * 133;
                    economy.gold += hundredths / 100;
                    remainder = hundredths % 100;
                } else {
                    economy.gold += building.relic_count;
                }
            }
        }
    }
    for (UnitDeathEffect& effect : death_effects_) {
        --effect.ticks_remaining;
    }
    std::erase_if(death_effects_, [](const UnitDeathEffect& effect) {
        return effect.ticks_remaining <= 0;
    });
    for (BuildingRubbleEffect& effect : rubble_effects_) {
        --effect.ticks_remaining;
    }
    std::erase_if(rubble_effects_, [](const BuildingRubbleEffect& effect) {
        return effect.ticks_remaining <= 0;
    });

    for (Unit& unit : units_) {
        if (unit.garrisoned_in != 0) {
            continue;
        }
        if (unit.kind == UnitKind::fishing_ship &&
            unit.has_resource_target) {
            Building* nearest_dock{};
            int nearest_distance = map_.width() + map_.height() + 1;
            for (Building& building : buildings_) {
                if (building.owner == unit.owner &&
                    building.kind == BuildingKind::dock &&
                    building.completed()) {
                    const int distance =
                        distance_to_building(unit.position, building);
                    if (distance < nearest_distance) {
                        nearest_distance = distance;
                        nearest_dock = &building;
                    }
                }
            }
            if (unit.returning_resource) {
                if (nearest_dock == nullptr) {
                    unit.has_resource_target = false;
                    unit.returning_resource = false;
                } else if (nearest_distance <= 1) {
                    Economy& economy = unit.owner == Player::blue
                        ? player_states_[0].economy : player_states_[1].economy;
                    economy.food += unit.carried_amount;
                    unit.carried_amount = 0;
                    unit.returning_resource = false;
                    if (map_.terrain_at(unit.resource_target) ==
                            Terrain::fish &&
                        map_.resource_amount_at(unit.resource_target) > 0) {
                        route_unit(unit, unit.resource_target);
                    } else if (Building* trap =
                                   find_building(
                                       unit.resource_building_id
                                   );
                               trap != nullptr &&
                               trap->kind == BuildingKind::fish_trap &&
                               trap->resource_amount > 0) {
                        route_unit(unit, trap->position);
                    } else {
                        unit.has_resource_target = false;
                    }
                } else if (!unit.moving) {
                    route_unit(unit, nearest_dock->position);
                }
            }
            if (!unit.moving && unit.has_resource_target &&
                unit.position == unit.resource_target &&
                map_.terrain_at(unit.resource_target) == Terrain::fish) {
                unit.carried_resource = ResourceKind::food;
                const auto [credited, consumed] =
                    finite_resource_yield(
                        unit.owner,
                        map_.resource_amount_at(unit.resource_target),
                        1
                    );
                map_.take_resource(unit.resource_target, consumed);
                unit.carried_amount += credited;
                credit_gathered(
                    unit.owner, ResourceKind::food, credited
                );
                if (unit.carried_amount >= 15 ||
                    map_.resource_amount_at(unit.resource_target) == 0) {
                    unit.returning_resource = true;
                    if (nearest_dock != nullptr) {
                        route_unit(unit, nearest_dock->position);
                    }
                }
                continue;
            }
            if (!unit.moving && unit.has_resource_target &&
                unit.resource_building_id != 0) {
                Building* trap =
                    find_building(unit.resource_building_id);
                if (trap != nullptr &&
                    trap->kind == BuildingKind::fish_trap &&
                    trap->owner == unit.owner &&
                    trap->resource_amount > 0 &&
                    distance_to_building(unit.position, *trap) <= 1) {
                    unit.carried_resource = ResourceKind::food;
                    const auto [credited, consumed] =
                        finite_resource_yield(
                            unit.owner, trap->resource_amount, 1
                        );
                    trap->resource_amount -= consumed;
                    unit.carried_amount += credited;
                    credit_gathered(
                        unit.owner, ResourceKind::food, credited
                    );
                    if (unit.carried_amount >= 15 ||
                        trap->resource_amount == 0) {
                        unit.returning_resource = true;
                        if (nearest_dock != nullptr) {
                            route_unit(unit, nearest_dock->position);
                        }
                    }
                    continue;
                }
                unit.has_resource_target = false;
                unit.resource_building_id = 0;
            }
        }
        if (unit.garrison_target_id != 0) {
            Building* shelter = find_building(unit.garrison_target_id);
            if (shelter == nullptr || !can_garrison(unit, *shelter) ||
                garrison_count(shelter->id) >=
                    garrison_capacity(shelter->kind)) {
                unit.garrison_target_id = 0;
            } else if (distance_to_building(unit.position, *shelter) <= 1) {
                if (unit.carried_amount > 0) {
                    Economy& economy = unit.owner == Player::blue
                        ? player_states_[0].economy
                        : player_states_[1].economy;
                    switch (unit.carried_resource) {
                        case ResourceKind::wood:
                            economy.wood += unit.carried_amount;
                            break;
                        case ResourceKind::food:
                            economy.food += unit.carried_amount;
                            break;
                        case ResourceKind::gold:
                            economy.gold += unit.carried_amount;
                            break;
                        case ResourceKind::stone:
                            economy.stone += unit.carried_amount;
                            break;
                        case ResourceKind::none:
                            break;
                    }
                    unit.carried_amount = 0;
                    unit.carried_resource = ResourceKind::none;
                }
                unit.garrisoned_in = shelter->id;
                unit.garrison_target_id = 0;
                unit.position = shelter->position;
                unit.previous_position = shelter->position;
                unit.destination = shelter->position;
                unit.moving = false;
                unit.path.clear();
                unit.next_path_step = 0;
                std::erase(selected_units_, unit.id);
                if (selected_unit_ == unit.id) {
                    selected_unit_.reset();
                }
                continue;
            }
        }
        if (unit.attack_cooldown > 0) {
            --unit.attack_cooldown;
        }
        if (unit.conversion_cooldown > 0) {
            --unit.conversion_cooldown;
        }
        if (unit.conversion_target_id != 0) {
            Unit* target = find_unit(unit.conversion_target_id);
            Building* conversion_building =
                target == nullptr
                    ? find_building(unit.conversion_target_id)
                    : nullptr;
            if (conversion_building != nullptr &&
                (unit.kind == UnitKind::monk ||
                 unit.kind == UnitKind::missionary) &&
                unit.garrisoned_in == 0 &&
                has_technology(unit.owner, Technology::redemption) &&
                is_enemy(conversion_building->owner, unit.owner) &&
                conversion_building->completed() &&
                conversion_building->hit_points > 0 &&
                is_building_visible(unit.owner, *conversion_building)) {
                const int distance_squared = combat_distance_squared(
                    unit.position, *conversion_building
                );
                const int range =
                    rules_for(unit.kind).attack_range +
                    (has_technology(
                        unit.owner, Technology::block_printing
                    ) ? 3 : 0);
                if (distance_squared > range * range) {
                    unit.conversion_target_id = 0;
                    unit.conversion_progress = 0;
                } else if (++unit.conversion_progress >= 8) {
                    conversion_building->owner = unit.owner;
                    ++mutable_statistics(unit.owner).conversions;
                    unit.conversion_target_id = 0;
                    unit.conversion_progress = 0;
                    unit.conversion_cooldown =
                        has_technology(
                            unit.owner, Technology::illumination
                        ) ? 10 : 20;
                }
                continue;
            }
            const bool valid =
                (unit.kind == UnitKind::monk ||
                 unit.kind == UnitKind::missionary) &&
                unit.garrisoned_in == 0 &&
                target != nullptr &&
                is_enemy(target->owner, unit.owner) &&
                target->garrisoned_in == 0 &&
                !is_animal(target->kind) &&
                !is_relic(target->kind) &&
                target->hit_points > 0 &&
                !target->unconvertible &&
                ((target->kind != UnitKind::monk &&
                  target->kind != UnitKind::missionary) ||
                 has_technology(unit.owner, Technology::atonement)) &&
                ((rules_for(target->kind).trained_at !=
                      BuildingKind::siege_workshop &&
                  target->kind != UnitKind::packed_trebuchet &&
                  target->kind != UnitKind::trebuchet) ||
                 has_technology(unit.owner, Technology::redemption)) &&
                is_unit_visible(unit.owner, *target);
            const int distance_squared = valid
                ? combat_distance_squared(
                    unit.position, target->position
                )
                : 9999;
            const int conversion_range =
                rules_for(unit.kind).attack_range +
                (has_technology(
                    unit.owner, Technology::block_printing
                ) ? 3 : 0);
            if (!valid ||
                distance_squared > conversion_range * conversion_range) {
                unit.conversion_target_id = 0;
                unit.conversion_progress = 0;
            } else {
                unit.moving = false;
                unit.path.clear();
                unit.next_path_step = 0;
                ++unit.conversion_progress;
                // FUN_00413a80 checks once per active action update and draws
                // from the process-wide MSVCRT stream even while minimum time
                // forces failure. Faith and Teuton team effect 404 supply the
                // recovered target resource 77/178/179 contributions.
                float resistance =
                    commercial_conversion_class_resistance(target->kind);
                float minimum_time = 4.0F;
                float maximum_time = 10.0F;
                if (has_technology(target->owner, Technology::faith)) {
                    resistance += 3.0F;
                    minimum_time += 2.0F;
                    maximum_time += 4.0F;
                }
                if (team_has_civilization(
                        target->owner, Civilization::teutons
                    )) {
                    resistance += 2.0F;
                    minimum_time += 1.0F;
                    maximum_time += 2.0F;
                }
                const ConversionCheck check = evaluate_conversion_check(
                    consume_commercial_random(), resistance, 25,
                    static_cast<float>(unit.conversion_progress),
                    minimum_time, maximum_time
                );
                if (check.succeeds) {
                    const EntityId converted_target = target->id;
                    const Player new_owner = unit.owner;
                    const bool heresy =
                        has_technology(target->owner, Technology::heresy);
                    if (heresy) {
                        target->hit_points = 0;
                    } else {
                        target->owner = new_owner;
                    }
                    ++mutable_statistics(new_owner).conversions;
                    target->attack_target_id = 0;
                    target->attack_target_is_building = false;
                    target->attack_target_auto = false;
                    target->conversion_target_id = 0;
                    target->conversion_progress = 0;
                    target->moving = false;
                    target->path.clear();
                    target->next_path_step = 0;
                    target->stance_anchor = target->position;
                    unit.conversion_target_id = 0;
                    unit.conversion_progress = 0;
                    unit.conversion_cooldown =
                        has_technology(
                            unit.owner, Technology::illumination
                        ) ? 10 : 20;
                    if (!has_technology(
                            unit.owner, Technology::theocracy
                        )) {
                        for (Unit& participant : units_) {
                            if (participant.owner == unit.owner &&
                                participant.conversion_target_id ==
                                    converted_target) {
                                participant.conversion_target_id = 0;
                                participant.conversion_progress = 0;
                                participant.conversion_cooldown =
                                    unit.conversion_cooldown;
                            }
                        }
                    }
                    update_match_outcome();
                }
                continue;
            }
        }
        if (unit.healing_target_id != 0) {
            Unit* target = find_unit(unit.healing_target_id);
            const bool valid =
                (unit.kind == UnitKind::monk ||
                 unit.kind == UnitKind::missionary) &&
                target != nullptr &&
                target->owner == unit.owner &&
                is_organic(target->kind) &&
                target->garrisoned_in == 0 &&
                target->hit_points > 0 &&
                target->hit_points < maximum_hit_points(*target);
            const int distance_squared = valid
                ? combat_distance_squared(
                    unit.position, target->position
                )
                : 9999;
            const int healing_range =
                civilization(unit.owner) == Civilization::teutons ? 8 : 4;
            if (!valid ||
                distance_squared > healing_range * healing_range) {
                unit.healing_target_id = 0;
            } else {
                const int healing =
                    team_has_civilization(
                        unit.owner, Civilization::byzantines
                    ) && tick_number_ % 2 == 0
                        ? 2 : 1;
                target->hit_points = std::min(
                    maximum_hit_points(*target),
                    target->hit_points + healing
                );
                unit.moving = false;
                if (target->hit_points >= maximum_hit_points(*target)) {
                    unit.healing_target_id = 0;
                }
                continue;
            }
        }
        if (unit.attacking_ground) {
            const int distance_squared = combat_distance_squared(
                unit.position, unit.attack_ground_target
            );
            const int minimum_range =
                effective_minimum_attack_range(unit);
            if (distance_squared <=
                minimum_range * minimum_range) {
                unit.attacking_ground = false;
                unit.attack_ground_target = {-1, -1};
                unit.moving = false;
            } else if (
                distance_squared <=
                    effective_attack_range(unit) *
                    effective_attack_range(unit)
            ) {
                unit.moving = false;
                unit.path.clear();
                unit.next_path_step = 0;
                if (unit.attack_cooldown == 0) {
                    launch_ground_projectile(
                        unit,
                        unit.attack_ground_target
                    );
                    unit.attack_cooldown =
                        effective_attack_interval(unit);
                    unit.attacking_ground = false;
                    unit.attack_ground_target = {-1, -1};
                }
                continue;
            } else if (
                unit.destination != unit.attack_ground_target ||
                !unit.moving
            ) {
                if (!route_unit(unit, unit.attack_ground_target)) {
                    unit.attacking_ground = false;
                    unit.attack_ground_target = {-1, -1};
                    unit.moving = false;
                }
            }
        }
        if (unit.attack_moving && unit.attack_target_id == 0) {
            if (unit.position == unit.attack_move_destination) {
                if (unit.patrolling) {
                    unit.attack_move_destination =
                        unit.position == unit.patrol_destination
                            ? unit.patrol_origin
                            : unit.patrol_destination;
                    if (!route_unit(
                            unit,
                            unit.attack_move_destination
                        )) {
                        unit.attack_moving = false;
                        unit.patrolling = false;
                    }
                } else {
                    unit.attack_moving = false;
                    unit.attack_move_destination = {-1, -1};
                }
            } else if (
                (unit.stance == UnitStance::passive ||
                 !acquire_nearby_target(
                     unit,
                     unit.stance != UnitStance::stand_ground
                 )) &&
                       (unit.destination !=
                            unit.attack_move_destination ||
                        !unit.moving)) {
                if (!route_unit(unit, unit.attack_move_destination)) {
                    unit.attack_moving = false;
                    unit.patrolling = false;
                    unit.attack_move_destination = {-1, -1};
                }
            }
        }
        if (unit.guard_target_id != 0 &&
            unit.attack_target_id == 0) {
            Unit* guarded_unit = unit.guard_target_is_building
                ? nullptr
                : find_unit(unit.guard_target_id);
            Building* guarded_building = unit.guard_target_is_building
                ? find_building(unit.guard_target_id)
                : nullptr;
            const bool valid_unit =
                guarded_unit != nullptr &&
                guarded_unit->garrisoned_in == 0 &&
                guarded_unit->owner == unit.owner;
            const bool valid_building =
                guarded_building != nullptr &&
                guarded_building->owner == unit.owner &&
                guarded_building->completed();
            if (!valid_unit && !valid_building) {
                unit.guard_target_id = 0;
                unit.guard_target_is_building = false;
            } else if (
                unit.stance == UnitStance::passive ||
                !acquire_nearby_target(
                    unit,
                    unit.stance != UnitStance::stand_ground
                )
            ) {
                TilePosition target_position = valid_unit
                    ? guarded_unit->position
                    : guarded_building->position;
                if (unit.formation_group_id != 0) {
                    target_position.x +=
                        unit.formation_slot.x - unit.formation_anchor.x;
                    target_position.y +=
                        unit.formation_slot.y - unit.formation_anchor.y;
                }
                const int distance =
                    std::abs(target_position.x - unit.position.x) +
                    std::abs(target_position.y - unit.position.y);
                if (distance > 2 &&
                    (unit.destination != target_position ||
                     !unit.moving)) {
                    route_unit(unit, target_position);
                } else if (distance <= 2) {
                    unit.previous_position = unit.position;
                    unit.destination = unit.position;
                    unit.moving = false;
                    unit.path.clear();
                    unit.next_path_step = 0;
                }
            }
        }
        if (unit.repair_target_id != 0) {
            Building* repair_target =
                find_building(unit.repair_target_id);
            const bool valid_repair =
                (unit.kind == UnitKind::villager ||
                 commercial_has_ability(unit, CommercialTaskAbility::repair)) &&
                repair_target != nullptr &&
                repair_target->owner == unit.owner &&
                repair_target->completed() &&
                repair_target->hit_points > 0 &&
                repair_target->hit_points <
                    maximum_hit_points(*repair_target);
            if (!valid_repair) {
                unit.repair_target_id = 0;
                unit.repair_wood_remainder = 0;
                unit.repair_stone_remainder = 0;
            } else if (
                distance_to_building(unit.position, *repair_target) <= 1
            ) {
                const int repair_hit_points_per_tick = unit.commercial_identity
                    ? std::max(1, static_cast<int>(std::lround(
                          10.0f * effective_commercial_attribute(
                              unit.owner, *unit.commercial_identity, 13, 1.0f
                          )
                      )))
                    : 10;
                const BuildingRules& repair_rules =
                    rules_for(repair_target->kind);
                const int repair_maximum =
                    maximum_hit_points(*repair_target);
                const int healing = std::min(
                    repair_hit_points_per_tick,
                    repair_maximum - repair_target->hit_points
                );
                const int denominator = 2 * repair_maximum;
                const int repair_wood_cost =
                    repair_target->kind == BuildingKind::town_center
                        ? 1100
                        : repair_rules.wood_cost;
                const int repair_stone_cost =
                    repair_target->kind == BuildingKind::town_center
                        ? 0
                        : repair_rules.stone_cost;
                const int wood_total =
                    unit.repair_wood_remainder +
                    healing * repair_wood_cost;
                const int stone_total =
                    unit.repair_stone_remainder +
                    healing * repair_stone_cost;
                const int wood_due = wood_total / denominator;
                const int stone_due = stone_total / denominator;
                Economy& economy = unit.owner == Player::blue
                    ? player_states_[0].economy
                    : player_states_[1].economy;
                const bool has_repair_resources =
                    economy.wood >= wood_due &&
                    economy.stone >= stone_due &&
                    (repair_wood_cost == 0 || economy.wood > 0) &&
                    (repair_stone_cost == 0 || economy.stone > 0);
                if (!has_repair_resources) {
                    unit.moving = false;
                    continue;
                }
                economy.wood -= wood_due;
                economy.stone -= stone_due;
                unit.repair_wood_remainder = wood_total % denominator;
                unit.repair_stone_remainder = stone_total % denominator;
                repair_target->hit_points += healing;
                unit.moving = false;
                if (repair_target->hit_points == repair_maximum) {
                    unit.repair_target_id = 0;
                    unit.repair_wood_remainder = 0;
                    unit.repair_stone_remainder = 0;
                }
                continue;
            }
        }
        if (unit.attack_target_id != 0) {
            Unit* ordered_unit = unit.attack_target_is_building
                ? nullptr
                : find_unit(unit.attack_target_id);
            Building* ordered_building = unit.attack_target_is_building
                ? find_building(unit.attack_target_id)
                : nullptr;
            const bool valid_unit =
                ordered_unit != nullptr &&
                ordered_unit->garrisoned_in == 0 &&
                !is_ram(unit.kind) &&
                is_enemy(ordered_unit->owner, unit.owner) &&
                ordered_unit->hit_points > 0 &&
                (unit.kind == UnitKind::boar ||
                 is_unit_visible(unit.owner, *ordered_unit));
            const bool valid_building =
                ordered_building != nullptr &&
                is_enemy(ordered_building->owner, unit.owner) &&
                ordered_building->hit_points > 0 &&
                (!unit.attack_target_auto ||
                 is_building_visible(unit.owner, *ordered_building));
            if (!valid_unit && !valid_building) {
                const bool return_to_anchor =
                    unit.attack_target_auto &&
                    unit.stance == UnitStance::defensive &&
                    !unit.attack_moving &&
                    unit.guard_target_id == 0;
                unit.attack_target_id = 0;
                unit.attack_target_is_building = false;
                unit.attack_target_auto = false;
                unit.moving = return_to_anchor &&
                    route_unit(unit, unit.stance_anchor);
                unit.returning_to_stance = unit.moving;
            } else {
                const TilePosition target_position = valid_unit
                    ? ordered_unit->position
                    : nearest_point_on_building(
                        unit.position,
                        *ordered_building
                    );
                const int distance_squared = valid_unit
                    ? combat_distance_squared(
                        unit.position, target_position
                    )
                    : combat_distance_squared(
                        unit.position, *ordered_building
                    );
                const int minimum_range =
                    effective_minimum_attack_range(unit);
                const int minimum_range_squared =
                    minimum_range * minimum_range;
                const int attack_range =
                    effective_attack_range(unit);
                const int attack_range_squared =
                    attack_range * attack_range;
                const int anchor_distance =
                    std::abs(
                        target_position.x - unit.stance_anchor.x
                    ) +
                    std::abs(
                        target_position.y - unit.stance_anchor.y
                    );
                if (unit.attack_target_auto &&
                    unit.stance == UnitStance::defensive &&
                    !unit.attack_moving &&
                    unit.guard_target_id == 0 &&
                    anchor_distance > 6) {
                    unit.attack_target_id = 0;
                    unit.attack_target_is_building = false;
                    unit.attack_target_auto = false;
                    unit.moving =
                        route_unit(unit, unit.stance_anchor);
                    unit.returning_to_stance = unit.moving;
                    continue;
                }
                if (unit.attack_target_auto &&
                    unit.stance == UnitStance::stand_ground &&
                    distance_squared > attack_range_squared) {
                    unit.attack_target_id = 0;
                    unit.attack_target_is_building = false;
                    unit.attack_target_auto = false;
                    unit.moving = false;
                    continue;
                }
                if (distance_squared > minimum_range_squared &&
                    distance_squared <= attack_range_squared) {
                    if (unit.attack_cooldown == 0) {
                        if (valid_unit) {
                            if (effective_attack_range(unit) > 1) {
                                launch_projectile(unit, *ordered_unit);
                            } else {
                                perform_attack(unit, *ordered_unit);
                            }
                        } else if (effective_attack_range(unit) > 1) {
                            launch_projectile(unit, *ordered_building);
                        } else {
                            perform_attack(unit, *ordered_building);
                        }
                        unit.attack_cooldown =
                            effective_attack_interval(unit);
                    }
                    unit.moving =
                        valid_unit
                            ? ordered_unit->hit_points > 0
                            : ordered_building->hit_points > 0;
                    continue;
                }
                if (distance_squared <= minimum_range_squared) {
                    unit.moving = false;
                    continue;
                }
                if (unit.destination != target_position || !unit.moving) {
                    if (!route_unit(unit, target_position)) {
                        unit.attack_target_id = 0;
                        unit.attack_target_is_building = false;
                        unit.attack_target_auto = false;
                        unit.moving = false;
                    }
                }
            }
        }
        if (!unit.moving &&
            unit.attack_target_id == 0 &&
            unit.repair_target_id == 0 &&
            !unit.has_resource_target &&
            unit.garrison_target_id == 0 &&
            !unit.attack_moving &&
            unit.guard_target_id == 0 &&
            !unit.formation_waypoints.empty()) {
            const std::uint64_t next_group =
                unit.formation_waypoints.front().group_id;
            const auto is_member = [next_group](const Unit& member) {
                return !member.formation_waypoints.empty() &&
                    member.formation_waypoints.front().group_id ==
                        next_group;
            };
            const bool leader = std::ranges::none_of(
                units_, [next_group, id = unit.id](const Unit& member) {
                    return !member.formation_waypoints.empty() &&
                        member.formation_waypoints.front().group_id ==
                            next_group &&
                        member.id < id;
                }
            );
            const bool group_ready = std::ranges::all_of(
                units_, [&is_member](const Unit& member) {
                    if (!is_member(member)) return true;
                    return !member.moving &&
                        member.attack_target_id == 0 &&
                        member.repair_target_id == 0 &&
                        !member.has_resource_target &&
                        member.garrison_target_id == 0 &&
                        !member.attack_moving &&
                        member.guard_target_id == 0;
                }
            );
            if (leader && group_ready) {
                FormationKind next_kind{FormationKind::compact};
                Player owner{Player::neutral};
                for (Unit& member : units_) {
                    if (!is_member(member)) continue;
                    const FormationWaypoint leg =
                        member.formation_waypoints.front();
                    member.formation_waypoints.erase(
                        member.formation_waypoints.begin()
                    );
                    member.formation_group_id = leg.group_id;
                    member.formation_move_interval = leg.move_interval;
                    member.formation_speed_numerator =
                        leg.speed_numerator;
                    member.formation_anchor = leg.anchor;
                    member.formation_slot = leg.slot;
                    member.movement_speed_remainder = 0;
                    route_unit(member, leg.destination);
                    next_kind = leg.kind;
                    owner = member.owner;
                }
                set_formation_kind(owner, next_kind);
            }
        }
        if (!unit.moving &&
            unit.attack_target_id == 0 &&
            unit.repair_target_id == 0 &&
            !unit.has_resource_target &&
            unit.garrison_target_id == 0 &&
            !unit.attack_moving &&
            unit.guard_target_id == 0 &&
            unit.formation_waypoints.empty() &&
            !unit.waypoints.empty()) {
            const TilePosition next_waypoint = unit.waypoints.front();
            unit.waypoints.erase(unit.waypoints.begin());
            route_unit(unit, next_waypoint);
        }
        if (unit.returning_to_stance &&
            unit.position == unit.stance_anchor) {
            unit.returning_to_stance = false;
        }
        if (!unit.moving &&
            unit.kind != UnitKind::villager &&
            unit.stance != UnitStance::passive) {
            if (unit.stance == UnitStance::defensive) {
                unit.stance_anchor = unit.position;
            }
            acquire_nearby_target(
                unit,
                unit.stance != UnitStance::stand_ground
            );
        }
        const Unit* ordered_animal = unit.resource_unit_id == 0
            ? nullptr : find_unit(unit.resource_unit_id);
        const bool actively_hunting =
            ordered_animal != nullptr &&
            is_huntable(ordered_animal->kind) &&
            ordered_animal->hit_points > 0;
        if (unit.has_resource_target && !actively_hunting) {
            if (unit.returning_resource) {
                Building* drop_off = building_at(unit.destination);
                if (drop_off == nullptr ||
                    drop_off->owner != unit.owner ||
                    !drop_off->completed() ||
                    !accepts_resource(
                        drop_off->kind,
                        unit.carried_resource
                    )) {
                    drop_off = nearest_drop_off(unit);
                    if (drop_off == nullptr) {
                        unit.moving = false;
                    } else {
                        route_unit(unit, drop_off->position);
                    }
                    continue;
                }
                const int distance = drop_off == nullptr
                    ? 9999
                    : distance_to_building(unit.position, *drop_off);
                if (drop_off != nullptr &&
                    drop_off->owner == unit.owner &&
                    drop_off->completed() &&
                    accepts_resource(
                        drop_off->kind,
                        unit.carried_resource
                    ) &&
                    distance <= 1) {
                    Economy& economy = unit.owner == Player::blue
                        ? player_states_[0].economy
                        : player_states_[1].economy;
                    switch (unit.carried_resource) {
                        case ResourceKind::wood:
                            economy.wood += unit.carried_amount;
                            break;
                        case ResourceKind::food:
                            economy.food += unit.carried_amount;
                            break;
                        case ResourceKind::gold:
                            economy.gold += unit.carried_amount;
                            break;
                        case ResourceKind::stone:
                            economy.stone += unit.carried_amount;
                            break;
                        case ResourceKind::none:
                            break;
                    }
                    const ResourceKind completed_resource =
                        unit.carried_resource;
                    unit.carried_amount = 0;
                    unit.returning_resource = false;
                    if (work_resource_amount(unit) > 0) {
                        if (unit.resource_building_id != 0) {
                            route_unit(unit, unit.resource_target);
                        } else if (unit.resource_unit_id != 0) {
                            route_unit(unit, unit.resource_target);
                        } else {
                            route_to_resource_interaction(
                                unit, unit.resource_target
                            );
                        }
                    } else if (
                        unit.resource_building_id == 0 &&
                        unit.resource_unit_id == 0 &&
                        route_to_nearest_resource(
                            unit,
                            completed_resource
                        )
                    ) {
                        unit.has_resource_target = true;
                    } else {
                        unit.has_resource_target = false;
                        unit.carried_resource = ResourceKind::none;
                        unit.moving = false;
                    }
                    continue;
                }
                if (drop_off != nullptr && distance > 1 &&
                    !unit.moving) {
                    route_unit(unit, drop_off->position);
                }
            } else {
                if (unit.resource_building_id == 0 &&
                    unit.resource_unit_id == 0 &&
                    work_resource_amount(unit) == 0) {
                    gather(unit);
                    continue;
                }
                const int distance = std::max(
                    std::abs(
                        unit.resource_target.x - unit.position.x
                    ),
                    std::abs(
                        unit.resource_target.y - unit.position.y
                    )
                );
                if (distance <= 2) {
                    // Resource sprites and nearby units occupy a wider
                    // visual/collision envelope than one logical tile.
                    // Workers inside that envelope must be able to work
                    // without claiming an already occupied ring tile.
                    unit.path.clear();
                    unit.next_path_step = 0;
                    unit.moving = false;
                    gather(unit);
                    continue;
                }
                if (!unit.moving) {
                    if (unit.resource_building_id != 0 ||
                        unit.resource_unit_id != 0) {
                        route_unit(unit, unit.resource_target);
                    } else {
                        route_to_resource_interaction(
                            unit, unit.resource_target
                        );
                    }
                }
            }
        }

        if (!unit.moving) {
            continue;
        }

        Unit* target = unit_at(unit.destination);
        if (target != nullptr && is_enemy(target->owner, unit.owner)) {
            const int distance =
                std::abs(target->position.x - unit.position.x) +
                std::abs(target->position.y - unit.position.y);
            if (distance <= effective_attack_range(unit)) {
                if (unit.attack_cooldown == 0) {
                    if (effective_attack_range(unit) > 1) {
                        launch_projectile(unit, *target);
                    } else {
                        perform_attack(unit, *target);
                    }
                    unit.attack_cooldown =
                        effective_attack_interval(unit);
                }
                unit.moving = target->hit_points > 0;
                continue;
            }
        }
        Building* building_target = building_at(unit.destination);
        if (building_target != nullptr &&
            is_enemy(building_target->owner, unit.owner)) {
            const int distance =
                distance_to_building(unit.position, *building_target);
            if (distance <= effective_attack_range(unit)) {
                if (unit.attack_cooldown == 0) {
                    if (effective_attack_range(unit) > 1) {
                        launch_projectile(unit, *building_target);
                    } else {
                        perform_attack(unit, *building_target);
                    }
                    unit.attack_cooldown =
                        effective_attack_interval(unit);
                }
                unit.moving = building_target->hit_points > 0;
                continue;
            }
        }

        if (unit.formation_group_id != 0 && unit.moving) {
            const std::size_t remaining =
                unit.path.size() - std::min(
                    unit.next_path_step, unit.path.size()
                );
            std::size_t group_max_remaining = remaining;
            for (const Unit& member : units_) {
                if (member.id == unit.id ||
                    member.formation_group_id !=
                        unit.formation_group_id) continue;
                const std::size_t member_remaining = member.moving
                    ? member.path.size() - std::min(
                        member.next_path_step, member.path.size()
                      )
                    : static_cast<std::size_t>(
                        std::abs(
                            member.position.x - member.formation_slot.x
                        ) +
                        std::abs(
                            member.position.y - member.formation_slot.y
                        )
                      );
                group_max_remaining = std::max(
                    group_max_remaining,
                    member_remaining
                );
            }
            // Bounded regroup contract: leaders pause when over four path
            // steps ahead of slowest remaining member.
            if (group_max_remaining > remaining + 4) continue;
        }

        if (unit.movement_cooldown > 0) {
            --unit.movement_cooldown;
            continue;
        }
        const int movement_speed_remainder_before =
            unit.movement_speed_remainder;
        constexpr int cavalry_movement_denominator = 320;
        const bool in_formation = unit.formation_move_interval > 0;
        if (in_formation) {
            unit.movement_speed_remainder +=
                unit.formation_speed_numerator;
            if (unit.movement_speed_remainder < 32000) continue;
            unit.movement_speed_remainder -= 32000;
        }
        const bool fixed_point_cavalry = !in_formation && (
            unit.kind == UnitKind::scout_cavalry ||
            unit.kind == UnitKind::knight ||
            unit.kind == UnitKind::cavalier ||
            unit.kind == UnitKind::paladin ||
            unit.kind == UnitKind::light_cavalry ||
            unit.kind == UnitKind::hussar ||
            unit.kind == UnitKind::camel_rider ||
            unit.kind == UnitKind::heavy_camel);
        const bool fixed_point_ship = !in_formation && is_ship(unit.kind) &&
            ship_movement_numerator(unit) != 100;
        const bool fixed_point_unique = !in_formation &&
            unique_unit_movement_numerator(unit) != 100;
        if (fixed_point_cavalry) {
            unit.movement_speed_remainder +=
                cavalry_movement_numerator(unit);
            if (unit.movement_speed_remainder <
                cavalry_movement_denominator) {
                continue;
            }
            unit.movement_speed_remainder -=
                cavalry_movement_denominator;
        }
        if (fixed_point_ship) {
            unit.movement_speed_remainder +=
                ship_movement_numerator(unit);
            if (unit.movement_speed_remainder < 100) {
                continue;
            }
            unit.movement_speed_remainder -= 100;
        }
        if (fixed_point_unique) {
            unit.movement_speed_remainder +=
                unique_unit_movement_numerator(unit);
            if (unit.movement_speed_remainder < 100) {
                continue;
            }
            unit.movement_speed_remainder -= 100;
        }
        if (unit.next_path_step >= unit.path.size()) {
            unit.moving = false;
            continue;
        }
        const TilePosition next = unit.path[unit.next_path_step];

        const bool next_walkable = is_ship(unit.kind)
            ? map_.sailable(next) : map_.traversable(unit.position, next);
        if (next_walkable &&
            !occupied(next, unit.id, unit.owner)) {
            unit.previous_position = unit.position;
            unit.position = next;
            unit.last_move_tick = tick_number_;
            ++unit.next_path_step;
            int movement_interval =
                effective_movement_interval(unit);
            if (is_ram(unit.kind) ||
                unit.kind == UnitKind::mangonel ||
                unit.kind == UnitKind::onager ||
                unit.kind == UnitKind::siege_onager ||
                unit.kind == UnitKind::scorpion ||
                unit.kind == UnitKind::heavy_scorpion ||
                unit.kind == UnitKind::packed_trebuchet) {
                movement_interval = 1;
            }
            if (in_formation) {
                movement_interval = 1;
            }
            if (fixed_point_cavalry &&
                unit.movement_speed_remainder >=
                    cavalry_movement_denominator &&
                unit.next_path_step < unit.path.size()) {
                const TilePosition bonus =
                    unit.path[unit.next_path_step];
                const bool bonus_walkable = is_ship(unit.kind)
                    ? map_.sailable(bonus) : map_.traversable(unit.position, bonus);
                if (bonus_walkable &&
                    !occupied(bonus, unit.id, unit.owner)) {
                    unit.movement_speed_remainder -=
                        cavalry_movement_denominator;
                    unit.previous_position = unit.position;
                    unit.position = bonus;
                    unit.last_move_tick = tick_number_;
                    ++unit.next_path_step;
                }
            }
            if (fixed_point_ship) {
                if (unit.movement_speed_remainder >= 100 &&
                    unit.next_path_step < unit.path.size()) {
                    const TilePosition bonus =
                        unit.path[unit.next_path_step];
                    const bool bonus_walkable = map_.sailable(bonus);
                    if (bonus_walkable &&
                        !occupied(bonus, unit.id, unit.owner)) {
                        unit.movement_speed_remainder -= 100;
                        unit.previous_position = unit.position;
                        unit.position = bonus;
                        unit.last_move_tick = tick_number_;
                        ++unit.next_path_step;
                    }
                }
            }
            if (fixed_point_unique &&
                unit.movement_speed_remainder >= 100 &&
                unit.next_path_step < unit.path.size()) {
                const TilePosition bonus =
                    unit.path[unit.next_path_step];
                if (map_.traversable(unit.position, bonus) &&
                    !occupied(bonus, unit.id, unit.owner)) {
                    unit.movement_speed_remainder -= 100;
                    unit.previous_position = unit.position;
                    unit.position = bonus;
                    unit.last_move_tick = tick_number_;
                    ++unit.next_path_step;
                }
            }
            unit.movement_cooldown = movement_interval - 1;
            unit.moving = unit.next_path_step < unit.path.size();
        } else {
            if (in_formation || fixed_point_cavalry ||
                fixed_point_ship || fixed_point_unique) {
                unit.movement_speed_remainder =
                    movement_speed_remainder_before;
            }
            if (unit.has_resource_target) {
                unit.path.clear();
                unit.next_path_step = 0;
                unit.moving = false;
            } else {
                route_unit(unit, unit.destination);
            }
        }
    }

    for (Unit& unit : units_) {
        refresh_unit_render_subtile(unit);
    }

    if (gather_trace_enabled() && tick_number_ % 5 == 0) {
        for (const Unit& unit : units_) {
            if (unit.kind != UnitKind::villager ||
                (!unit.has_resource_target && unit.carried_amount == 0)) {
                continue;
            }
            std::cerr << "GATHER_STATE tick=" << tick_number_
                      << " id=" << unit.id
                      << " position=" << unit.position.x << ','
                      << unit.position.y
                      << " moving=" << unit.moving
                      << " active=" << unit.has_resource_target
                      << " returning=" << unit.returning_resource
                      << " carried_kind="
                      << static_cast<int>(unit.carried_resource)
                      << " carried=" << unit.carried_amount
                      << " target=" << unit.resource_target.x << ','
                      << unit.resource_target.y
                      << " target_amount=" << work_resource_amount(unit)
                      << " destination=" << unit.destination.x << ','
                      << unit.destination.y
                      << " path_step=" << unit.next_path_step << '/'
                      << unit.path.size() << '\n';
        }
    }

    std::vector<std::uint64_t> formation_groups;
    for (const Unit& unit : units_) {
        if (unit.formation_group_id != 0 &&
            std::ranges::find(
                formation_groups, unit.formation_group_id
            ) == formation_groups.end()) {
            formation_groups.push_back(unit.formation_group_id);
        }
    }
    for (std::uint64_t group : formation_groups) {
        const bool active = std::ranges::any_of(
            units_, [group](const Unit& unit) {
                return unit.formation_group_id == group &&
                    (unit.moving || !unit.waypoints.empty() ||
                     !unit.formation_waypoints.empty() ||
                     unit.attack_moving || unit.guard_target_id != 0 ||
                     unit.attack_target_id != 0);
            }
        );
        if (active) continue;
        for (Unit& unit : units_) {
            if (unit.formation_group_id != group) continue;
            unit.formation_move_interval = 0;
            unit.formation_speed_numerator = 0;
            unit.formation_group_id = 0;
            unit.formation_anchor = {-1, -1};
            unit.formation_slot = {-1, -1};
            unit.movement_speed_remainder = 0;
        }
    }

    for (Unit& unit : units_) {
        if ((unit.kind != UnitKind::trade_cart &&
             unit.kind != UnitKind::trade_cog) ||
            unit.trade_home_market_id == 0 ||
            unit.trade_target_market_id == 0) {
            continue;
        }
        Building* home = find_building(unit.trade_home_market_id);
        Building* target = find_building(unit.trade_target_market_id);
        const BuildingKind route_building =
            unit.kind == UnitKind::trade_cog
                ? BuildingKind::dock : BuildingKind::market;
        if (home == nullptr || target == nullptr || !home->completed() ||
            !target->completed() || home->hit_points <= 0 ||
            target->hit_points <= 0 || home->owner != unit.owner ||
            home->kind != route_building ||
            target->kind != route_building ||
            !is_ally(unit.owner, target->owner) ||
            target->owner == unit.owner) {
            unit.trade_home_market_id = 0;
            unit.trade_target_market_id = 0;
            unit.trade_returning = false;
            unit.trade_waiting = false;
            unit.trade_work_ticks_remaining = 0;
            unit.moving = false;
            unit.path.clear();
            unit.next_path_step = 0;
            unit.destination = unit.position;
            continue;
        }
        if (unit.moving) {
            continue;
        }
        Building& destination =
            unit.trade_returning ? *home : *target;
        const auto route_position =
            [this, &unit](const Building& building)
                -> std::optional<TilePosition> {
                if (unit.kind != UnitKind::trade_cog) {
                    return spawn_position(building);
                }
                const BuildingRules& rules = rules_for(building.kind);
                for (int y = building.position.y - 1;
                     y <= building.position.y + rules.footprint_height;
                     ++y) {
                    for (int x = building.position.x - 1;
                         x <= building.position.x + rules.footprint_width;
                         ++x) {
                        const TilePosition tile{x, y};
                        if (map_.sailable(tile) &&
                            !occupied(tile, unit.id)) {
                            return tile;
                        }
                    }
                }
                return std::nullopt;
            };
        if (distance_to_building(unit.position, destination) > 1) {
            const auto next = route_position(destination);
            if (next) {
                route_unit(unit, *next);
            }
            continue;
        }
        if (!unit.trade_waiting) {
            unit.trade_waiting = true;
            unit.trade_work_ticks_remaining =
                has_technology(unit.owner, Technology::caravan) ? 2 : 3;
            continue;
        }
        if (unit.trade_work_ticks_remaining > 0) {
            --unit.trade_work_ticks_remaining;
            continue;
        }
        unit.trade_waiting = false;
        if (unit.trade_returning) {
            const int distance =
                std::abs(home->position.x - target->position.x) +
                std::abs(home->position.y - target->position.y);
            Economy& economy = unit.owner == Player::blue
                ? player_states_[0].economy : player_states_[1].economy;
            const int base_gold = std::max(1, distance * 2);
            economy.gold +=
                team_has_civilization(
                    unit.owner, Civilization::spanish
                ) ? base_gold * 133 / 100 : base_gold;
        }
        unit.trade_returning = !unit.trade_returning;
        const Building& next_market =
            unit.trade_returning ? *home : *target;
        const auto next = route_position(next_market);
        if (next) {
            route_unit(unit, *next);
        }
    }

    update_building_defenses();
    update_projectiles();

    // Destroyed shelters must release occupants before dead-unit cleanup. If
    // every exit tile is blocked, ungarrison_at kills the trapped occupant so
    // a removed building cannot leave an unreachable entity in the match.
    for (const Building& building : buildings_) {
        if (building.hit_points <= 0) {
            ungarrison_at(building.id);
        }
    }

    for (const Unit& transport : units_) {
        if (transport.kind == UnitKind::transport_ship &&
            transport.hit_points <= 0) {
            for (Unit& passenger : units_) {
                if (passenger.garrisoned_in == transport.id) {
                    passenger.hit_points = 0;
                }
            }
        }
    }
    for (Unit& unit : units_) {
        if (!is_animal(unit.kind) || unit.hit_points > 0) {
            continue;
        }
        // Animal carcasses remain finite food resources, but no longer act
        // as live mobile units after lethal damage.
        unit.hit_points = 0;
        unit.previous_position = unit.position;
        unit.destination = unit.position;
        unit.moving = false;
        unit.path.clear();
        unit.next_path_step = 0;
        unit.waypoints.clear();
        unit.formation_waypoints.clear();
        unit.attack_target_id = 0;
        unit.attack_target_is_building = false;
        unit.attack_target_auto = false;
        unit.attack_moving = false;
        unit.patrolling = false;
        unit.guard_target_id = 0;
        unit.returning_to_stance = false;
    }
    for (const Unit& unit : units_) {
        if (unit.hit_points <= 0 && unit.garrisoned_in == 0 &&
            (!is_animal(unit.kind) || unit.food_remaining <= 0)) {
            if (unit.owner != Player::neutral &&
                !is_animal(unit.kind) && !is_relic(unit.kind)) {
                ++mutable_statistics(unit.owner).units_lost;
                if (unit.last_damage_owner &&
                    !unit.last_damage_owner->is_neutral() &&
                    *unit.last_damage_owner != unit.owner) {
                    ++mutable_statistics(
                        *unit.last_damage_owner
                    ).units_killed;
                }
            }
            constexpr int death_effect_ticks = 18;
            death_effects_.push_back({
                unit.position,
                unit.kind,
                unit.owner,
                death_effect_ticks,
                death_effect_ticks,
                unit.id,
                unit.previous_position,
            });
        }
    }
    for (const Building& building : buildings_) {
        if (building.hit_points <= 0) {
            if (building.owner != Player::neutral) {
                ++mutable_statistics(building.owner).buildings_lost;
                if (building.last_damage_owner &&
                    !building.last_damage_owner->is_neutral() &&
                    *building.last_damage_owner != building.owner) {
                    ++mutable_statistics(
                        *building.last_damage_owner
                    ).buildings_razed;
                }
            }
            constexpr int rubble_effect_ticks = 30;
            rubble_effects_.push_back({
                building.position,
                building.kind,
                building.owner,
                rubble_effect_ticks,
                rubble_effect_ticks,
                building.id,
            });
        }
    }

    const std::size_t unit_count_before_cleanup = units_.size();
    units_.erase(
        std::remove_if(
            units_.begin(),
            units_.end(),
            [](const Unit& unit) {
                return unit.hit_points <= 0 &&
                    (!is_animal(unit.kind) ||
                     unit.food_remaining <= 0);
            }
        ),
        units_.end()
    );
    if (units_.size() != unit_count_before_cleanup) {
        prune_unit_render_elevations();
        prune_attack_reveals();
    }
    buildings_.erase(
        std::remove_if(
            buildings_.begin(),
            buildings_.end(),
            [](const Building& building) {
                return building.hit_points <= 0;
            }
        ),
        buildings_.end()
    );

    if (selected_unit_ && find_unit(*selected_unit_) == nullptr) {
        selected_unit_.reset();
    }
    std::erase_if(selected_units_, [this](EntityId id) {
        return find_unit(id) == nullptr;
    });
    if (!selected_unit_ && !selected_units_.empty()) {
        selected_unit_ = selected_units_.front();
    }
    if (selected_building_ &&
        find_building(*selected_building_) == nullptr) {
        selected_building_.reset();
    }
    update_production();
    update_match_outcome();
    update_exploration();
    if (tick_number_ % 100 == 0) sample_match_statistics();
}

void Simulation::reveal_attacker_to(
    const Unit& attacker,
    EntityOwner victim,
    int minimum_duration_ticks
) {
    if (attacker.hit_points <= 0 || attacker.garrisoned_in != 0 ||
        victim.is_neutral() || !is_enemy(victim, attacker.owner)) return;
    const auto slot = entity_owner_slot(victim);
    if (!slot || slot->is_neutral() || !roster_.slot(*slot).occupied) return;
    const std::uint64_t duration = static_cast<std::uint64_t>(std::max(
        {1, effective_attack_interval(attacker), minimum_duration_ticks}
    ));
    auto& expiry = player_states_.at(*slot->index())
        .attack_reveal_expiries[attacker.id];
    expiry = std::max(expiry, tick_number_ + duration);
}

void Simulation::reveal_attacker_to_ground_victims(
    const Unit& attacker,
    TilePosition center,
    int radius,
    int minimum_duration_ticks
) {
    const int bounded_radius = std::max(0, radius);
    for (const Unit& victim : units_) {
        if (victim.id == attacker.id || victim.hit_points <= 0 ||
            victim.garrisoned_in != 0 ||
            !is_enemy(victim.owner, attacker.owner)) continue;
        const int dx = victim.position.x - center.x;
        const int dy = victim.position.y - center.y;
        if (dx * dx + dy * dy <= bounded_radius * bounded_radius) {
            reveal_attacker_to(attacker, victim.owner, minimum_duration_ticks);
        }
    }
    for (const Building& victim : buildings_) {
        if (victim.hit_points <= 0 ||
            !is_enemy(victim.owner, attacker.owner) ||
            distance_to_building(center, victim) > bounded_radius) continue;
        reveal_attacker_to(attacker, victim.owner, minimum_duration_ticks);
    }
}

void Simulation::prune_attack_reveals() {
    for (PlayerState& state : player_states_) {
        std::erase_if(
            state.attack_reveal_expiries,
            [this](const auto& entry) {
                return entry.second <= tick_number_ ||
                    find_unit(entry.first) == nullptr;
            }
        );
    }
}

void Simulation::refresh_unit_render_subtile(Unit& unit) {
    constexpr int render_scale = 320;
    unit.render_current_subtile = {
        unit.position.x * render_scale,
        unit.position.y * render_scale,
    };
    unit_render_elevations_.at(unit.id).current = unit.position;

    const int movement_interval =
        effective_movement_interval(unit);
    const bool ordinary_paced_movement =
        unit.moving &&
        unit.formation_move_interval == 0 &&
        movement_interval > 1 &&
        unit.previous_position != unit.position &&
        unit.movement_cooldown > 0;
    if (ordinary_paced_movement) {
        // Original RGE moving objects retain floating-point coordinates and
        // are serviced from the timeGetTime()-driven game loop. Preserve our
        // integer authoritative tiles, but spread a paced logical step across
        // its full presentation interval.
        const int elapsed_ticks = std::clamp(
            movement_interval - unit.movement_cooldown,
            1,
            movement_interval
        );
        unit.render_current_subtile = {
            unit.previous_position.x * render_scale +
                (unit.position.x - unit.previous_position.x) *
                    elapsed_ticks * render_scale / movement_interval,
            unit.previous_position.y * render_scale +
                (unit.position.y - unit.previous_position.y) *
                    elapsed_ticks * render_scale / movement_interval,
        };
        return;
    }

    if (!unit.moving || unit.next_path_step >= unit.path.size()) {
        return;
    }

    int denominator = 0;
    if (unit.formation_move_interval > 0) {
        denominator = 32000;
    } else if (
        unit.kind == UnitKind::scout_cavalry ||
        unit.kind == UnitKind::knight ||
        unit.kind == UnitKind::cavalier ||
        unit.kind == UnitKind::paladin ||
        unit.kind == UnitKind::light_cavalry ||
        unit.kind == UnitKind::hussar ||
        unit.kind == UnitKind::camel_rider ||
        unit.kind == UnitKind::heavy_camel
    ) {
        denominator = 320;
    } else if (
        is_ship(unit.kind) && ship_movement_numerator(unit) != 100
    ) {
        denominator = 100;
    } else if (unique_unit_movement_numerator(unit) != 100) {
        denominator = 100;
    }
    if (denominator == 0) {
        return;
    }

    const int remainder = std::clamp(
        unit.movement_speed_remainder, 0, denominator - 1
    );
    const TilePosition next = unit.path[unit.next_path_step];
    unit.render_current_subtile.x +=
        (next.x - unit.position.x) * remainder * render_scale /
        denominator;
    unit.render_current_subtile.y +=
        (next.y - unit.position.y) * remainder * render_scale /
        denominator;
}

TilePosition Simulation::render_previous_elevation_position(
    const Unit& unit
) const {
    const auto found = unit_render_elevations_.find(unit.id);
    return found == unit_render_elevations_.end()
        ? unit.position : found->second.previous;
}

TilePosition Simulation::render_current_elevation_position(
    const Unit& unit
) const {
    const auto found = unit_render_elevations_.find(unit.id);
    return found == unit_render_elevations_.end()
        ? unit.position : found->second.current;
}

void Simulation::initialize_unit_render_elevation(const Unit& unit) {
    unit_render_elevations_.insert_or_assign(
        unit.id,
        RenderElevationPositions{unit.position, unit.position}
    );
}

void Simulation::prune_unit_render_elevations() {
    std::erase_if(
        unit_render_elevations_,
        [this](const auto& entry) {
            return std::ranges::none_of(
                units_,
                [&entry](const Unit& unit) {
                    return unit.id == entry.first;
                }
            );
        }
    );
}

void Simulation::replace_state(
    std::vector<Unit> units,
    std::vector<Building> buildings,
    Economy blue,
    Economy red,
    std::uint64_t tick_number
) {
    units_ = std::move(units);
    unit_render_elevations_.clear();
    for (const Unit& unit : units_) {
        initialize_unit_render_elevation(unit);
    }
    buildings_ = std::move(buildings);
    player_states_[0].economy = blue;
    player_states_[1].economy = red;
    tick_number_ = tick_number;
    selected_unit_.reset();
    selected_units_.clear();
    selected_building_.reset();
    outcome_ = MatchOutcome::ongoing;
    next_id_ = 1;
    next_formation_group_id_ = 1;
    for (const Unit& unit : units_) {
        next_id_ = std::max(next_id_, unit.id + 1);
        next_formation_group_id_ = std::max(
            next_formation_group_id_, unit.formation_group_id + 1
        );
        for (const FormationWaypoint& waypoint :
             unit.formation_waypoints) {
            next_formation_group_id_ = std::max(
                next_formation_group_id_, waypoint.group_id + 1
            );
        }
    }
    for (const Building& building : buildings_) {
        next_id_ = std::max(next_id_, building.id + 1);
    }
    for (Unit& unit : units_) {
        if (unit.moving) {
            const int movement_cooldown = unit.movement_cooldown;
            route_unit(unit, unit.destination);
            unit.movement_cooldown = movement_cooldown;
        }
    }
    update_match_outcome();
    update_exploration();
}

void Simulation::merge_exploration(
    Player player,
    const std::vector<TilePosition>& explored
) {
    auto& destination =
        player == Player::blue ? player_states_[0].explored : player_states_[1].explored;
    for (TilePosition position : explored) {
        if (!map_.contains(position)) {
            throw std::invalid_argument("explored tile outside map");
        }
        destination.at(map_index(position)) = true;
    }
}

void Simulation::replace_projectiles(
    std::vector<Projectile> projectiles
) {
    projectiles_ = std::move(projectiles);
}

void Simulation::replace_impact_effects(
    std::vector<ImpactEffect> effects
) {
    impact_effects_ = std::move(effects);
}

void Simulation::replace_death_effects(
    std::vector<UnitDeathEffect> effects
) {
    death_effects_ = std::move(effects);
}

void Simulation::replace_rubble_effects(
    std::vector<BuildingRubbleEffect> effects
) {
    rubble_effects_ = std::move(effects);
}

void Simulation::replace_ages(Age blue, Age red) {
    player_states_[0].age = blue;
    player_states_[1].age = red;
    refresh_unit_attacks(Player::blue);
    refresh_unit_attacks(Player::red);
}

void Simulation::replace_technologies(
    Player player,
    const std::vector<Technology>& technologies
) {
    auto& destination = player == Player::blue
        ? player_states_[0].technologies
        : player_states_[1].technologies;
    destination.fill(false);
    for (Technology technology : technologies) {
        destination.at(static_cast<std::size_t>(technology)) = true;
    }
    if (has_technology(player, Technology::man_at_arms)) {
        apply_man_at_arms_upgrade(player);
    }
    if (has_technology(player, Technology::crossbowman)) {
        apply_crossbowman_upgrade(player);
    }
    if (has_technology(player, Technology::pikeman)) {
        apply_pikeman_upgrade(player);
    }
    if (has_technology(player, Technology::long_swordsman)) {
        apply_long_swordsman_upgrade(player);
    }
    if (has_technology(player, Technology::fortified_wall)) {
        apply_fortified_wall_upgrade(player);
    }
    if (has_technology(player, Technology::guard_tower)) {
        apply_guard_tower_upgrade(player);
    }
    if (has_technology(player, Technology::keep)) {
        apply_keep_upgrade(player);
    }
    refresh_unit_attacks(player);
}

void Simulation::replace_market_prices(int food, int wood, int stone) {
    food_market_price_ = std::clamp(food, 20, 999);
    wood_market_price_ = std::clamp(wood, 20, 999);
    stone_market_price_ = std::clamp(stone, 20, 999);
}

void Simulation::replace_diplomacy(Diplomacy relation) {
    blue_red_diplomacy_ = relation;
    roster_diplomacy_.set_symmetric_stance(
        *PlayerSlotId::from_index(0),
        *PlayerSlotId::from_index(1),
        relation
    );
    update_exploration();
    update_match_outcome();
}

void Simulation::replace_civilizations(
    Civilization blue,
    Civilization red
) {
    player_states_[0].civilization = blue;
    player_states_[1].civilization = red;
}

void Simulation::replace_farm_reseed_queues(int blue, int red) {
    if (blue < 0 || red < 0 ||
        blue > maximum_farm_reseed_queue ||
        red > maximum_farm_reseed_queue) {
        throw std::invalid_argument("invalid farm reseed queue");
    }
    player_states_[0].farm_reseed_queue = blue;
    player_states_[1].farm_reseed_queue = red;
}

void Simulation::replace_mayan_resource_remainders(
    int blue,
    int red
) {
    if (blue < 0 || blue >= 115 || red < 0 || red >= 115) {
        throw std::invalid_argument("invalid Mayan resource remainder");
    }
    player_states_[0].mayan_resource_remainder = blue;
    player_states_[1].mayan_resource_remainder = red;
}

void Simulation::replace_aztec_relic_gold_remainders(
    int blue,
    int red
) {
    if (blue < 0 || blue >= 100 || red < 0 || red >= 100) {
        throw std::invalid_argument("invalid Aztec relic gold remainder");
    }
    player_states_[0].aztec_relic_gold_remainder = blue;
    player_states_[1].aztec_relic_gold_remainder = red;
}

void Simulation::replace_scenario_runtime(
    std::vector<ObjectiveState> objectives,
    std::vector<TriggerState> triggers,
    std::vector<ScenarioMessage> messages
) {
    for (TriggerState& trigger : triggers) {
        if (trigger.executable && trigger.conditions.empty()) {
            trigger.conditions.push_back(trigger.condition);
        }
        if (trigger.executable && trigger.effects.empty()) {
            trigger.effects.push_back(trigger.effect);
        }
        if (!trigger.conditions.empty()) {
            trigger.condition = trigger.conditions.front();
        }
        if (!trigger.effects.empty()) {
            trigger.effect = trigger.effects.front();
        }
    }
    validate_trigger_runtime_semantics(
        map_, units_, buildings_, objectives, triggers, tick_number_, false
    );
    std::ranges::sort(
        objectives, {}, &ObjectiveState::id
    );
    std::ranges::sort(
        triggers,
        [](const TriggerState& left, const TriggerState& right) {
            return left.priority != right.priority
                ? left.priority > right.priority
                : left.id < right.id;
        }
    );
    objectives_ = std::move(objectives);
    triggers_ = std::move(triggers);
    scenario_messages_ = std::move(messages);
}

bool Simulation::trigger_condition_met(
    const TriggerState& trigger
) const {
    const auto condition_met = [&](const TriggerCondition& condition) {
    const auto entity_exists = [&](bool building) {
        return building
            ? std::ranges::any_of(
                buildings_, [&](const Building& candidate) {
                    return candidate.id == condition.entity &&
                        candidate.hit_points > 0;
                }
              )
            : std::ranges::any_of(
                units_, [&](const Unit& candidate) {
                    return candidate.id == condition.entity &&
                        candidate.hit_points > 0;
                }
              );
    };
    switch (condition.kind) {
        case TriggerConditionKind::elapsed_ticks:
            return tick_number_ >=
                (trigger.fired_count == 0
                    ? trigger.activation_tick
                    : trigger.last_fired_tick) +
                static_cast<std::uint64_t>(condition.amount);
        case TriggerConditionKind::unit_exists:
            return entity_exists(false);
        case TriggerConditionKind::unit_destroyed:
            return !entity_exists(false);
        case TriggerConditionKind::building_exists:
            return entity_exists(true);
        case TriggerConditionKind::building_destroyed:
            return !entity_exists(true);
        case TriggerConditionKind::resource_at_least: {
            const Economy& value = economy(condition.player);
            const int amount =
                condition.resource == ResourceKind::wood ? value.wood :
                condition.resource == ResourceKind::food ? value.food :
                condition.resource == ResourceKind::gold ? value.gold :
                condition.resource == ResourceKind::stone ? value.stone : 0;
            return amount >= condition.amount;
        }
        case TriggerConditionKind::area_presence:
            return static_cast<int>(std::ranges::count_if(
                units_, [&](const Unit& unit) {
                    return unit.owner == condition.player &&
                        unit.hit_points > 0 && unit.garrisoned_in == 0 &&
                        unit.position.x >= condition.first.x &&
                        unit.position.y >= condition.first.y &&
                        unit.position.x <= condition.second.x &&
                        unit.position.y <= condition.second.y;
                }
            )) >= condition.amount;
        case TriggerConditionKind::object_hit_points_at_least: {
            const auto unit = std::ranges::find(
                units_, condition.entity, &Unit::id
            );
            if (unit != units_.end()) {
                return unit->hit_points >= condition.amount;
            }
            const auto building = std::ranges::find(
                buildings_, condition.entity, &Building::id
            );
            return building != buildings_.end() &&
                building->hit_points >= condition.amount;
        }
    }
    return false;
    };
    return std::ranges::all_of(trigger.conditions, condition_met);
}

void Simulation::apply_trigger_effect(const TriggerEffect& effect) {
    switch (effect.kind) {
        case TriggerEffectKind::message:
            scenario_messages_.push_back({
                effect.text, effect.player,
                tick_number_ + static_cast<std::uint64_t>(effect.amount),
                effect.audio_file
            });
            break;
        case TriggerEffectKind::complete_objective: {
            const auto objective = std::ranges::find(
                objectives_, effect.amount, &ObjectiveState::id
            );
            if (objective != objectives_.end()) objective->completed = true;
            break;
        }
        case TriggerEffectKind::add_resource: {
            Economy& value = effect.player == Player::blue
                ? player_states_[0].economy : player_states_[1].economy;
            int* destination =
                effect.resource == ResourceKind::wood ? &value.wood :
                effect.resource == ResourceKind::food ? &value.food :
                effect.resource == ResourceKind::gold ? &value.gold :
                effect.resource == ResourceKind::stone ? &value.stone :
                nullptr;
            if (destination != nullptr) {
                const std::int64_t result =
                    static_cast<std::int64_t>(*destination) + effect.amount;
                *destination = static_cast<int>(std::clamp<std::int64_t>(
                    result, 0, std::numeric_limits<int>::max()
                ));
            }
            break;
        }
        case TriggerEffectKind::create_unit:
            add_unit(effect.unit, effect.player, effect.position);
            break;
        case TriggerEffectKind::create_building:
            add_building(effect.building, effect.player, effect.position);
            break;
        case TriggerEffectKind::diplomacy:
            replace_diplomacy(effect.diplomacy);
            break;
        case TriggerEffectKind::victory:
            outcome_ = effect.player == Player::blue
                ? MatchOutcome::blue_victory
                : MatchOutcome::red_victory;
            break;
        case TriggerEffectKind::defeat:
            outcome_ = effect.player == Player::blue
                ? MatchOutcome::red_victory
                : MatchOutcome::blue_victory;
            break;
        case TriggerEffectKind::research: {
            auto& technologies = effect.player == Player::blue
                ? player_states_[0].technologies : player_states_[1].technologies;
            technologies.at(
                static_cast<std::size_t>(effect.technology)
            ) = true;
            std::vector<Technology> represented;
            for (std::size_t index = 0; index < technologies.size(); ++index) {
                if (technologies[index]) {
                    represented.push_back(static_cast<Technology>(index));
                }
            }
            replace_technologies(effect.player, represented);
            break;
        }
        case TriggerEffectKind::tribute: {
            Economy& source = effect.player == Player::blue
                ? player_states_[0].economy : player_states_[1].economy;
            Economy& target = effect.target_player == Player::blue
                ? player_states_[0].economy : player_states_[1].economy;
            int* from =
                effect.resource == ResourceKind::wood ? &source.wood :
                effect.resource == ResourceKind::food ? &source.food :
                effect.resource == ResourceKind::gold ? &source.gold :
                effect.resource == ResourceKind::stone ? &source.stone :
                nullptr;
            int* to =
                effect.resource == ResourceKind::wood ? &target.wood :
                effect.resource == ResourceKind::food ? &target.food :
                effect.resource == ResourceKind::gold ? &target.gold :
                effect.resource == ResourceKind::stone ? &target.stone :
                nullptr;
            if (from == nullptr || to == nullptr || *from < effect.amount) {
                throw std::invalid_argument("invalid trigger tribute");
            }
            *from -= effect.amount;
            *to = static_cast<int>(std::min<std::int64_t>(
                static_cast<std::int64_t>(*to) + effect.amount,
                std::numeric_limits<int>::max()
            ));
            break;
        }
        case TriggerEffectKind::remove_object:
            if (!delete_unit(effect.entity) &&
                !delete_building(effect.entity)) {
                throw std::invalid_argument("trigger removes missing object");
            }
            break;
        case TriggerEffectKind::set_objective_state: {
            const auto objective = std::ranges::find(
                objectives_, effect.objective_id, &ObjectiveState::id
            );
            if (objective == objectives_.end()) {
                throw std::invalid_argument("trigger references missing objective");
            }
            if (effect.amount == 0) objective->completed = effect.state;
            else objective->hidden = !effect.state;
            break;
        }
        case TriggerEffectKind::activate_trigger:
        case TriggerEffectKind::deactivate_trigger: {
            const auto trigger = std::ranges::find(
                triggers_, effect.trigger_id, &TriggerState::id
            );
            if (trigger == triggers_.end()) {
                throw std::invalid_argument("trigger references missing trigger");
            }
            trigger->enabled =
                effect.kind == TriggerEffectKind::activate_trigger;
            trigger->activation_tick = tick_number_;
            break;
        }
    }
}

bool Simulation::evaluate_scenario_triggers() {
    validate_trigger_runtime_semantics(
        map_, units_, buildings_, objectives_, triggers_, tick_number_, false
    );
    std::erase_if(
        scenario_messages_,
        [this](const ScenarioMessage& message) {
            return message.expires_tick <= tick_number_;
        }
    );
    std::vector<std::size_t> selected;
    for (std::size_t index = 0; index < triggers_.size(); ++index) {
        const TriggerState& trigger = triggers_[index];
        if (!trigger.enabled || !trigger.executable ||
            !trigger_condition_met(trigger)) {
            continue;
        }
        selected.push_back(index);
    }
    Simulation preview = *this;
    for (const std::size_t index : selected) {
        TriggerState& trigger = preview.triggers_[index];
        for (const TriggerEffect& effect : trigger.effects) {
            preview.apply_trigger_effect(effect);
        }
        trigger.last_fired_tick = tick_number_;
        ++trigger.fired_count;
        if (!trigger.looping) trigger.enabled = false;
        if (preview.outcome_ != MatchOutcome::ongoing) break;
    }
    const bool terminal = preview.outcome_ != MatchOutcome::ongoing;
    *this = std::move(preview);
    return terminal;
}

Unit* Simulation::find_unit(EntityId id) {
    const auto found = std::find_if(
        units_.begin(),
        units_.end(),
        [id](const Unit& unit) { return unit.id == id; }
    );
    return found == units_.end() ? nullptr : &*found;
}

Building* Simulation::find_building(EntityId id) {
    const auto found = std::find_if(
        buildings_.begin(),
        buildings_.end(),
        [id](const Building& building) { return building.id == id; }
    );
    return found == buildings_.end() ? nullptr : &*found;
}

Unit* Simulation::unit_at(TilePosition position) {
    const auto found = std::find_if(
        units_.begin(),
        units_.end(),
        [position](const Unit& unit) {
            return unit.garrisoned_in == 0 && unit.position == position;
        }
    );
    return found == units_.end() ? nullptr : &*found;
}

Building* Simulation::building_at(TilePosition position) {
    const auto found = std::find_if(
        buildings_.begin(),
        buildings_.end(),
        [position](const Building& building) {
            const BuildingRules& rules = rules_for(building.kind);
            return position.x >= building.position.x &&
                position.x < building.position.x + rules.footprint_width &&
                position.y >= building.position.y &&
                position.y < building.position.y + rules.footprint_height;
        }
    );
    return found == buildings_.end() ? nullptr : &*found;
}

TilePosition Simulation::nearest_point_on_building(
    TilePosition source,
    const Building& building
) const {
    const BuildingRules& rules = rules_for(building.kind);
    return {
        std::clamp(
            source.x,
            building.position.x,
            building.position.x + rules.footprint_width - 1
        ),
        std::clamp(
            source.y,
            building.position.y,
            building.position.y + rules.footprint_height - 1
        ),
    };
}

int Simulation::distance_to_building(
    TilePosition source,
    const Building& building
) const {
    const TilePosition nearest =
        nearest_point_on_building(source, building);
    return std::abs(source.x - nearest.x) +
        std::abs(source.y - nearest.y);
}

int Simulation::combat_distance_squared(
    TilePosition first,
    TilePosition second
) const {
    const int dx = first.x - second.x;
    const int dy = first.y - second.y;
    return dx * dx + dy * dy;
}

int Simulation::combat_distance_squared(
    TilePosition source,
    const Building& building
) const {
    return combat_distance_squared(
        source,
        nearest_point_on_building(source, building)
    );
}

int Simulation::combat_distance_squared(
    const Building& first,
    const Building& second
) const {
    const BuildingRules& first_rules = rules_for(first.kind);
    const BuildingRules& second_rules = rules_for(second.kind);
    const int first_right =
        first.position.x + first_rules.footprint_width - 1;
    const int second_right =
        second.position.x + second_rules.footprint_width - 1;
    const int first_bottom =
        first.position.y + first_rules.footprint_height - 1;
    const int second_bottom =
        second.position.y + second_rules.footprint_height - 1;
    const int dx =
        first_right < second.position.x
            ? second.position.x - first_right
            : second_right < first.position.x
                ? first.position.x - second_right
                : 0;
    const int dy =
        first_bottom < second.position.y
            ? second.position.y - first_bottom
            : second_bottom < first.position.y
                ? first.position.y - second_bottom
                : 0;
    return dx * dx + dy * dy;
}

int Simulation::projectile_travel_ticks(
    int distance_squared,
    int projectile_speed_tenths
) const {
    // Bounded contract: nonzero DAT speed is tenths of a tile/second.
    // Zero or unavailable unit-projectile speed retains the prior effective
    // cardinal speed of two tiles/tick (10 tiles/second). The first whole
    // tick whose squared travel reaches the target wins, avoiding floats.
    constexpr int simulation_ticks_per_second = 5;
    const std::int64_t speed =
        projectile_speed_tenths > 0 ? projectile_speed_tenths : 100;
    const std::int64_t scaled_distance_squared =
        static_cast<std::int64_t>(distance_squared) *
        100 * simulation_ticks_per_second * simulation_ticks_per_second;
    int ticks = 1;
    while (speed * ticks * speed * ticks <
           scaled_distance_squared) {
        ++ticks;
    }
    return ticks;
}

int Simulation::distance_between_buildings(
    const Building& first,
    const Building& second
) const {
    const TilePosition first_nearest =
        nearest_point_on_building(second.position, first);
    return distance_to_building(first_nearest, second);
}

Building* Simulation::nearest_drop_off(const Unit& unit) {
    Building* nearest = nullptr;
    int nearest_distance = 0;
    ResourceKind resource = unit.carried_resource;
    if (resource == ResourceKind::none && unit.has_resource_target) {
        resource = work_resource(unit);
    }
    for (Building& building : buildings_) {
        if (building.owner != unit.owner ||
            !building.completed() ||
            !accepts_resource(building.kind, resource)) {
            continue;
        }
        const int distance = distance_to_building(unit.position, building);
        if (nearest == nullptr || distance < nearest_distance) {
            nearest = &building;
            nearest_distance = distance;
        }
    }
    return nearest;
}

bool Simulation::route_to_resource_interaction(
    Unit& unit,
    TilePosition resource_target
) {
    const int current_distance = std::max(
        std::abs(resource_target.x - unit.position.x),
        std::abs(resource_target.y - unit.position.y)
    );
    if (current_distance <= 1) {
        unit.path.clear();
        unit.next_path_step = 0;
        unit.destination = resource_target;
        unit.moving = false;
        return true;
    }

    std::array<TilePosition, 8> candidates{{
        {resource_target.x - 1, resource_target.y - 1},
        {resource_target.x - 1, resource_target.y},
        {resource_target.x - 1, resource_target.y + 1},
        {resource_target.x, resource_target.y - 1},
        {resource_target.x, resource_target.y + 1},
        {resource_target.x + 1, resource_target.y - 1},
        {resource_target.x + 1, resource_target.y},
        {resource_target.x + 1, resource_target.y + 1},
    }};
    std::ranges::sort(
        candidates,
        [&unit](TilePosition first, TilePosition second) {
            const int first_distance =
                std::abs(first.x - unit.position.x) +
                std::abs(first.y - unit.position.y);
            const int second_distance =
                std::abs(second.x - unit.position.x) +
                std::abs(second.y - unit.position.y);
            if (first_distance != second_distance) {
                return first_distance < second_distance;
            }
            return first.y != second.y
                ? first.y < second.y
                : first.x < second.x;
        }
    );
    for (TilePosition candidate : candidates) {
        if (map_.contains(candidate) && map_.walkable(candidate) &&
            route_unit(unit, candidate)) {
            return true;
        }
    }
    unit.moving = false;
    return false;
}

bool Simulation::route_to_nearest_resource(
    Unit& unit,
    ResourceKind resource
) {
    std::vector<TilePosition> candidates;
    for (int y = 0; y < map_.height(); ++y) {
        for (int x = 0; x < map_.width(); ++x) {
            const TilePosition position{x, y};
            if (resource_for(map_.terrain_at(position)) == resource &&
                map_.resource_amount_at(position) > 0) {
                candidates.push_back(position);
            }
        }
    }
    std::ranges::sort(
        candidates,
        [&unit](TilePosition first, TilePosition second) {
            const int first_distance =
                std::abs(first.x - unit.position.x) +
                std::abs(first.y - unit.position.y);
            const int second_distance =
                std::abs(second.x - unit.position.x) +
                std::abs(second.y - unit.position.y);
            if (first_distance != second_distance) {
                return first_distance < second_distance;
            }
            if (first.y != second.y) {
                return first.y < second.y;
            }
            return first.x < second.x;
        }
    );
    for (TilePosition candidate : candidates) {
        if (route_to_resource_interaction(unit, candidate)) {
            unit.resource_target = candidate;
            unit.resource_building_id = 0;
            unit.resource_unit_id = 0;
            unit.returning_resource = false;
            return true;
        }
    }
    if (!candidates.empty()) {
        unit.resource_target = candidates.front();
        unit.resource_building_id = 0;
        unit.resource_unit_id = 0;
        unit.returning_resource = false;
        unit.moving = false;
        return true;
    }
    return false;
}

bool Simulation::occupied(
    TilePosition position,
    EntityId except,
    std::optional<EntityOwner> mover,
    bool plan_owned_gate
) const {
    const bool has_unit =
        std::ranges::any_of(units_, [position, except](const Unit& unit) {
            return unit.garrisoned_in == 0 &&
                unit.id != except && unit.position == position;
        });
    const bool has_building =
        std::ranges::any_of(
            buildings_,
            [position, except, mover, plan_owned_gate](
                const Building& building
            ) {
                const BuildingRules& rules = rules_for(building.kind);
                const bool gate =
                    building.kind == BuildingKind::palisade_gate_x ||
                    building.kind == BuildingKind::palisade_gate_y ||
                    building.kind == BuildingKind::stone_gate_x ||
                    building.kind == BuildingKind::stone_gate_y ||
                    building.kind == BuildingKind::fortified_gate_x ||
                    building.kind == BuildingKind::fortified_gate_y;
                const bool gate_passable =
                    gate && mover &&
                    (building.gate_open ||
                     (plan_owned_gate && building.owner == *mover));
                return building.id != except && !gate_passable &&
                    position.x >= building.position.x &&
                    position.x <
                        building.position.x + rules.footprint_width &&
                    position.y >= building.position.y &&
                    position.y <
                        building.position.y + rules.footprint_height;
            }
        );
    return has_unit || has_building;
}

bool Simulation::footprint_available(
    BuildingKind kind,
    TilePosition position,
    EntityId except
) const {
    const BuildingRules& rules = rules_for(kind);
    for (int y = 0; y < rules.footprint_height; ++y) {
        for (int x = 0; x < rules.footprint_width; ++x) {
            const TilePosition tile{position.x + x, position.y + y};
            const bool terrain_allowed =
                kind == BuildingKind::fish_trap
                ? map_.sailable(tile)
                : map_.contains(tile) &&
                    map_.terrain_at(tile) == Terrain::grass;
            if (!terrain_allowed ||
                occupied(tile, except)) {
                return false;
            }
        }
    }
    return true;
}

void Simulation::detach_builder(EntityId unit_id) {
    for (Building& building : buildings_) {
        if (building.completed()) {
            continue;
        }
        std::erase(building.builder_ids, unit_id);
        if (building.builder_id == unit_id) {
            building.builder_id = building.builder_ids.empty()
                ? 0
                : building.builder_ids.front();
        }
    }
}

bool Simulation::route_unit(Unit& unit, TilePosition destination) {
    const bool fishing_ship = is_ship(unit.kind);
    const auto traversable = [this, fishing_ship](TilePosition position) {
        return fishing_ship
            ? map_.sailable(position)
            : map_.walkable(position);
    };
    const auto tile_index = [this](TilePosition position) {
        return static_cast<std::size_t>(position.y) *
                   static_cast<std::size_t>(map_.width()) +
               static_cast<std::size_t>(position.x);
    };
    std::vector<bool> blocked(
        static_cast<std::size_t>(map_.width()) *
            static_cast<std::size_t>(map_.height()),
        false
    );
    for (const Unit& other : units_) {
        if (other.garrisoned_in == 0 && other.id != unit.id &&
            map_.contains(other.position)) {
            blocked[tile_index(other.position)] = true;
        }
    }
    for (const Building& building : buildings_) {
        if (building.id == unit.id) {
            continue;
        }
        const bool gate =
            building.kind == BuildingKind::palisade_gate_x ||
            building.kind == BuildingKind::palisade_gate_y ||
            building.kind == BuildingKind::stone_gate_x ||
            building.kind == BuildingKind::stone_gate_y ||
            building.kind == BuildingKind::fortified_gate_x ||
            building.kind == BuildingKind::fortified_gate_y;
        if (gate &&
            (building.gate_open || building.owner == unit.owner)) {
            continue;
        }
        const BuildingRules& rules = rules_for(building.kind);
        for (int y = building.position.y;
             y < building.position.y + rules.footprint_height;
             ++y) {
            for (int x = building.position.x;
                 x < building.position.x + rules.footprint_width;
                 ++x) {
                const TilePosition position{x, y};
                if (map_.contains(position)) {
                    blocked[tile_index(position)] = true;
                }
            }
        }
    }
    const auto occupied_for_route =
        [&blocked, &tile_index](TilePosition position) {
            return blocked[tile_index(position)];
        };
    TilePosition route_target = destination;
    if (Building* building = building_at(destination);
        building != nullptr && building->id != unit.id) {
        const BuildingRules& rules = rules_for(building->kind);
        int best_distance = map_.width() + map_.height() + 1;
        for (int y = building->position.y - 1;
             y <= building->position.y + rules.footprint_height;
             ++y) {
            for (int x = building->position.x - 1;
                 x <= building->position.x + rules.footprint_width;
                 ++x) {
                const bool beside_x =
                    (x == building->position.x - 1 ||
                     x == building->position.x + rules.footprint_width) &&
                    y >= building->position.y &&
                    y < building->position.y + rules.footprint_height;
                const bool beside_y =
                    (y == building->position.y - 1 ||
                     y == building->position.y + rules.footprint_height) &&
                    x >= building->position.x &&
                    x < building->position.x + rules.footprint_width;
                const bool on_perimeter = beside_x || beside_y;
                const TilePosition candidate{x, y};
                if (!on_perimeter || !map_.contains(candidate) ||
                    !traversable(candidate) ||
                    occupied_for_route(candidate)) {
                    continue;
                }
                const int distance =
                    std::abs(candidate.x - unit.position.x) +
                    std::abs(candidate.y - unit.position.y);
                if (distance < best_distance) {
                    route_target = candidate;
                    best_distance = distance;
                }
            }
        }
        if (best_distance > map_.width() + map_.height()) {
            return false;
        }
    }
    const auto blocked_for_route =
        [this, route_target, &occupied_for_route](TilePosition position) {
            const bool unrelated_resource =
                position != route_target &&
                resource_for(map_.terrain_at(position)) !=
                    ResourceKind::none;
            return unrelated_resource ||
                occupied_for_route(position);
        };
    unit.path = fishing_ship
        ? find_path(
              map_, unit.position, route_target,
              blocked_for_route, traversable
          )
        : find_path(
              map_, unit.position, route_target,
              blocked_for_route
          );
    if (unit.position != route_target && unit.path.empty()) {
        unit.moving = false;
        return false;
    }
    unit.destination = destination;
    unit.next_path_step = 0;
    unit.movement_cooldown = 0;
    unit.moving = !unit.path.empty();
    return true;
}

void Simulation::update_gate_states() {
    for (Building& gate : buildings_) {
        if ((gate.kind != BuildingKind::palisade_gate_x &&
             gate.kind != BuildingKind::palisade_gate_y &&
             gate.kind != BuildingKind::stone_gate_x &&
             gate.kind != BuildingKind::stone_gate_y &&
             gate.kind != BuildingKind::fortified_gate_x &&
             gate.kind != BuildingKind::fortified_gate_y) ||
            !gate.completed()) {
            gate.gate_open = false;
            continue;
        }
        const BuildingRules& rules = rules_for(gate.kind);
        const auto on_footprint = [&gate, &rules](const Unit& unit) {
            return unit.garrisoned_in == 0 &&
                unit.position.x >= gate.position.x &&
                unit.position.x <
                    gate.position.x + rules.footprint_width &&
                unit.position.y >= gate.position.y &&
                unit.position.y <
                    gate.position.y + rules.footprint_height;
        };
        const bool occupied_span =
            std::ranges::any_of(units_, on_footprint);
        const bool friendly_near = std::ranges::any_of(
            units_,
            [this, &gate](const Unit& unit) {
                return unit.garrisoned_in == 0 &&
                    unit.owner == gate.owner &&
                    distance_to_building(unit.position, gate) <= 2;
            }
        );
        gate.gate_open = occupied_span || friendly_near;
    }
}

std::optional<TilePosition> Simulation::spawn_position(
    const Building& building
) const {
    const auto available = [this](TilePosition candidate) {
        return map_.contains(candidate) &&
            map_.walkable(candidate) &&
            resource_for(map_.terrain_at(candidate)) == ResourceKind::none &&
            !occupied(candidate, 0);
    };
    const BuildingRules& rules = rules_for(building.kind);
    if (rules.footprint_width == 1 && rules.footprint_height == 1) {
        constexpr TilePosition offsets[] = {
            {1, 0}, {0, 1}, {-1, 0}, {0, -1},
            {1, 1}, {-1, 1}, {-1, -1}, {1, -1},
        };
        for (TilePosition offset : offsets) {
            const TilePosition candidate{
                building.position.x + offset.x,
                building.position.y + offset.y,
            };
            if (available(candidate)) {
                return candidate;
            }
        }
        return std::nullopt;
    }

    for (int x = 0; x < rules.footprint_width; ++x) {
        const TilePosition candidate{
            building.position.x + x,
            building.position.y - 1,
        };
        if (available(candidate)) {
            return candidate;
        }
    }
    for (int y = 0; y < rules.footprint_height; ++y) {
        const TilePosition candidate{
            building.position.x + rules.footprint_width,
            building.position.y + y,
        };
        if (available(candidate)) {
            return candidate;
        }
    }
    for (int x = rules.footprint_width - 1; x >= 0; --x) {
        const TilePosition candidate{
            building.position.x + x,
            building.position.y + rules.footprint_height,
        };
        if (available(candidate)) {
            return candidate;
        }
    }
    for (int y = rules.footprint_height - 1; y >= 0; --y) {
        const TilePosition candidate{
            building.position.x - 1,
            building.position.y + y,
        };
        if (available(candidate)) {
            return candidate;
        }
    }
    return std::nullopt;
}

void Simulation::perform_attack(Unit& attacker, Unit& defender) {
    reveal_attacker_to(attacker, defender.owner);
    if (attacker.commercial_identity) {
        defender.hit_points -= apply_elevation_damage(
            map_, attacker.position, defender.position,
            commercial_damage(attacker, defender)
        );
        defender.last_damage_owner = attacker.owner;
        return;
    }
    const UnitRules& reveal_rules = rules_for(attacker.kind);
    if (reveal_rules.splash_radius > 0) {
        reveal_attacker_to_ground_victims(
            attacker, defender.position, reveal_rules.splash_radius, 1
        );
    }
    if (attacker.kind == UnitKind::petard) {
        const UnitRules& rules = rules_for(attacker.kind);
        const int bonus = receives_siege_engineers(defender.kind)
            ? rules.bonus_vs_siege : 0;
        defender.hit_points -= apply_elevation_damage(
            map_, attacker.position, defender.position,
            damage_after_armor(
                attacker.attack + bonus, rules.damage_class,
                melee_armor(defender), pierce_armor(defender)
            )
        );
        if (is_animal(defender.kind) && defender.hit_points <= 0) {
            defender.food_remaining = 0;
        }
        detonate_petard(attacker, defender.position, defender.id);
        return;
    }
    if (attacker.kind == UnitKind::demolition_ship ||
        attacker.kind == UnitKind::heavy_demolition_ship) {
        detonate_demolition_ship(attacker);
        return;
    }
    const UnitRules& attacker_rules = rules_for(attacker.kind);
    const bool persian_knight =
        attacker.kind == UnitKind::knight ||
        attacker.kind == UnitKind::cavalier ||
        attacker.kind == UnitKind::paladin;
    const int bonus =
        (is_cavalry(defender.kind)
            ? attacker_rules.bonus_vs_cavalry
            : 0) +
        (is_archer(defender.kind)
            ? attacker_rules.bonus_vs_archers +
                (persian_knight &&
                 team_has_civilization(
                     attacker.owner, Civilization::persians
                 ) ? 2 : 0)
            : 0) +
        ((defender.kind == UnitKind::spearman ||
          defender.kind == UnitKind::pikeman ||
          defender.kind == UnitKind::halberdier)
            ? attacker_rules.bonus_vs_spearmen +
                (has_technology(
                     attacker.owner, Technology::parthian_tactics
                 ) ? (
                     attacker.kind == UnitKind::cavalry_archer ||
                     attacker.kind == UnitKind::heavy_cavalry_archer
                        ? 4 :
                     attacker.kind == UnitKind::mangudai ||
                     attacker.kind == UnitKind::elite_mangudai
                        ? 2 : 0
                 ) : 0)
            : 0) +
        ((defender.kind == UnitKind::war_elephant ||
          defender.kind == UnitKind::elite_war_elephant)
            ? attacker_rules.bonus_vs_war_elephants : 0) +
        ((defender.kind == UnitKind::camel_rider ||
          defender.kind == UnitKind::heavy_camel)
            ? attacker_rules.bonus_vs_camels : 0) +
        (is_unique_unit(defender.kind)
            ? attacker_rules.bonus_vs_unique_units : 0) +
        ((defender.kind == UnitKind::eagle_warrior ||
          defender.kind == UnitKind::elite_eagle_warrior)
            ? attacker_rules.bonus_vs_eagle_warriors : 0) +
        (is_infantry(defender.kind)
            ? attacker_rules.bonus_vs_infantry +
                (((attacker.kind == UnitKind::cataphract ||
                   attacker.kind == UnitKind::elite_cataphract) &&
                  has_technology(attacker.owner, Technology::logistica))
                    ? 6 : 0)
            : 0) +
        ((is_ram(defender.kind) ||
          defender.kind == UnitKind::mangonel ||
          defender.kind == UnitKind::onager ||
          defender.kind == UnitKind::siege_onager ||
          defender.kind == UnitKind::scorpion ||
          defender.kind == UnitKind::heavy_scorpion ||
          defender.kind == UnitKind::bombard_cannon ||
          defender.kind == UnitKind::packed_trebuchet ||
          defender.kind == UnitKind::trebuchet)
            ? attacker_rules.bonus_vs_siege : 0);
    defender.hit_points -= apply_elevation_damage(
        map_, attacker.position, defender.position,
        damage_after_armor(
            attacker.attack + bonus,
            attacker_rules.damage_class,
            melee_armor(defender),
            pierce_armor(defender)
        )
    );
    defender.last_damage_owner = attacker.owner;
    if (is_animal(defender.kind) && defender.hit_points <= 0 &&
        attacker.kind != UnitKind::villager) {
        defender.food_remaining = 0;
    }
    if (defender.kind == UnitKind::boar &&
        defender.hit_points > 0 &&
        attacker.owner != defender.owner) {
        defender.attack_target_id = attacker.id;
        defender.attack_target_is_building = false;
        defender.attack_target_auto = false;
        defender.stance_anchor = defender.position;
        route_unit(defender, attacker.position);
    }
    if (attacker.kind == UnitKind::elite_war_elephant) {
        for (Unit& candidate : units_) {
            if (candidate.id == attacker.id ||
                candidate.id == defender.id ||
                candidate.garrisoned_in != 0 ||
                candidate.hit_points <= 0) {
                continue;
            }
            const int distance =
                std::abs(candidate.position.x - defender.position.x) +
                std::abs(candidate.position.y - defender.position.y);
            if (distance <= 1) {
                candidate.hit_points -= damage_after_armor(
                    attacker.attack, DamageClass::melee,
                    melee_armor(candidate), pierce_armor(candidate)
                );
                candidate.last_damage_owner = attacker.owner;
                if (is_animal(candidate.kind) &&
                    candidate.hit_points <= 0) {
                    candidate.food_remaining = 0;
                }
            }
        }
    }
    if ((attacker.kind == UnitKind::cataphract ||
         attacker.kind == UnitKind::elite_cataphract) &&
        has_technology(attacker.owner, Technology::logistica)) {
        impact_effects_.push_back({
            defender.position, true, 5, 5, attacker.kind
        });
        impact_effects_.back().source_entity_id = attacker.id;
        for (Unit& candidate : units_) {
            if (candidate.id == attacker.id ||
                candidate.id == defender.id ||
                candidate.garrisoned_in != 0 ||
                candidate.hit_points <= 0 ||
                !is_enemy(candidate.owner, attacker.owner)) {
                continue;
            }
            const int distance =
                std::abs(candidate.position.x - defender.position.x) +
                std::abs(candidate.position.y - defender.position.y);
            if (distance <= 1) {
                const int logistica_bonus =
                    is_infantry(candidate.kind) ? 6 : 0;
                candidate.hit_points -= damage_after_armor(
                    attacker.attack +
                        attacker_rules.bonus_vs_infantry *
                            static_cast<int>(is_infantry(candidate.kind)) +
                        logistica_bonus,
                    DamageClass::melee,
                    melee_armor(candidate), pierce_armor(candidate)
                );
                candidate.last_damage_owner = attacker.owner;
                if (is_animal(candidate.kind) &&
                    candidate.hit_points <= 0) {
                    candidate.food_remaining = 0;
                }
            }
        }
    }
}

void Simulation::perform_attack(Unit& attacker, Building& defender) {
    reveal_attacker_to(attacker, defender.owner);
    if (attacker.commercial_identity) {
        defender.hit_points -= apply_elevation_damage(
            map_, attacker.position, defender.position,
            commercial_damage(attacker, defender)
        );
        defender.last_damage_owner = attacker.owner;
        return;
    }
    const UnitRules& reveal_rules = rules_for(attacker.kind);
    if (reveal_rules.splash_radius > 0) {
        reveal_attacker_to_ground_victims(
            attacker, defender.position, reveal_rules.splash_radius, 1
        );
    }
    if (attacker.kind == UnitKind::petard) {
        const UnitRules& rules = rules_for(attacker.kind);
        int building_bonus = rules.bonus_vs_buildings;
        if (has_technology(
                attacker.owner, Technology::siege_engineers
            )) {
            building_bonus += 200;
        }
        building_bonus = std::max(
            0, building_bonus -
                class_11_building_armor(*this, defender)
        );
        const int wall_bonus =
            is_wall(defender.kind) ? rules.bonus_vs_walls : 0;
        defender.hit_points -= apply_elevation_damage(
            map_, attacker.position, defender.position,
            damage_after_armor(
                attacker.attack + building_bonus + wall_bonus,
                rules.damage_class,
                melee_armor(defender), pierce_armor(defender)
            )
        );
        defender.last_damage_owner = attacker.owner;
        detonate_petard(attacker, defender.position, defender.id);
        return;
    }
    if (attacker.kind == UnitKind::demolition_ship ||
        attacker.kind == UnitKind::heavy_demolition_ship) {
        detonate_demolition_ship(attacker);
        return;
    }
    const UnitRules& attacker_rules = rules_for(attacker.kind);
    int building_bonus = attacker_rules.bonus_vs_buildings;
    if (is_archer(attacker.kind) && !is_cavalry(attacker.kind) &&
        team_has_civilization(
            attacker.owner, Civilization::saracens
        )) {
        ++building_bonus;
    }
    const bool sappers =
        attacker.kind == UnitKind::villager &&
        has_technology(attacker.owner, Technology::sappers);
    if (sappers) building_bonus += std::min(
        15, sappers_attack_bonus(attacker.owner, defender.kind)
    );
    if (receives_siege_engineers(attacker.kind) &&
        has_technology(attacker.owner, Technology::siege_engineers)) {
        building_bonus = building_bonus * 120 / 100;
    }
    building_bonus = std::max(
        0, building_bonus - class_11_building_armor(*this, defender)
    );
    const int class_13_bonus = sappers
        ? sappers_attack_bonus(attacker.owner, defender.kind) -
            std::min(
                15, sappers_attack_bonus(attacker.owner, defender.kind)
            )
        : 0;
    defender.hit_points -= apply_elevation_damage(
        map_, attacker.position, defender.position,
        damage_after_armor(
            attacker.attack + building_bonus + class_13_bonus,
            attacker_rules.damage_class,
            melee_armor(defender),
            pierce_armor(defender)
        )
    );
    defender.last_damage_owner = attacker.owner;
    if (!is_ram(attacker.kind) || attacker_rules.splash_radius == 0) {
        return;
    }
    for (Building& candidate : buildings_) {
        if (candidate.id == defender.id || candidate.hit_points <= 0 ||
            distance_to_building(defender.position, candidate) >
                attacker_rules.splash_radius) {
            continue;
        }
        int splash_building_bonus = attacker_rules.bonus_vs_buildings;
        if (receives_siege_engineers(attacker.kind) &&
            has_technology(
                attacker.owner, Technology::siege_engineers
            )) {
            splash_building_bonus = splash_building_bonus * 120 / 100;
        }
        splash_building_bonus = std::max(
            0, splash_building_bonus -
                class_11_building_armor(*this, candidate)
        );
        candidate.hit_points -= damage_after_armor(
            attacker.attack + splash_building_bonus,
            attacker_rules.damage_class,
            melee_armor(candidate),
            pierce_armor(candidate)
        );
        candidate.last_damage_owner = attacker.owner;
    }
    for (Unit& candidate : units_) {
        if (candidate.id == attacker.id || candidate.garrisoned_in != 0 ||
            candidate.hit_points <= 0) {
            continue;
        }
        const int distance =
            std::abs(candidate.position.x - defender.position.x) +
            std::abs(candidate.position.y - defender.position.y);
        if (distance <= attacker_rules.splash_radius) {
            candidate.hit_points -= damage_after_armor(
                attacker.attack,
                attacker_rules.damage_class,
                melee_armor(candidate),
                pierce_armor(candidate)
            );
            candidate.last_damage_owner = attacker.owner;
            if (is_animal(candidate.kind) &&
                candidate.hit_points <= 0) {
                candidate.food_remaining = 0;
            }
        }
    }
}

void Simulation::detonate_demolition_ship(Unit& attacker) {
    const TilePosition center = attacker.position;
    const int unit_damage = attacker.attack;
    const int building_damage =
        attacker.kind == UnitKind::heavy_demolition_ship ? 280 : 220;
    for (Unit& candidate : units_) {
        if (candidate.id == attacker.id || candidate.garrisoned_in != 0 ||
            candidate.hit_points <= 0) {
            continue;
        }
        const int distance =
            std::abs(candidate.position.x - center.x) +
            std::abs(candidate.position.y - center.y);
        if (distance <= 1) {
            candidate.hit_points -= damage_after_armor(
                unit_damage, DamageClass::melee,
                melee_armor(candidate), pierce_armor(candidate)
            );
            candidate.last_damage_owner = attacker.owner;
            if (is_animal(candidate.kind) &&
                candidate.hit_points <= 0) {
                candidate.food_remaining = 0;
            }
        }
    }
    for (Building& candidate : buildings_) {
        if (candidate.hit_points <= 0 ||
            distance_to_building(center, candidate) > 1) {
            continue;
        }
        candidate.hit_points -= damage_after_armor(
            building_damage, DamageClass::melee,
            melee_armor(candidate), pierce_armor(candidate)
        );
        candidate.last_damage_owner = attacker.owner;
    }
    attacker.hit_points = 0;
}

void Simulation::detonate_petard(
    Unit& attacker,
    TilePosition center,
    EntityId primary_target
) {
    const UnitRules& rules = rules_for(UnitKind::petard);
    impact_effects_.push_back({
        center, true, 5, 5, UnitKind::petard
    });
    impact_effects_.back().source_entity_id = attacker.id;
    for (Unit& target : units_) {
        if (target.id == attacker.id || target.id == primary_target ||
            target.garrisoned_in != 0 || target.hit_points <= 0) continue;
        const int dx = target.position.x - center.x;
        const int dy = target.position.y - center.y;
        if (4 * (dx * dx + dy * dy) >
            rules.splash_radius_half_tiles *
                rules.splash_radius_half_tiles) continue;
        const int bonus = receives_siege_engineers(target.kind)
            ? rules.bonus_vs_siege : 0;
        target.hit_points -= damage_after_armor(
            attacker.attack + bonus, rules.damage_class,
            melee_armor(target), pierce_armor(target)
        );
        target.last_damage_owner = attacker.owner;
        if (is_animal(target.kind) && target.hit_points <= 0) {
            target.food_remaining = 0;
        }
    }
    for (Building& target : buildings_) {
        if (target.id == primary_target || target.hit_points <= 0) continue;
        const TilePosition nearest = nearest_point_on_building(center, target);
        const int dx = nearest.x - center.x;
        const int dy = nearest.y - center.y;
        if (4 * (dx * dx + dy * dy) >
            rules.splash_radius_half_tiles *
                rules.splash_radius_half_tiles) continue;
        int building_bonus = rules.bonus_vs_buildings;
        if (has_technology(
                attacker.owner, Technology::siege_engineers
            )) {
            building_bonus += 200;
        }
        building_bonus = std::max(
            0, building_bonus - class_11_building_armor(*this, target)
        );
        const int wall_bonus =
            is_wall(target.kind) ? rules.bonus_vs_walls : 0;
        target.hit_points -= damage_after_armor(
            attacker.attack + building_bonus + wall_bonus,
            rules.damage_class,
            melee_armor(target), pierce_armor(target)
        );
        target.last_damage_owner = attacker.owner;
    }
    attacker.hit_points = 0;
}

void Simulation::launch_projectile(
    Unit& attacker,
    const Unit& defender
) {
    if (attacker.commercial_identity) {
        const auto* record = commercial_content_catalog().object(
            attacker.commercial_identity->civilization_id,
            attacker.commercial_identity->object_id
        );
        if (!record) return;
        const int accuracy = std::clamp(static_cast<int>(std::lround(
            effective_commercial_attribute(
                attacker.owner, *attacker.commercial_identity, 11,
                static_cast<float>(record->accuracy)
            )
        )), 0, 100);
        const bool hits = static_cast<int>(
            (tick_number_ * 37 + attacker.id * 17) % 100
        ) < accuracy;
        int projectile_speed{};
        std::optional<CommercialObjectIdentity> missile;
        if (record->missile_object_id) {
            missile = CommercialObjectIdentity{
                attacker.commercial_identity->civilization_id,
                *record->missile_object_id,
            };
            if (const auto* missile_record = commercial_content_catalog().object(
                    missile->civilization_id, missile->object_id)) {
                projectile_speed = static_cast<int>(std::lround(
                    missile_record->speed * 10.0f
                ));
            }
        }
        const int travel_ticks = projectile_travel_ticks(
            combat_distance_squared(attacker.position, defender.position),
            projectile_speed
        );
        projectiles_.push_back({
            attacker.owner, hits ? defender.id : 0, false,
            attacker.position, defender.position,
            commercial_damage(attacker, defender), DamageClass::melee,
            travel_ticks + 1, travel_ticks, 0,
            std::max(0, static_cast<int>(std::lround(
                effective_commercial_attribute(
                    attacker.owner, *attacker.commercial_identity, 22,
                    record->area_effect_range
                )
            ))),
            attacker.kind, 0, false, BuildingKind::town_center,
            projectile_speed, attacker.id, missile, true,
            effective_commercial_attribute(
                attacker.owner, *attacker.commercial_identity, 19, 0.0f
            ) != 0.0f,
        });
        const int projectile_count = std::max(1, static_cast<int>(std::lround(
            effective_commercial_attribute(
                attacker.owner, *attacker.commercial_identity, 107, 1.0f
            )
        )));
        for (int volley = 1; volley < projectile_count; ++volley) {
            Projectile extra = projectiles_.back();
            extra.target = 0;
            extra.damage = 0;
            extra.visual_lane = volley - projectile_count / 2;
            projectiles_.push_back(extra);
        }
        reveal_attacker_to(attacker, defender.owner, travel_ticks + 1);
        return;
    }
    const UnitRules& attacker_rules = rules_for(attacker.kind);
    int building_bonus = attacker_rules.bonus_vs_buildings;
    if (receives_siege_engineers(attacker.kind) &&
        has_technology(attacker.owner, Technology::siege_engineers)) {
        building_bonus = building_bonus * 120 / 100;
    }
    const bool thumb_ring_accuracy =
        has_technology(attacker.owner, Technology::thumb_ring) &&
        is_archer(attacker.kind) &&
        attacker.kind != UnitKind::hand_cannoneer &&
        attacker.kind != UnitKind::janissary &&
        attacker.kind != UnitKind::elite_janissary;
    const bool hits =
        thumb_ring_accuracy ||
        attacker_rules.accuracy_percent >= 100 ||
        static_cast<int>(
            ((tick_number_ /
             static_cast<std::uint64_t>(
                 std::max(1, attacker_rules.attack_interval_ticks)
             )) * 37 + attacker.id * 17) % 100
        ) < attacker_rules.accuracy_percent;
    const int travel_ticks = projectile_travel_ticks(
        combat_distance_squared(attacker.position, defender.position),
        0
    );
    reveal_attacker_to(attacker, defender.owner, travel_ticks + 1);
    reveal_attacker_to_ground_victims(
        attacker,
        defender.position,
        std::max(
            attacker_rules.splash_radius,
            (attacker_rules.splash_radius_half_tiles + 1) / 2
        ),
        travel_ticks + 1
    );
    projectiles_.push_back({
        attacker.owner,
        hits ? defender.id : 0,
        false,
        attacker.position,
        defender.position,
        attacker.attack +
            (is_ship(defender.kind)
                ? attacker_rules.bonus_vs_ships
                : 0) +
            (is_archer(defender.kind)
                ? attacker_rules.bonus_vs_archers
                : 0) +
            ((defender.kind == UnitKind::spearman ||
              defender.kind == UnitKind::pikeman ||
              defender.kind == UnitKind::halberdier)
                ? attacker_rules.bonus_vs_spearmen +
                    (has_technology(
                         attacker.owner, Technology::parthian_tactics
                     ) ? (
                         attacker.kind == UnitKind::cavalry_archer ||
                         attacker.kind == UnitKind::heavy_cavalry_archer
                            ? 4 :
                         attacker.kind == UnitKind::mangudai ||
                         attacker.kind == UnitKind::elite_mangudai
                            ? 2 : 0
                     ) : 0)
                : 0) +
            ((defender.kind == UnitKind::war_elephant ||
              defender.kind == UnitKind::elite_war_elephant)
                ? attacker_rules.bonus_vs_war_elephants : 0) +
            ((defender.kind == UnitKind::camel_rider ||
              defender.kind == UnitKind::heavy_camel)
                ? attacker_rules.bonus_vs_camels : 0) +
            (is_unique_unit(defender.kind)
                ? attacker_rules.bonus_vs_unique_units : 0) +
            ((defender.kind == UnitKind::eagle_warrior ||
              defender.kind == UnitKind::elite_eagle_warrior)
                ? attacker_rules.bonus_vs_eagle_warriors : 0) +
            (is_infantry(defender.kind)
                ? attacker_rules.bonus_vs_infantry : 0) +
            ((is_ram(defender.kind) ||
              defender.kind == UnitKind::mangonel ||
              defender.kind == UnitKind::onager ||
              defender.kind == UnitKind::siege_onager ||
              defender.kind == UnitKind::scorpion ||
              defender.kind == UnitKind::heavy_scorpion ||
              defender.kind == UnitKind::bombard_cannon ||
              defender.kind == UnitKind::packed_trebuchet ||
              defender.kind == UnitKind::trebuchet)
                ? attacker_rules.bonus_vs_siege : 0),
        attacker_rules.damage_class,
        travel_ticks + 1,
        travel_ticks,
        0,
        attacker_rules.splash_radius,
        attacker.kind,
        attacker_rules.splash_radius_half_tiles,
        false, BuildingKind::town_center, 0, attacker.id, std::nullopt, false,
        false,
    });
    projectiles_.back().source_entity_id = attacker.id;
    for (int volley = 1; volley < attacker_rules.projectile_count; ++volley) {
        projectiles_.push_back({
            attacker.owner,
            0,
            false,
            attacker.position,
            defender.position,
            0,
            attacker_rules.damage_class,
            travel_ticks + 1,
            travel_ticks,
            (volley - attacker_rules.projectile_count / 2) *
                attacker_rules.projectile_spread,
            0,
            attacker.kind,
            0,
            false, BuildingKind::town_center, 0, attacker.id,
            std::nullopt, false, false,
        });
        projectiles_.back().source_entity_id = attacker.id;
    }
}

void Simulation::launch_projectile(
    Unit& attacker,
    const Building& defender
) {
    if (attacker.commercial_identity) {
        const auto* record = commercial_content_catalog().object(
            attacker.commercial_identity->civilization_id,
            attacker.commercial_identity->object_id
        );
        if (!record) return;
        const int accuracy = std::clamp(static_cast<int>(std::lround(
            effective_commercial_attribute(
                attacker.owner, *attacker.commercial_identity, 11,
                static_cast<float>(record->accuracy)
            )
        )), 0, 100);
        const bool hits = static_cast<int>(
            (tick_number_ * 37 + attacker.id * 17) % 100
        ) < accuracy;
        int projectile_speed{};
        std::optional<CommercialObjectIdentity> missile;
        if (record->missile_object_id) {
            missile = CommercialObjectIdentity{
                attacker.commercial_identity->civilization_id,
                *record->missile_object_id,
            };
            if (const auto* missile_record = commercial_content_catalog().object(
                    missile->civilization_id, missile->object_id)) {
                projectile_speed = static_cast<int>(std::lround(
                    missile_record->speed * 10.0f
                ));
            }
        }
        const TilePosition destination = nearest_point_on_building(
            attacker.position, defender
        );
        const int travel_ticks = projectile_travel_ticks(
            combat_distance_squared(attacker.position, defender),
            projectile_speed
        );
        projectiles_.push_back({
            attacker.owner, hits ? defender.id : 0, hits,
            attacker.position, destination,
            commercial_damage(attacker, defender), DamageClass::melee,
            travel_ticks + 1, travel_ticks, 0,
            std::max(0, static_cast<int>(std::lround(
                effective_commercial_attribute(
                    attacker.owner, *attacker.commercial_identity, 22,
                    record->area_effect_range
                )
            ))),
            attacker.kind, 0, false, BuildingKind::town_center,
            projectile_speed, attacker.id, missile, true,
            effective_commercial_attribute(
                attacker.owner, *attacker.commercial_identity, 19, 0.0f
            ) != 0.0f,
        });
        const int projectile_count = std::max(1, static_cast<int>(std::lround(
            effective_commercial_attribute(
                attacker.owner, *attacker.commercial_identity, 107, 1.0f
            )
        )));
        for (int volley = 1; volley < projectile_count; ++volley) {
            Projectile extra = projectiles_.back();
            extra.target = 0;
            extra.damage = 0;
            extra.visual_lane = volley - projectile_count / 2;
            projectiles_.push_back(extra);
        }
        reveal_attacker_to(attacker, defender.owner, travel_ticks + 1);
        return;
    }
    const UnitRules& attacker_rules = rules_for(attacker.kind);
    int building_bonus = attacker_rules.bonus_vs_buildings;
    const bool sappers =
        attacker.kind == UnitKind::villager &&
        has_technology(attacker.owner, Technology::sappers);
    if (sappers) building_bonus += std::min(
        15, sappers_attack_bonus(attacker.owner, defender.kind)
    );
    if (receives_siege_engineers(attacker.kind) &&
        has_technology(attacker.owner, Technology::siege_engineers)) {
        building_bonus = building_bonus * 120 / 100;
    }
    building_bonus = std::max(
        0, building_bonus - class_11_building_armor(*this, defender)
    );
    const int class_13_bonus = sappers
        ? sappers_attack_bonus(attacker.owner, defender.kind) -
            std::min(
                15, sappers_attack_bonus(attacker.owner, defender.kind)
            )
        : 0;
    const bool thumb_ring_accuracy =
        has_technology(attacker.owner, Technology::thumb_ring) &&
        is_archer(attacker.kind) &&
        attacker.kind != UnitKind::hand_cannoneer &&
        attacker.kind != UnitKind::janissary &&
        attacker.kind != UnitKind::elite_janissary;
    const bool hits =
        thumb_ring_accuracy ||
        attacker_rules.accuracy_percent >= 100 ||
        static_cast<int>(
            ((tick_number_ /
             static_cast<std::uint64_t>(
                 std::max(1, attacker_rules.attack_interval_ticks)
             )) * 37 + attacker.id * 17) % 100
        ) < attacker_rules.accuracy_percent;
    const TilePosition destination =
        nearest_point_on_building(attacker.position, defender);
    const int travel_ticks = projectile_travel_ticks(
        combat_distance_squared(attacker.position, defender),
        0
    );
    reveal_attacker_to(attacker, defender.owner, travel_ticks + 1);
    reveal_attacker_to_ground_victims(
        attacker,
        destination,
        std::max(
            attacker_rules.splash_radius,
            (attacker_rules.splash_radius_half_tiles + 1) / 2
        ),
        travel_ticks + 1
    );
    projectiles_.push_back({
        attacker.owner,
        hits ? defender.id : 0,
        hits,
        attacker.position,
        destination,
        attacker.attack + building_bonus + class_13_bonus,
        attacker_rules.damage_class,
        travel_ticks + 1,
        travel_ticks,
        0,
        attacker_rules.splash_radius,
        attacker.kind,
        attacker_rules.splash_radius_half_tiles,
        false, BuildingKind::town_center, 0, attacker.id, std::nullopt, false,
        false,
    });
    projectiles_.back().source_entity_id = attacker.id;
    for (int volley = 1; volley < attacker_rules.projectile_count; ++volley) {
        projectiles_.push_back({
            attacker.owner,
            0,
            false,
            attacker.position,
            destination,
            0,
            attacker_rules.damage_class,
            travel_ticks + 1,
            travel_ticks,
            (volley - attacker_rules.projectile_count / 2) *
                attacker_rules.projectile_spread,
            0,
            attacker.kind,
            0,
            false, BuildingKind::town_center, 0, attacker.id,
            std::nullopt, false, false,
        });
        projectiles_.back().source_entity_id = attacker.id;
    }
}

void Simulation::launch_ground_projectile(
    Unit& attacker,
    TilePosition destination
) {
    const int travel_ticks = projectile_travel_ticks(
        combat_distance_squared(attacker.position, destination),
        0
    );
    reveal_attacker_to_ground_victims(
        attacker,
        destination,
        std::max(
            rules_for(attacker.kind).splash_radius,
            (rules_for(attacker.kind).splash_radius_half_tiles + 1) / 2
        ),
        travel_ticks + 1
    );
    projectiles_.push_back({
        attacker.owner,
        0,
        false,
        attacker.position,
        destination,
        attacker.attack,
        rules_for(attacker.kind).damage_class,
        travel_ticks + 1,
        travel_ticks,
        0,
        rules_for(attacker.kind).splash_radius,
        attacker.kind,
        0, false, BuildingKind::town_center, 0, attacker.id,
        std::nullopt, false, false,
    });
    projectiles_.back().source_entity_id = attacker.id;
}

void Simulation::update_building_defenses() {
    for (Building& building : buildings_) {
        const BuildingRules& building_rules = rules_for(building.kind);
        int projectile_count = building_rules.projectile_count;
        const int contributing_occupants =
            static_cast<int>(std::ranges::count_if(
                units_,
                [this, &building](const Unit& unit) {
                    return unit.garrisoned_in == building.id &&
                        can_garrison(unit, building) &&
                        (unit.kind == UnitKind::villager ||
                         is_archer(unit.kind));
                }
            ));
        projectile_count = garrison_volley_projectile_count(
            building.kind, projectile_count, contributing_occupants
        );
        if (!building.completed() || building_rules.attack <= 0 ||
            projectile_count <= 0) {
            continue;
        }
        if (building.attack_cooldown > 0) {
            --building.attack_cooldown;
        }
        if (building.attack_cooldown > 0) {
            continue;
        }

        const Unit* unit_target{};
        const Building* building_target{};
        const int minimum_range =
            building.kind != BuildingKind::bombard_tower &&
            has_technology(building.owner, Technology::murder_holes)
                ? 0
                : building_rules.minimum_attack_range;
        const int attack_range =
            effective_building_attack_range(building);
        int target_distance_squared =
            attack_range * attack_range + 1;
        for (const Unit& unit : units_) {
            if (unit.garrisoned_in != 0 ||
                !is_enemy(unit.owner, building.owner) ||
                unit.hit_points <= 0 ||
                is_relic(unit.kind) ||
                !is_visible(building.owner, unit.position)) {
                continue;
            }
            const int distance_squared =
                combat_distance_squared(unit.position, building);
            if (distance_squared > minimum_range * minimum_range &&
                distance_squared <= attack_range * attack_range &&
                distance_squared < target_distance_squared) {
                unit_target = &unit;
                building_target = nullptr;
                target_distance_squared = distance_squared;
            }
        }
        for (const Building& candidate : buildings_) {
            if (!is_enemy(candidate.owner, building.owner) ||
                candidate.hit_points <= 0 ||
                !is_building_visible(building.owner, candidate)) {
                continue;
            }
            const int distance_squared =
                combat_distance_squared(building, candidate);
            if (distance_squared > minimum_range * minimum_range &&
                distance_squared <= attack_range * attack_range &&
                distance_squared < target_distance_squared) {
                unit_target = nullptr;
                building_target = &candidate;
                target_distance_squared = distance_squared;
            }
        }
        if (unit_target == nullptr && building_target == nullptr) {
            continue;
        }

        const int travel_ticks = projectile_travel_ticks(
            target_distance_squared,
            building_rules.projectile_speed_tenths
        );
        const EntityId target_id = unit_target != nullptr
            ? unit_target->id
            : building_target->id;
        const bool target_is_building = building_target != nullptr;
        TilePosition origin = unit_target != nullptr
            ? nearest_point_on_building(unit_target->position, building)
            : nearest_point_on_building(
                building_target->position,
                building
            );
        const TilePosition target_position = unit_target != nullptr
            ? unit_target->position
            : nearest_point_on_building(origin, *building_target);
        if (building_target != nullptr) {
            origin = nearest_point_on_building(
                target_position,
                building
            );
        }
        for (int arrow = 0; arrow < projectile_count; ++arrow) {
            const bool hits =
                static_cast<int>(
                    (tick_number_ * 17 + building.id * 31 +
                     target_id * 13 + arrow * 7) % 100
                ) < building_rules.accuracy_percent;
            const int camel_bonus =
                unit_target != nullptr &&
                (unit_target->kind == UnitKind::camel_rider ||
                 unit_target->kind == UnitKind::heavy_camel)
                    ? building_rules.bonus_vs_camels
                    : 0;
            const int ship_bonus =
                unit_target != nullptr && is_ship(unit_target->kind)
                    ? defensive_ship_bonus(
                        building.owner, building.kind
                    )
                    : 0;
            projectiles_.push_back({
                building.owner,
                hits ? target_id : 0,
                hits && target_is_building,
                origin,
                target_position,
                effective_building_attack(building) + camel_bonus +
                    ship_bonus,
                building_rules.damage_class,
                travel_ticks + 1,
                travel_ticks,
                arrow - projectile_count / 2,
                0,
                UnitKind::villager,
                0,
                true,
                building.kind,
                building_rules.projectile_speed_tenths,
                building.id,
                std::nullopt,
                false,
                false,
            });
        }
        building.attack_cooldown =
            building_rules.attack_interval_ticks;
    }
}

void Simulation::update_projectiles() {
    for (ImpactEffect& effect : impact_effects_) {
        --effect.ticks_remaining;
    }
    std::erase_if(impact_effects_, [](const ImpactEffect& effect) {
        return effect.ticks_remaining <= 0;
    });

    for (Projectile& projectile : projectiles_) {
        if (projectile.splash_radius == 0 &&
            projectile.splash_radius_half_tiles == 0 &&
            !projectile.target_is_building &&
            projectile.target != 0 &&
            (projectile.tracks_target ||
             (has_technology(projectile.owner, Technology::ballistics) &&
              (projectile.source_is_building
                   ? ballistics_tracks(projectile.source_building_kind)
                   : ballistics_tracks(projectile.source_kind))))) {
            const Unit* target = find_unit(projectile.target);
            if (target != nullptr && target->hit_points > 0 &&
                target->garrisoned_in == 0) {
                projectile.destination = target->position;
            }
        }
        --projectile.ticks_remaining;
        if (projectile.ticks_remaining > 0) {
            continue;
        }
        const bool splash =
            projectile.splash_radius > 0 ||
            projectile.splash_radius_half_tiles > 0;
        const int duration = splash ? 5 : 4;
        impact_effects_.push_back({
            projectile.destination,
            splash,
            duration,
            duration,
            projectile.source_kind,
            projectile.source_is_building,
            projectile.source_building_kind,
            projectile.source_entity_id,
        });
        if (splash) {
            for (Unit& target : units_) {
                const int dx =
                    target.position.x - projectile.destination.x;
                const int dy =
                    target.position.y - projectile.destination.y;
                const bool within_half_radius =
                    projectile.splash_radius_half_tiles > 0 &&
                    4 * (dx * dx + dy * dy) <=
                        projectile.splash_radius_half_tiles *
                        projectile.splash_radius_half_tiles;
                const int distance = std::abs(dx) + std::abs(dy);
                if (target.garrisoned_in == 0 &&
                    target.hit_points > 0 &&
                    (within_half_radius ||
                     (projectile.splash_radius > 0 &&
                      distance <= projectile.splash_radius))) {
                    target.hit_points -= projectile.precomputed_damage
                        ? projectile.damage
                        : damage_after_armor(
                              projectile.damage, projectile.damage_class,
                              melee_armor(target), pierce_armor(target)
                          );
                    target.last_damage_owner = projectile.owner;
                    if (is_animal(target.kind) &&
                        target.hit_points <= 0) {
                        target.food_remaining = 0;
                    }
                }
            }
            for (Building& target : buildings_) {
                const TilePosition nearest = nearest_point_on_building(
                    projectile.destination, target
                );
                const int dx = nearest.x - projectile.destination.x;
                const int dy = nearest.y - projectile.destination.y;
                const bool within_half_radius =
                    projectile.splash_radius_half_tiles > 0 &&
                    4 * (dx * dx + dy * dy) <=
                        projectile.splash_radius_half_tiles *
                        projectile.splash_radius_half_tiles;
                if (target.hit_points > 0 &&
                    (within_half_radius ||
                     (projectile.splash_radius > 0 &&
                      distance_to_building(
                          projectile.destination, target
                      ) <= projectile.splash_radius))) {
                    target.hit_points -= projectile.precomputed_damage
                        ? projectile.damage
                        : damage_after_armor(
                              projectile.damage, projectile.damage_class,
                              melee_armor(target), pierce_armor(target)
                          );
                    target.last_damage_owner = projectile.owner;
                }
            }
            continue;
        }
        if (projectile.target_is_building) {
            Building* target = find_building(projectile.target);
            if (target != nullptr &&
                is_enemy(projectile.owner, target->owner)) {
                target->hit_points -= apply_elevation_damage(
                    map_, projectile.origin, target->position,
                    projectile.precomputed_damage
                        ? projectile.damage
                        : damage_after_armor(
                              projectile.damage, projectile.damage_class,
                              melee_armor(*target), pierce_armor(*target)
                          )
                );
                target->last_damage_owner = projectile.owner;
            }
        } else {
            Unit* target = find_unit(projectile.target);
            if (target != nullptr && target->garrisoned_in == 0 &&
                is_enemy(projectile.owner, target->owner)) {
                if (target->position != projectile.destination) {
                    continue;
                }
                target->hit_points -= apply_elevation_damage(
                    map_, projectile.origin, target->position,
                    projectile.precomputed_damage
                        ? projectile.damage
                        : damage_after_armor(
                              projectile.damage, projectile.damage_class,
                              melee_armor(*target), pierce_armor(*target)
                          )
                );
                target->last_damage_owner = projectile.owner;
                if (is_animal(target->kind) &&
                    target->hit_points <= 0) {
                    target->food_remaining = 0;
                }
            }
        }
    }
    std::erase_if(projectiles_, [](const Projectile& projectile) {
        return projectile.ticks_remaining <= 0;
    });
}

void Simulation::gather(Unit& unit) {
    const int capacity = carry_capacity(unit);
    const ResourceKind resource = work_resource(unit);
    if (resource == ResourceKind::none && unit.carried_amount == 0) {
        if (unit.resource_building_id == 0 &&
            unit.resource_unit_id == 0 &&
            unit.carried_resource != ResourceKind::none &&
            route_to_nearest_resource(unit, unit.carried_resource)) {
            return;
        }
        unit.has_resource_target = false;
        unit.carried_resource = ResourceKind::none;
        unit.moving = false;
        return;
    }
    if (unit.carried_amount == 0) {
        unit.carried_resource = resource;
    }
    const Unit* animal = unit.resource_unit_id == 0
        ? nullptr : find_unit(unit.resource_unit_id);
    const bool mongol_hunter =
        civilization(unit.owner) == Civilization::mongols &&
        animal != nullptr && is_huntable(animal->kind);
    const bool turk_gold =
        resource == ResourceKind::gold &&
        civilization(unit.owner) == Civilization::turks;
    const bool korean_stone =
        resource == ResourceKind::stone &&
        civilization(unit.owner) == Civilization::koreans;
    const bool extended_rate =
        mongol_hunter || turk_gold || korean_stone ||
        (resource == ResourceKind::wood &&
         (has_technology(unit.owner, Technology::bow_saw) ||
          has_technology(unit.owner, Technology::two_man_saw))) ||
        (resource == ResourceKind::gold &&
         (has_technology(unit.owner, Technology::gold_mining) ||
          has_technology(
              unit.owner, Technology::gold_shaft_mining
          ))) ||
        (resource == ResourceKind::stone &&
         (has_technology(unit.owner, Technology::stone_mining) ||
          has_technology(
              unit.owner, Technology::stone_shaft_mining
          )));
    const int base_work = extended_rate ? 10000 : 5;
    int work = base_work;
    if (resource == ResourceKind::wood) {
        if (extended_rate) {
            if (has_technology(
                    unit.owner, Technology::double_bit_axe
                )) work = work * 6 / 5;
            if (has_technology(
                    unit.owner, Technology::bow_saw
                )) work = work * 6 / 5;
            if (has_technology(
                    unit.owner, Technology::two_man_saw
                )) work = work * 11 / 10;
        } else if (has_technology(
                       unit.owner, Technology::double_bit_axe
                   )) {
            ++work;
        }
    } else if (resource == ResourceKind::gold) {
        if (turk_gold) work = work * 23 / 20;
        if (has_technology(
                unit.owner, Technology::gold_mining
            )) work = work * 23 / 20;
        if (has_technology(
                unit.owner, Technology::gold_shaft_mining
            )) work = work * 23 / 20;
    } else if (resource == ResourceKind::stone) {
        if (korean_stone) work = work * 6 / 5;
        if (has_technology(
                unit.owner, Technology::stone_mining
            )) work = work * 23 / 20;
        if (has_technology(
                unit.owner, Technology::stone_shaft_mining
            )) work = work * 23 / 20;
    }
    if (mongol_hunter) work = work * 3 / 2;
    unit.gather_work_remainder += work;
    const int resource_per_tick =
        unit.gather_work_remainder / base_work;
    unit.gather_work_remainder %= base_work;
    const int requested =
        std::min(resource_per_tick, capacity - unit.carried_amount);
    int credited_this_tick{};
    if (unit.resource_unit_id != 0) {
        Unit* herdable = find_unit(unit.resource_unit_id);
        if (herdable != nullptr && is_animal(herdable->kind) &&
            (is_huntable(herdable->kind) ||
             herdable->owner == unit.owner)) {
            const auto [credited, consumed] = finite_resource_yield(
                unit.owner, herdable->food_remaining, requested
            );
            herdable->food_remaining -= consumed;
            unit.carried_amount += credited;
            credited_this_tick = credited;
            if (herdable->food_remaining == 0) {
                herdable->hit_points = 0;
            }
        }
    } else if (unit.resource_building_id != 0) {
        Building* farm = find_building(unit.resource_building_id);
        if (farm != nullptr && farm->kind == BuildingKind::farm) {
            const auto [credited, consumed] = finite_resource_yield(
                unit.owner, farm->resource_amount, requested
            );
            farm->resource_amount -= consumed;
            unit.carried_amount += credited;
            credited_this_tick = credited;
            int& queue = farm->owner == Player::blue
                ? player_states_[0].farm_reseed_queue
                : player_states_[1].farm_reseed_queue;
            if (farm->resource_amount == 0 && queue > 0) {
                --queue;
                farm->resource_amount = farm_capacity(farm->owner);
            }
        }
    } else {
        const auto [credited, consumed] = finite_resource_yield(
            unit.owner,
            map_.resource_amount_at(unit.resource_target),
            requested
        );
        map_.take_resource(unit.resource_target, consumed);
        unit.carried_amount += credited;
        credited_this_tick = credited;
    }
    credit_gathered(unit.owner, resource, credited_this_tick);

    if (unit.carried_amount == 0 &&
        work_resource_amount(unit) == 0 &&
        unit.resource_building_id == 0 &&
        unit.resource_unit_id == 0) {
        if (!route_to_nearest_resource(unit, resource)) {
            unit.has_resource_target = false;
            unit.moving = false;
        }
        return;
    }

    if (unit.carried_amount < capacity &&
        work_resource_amount(unit) > 0) {
        return;
    }

    Building* drop_off = nearest_drop_off(unit);
    unit.returning_resource = true;
    if (drop_off == nullptr) {
        unit.moving = false;
        return;
    }
    route_unit(unit, drop_off->position);
}

std::pair<int, int> Simulation::finite_resource_yield(
    Player player,
    int available,
    int requested
) {
    if (available <= 0 || requested <= 0) return {0, 0};
    if (civilization(player) != Civilization::mayans) {
        const int taken = std::min(available, requested);
        return {taken, taken};
    }
    int& remainder = player == Player::blue
        ? player_states_[0].mayan_resource_remainder
        : player_states_[1].mayan_resource_remainder;
    int credited{};
    int consumed{};
    while (credited < requested && consumed < available) {
        const int total = remainder + 100;
        const int next_consumed = total / 115;
        if (consumed + next_consumed > available) break;
        remainder = total % 115;
        consumed += next_consumed;
        ++credited;
    }
    return {credited, consumed};
}

ResourceKind Simulation::work_resource(const Unit& unit) const {
    if (unit.resource_unit_id != 0) {
        const auto found = std::ranges::find_if(
            units_,
            [&unit](const Unit& target) {
                return target.id == unit.resource_unit_id &&
                    is_animal(target.kind) &&
                    (is_huntable(target.kind) ||
                     target.owner == unit.owner) &&
                    target.food_remaining > 0;
            }
        );
        return found == units_.end()
            ? ResourceKind::none
            : ResourceKind::food;
    }
    if (unit.resource_building_id != 0) {
        const auto found = std::ranges::find_if(
            buildings_,
            [&unit](const Building& building) {
                return building.id == unit.resource_building_id &&
                       building.kind == BuildingKind::farm &&
                       building.completed() &&
                       building.resource_amount > 0;
            }
        );
        return found == buildings_.end()
            ? ResourceKind::none
            : ResourceKind::food;
    }
    if (!map_.contains(unit.resource_target)) {
        return ResourceKind::none;
    }
    return resource_for(map_.terrain_at(unit.resource_target));
}

int Simulation::work_resource_amount(const Unit& unit) const {
    if (unit.resource_unit_id != 0) {
        const auto found = std::ranges::find_if(
            units_,
            [&unit](const Unit& target) {
                return target.id == unit.resource_unit_id &&
                    is_animal(target.kind);
            }
        );
        return found == units_.end() ? 0 : found->food_remaining;
    }
    if (unit.resource_building_id != 0) {
        const auto found = std::ranges::find_if(
            buildings_,
            [&unit](const Building& building) {
                return building.id == unit.resource_building_id &&
                       building.kind == BuildingKind::farm;
            }
        );
        return found == buildings_.end() ? 0 : found->resource_amount;
    }
    return map_.contains(unit.resource_target)
        ? map_.resource_amount_at(unit.resource_target)
        : 0;
}

void Simulation::update_production() {
    struct CompletedOrder {
        UnitKind kind;
        std::optional<CommercialObjectIdentity> commercial_identity;
        EntityOwner owner;
        TilePosition position;
        TilePosition rally_point;
        bool has_rally_point;
    };
    std::vector<CompletedOrder> completed;

    for (Building& building : buildings_) {
        if (building.technology_research_ticks_remaining > 0) {
            --building.technology_research_ticks_remaining;
            if (building.technology_research_ticks_remaining == 0) {
                if (building.commercial_research_target) {
                    const CommercialTechnologyId technology_id =
                        *building.commercial_research_target;
                    auto& researched = player_states_.at(
                        building.owner.stable_id()
                    ).commercial_technologies;
                    researched.at(technology_id) = true;
                    if (const auto* technology =
                            commercial_content_catalog().technology(
                                technology_id
                            )) {
                        apply_commercial_effect(
                            building.owner, technology->effect_id
                        );
                    }
                    building.commercial_research_target.reset();
                    ++mutable_statistics(
                        building.owner
                    ).technologies_researched;
                    continue;
                }
                std::vector<int> old_building_maxima;
                old_building_maxima.reserve(buildings_.size());
                for (const Building& candidate : buildings_) {
                    old_building_maxima.push_back(
                        maximum_hit_points(candidate)
                    );
                }
                auto& technologies = building.owner == Player::blue
                    ? player_states_[0].technologies
                    : player_states_[1].technologies;
                technologies.at(static_cast<std::size_t>(
                    building.technology_research_target
                )) = true;
                ++mutable_statistics(
                    building.owner
                ).technologies_researched;
                if (building.technology_research_target ==
                        Technology::masonry ||
                    building.technology_research_target ==
                        Technology::architecture ||
                    building.technology_research_target ==
                        Technology::hoardings) {
                    for (std::size_t i = 0; i < buildings_.size(); ++i) {
                        Building& candidate = buildings_[i];
                        if (candidate.owner != building.owner) continue;
                        const int increase =
                            maximum_hit_points(candidate) -
                            old_building_maxima[i];
                        candidate.hit_points += std::max(0, increase);
                    }
                }
                if (building.technology_research_target ==
                    Technology::zealotry) {
                    for (Unit& unit : units_) {
                        if (unit.owner == building.owner &&
                            (unit.kind == UnitKind::mameluke ||
                             unit.kind == UnitKind::elite_mameluke ||
                             unit.kind == UnitKind::camel_rider ||
                             unit.kind == UnitKind::heavy_camel)) {
                            unit.hit_points = std::min(
                                maximum_hit_points(unit),
                                unit.hit_points + 30
                            );
                        }
                    }
                }
                if (building.technology_research_target ==
                    Technology::supremacy) {
                    for (Unit& unit : units_) {
                        if (unit.owner == building.owner &&
                            unit.kind == UnitKind::villager) {
                            unit.hit_points = std::min(
                                maximum_hit_points(unit),
                                unit.hit_points + 40
                            );
                        }
                    }
                }
                const auto upgrade_line = [
                    this, owner = building.owner
                ](Technology researched, UnitKind from, UnitKind to) {
                    for (Unit& unit : units_) {
                        if (unit.owner == owner && unit.kind == from) {
                            const int damage =
                                maximum_hit_points(unit) - unit.hit_points;
                            unit.kind = to;
                            unit.hit_points = std::max(
                                0, maximum_hit_points(unit) - damage
                            );
                        }
                    }
                    for (Building& candidate : buildings_) {
                        if (candidate.owner != owner) continue;
                        for (ProductionOrder& order :
                             candidate.production_queue) {
                            if (order.kind == from) order.kind = to;
                        }
                    }
                    (void)researched;
                };
                if (building.technology_research_target ==
                    Technology::elite_eagle_warrior) {
                    upgrade_line(
                        Technology::elite_eagle_warrior,
                        UnitKind::eagle_warrior,
                        UnitKind::elite_eagle_warrior
                    );
                }
                if (building.technology_research_target ==
                    Technology::heavy_scorpion) {
                    upgrade_line(
                        Technology::heavy_scorpion,
                        UnitKind::scorpion, UnitKind::heavy_scorpion
                    );
                }
                if (building.technology_research_target ==
                    Technology::onager) {
                    upgrade_line(
                        Technology::onager,
                        UnitKind::mangonel, UnitKind::onager
                    );
                }
                if (building.technology_research_target ==
                    Technology::siege_onager) {
                    upgrade_line(
                        Technology::siege_onager,
                        UnitKind::mangonel, UnitKind::siege_onager
                    );
                    upgrade_line(
                        Technology::siege_onager,
                        UnitKind::onager, UnitKind::siege_onager
                    );
                }
                if (building.technology_research_target ==
                    Technology::capped_ram) {
                    upgrade_line(
                        Technology::capped_ram,
                        UnitKind::battering_ram, UnitKind::capped_ram
                    );
                }
                if (building.technology_research_target ==
                    Technology::siege_ram) {
                    upgrade_line(
                        Technology::siege_ram,
                        UnitKind::battering_ram, UnitKind::siege_ram
                    );
                    upgrade_line(
                        Technology::siege_ram,
                        UnitKind::capped_ram, UnitKind::siege_ram
                    );
                }
                if (building.technology_research_target ==
                    Technology::halberdier) {
                    upgrade_line(
                        Technology::halberdier,
                        UnitKind::spearman, UnitKind::halberdier
                    );
                    upgrade_line(
                        Technology::halberdier,
                        UnitKind::pikeman, UnitKind::halberdier
                    );
                }
                if (building.technology_research_target ==
                    Technology::heavy_cavalry_archer) {
                    upgrade_line(
                        Technology::heavy_cavalry_archer,
                        UnitKind::cavalry_archer,
                        UnitKind::heavy_cavalry_archer
                    );
                }
                if (building.technology_research_target ==
                    Technology::heavy_camel) {
                    upgrade_line(
                        Technology::heavy_camel,
                        UnitKind::camel_rider, UnitKind::heavy_camel
                    );
                }
                if (building.technology_research_target ==
                    Technology::man_at_arms) {
                    apply_man_at_arms_upgrade(building.owner);
                }
                if (building.technology_research_target ==
                    Technology::crossbowman) {
                    apply_crossbowman_upgrade(building.owner);
                }
                if (building.technology_research_target ==
                    Technology::arbalester) {
                    apply_arbalester_upgrade(building.owner);
                }
                if (building.technology_research_target ==
                    Technology::elite_skirmisher) {
                    apply_elite_skirmisher_upgrade(building.owner);
                }
                if (building.technology_research_target ==
                        Technology::war_galley ||
                    building.technology_research_target ==
                        Technology::galleon) {
                    for (Unit& unit : units_) {
                        if (unit.owner != building.owner) continue;
                        if (building.technology_research_target ==
                                Technology::war_galley &&
                            unit.kind == UnitKind::galley) {
                            unit.kind = UnitKind::war_galley;
                            unit.hit_points = maximum_hit_points(unit);
                        }
                        if (building.technology_research_target ==
                                Technology::galleon &&
                            (unit.kind == UnitKind::galley ||
                             unit.kind == UnitKind::war_galley)) {
                            unit.kind = UnitKind::galleon;
                            unit.hit_points = maximum_hit_points(unit);
                        }
                    }
                }
                if (building.technology_research_target ==
                        Technology::fast_fire_ship ||
                    building.technology_research_target ==
                        Technology::heavy_demolition_ship) {
                    for (Unit& unit : units_) {
                        if (unit.owner != building.owner) continue;
                        if (building.technology_research_target ==
                                Technology::fast_fire_ship &&
                            unit.kind == UnitKind::fire_ship) {
                            unit.kind = UnitKind::fast_fire_ship;
                            unit.hit_points = maximum_hit_points(unit);
                        }
                        if (building.technology_research_target ==
                                Technology::heavy_demolition_ship &&
                            unit.kind == UnitKind::demolition_ship) {
                            unit.kind = UnitKind::heavy_demolition_ship;
                            unit.hit_points = maximum_hit_points(unit);
                        }
                    }
                }
                if (building.technology_research_target ==
                    Technology::elite_cannon_galleon) {
                    for (Unit& unit : units_) {
                        if (unit.owner == building.owner &&
                            unit.kind == UnitKind::cannon_galleon) {
                            unit.kind = UnitKind::elite_cannon_galleon;
                            unit.hit_points = maximum_hit_points(unit);
                        }
                    }
                }
                if (building.technology_research_target ==
                        Technology::elite_longboat ||
                    building.technology_research_target ==
                        Technology::elite_turtle_ship) {
                    for (Unit& unit : units_) {
                        if (unit.owner != building.owner) continue;
                        if (building.technology_research_target ==
                                Technology::elite_longboat &&
                            unit.kind == UnitKind::longboat) {
                            unit.kind = UnitKind::elite_longboat;
                            unit.hit_points = maximum_hit_points(unit);
                        }
                        if (building.technology_research_target ==
                                Technology::elite_turtle_ship &&
                            unit.kind == UnitKind::turtle_ship) {
                            unit.kind = UnitKind::elite_turtle_ship;
                            unit.hit_points = maximum_hit_points(unit);
                        }
                    }
                }
                const Technology unique_upgrade =
                    building.technology_research_target;
                if (unique_upgrade == Technology::elite_longbowman ||
                    unique_upgrade ==
                        Technology::elite_throwing_axeman ||
                    unique_upgrade == Technology::elite_huskarl ||
                    unique_upgrade ==
                        Technology::elite_teutonic_knight ||
                    unique_upgrade == Technology::elite_samurai ||
                    unique_upgrade == Technology::elite_chu_ko_nu ||
                    unique_upgrade == Technology::elite_cataphract ||
                    unique_upgrade == Technology::elite_war_elephant ||
                    unique_upgrade == Technology::elite_mameluke ||
                    unique_upgrade == Technology::elite_janissary ||
                    unique_upgrade == Technology::elite_berserk ||
                    unique_upgrade == Technology::elite_mangudai ||
                    unique_upgrade == Technology::elite_jaguar_warrior ||
                    unique_upgrade == Technology::elite_plumed_archer ||
                    unique_upgrade == Technology::elite_conquistador ||
                    unique_upgrade == Technology::elite_tarkan ||
                    unique_upgrade == Technology::elite_woad_raider) {
                    for (Unit& unit : units_) {
                        if (unit.owner != building.owner) continue;
                        if (unique_upgrade ==
                                Technology::elite_longbowman &&
                            unit.kind == UnitKind::longbowman) {
                            unit.kind = UnitKind::elite_longbowman;
                            unit.hit_points = maximum_hit_points(unit);
                        }
                        if (unique_upgrade ==
                                Technology::elite_samurai &&
                            unit.kind == UnitKind::samurai) {
                            unit.kind = UnitKind::elite_samurai;
                            unit.hit_points = maximum_hit_points(unit);
                        }
                        if (unique_upgrade ==
                                Technology::elite_chu_ko_nu &&
                            unit.kind == UnitKind::chu_ko_nu) {
                            unit.kind = UnitKind::elite_chu_ko_nu;
                            unit.hit_points = maximum_hit_points(unit);
                        }
                        if (unique_upgrade ==
                                Technology::elite_cataphract &&
                            unit.kind == UnitKind::cataphract) {
                            unit.kind = UnitKind::elite_cataphract;
                            unit.hit_points = maximum_hit_points(unit);
                        }
                        if (unique_upgrade ==
                                Technology::elite_war_elephant &&
                            unit.kind == UnitKind::war_elephant) {
                            unit.kind = UnitKind::elite_war_elephant;
                            unit.hit_points = maximum_hit_points(unit);
                        }
                        if (unique_upgrade == Technology::elite_mameluke &&
                            unit.kind == UnitKind::mameluke) {
                            unit.kind = UnitKind::elite_mameluke;
                            unit.hit_points = maximum_hit_points(unit);
                        }
                        if (unique_upgrade == Technology::elite_janissary &&
                            unit.kind == UnitKind::janissary) {
                            unit.kind = UnitKind::elite_janissary;
                            unit.hit_points = maximum_hit_points(unit);
                        }
                        if (unique_upgrade == Technology::elite_berserk &&
                            unit.kind == UnitKind::berserk) {
                            unit.kind = UnitKind::elite_berserk;
                            unit.hit_points = maximum_hit_points(unit);
                        }
                        if (unique_upgrade == Technology::elite_mangudai &&
                            unit.kind == UnitKind::mangudai) {
                            unit.kind = UnitKind::elite_mangudai;
                            unit.hit_points = maximum_hit_points(unit);
                        }
                        if (unique_upgrade ==
                                Technology::elite_jaguar_warrior &&
                            unit.kind == UnitKind::jaguar_warrior) {
                            unit.kind = UnitKind::elite_jaguar_warrior;
                            unit.hit_points = maximum_hit_points(unit);
                        }
                        if (unique_upgrade ==
                                Technology::elite_plumed_archer &&
                            unit.kind == UnitKind::plumed_archer) {
                            unit.kind = UnitKind::elite_plumed_archer;
                            unit.hit_points = maximum_hit_points(unit);
                        }
                        if (unique_upgrade ==
                                Technology::elite_conquistador &&
                            unit.kind == UnitKind::conquistador) {
                            unit.kind = UnitKind::elite_conquistador;
                            unit.hit_points = maximum_hit_points(unit);
                        }
                        if (unique_upgrade == Technology::elite_tarkan &&
                            unit.kind == UnitKind::tarkan) {
                            unit.kind = UnitKind::elite_tarkan;
                            unit.hit_points = maximum_hit_points(unit);
                        }
                        if (unique_upgrade ==
                                Technology::elite_woad_raider &&
                            unit.kind == UnitKind::woad_raider) {
                            unit.kind = UnitKind::elite_woad_raider;
                            unit.hit_points = maximum_hit_points(unit);
                        }
                        if (unique_upgrade ==
                                Technology::elite_throwing_axeman &&
                            unit.kind == UnitKind::throwing_axeman) {
                            unit.kind =
                                UnitKind::elite_throwing_axeman;
                            unit.hit_points = maximum_hit_points(unit);
                        }
                        if (unique_upgrade ==
                                Technology::elite_huskarl &&
                            unit.kind == UnitKind::huskarl) {
                            unit.kind = UnitKind::elite_huskarl;
                            unit.hit_points = maximum_hit_points(unit);
                        }
                        if (unique_upgrade ==
                                Technology::elite_teutonic_knight &&
                            unit.kind == UnitKind::teutonic_knight) {
                            unit.kind =
                                UnitKind::elite_teutonic_knight;
                            unit.hit_points = maximum_hit_points(unit);
                        }
                    }
                }
                if (building.technology_research_target ==
                    Technology::pikeman) {
                    apply_pikeman_upgrade(building.owner);
                }
                if (building.technology_research_target ==
                    Technology::long_swordsman) {
                    apply_long_swordsman_upgrade(building.owner);
                }
                if (building.technology_research_target ==
                    Technology::two_handed_swordsman) {
                    apply_two_handed_swordsman_upgrade(building.owner);
                }
                if (building.technology_research_target ==
                    Technology::champion) {
                    apply_champion_upgrade(building.owner);
                }
                if (building.technology_research_target ==
                    Technology::cavalier) {
                    apply_cavalier_upgrade(building.owner);
                }
                if (building.technology_research_target ==
                    Technology::paladin) {
                    apply_paladin_upgrade(building.owner);
                }
                if (building.technology_research_target ==
                    Technology::light_cavalry) {
                    apply_light_cavalry_upgrade(building.owner);
                }
                if (building.technology_research_target ==
                    Technology::hussar) {
                    apply_hussar_upgrade(building.owner);
                }
                if (building.technology_research_target ==
                    Technology::loom) {
                    apply_loom_upgrade(building.owner);
                }
                if (building.technology_research_target ==
                    Technology::bloodlines) {
                    apply_bloodlines_upgrade(building.owner);
                }
                if (building.technology_research_target ==
                    Technology::horse_collar) {
                    apply_horse_collar_upgrade(building.owner);
                }
                if (building.technology_research_target ==
                        Technology::heavy_plow ||
                    building.technology_research_target ==
                        Technology::crop_rotation) {
                    const int addition =
                        building.technology_research_target ==
                            Technology::heavy_plow ? 125 : 175;
                    const int maximum =
                        building.technology_research_target ==
                            Technology::heavy_plow ? 375 : 550;
                    for (Building& farm : buildings_) {
                        if (farm.owner == building.owner &&
                            farm.kind == BuildingKind::farm &&
                            farm.resource_amount > 0) {
                            farm.resource_amount = std::min(
                                maximum, farm.resource_amount + addition
                            );
                        }
                    }
                }
                if (building.technology_research_target ==
                    Technology::fortified_wall) {
                    apply_fortified_wall_upgrade(building.owner);
                }
                if (building.technology_research_target ==
                    Technology::guard_tower) {
                    apply_guard_tower_upgrade(building.owner);
                }
                if (building.technology_research_target ==
                    Technology::keep) {
                    apply_keep_upgrade(building.owner);
                }
                refresh_unit_attacks(building.owner);
            }
            continue;
        }
        if (building.age_research_ticks_remaining > 0) {
            --building.age_research_ticks_remaining;
            if (building.age_research_ticks_remaining == 0) {
                std::vector<int> old_unit_maximums;
                std::vector<int> old_building_maximums;
                for (const Unit& unit : units_) {
                    if (unit.owner == building.owner) {
                        old_unit_maximums.push_back(
                            maximum_hit_points(unit)
                        );
                    }
                }
                for (const Building& candidate : buildings_) {
                    if (candidate.owner == building.owner) {
                        old_building_maximums.push_back(
                            maximum_hit_points(candidate)
                        );
                    }
                }
                (building.owner == Player::blue
                    ? player_states_[0].age
                    : player_states_[1].age) = building.age_research_target;
                AgeTimingStatistics& timings =
                    mutable_statistics(building.owner).age_times;
                if (building.age_research_target == Age::feudal) {
                    timings.feudal = tick_number_;
                } else if (
                    building.age_research_target == Age::castle
                ) {
                    timings.castle = tick_number_;
                } else if (
                    building.age_research_target == Age::imperial
                ) {
                    timings.imperial = tick_number_;
                }
                std::size_t index{};
                for (Unit& unit : units_) {
                    if (unit.owner == building.owner) {
                        const int maximum = maximum_hit_points(unit);
                        unit.hit_points = std::min(
                            maximum,
                            unit.hit_points + maximum -
                                old_unit_maximums[index++]
                        );
                    }
                }
                index = 0;
                for (Building& candidate : buildings_) {
                    if (candidate.owner == building.owner) {
                        const int maximum =
                            maximum_hit_points(candidate);
                        candidate.hit_points = std::min(
                            maximum,
                            candidate.hit_points + maximum -
                                old_building_maximums[index++]
                        );
                    }
                }
                refresh_unit_attacks(building.owner);
            }
            continue;
        }
        if (!building.completed()) {
            if (building.builder_ids.empty() &&
                building.builder_id != 0) {
                building.builder_ids.push_back(building.builder_id);
            }
            int active_builders{};
            for (EntityId builder_id : building.builder_ids) {
                Unit* builder = find_unit(builder_id);
                if (builder != nullptr &&
                    builder->owner == building.owner &&
                    (builder->kind == UnitKind::villager ||
                     (building.kind == BuildingKind::fish_trap &&
                      builder->kind == UnitKind::fishing_ship)) &&
                    builder->garrisoned_in == 0 &&
                    distance_to_building(builder->position, building) <= 1) {
                    ++active_builders;
                }
            }
            if (active_builders > 0) {
                const bool spanish =
                    civilization(building.owner) ==
                    Civilization::spanish;
                const bool stone_cutting = has_technology(
                    building.owner, Technology::stone_cutting
                );
                building.construction_work_remainder += stone_cutting
                    ? (active_builders + 2) * (spanish ? 78 : 6)
                    : spanish ? (active_builders + 2) * 13
                              : active_builders + 2;
                const int denominator = stone_cutting
                    ? (spanish ? 150 : 15)
                    : (spanish ? 30 : 3);
                const int completed_this_tick =
                    building.construction_work_remainder / denominator;
                building.construction_work_remainder %= denominator;
                building.construction_ticks_remaining = std::max(
                    0,
                    building.construction_ticks_remaining -
                        completed_this_tick
                );
                const BuildingRules& rules = rules_for(building.kind);
                const int maximum = maximum_hit_points(building);
                const int completed_ticks =
                    rules.construction_ticks -
                    building.construction_ticks_remaining;
                building.hit_points = std::max(
                    building.hit_points,
                    maximum * completed_ticks /
                        rules.construction_ticks
                );
                if (building.completed()) {
                    building.hit_points = maximum;
                    for (EntityId builder_id : building.builder_ids) {
                        Unit* builder = find_unit(builder_id);
                        if (builder != nullptr &&
                            distance_to_building(
                                builder->position,
                                building
                            ) <= 1) {
                            builder->moving = false;
                            builder->path.clear();
                        }
                    }
                }
            }
            continue;
        }
        if (building.production_queue.empty()) {
            continue;
        }
        ProductionOrder& order = building.production_queue.front();
        int production_rate = 100;
        const bool conscription_building =
            building.kind == BuildingKind::archery_range ||
            building.kind == BuildingKind::barracks ||
            building.kind == BuildingKind::stable ||
            building.kind == BuildingKind::castle;
        if (conscription_building &&
            has_technology(building.owner, Technology::conscription)) {
            production_rate = 133;
        }
        if (civilization(building.owner) == Civilization::persians &&
            (building.kind == BuildingKind::town_center ||
             building.kind == BuildingKind::dock) &&
            age(building.owner) >= Age::feudal) {
            production_rate =
                age(building.owner) >= Age::imperial ? 120 :
                age(building.owner) >= Age::castle ? 115 : 110;
        }
        if (team_has_civilization(
                building.owner, Civilization::britons
            ) && building.kind == BuildingKind::archery_range) {
            production_rate = production_rate * 120 / 100;
        }
        if (team_has_civilization(
                building.owner, Civilization::goths
            ) && building.kind == BuildingKind::barracks) {
            production_rate = production_rate * 120 / 100;
        }
        if (team_has_civilization(
                building.owner, Civilization::celts
            ) && building.kind == BuildingKind::siege_workshop) {
            production_rate = production_rate * 120 / 100;
        }
        if (team_has_civilization(
                building.owner, Civilization::huns
            ) && building.kind == BuildingKind::stable) {
            production_rate = production_rate * 120 / 100;
        }
        const bool gunpowder_order =
            order.kind == UnitKind::hand_cannoneer ||
            order.kind == UnitKind::bombard_cannon ||
            order.kind == UnitKind::janissary ||
            order.kind == UnitKind::elite_janissary;
        if (team_has_civilization(
                building.owner, Civilization::turks
            ) && gunpowder_order) {
            production_rate = production_rate * 120 / 100;
        }
        order.work_remainder += production_rate;
        const int production_work = order.work_remainder / 100;
        order.work_remainder %= 100;
        order.ticks_remaining = std::max(
            0, order.ticks_remaining - production_work
        );
        if (order.ticks_remaining > 0) {
            continue;
        }
        std::optional<TilePosition> position;
        if (is_ship(order.kind)) {
            const BuildingRules& dock_rules = rules_for(building.kind);
            for (int y = building.position.y - 1;
                 y <= building.position.y + dock_rules.footprint_height &&
                 !position;
                 ++y) {
                for (int x = building.position.x - 1;
                     x <= building.position.x + dock_rules.footprint_width;
                     ++x) {
                    const TilePosition candidate{x, y};
                    if (!map_.contains(candidate)) {
                        continue;
                    }
                    if (map_.sailable(candidate) &&
                        !occupied(candidate, 0)) {
                        position = candidate;
                        break;
                    }
                }
            }
        } else {
            position = spawn_position(building);
        }
        if (!position) {
            continue;
        }
        const int already_completed = static_cast<int>(std::ranges::count_if(
            completed,
            [&building](const CompletedOrder& completed_order) {
                return completed_order.owner == building.owner;
            }
        ));
        if (population(building.owner) + already_completed >=
            population_capacity(building.owner)) {
            continue;
        }
        completed.push_back({
            order.kind,
            order.commercial_identity,
            building.owner,
            *position,
            building.rally_point,
            building.has_rally_point,
        });
        building.production_queue.erase(building.production_queue.begin());
    }

    for (const CompletedOrder& order : completed) {
        const EntityId unit = order.commercial_identity
            ? add_commercial_object(
                  *order.commercial_identity, order.owner, order.position
              )
            : add_unit(order.kind, order.owner, order.position);
        if (order.has_rally_point) {
            command_unit(unit, order.rally_point);
        }
    }
}

std::size_t Simulation::map_index(TilePosition position) const {
    return static_cast<std::size_t>(
        position.y * map_.width() + position.x
    );
}

void Simulation::update_exploration() {
    // Same visibility contract as is_visible, but driven from each
    // vision source instead of from every tile. Scanning the whole map
    // per player per tick made tick cost scale with map area, which is
    // ruinous at the original 120-240 tile presets.
    for (std::size_t index = 0; index < player_states_.size(); ++index) {
        const PlayerSlotId slot = *PlayerSlotId::from_index(index);
        if (!roster_.slot(slot).occupied) continue;
        const EntityOwner player = entity_owner_from_slot(slot);
        std::vector<bool>& explored = player_states_[index].explored;
        const auto mark = [this, &explored](TilePosition tile) {
            if (map_.contains(tile)) {
                explored.at(map_index(tile)) = true;
            }
        };
        // Team-game setup reveals each ally's starting Town Center, but does
        // not grant that building's line of sight before Cartography.  Keep a
        // frozen image in the same per-viewer store used by explored enemy
        // buildings so world/minimap rendering cannot consult hidden live
        // state.  Setup paths finish while tick zero is current; buildings
        // created later in play therefore never enter this special reveal.
        if (tick_number_ == 0) {
            for (const Building& building : buildings_) {
                if (building.hit_points <= 0 ||
                    building.kind != BuildingKind::town_center ||
                    building.owner == player ||
                    !is_ally(player, building.owner)) {
                    continue;
                }
                const BuildingRules& rules = rules_for(building.kind);
                for (int y = 0; y < rules.footprint_height; ++y) {
                    for (int x = 0; x < rules.footprint_width; ++x) {
                        mark({
                            building.position.x + x,
                            building.position.y + y,
                        });
                    }
                }
                player_states_[index].remembered_buildings[building.id] = {
                    building,
                    age(building.owner),
                    civilization(building.owner),
                    maximum_hit_points(building),
                    2,
                };
            }
        }
        if (has_technology(player, Technology::spy_technology)) {
            for (const Unit& unit : units_) {
                if (unit.hit_points > 0 && unit.garrisoned_in == 0 &&
                    is_enemy(player, unit.owner)) {
                    mark(unit.position);
                }
            }
            for (const Building& building : buildings_) {
                if (building.hit_points <= 0 ||
                    !is_enemy(player, building.owner)) {
                    continue;
                }
                const BuildingRules& rules = rules_for(building.kind);
                for (int y = 0; y < rules.footprint_height; ++y) {
                    for (int x = 0; x < rules.footprint_width; ++x) {
                        mark({
                            building.position.x + x,
                            building.position.y + y,
                        });
                    }
                }
            }
        }
        const bool shares_allied_vision =
            has_technology(player, Technology::cartography);
        for (const Unit& unit : units_) {
            if (unit.garrisoned_in != 0) continue;
            if (unit.owner != player &&
                !(shares_allied_vision &&
                  is_ally(unit.owner, player))) {
                continue;
            }
            const int range = effective_unit_vision_range(unit);
            // A negative range still describes a disc of |range| under
            // the squared comparison, so bound the sweep by that reach.
            const int reach = std::abs(range);
            for (int y = unit.position.y - reach;
                 y <= unit.position.y + reach; ++y) {
                for (int x = unit.position.x - reach;
                     x <= unit.position.x + reach; ++x) {
                    const int dx = unit.position.x - x;
                    const int dy = unit.position.y - y;
                    if (dx * dx + dy * dy <= range * range) {
                        mark({x, y});
                    }
                }
            }
        }
        for (const Building& building : buildings_) {
            if (building.owner != player &&
                !(shares_allied_vision &&
                  is_ally(building.owner, player))) {
                continue;
            }
            const int range = effective_building_vision_range(building);
            const int reach = std::abs(range);
            const BuildingRules& rules = rules_for(building.kind);
            for (int y = building.position.y - reach;
                 y < building.position.y + rules.footprint_height + reach;
                 ++y) {
                for (int x = building.position.x - reach;
                     x < building.position.x + rules.footprint_width +
                         reach;
                     ++x) {
                    const TilePosition tile{x, y};
                    if (combat_distance_squared(tile, building) <=
                        range * range) {
                        mark(tile);
                    }
                }
            }
        }

        auto& memories = player_states_[index].remembered_buildings;
        const auto footprint_visible = [this, player](const Building& building) {
            const BuildingRules& rules = rules_for(building.kind);
            for (int y = 0; y < rules.footprint_height; ++y) {
                for (int x = 0; x < rules.footprint_width; ++x) {
                    if (is_visible(player, {
                            building.position.x + x,
                            building.position.y + y,
                        })) {
                        return true;
                    }
                }
            }
            return false;
        };
        for (const Building& building : buildings_) {
            if (!footprint_visible(building)) {
                continue;
            }
            if (!is_enemy(player, building.owner)) {
                // Normal LOS or Cartography refreshes a starting-team
                // snapshot already known to this viewer.  Never create new
                // allied memory here: that would reveal post-start centers.
                if (building.kind == BuildingKind::town_center &&
                    building.owner != player &&
                    memories.contains(building.id)) {
                    memories[building.id] = {
                        building,
                        age(building.owner),
                        civilization(building.owner),
                        maximum_hit_points(building),
                        2,
                    };
                }
                continue;
            }
            const auto adjacent = [
                this, &building, &memories, &footprint_visible
            ](int dx, int dy) {
                return std::ranges::any_of(
                    buildings_, [
                        &building, &memories, &footprint_visible, dx, dy
                    ](const Building& other) {
                        return (footprint_visible(other) ||
                                memories.contains(other.id)) &&
                            other.kind == building.kind &&
                            other.position.x == building.position.x + dx &&
                            other.position.y == building.position.y + dy;
                    }
                );
            };
            const bool connected_x = adjacent(-1, 0) || adjacent(1, 0);
            const bool connected_y = adjacent(0, -1) || adjacent(0, 1);
            memories[building.id] = {
                building,
                age(building.owner),
                civilization(building.owner),
                maximum_hit_points(building),
                connected_x && !connected_y ? 0 :
                    connected_y && !connected_x ? 1 : 2,
            };
        }
        std::erase_if(memories, [this, player, &footprint_visible](const auto& item) {
            const BuildingMemory& memory = item.second;
            const auto live = std::ranges::find_if(
                buildings_, [id = item.first](const Building& building) {
                    return building.id == id;
                }
            );
            if (live != buildings_.end()) {
                // Starting allied Town Centers intentionally use this memory
                // store too.  Preserve their frozen image while allied; a
                // later Cartography/current-LOS pass suppresses it at render
                // time without exposing live hidden state.
                return !is_enemy(player, live->owner) &&
                    memory.building.kind != BuildingKind::town_center;
            }
            return footprint_visible(memory.building);
        });
    }
}

int Simulation::committed_population(Player player) const {
    int committed = population(player);
    for (const Building& building : buildings_) {
        if (building.owner == player) {
            committed += static_cast<int>(
                building.production_queue.size()
            );
        }
    }
    return committed;
}

bool Simulation::has_age_prerequisites(
    Player player,
    Age target
) const {
    const auto has = [this, player](BuildingKind kind) {
        return std::ranges::any_of(
            buildings_,
            [player, kind](const Building& building) {
                return building.owner == player &&
                       building.kind == kind &&
                       building.completed();
            }
        );
    };
    if (target == Age::feudal) {
        int dark_age_buildings = 0;
        for (BuildingKind kind : {
                 BuildingKind::barracks,
                 BuildingKind::mill,
                 BuildingKind::lumber_camp,
                 BuildingKind::mining_camp,
             }) {
            dark_age_buildings += has(kind) ? 1 : 0;
        }
        return dark_age_buildings >= 2;
    }
    if (target == Age::castle) {
        int feudal_age_buildings = 0;
        for (BuildingKind kind : {
                 BuildingKind::archery_range,
                 BuildingKind::stable,
                 BuildingKind::blacksmith,
             }) {
            feudal_age_buildings += has(kind) ? 1 : 0;
        }
        return feudal_age_buildings >= 2;
    }
    if (target == Age::imperial) {
        return has(BuildingKind::castle) ||
            (has(BuildingKind::university) &&
             has(BuildingKind::siege_workshop));
    }
    return false;
}

int Simulation::effective_attack_range(const Unit& unit) const {
    if (unit.commercial_identity) {
        const auto* record = commercial_content_catalog().object(
            unit.commercial_identity->civilization_id,
            unit.commercial_identity->object_id
        );
        if (record) return std::max(
            record->attacks.empty() ? 0 : 1,
            static_cast<int>(std::lround(
            effective_commercial_attribute(
                unit.owner, *unit.commercial_identity, 12,
                record->maximum_range
            )
        )));
    }
    int range = rules_for(unit.kind).attack_range;
    if (unit.kind == UnitKind::archer ||
        unit.kind == UnitKind::crossbowman ||
        unit.kind == UnitKind::arbalester ||
        unit.kind == UnitKind::skirmisher ||
        unit.kind == UnitKind::elite_skirmisher) {
        range += has_technology(
            unit.owner, Technology::fletching
        ) ? 1 : 0;
        range += has_technology(
            unit.owner, Technology::bodkin_arrow
        ) ? 1 : 0;
        range += has_technology(
            unit.owner, Technology::bracer
        ) ? 1 : 0;
    }
    if (civilization(unit.owner) == Civilization::britons &&
        (unit.kind == UnitKind::archer ||
         unit.kind == UnitKind::crossbowman ||
         unit.kind == UnitKind::arbalester)) {
        range += age(unit.owner) >= Age::castle ? 1 : 0;
        range += age(unit.owner) >= Age::imperial ? 1 : 0;
    }
    const bool foot_archer_class =
        is_archer(unit.kind) &&
        unit.kind != UnitKind::janissary &&
        unit.kind != UnitKind::elite_janissary &&
        unit.kind != UnitKind::mangudai &&
        unit.kind != UnitKind::elite_mangudai &&
        unit.kind != UnitKind::cavalry_archer &&
        unit.kind != UnitKind::heavy_cavalry_archer;
    if (foot_archer_class &&
        has_technology(unit.owner, Technology::yeomen)) {
        ++range;
    }
    if ((unit.kind == UnitKind::throwing_axeman ||
         unit.kind == UnitKind::elite_throwing_axeman) &&
        has_technology(unit.owner, Technology::bearded_axe)) {
        ++range;
    }
    if ((unit.kind == UnitKind::cannon_galleon ||
         unit.kind == UnitKind::elite_cannon_galleon) &&
        has_technology(unit.owner, Technology::artillery)) {
        range += 2;
    }
    if ((unit.kind == UnitKind::mangonel ||
         unit.kind == UnitKind::onager ||
         unit.kind == UnitKind::siege_onager) &&
        has_technology(unit.owner, Technology::shinkichon)) {
        ++range;
    }
    if (!is_ram(unit.kind) &&
        receives_siege_engineers(unit.kind) &&
        has_technology(unit.owner, Technology::siege_engineers)) {
        ++range;
    }
    return range;
}

int Simulation::effective_minimum_attack_range(const Unit& unit) const {
    if (unit.commercial_identity) {
        const auto* record = commercial_content_catalog().object(
            unit.commercial_identity->civilization_id,
            unit.commercial_identity->object_id
        );
        if (record) return std::max(0, static_cast<int>(std::lround(
            effective_commercial_attribute(
                unit.owner, *unit.commercial_identity, 20,
                record->minimum_range
            )
        )));
    }
    const bool mangonel_line =
        unit.kind == UnitKind::mangonel ||
        unit.kind == UnitKind::onager ||
        unit.kind == UnitKind::siege_onager;
    return rules_for(unit.kind).minimum_attack_range +
        (mangonel_line &&
         team_has_civilization(unit.owner, Civilization::koreans) ? 1 : 0);
}

int Simulation::effective_attack_interval(const Unit& unit) const {
    if (unit.commercial_identity) {
        const auto* record = commercial_content_catalog().object(
            unit.commercial_identity->civilization_id,
            unit.commercial_identity->object_id
        );
        if (record) return std::max(1, static_cast<int>(std::lround(
            effective_commercial_attribute(
                unit.owner, *unit.commercial_identity, 10,
                record->reload_time
            ) * 5.0f
        )));
    }
    int interval = rules_for(unit.kind).attack_interval_ticks;
    if (has_technology(unit.owner, Technology::thumb_ring)) {
        if (unit.kind == UnitKind::chu_ko_nu ||
            unit.kind == UnitKind::elite_chu_ko_nu) {
            interval = std::max(1, interval * 80 / 100);
        } else if (
            unit.kind == UnitKind::archer ||
            unit.kind == UnitKind::crossbowman ||
            unit.kind == UnitKind::arbalester ||
            unit.kind == UnitKind::cavalry_archer ||
            unit.kind == UnitKind::heavy_cavalry_archer ||
            unit.kind == UnitKind::mangudai ||
            unit.kind == UnitKind::elite_mangudai ||
            unit.kind == UnitKind::plumed_archer ||
            unit.kind == UnitKind::elite_plumed_archer
        ) {
            interval = std::max(1, interval * 85 / 100);
        }
    }
    if (unit.kind == UnitKind::trebuchet &&
        has_technology(unit.owner, Technology::kataparuto)) {
        interval = interval * 3 / 4;
    }
    if ((civilization(unit.owner) == Civilization::celts ||
         civilization(unit.owner) == Civilization::mongols) &&
        (is_ram(unit.kind) ||
         unit.kind == UnitKind::mangonel)) {
        return std::max(1, interval * 3 / 4);
    }
    const bool infantry =
        unit.kind == UnitKind::militia ||
        unit.kind == UnitKind::man_at_arms ||
        unit.kind == UnitKind::long_swordsman ||
        unit.kind == UnitKind::two_handed_swordsman ||
        unit.kind == UnitKind::champion ||
        unit.kind == UnitKind::spearman ||
        unit.kind == UnitKind::pikeman ||
        unit.kind == UnitKind::halberdier;
    if (civilization(unit.owner) == Civilization::japanese &&
        infantry && age(unit.owner) >= Age::feudal) {
        return std::max(1, interval * 3 / 4);
    }
    return interval;
}

int Simulation::effective_movement_interval(const Unit& unit) const {
    if (unit.commercial_identity) {
        const auto* record = commercial_content_catalog().object(
            unit.commercial_identity->civilization_id,
            unit.commercial_identity->object_id
        );
        if (record) {
            const float speed = effective_commercial_attribute(
                unit.owner, *unit.commercial_identity, 5, record->speed
            );
            return speed <= 0.0f ? std::numeric_limits<int>::max()
                                 : std::max(1, static_cast<int>(std::lround(5.0f / speed)));
        }
    }
    return rules_for(unit.kind).movement_interval_ticks;
}

int Simulation::effective_unit_vision_range(const Unit& unit) const {
    if (unit.commercial_identity) {
        const auto* record = commercial_content_catalog().object(
            unit.commercial_identity->civilization_id,
            unit.commercial_identity->object_id
        );
        if (record) return std::max(0, static_cast<int>(std::lround(
            effective_commercial_attribute(
                unit.owner, *unit.commercial_identity, 1,
                record->line_of_sight
            )
        )));
    }
    int range = rules_for(unit.kind).vision_range;
    if (is_infantry(unit.kind) &&
        has_technology(unit.owner, Technology::tracking)) {
        range += 2;
    }
    if ((unit.kind == UnitKind::monk ||
         unit.kind == UnitKind::missionary) &&
        has_technology(unit.owner, Technology::block_printing)) {
        range += 3;
    }
    const bool foot_archer_class =
        is_archer(unit.kind) &&
        unit.kind != UnitKind::janissary &&
        unit.kind != UnitKind::elite_janissary &&
        unit.kind != UnitKind::mangudai &&
        unit.kind != UnitKind::elite_mangudai &&
        unit.kind != UnitKind::cavalry_archer &&
        unit.kind != UnitKind::heavy_cavalry_archer;
    if (foot_archer_class &&
        has_technology(unit.owner, Technology::yeomen)) {
        ++range;
    }
    if ((unit.kind == UnitKind::throwing_axeman ||
         unit.kind == UnitKind::elite_throwing_axeman) &&
        has_technology(unit.owner, Technology::bearded_axe)) {
        ++range;
    }
    if ((unit.kind == UnitKind::cannon_galleon ||
         unit.kind == UnitKind::elite_cannon_galleon) &&
        has_technology(unit.owner, Technology::artillery)) {
        range += 2;
    }
    if ((unit.kind == UnitKind::mangonel ||
         unit.kind == UnitKind::onager ||
         unit.kind == UnitKind::siege_onager) &&
        has_technology(unit.owner, Technology::shinkichon)) {
        ++range;
    }
    if (receives_siege_engineers(unit.kind) &&
        has_technology(unit.owner, Technology::siege_engineers)) {
        ++range;
    }
    if (civilization(unit.owner) == Civilization::koreans &&
        unit.kind == UnitKind::villager) {
        range += 3;
    }
    if (team_has_civilization(unit.owner, Civilization::franks) &&
        (unit.kind == UnitKind::knight ||
         unit.kind == UnitKind::cavalier ||
         unit.kind == UnitKind::paladin)) {
        range += 2;
    }
    if (team_has_civilization(unit.owner, Civilization::japanese) &&
        (unit.kind == UnitKind::galley ||
         unit.kind == UnitKind::war_galley ||
         unit.kind == UnitKind::galleon)) {
        range = range * 3 / 2;
    }
    const bool scout_line =
        unit.kind == UnitKind::scout_cavalry ||
        unit.kind == UnitKind::light_cavalry ||
        unit.kind == UnitKind::hussar;
    if (team_has_civilization(unit.owner, Civilization::mongols) &&
        scout_line) {
        range += 2;
    }
    if (unit.kind != UnitKind::scout_cavalry) {
        return range;
    }
    const Age owner_age = age(unit.owner);
    if (owner_age >= Age::feudal) {
        range += 2;
    }
    if (owner_age >= Age::castle) {
        range += 2;
    }
    if (owner_age >= Age::imperial) {
        range += 2;
    }
    return range;
}

int Simulation::effective_building_attack(
    const Building& building
) const {
    int attack = rules_for(building.kind).attack;
    if (building.kind == BuildingKind::castle ||
        building.kind == BuildingKind::watch_tower ||
        building.kind == BuildingKind::guard_tower ||
        building.kind == BuildingKind::keep ||
        building.kind == BuildingKind::town_center) {
        attack += has_technology(
            building.owner, Technology::fletching
        ) ? 1 : 0;
        attack += has_technology(
            building.owner, Technology::bodkin_arrow
        ) ? 1 : 0;
        attack += has_technology(
            building.owner, Technology::bracer
        ) ? 1 : 0;
        attack += has_technology(
            building.owner, Technology::chemistry
        ) ? 1 : 0;
    }
    if ((building.kind == BuildingKind::watch_tower ||
         building.kind == BuildingKind::guard_tower ||
         building.kind == BuildingKind::keep) &&
        has_technology(building.owner, Technology::yeomen)) {
        attack += 2;
    }
    return attack;
}

int Simulation::effective_building_attack_range(
    const Building& building
) const {
    int range = rules_for(building.kind).attack_range;
    if (building.kind == BuildingKind::castle ||
        building.kind == BuildingKind::watch_tower ||
        building.kind == BuildingKind::guard_tower ||
        building.kind == BuildingKind::keep) {
        range += has_technology(
            building.owner, Technology::fletching
        ) ? 1 : 0;
        range += has_technology(
            building.owner, Technology::bodkin_arrow
        ) ? 1 : 0;
        range += has_technology(
            building.owner, Technology::bracer
        ) ? 1 : 0;
    }
    if (building.kind == BuildingKind::castle &&
        has_technology(building.owner, Technology::crenellations)) {
        range += 3;
    }
    return range;
}

int Simulation::effective_building_vision_range(
    const Building& building
) const {
    int range = rules_for(building.kind).vision_range;
    if (receives_class_3_or_52_effects(building.kind)) {
        range += has_technology(
            building.owner, Technology::town_watch
        ) ? 4 : 0;
        range += has_technology(
            building.owner, Technology::town_patrol
        ) ? 4 : 0;
    }
    if (building.kind == BuildingKind::castle ||
        building.kind == BuildingKind::watch_tower ||
        building.kind == BuildingKind::guard_tower ||
        building.kind == BuildingKind::keep ||
        building.kind == BuildingKind::town_center) {
        range += has_technology(
            building.owner, Technology::fletching
        ) ? 1 : 0;
        range += has_technology(
            building.owner, Technology::bodkin_arrow
        ) ? 1 : 0;
        range += has_technology(
            building.owner, Technology::bracer
        ) ? 1 : 0;
    }
    if (building.kind == BuildingKind::castle &&
        has_technology(building.owner, Technology::crenellations)) {
        range += 3;
    }
    return range;
}

int Simulation::cavalry_movement_numerator(const Unit& unit) const {
    // Fixed-point source speed with denominator 320:
    // Dark 1.20 / Villager 0.80 = 240/320 simulation tiles per tick;
    // Feudal+ 1.55 / Villager 0.80 = 310/320;
    // Knight 1.35 / Villager 0.80 = 270/320;
    // Light Cavalry 1.50 / Villager 0.80 = 300/320.
    int numerator =
        (unit.kind == UnitKind::light_cavalry ||
         unit.kind == UnitKind::hussar)
        ? 300
        : (unit.kind == UnitKind::camel_rider ||
           unit.kind == UnitKind::heavy_camel)
        ? 290
        : (unit.kind == UnitKind::knight ||
         unit.kind == UnitKind::cavalier ||
         unit.kind == UnitKind::paladin)
        ? 270
        : (age(unit.owner) >= Age::feudal ? 310 : 240);
    if (has_technology(unit.owner, Technology::husbandry)) {
        numerator = numerator * 11 / 10;
    }
    return numerator;
}

int Simulation::ship_movement_numerator(const Unit& unit) const {
    int numerator =
        (unit.kind == UnitKind::longboat ||
         unit.kind == UnitKind::elite_longboat) ? 154 :
        (unit.kind == UnitKind::turtle_ship ||
         unit.kind == UnitKind::elite_turtle_ship) ? 90 :
        unit.kind == UnitKind::trade_cog ? 132 :
        (unit.kind == UnitKind::cannon_galleon ||
         unit.kind == UnitKind::elite_cannon_galleon) ? 110 : 100;
    if (has_technology(unit.owner, Technology::dry_dock)) {
        numerator = numerator * 115 / 100;
    }
    if (unit.kind == UnitKind::trade_cog &&
        has_technology(unit.owner, Technology::caravan)) {
        numerator = numerator * 3 / 2;
    }
    return numerator;
}

int Simulation::effective_siege_movement_numerator(
    const Unit& unit
) const {
    if (is_ram(unit.kind)) {
        const int base =
            unit.kind == UnitKind::siege_ram ? 60 : 50;
        return has_technology(unit.owner, Technology::drill)
            ? base * 3 / 2 : base;
    }
    if (unit.kind == UnitKind::mangonel ||
        unit.kind == UnitKind::onager ||
        unit.kind == UnitKind::siege_onager) {
        return has_technology(unit.owner, Technology::drill) ? 90 : 60;
    }
    if (unit.kind == UnitKind::scorpion ||
        unit.kind == UnitKind::heavy_scorpion) {
        return has_technology(unit.owner, Technology::drill) ? 97 : 65;
    }
    return 100;
}

int Simulation::berserk_regeneration_per_three_ticks(
    const Unit& unit
) const {
    if (unit.kind != UnitKind::berserk &&
        unit.kind != UnitKind::elite_berserk) {
        return 0;
    }
    return has_technology(unit.owner, Technology::berserkergang) ? 2 : 1;
}

int Simulation::unique_unit_movement_numerator(const Unit& unit) const {
    const int siege_numerator = effective_siege_movement_numerator(unit);
    if (siege_numerator != 100) return siege_numerator;
    if (unit.kind == UnitKind::cavalry_archer ||
        unit.kind == UnitKind::heavy_cavalry_archer) {
        return has_technology(unit.owner, Technology::husbandry) ? 154 : 140;
    }
    if (unit.kind == UnitKind::hand_cannoneer) return 96;
    if (unit.kind == UnitKind::bombard_cannon) {
        return has_technology(unit.owner, Technology::drill) ? 105 : 70;
    }
    if (unit.kind == UnitKind::petard) return 80;
    if (unit.kind == UnitKind::trade_cart &&
        has_technology(unit.owner, Technology::caravan)) {
        return 150;
    }
    if (unit.kind == UnitKind::villager &&
        has_technology(unit.owner, Technology::wheelbarrow)) {
        return has_technology(unit.owner, Technology::hand_cart)
            ? 121 : 110;
    }
    if (unit.kind == UnitKind::monk ||
        unit.kind == UnitKind::missionary) {
        const int base =
            unit.kind == UnitKind::missionary ? 110 : 100;
        return has_technology(unit.owner, Technology::fervor)
            ? base * 115 / 100
            : base;
    }
    const int base =
        (unit.kind == UnitKind::longbowman ||
         unit.kind == UnitKind::elite_longbowman) ? 96 :
        (unit.kind == UnitKind::throwing_axeman ||
         unit.kind == UnitKind::elite_throwing_axeman) ? 90 :
        (unit.kind == UnitKind::huskarl ||
         unit.kind == UnitKind::elite_huskarl) ? 105 :
        (unit.kind == UnitKind::teutonic_knight ||
         unit.kind == UnitKind::elite_teutonic_knight) ? 65 :
        (unit.kind == UnitKind::chu_ko_nu ||
         unit.kind == UnitKind::elite_chu_ko_nu) ? 96 :
        (unit.kind == UnitKind::cataphract ||
         unit.kind == UnitKind::elite_cataphract) ? 135 :
        (unit.kind == UnitKind::war_elephant ||
         unit.kind == UnitKind::elite_war_elephant)
            ? (has_technology(unit.owner, Technology::mahouts) ? 78 : 60) :
        (unit.kind == UnitKind::mameluke ||
         unit.kind == UnitKind::elite_mameluke) ? 140 :
        (unit.kind == UnitKind::janissary ||
         unit.kind == UnitKind::elite_janissary) ? 96 :
        (unit.kind == UnitKind::berserk ||
         unit.kind == UnitKind::elite_berserk) ? 105 :
        (unit.kind == UnitKind::mangudai ||
         unit.kind == UnitKind::elite_mangudai) ? 145 :
        (unit.kind == UnitKind::plumed_archer ||
         unit.kind == UnitKind::elite_plumed_archer) ? 120 :
        (unit.kind == UnitKind::conquistador ||
         unit.kind == UnitKind::elite_conquistador) ? 130 :
        (unit.kind == UnitKind::tarkan ||
         unit.kind == UnitKind::elite_tarkan) ? 135 : 100;
    return is_infantry(unit.kind) &&
        has_technology(unit.owner, Technology::squires)
        ? base * 110 / 100
        : base;
}

int Simulation::transport_capacity(Player player) const {
    if (has_technology(player, Technology::dry_dock)) return 15;
    if (has_technology(player, Technology::careening)) return 10;
    return 5;
}

int Simulation::garrison_capacity(BuildingKind building) const {
    switch (building) {
        case BuildingKind::town_center:
            return 15;
        case BuildingKind::castle:
            return 20;
        case BuildingKind::watch_tower:
        case BuildingKind::guard_tower:
        case BuildingKind::keep:
        case BuildingKind::bombard_tower:
            return 5;
        default:
            return 0;
    }
}

int Simulation::carry_capacity(const Unit& unit) const {
    if (unit.commercial_identity) {
        return std::max(0, static_cast<int>(std::lround(
            effective_commercial_attribute(
                unit.owner, *unit.commercial_identity, 14, 10.0f
            )
        )));
    }
    if (unit.kind == UnitKind::fishing_ship) {
        return 15;
    }
    int capacity = 10;
    if (unit.kind == UnitKind::villager &&
        has_technology(unit.owner, Technology::wheelbarrow)) {
        capacity = capacity * 5 / 4;
    }
    if (unit.kind == UnitKind::villager &&
        has_technology(unit.owner, Technology::hand_cart)) {
        capacity = capacity * 3 / 2;
    }
    const auto active_farm_iterator = std::ranges::find_if(
        buildings_,
        [&unit](const Building& building) {
            return building.id == unit.resource_building_id;
        }
    );
    const Building* active_farm =
        active_farm_iterator == buildings_.end()
            ? nullptr : &*active_farm_iterator;
    if (unit.kind == UnitKind::villager &&
        unit.has_resource_target &&
        !unit.returning_resource &&
        unit.carried_resource == ResourceKind::food &&
        active_farm != nullptr &&
        active_farm->kind == BuildingKind::farm &&
        has_technology(unit.owner, Technology::heavy_plow)) {
        ++capacity;
    }
    if (unit.kind == UnitKind::villager &&
        civilization(unit.owner) == Civilization::aztecs) {
        capacity += 3;
    }
    return capacity;
}

bool Simulation::can_garrison(
    const Unit& unit,
    const Building& building
) const {
    if (!building.completed() ||
        garrison_capacity(building.kind) == 0 ||
        building.owner != unit.owner) {
        return false;
    }
    const bool is_siege_unit =
        is_ram(unit.kind) ||
        unit.kind == UnitKind::mangonel ||
        unit.kind == UnitKind::onager ||
        unit.kind == UnitKind::siege_onager ||
        unit.kind == UnitKind::scorpion ||
        unit.kind == UnitKind::heavy_scorpion ||
        unit.kind == UnitKind::packed_trebuchet ||
        unit.kind == UnitKind::trebuchet ||
        unit.kind == UnitKind::bombard_cannon;
    const bool human =
        !is_ship(unit.kind) && !is_siege_unit &&
        !is_herdable(unit.kind) && !is_huntable(unit.kind) &&
        !is_relic(unit.kind) &&
        unit.kind != UnitKind::trade_cart;
    if (!human) {
        return false;
    }
    if (building.kind == BuildingKind::castle) {
        return true;
    }
    return unit.kind == UnitKind::villager ||
        unit.kind == UnitKind::monk ||
        is_infantry(unit.kind) ||
        (is_archer(unit.kind) && !is_cavalry(unit.kind));
}

void Simulation::apply_man_at_arms_upgrade(Player player) {
    const int upgraded_hit_points =
        rules_for(UnitKind::man_at_arms).hit_points;
    for (Unit& unit : units_) {
        if (unit.owner == player && unit.kind == UnitKind::militia) {
            unit.kind = UnitKind::man_at_arms;
            unit.hit_points = std::min(
                upgraded_hit_points,
                unit.hit_points +
                    upgraded_hit_points -
                    rules_for(UnitKind::militia).hit_points
            );
        }
    }
    for (Building& building : buildings_) {
        if (building.owner != player) {
            continue;
        }
        for (ProductionOrder& order : building.production_queue) {
            if (order.kind == UnitKind::militia) {
                order.kind = UnitKind::man_at_arms;
            }
        }
    }
}

void Simulation::apply_crossbowman_upgrade(Player player) {
    const int upgraded_hit_points =
        rules_for(UnitKind::crossbowman).hit_points;
    for (Unit& unit : units_) {
        if (unit.owner == player && unit.kind == UnitKind::archer) {
            unit.kind = UnitKind::crossbowman;
            unit.hit_points = std::min(
                upgraded_hit_points,
                unit.hit_points +
                    upgraded_hit_points -
                    rules_for(UnitKind::archer).hit_points
            );
        }
    }
    for (Building& building : buildings_) {
        if (building.owner != player) {
            continue;
        }
        for (ProductionOrder& order : building.production_queue) {
            if (order.kind == UnitKind::archer) {
                order.kind = UnitKind::crossbowman;
            }
        }
    }
}

void Simulation::apply_arbalester_upgrade(Player player) {
    for (Unit& unit : units_) {
        if (unit.owner != player ||
            (unit.kind != UnitKind::archer &&
             unit.kind != UnitKind::crossbowman)) {
            continue;
        }
        const int old_maximum = maximum_hit_points(unit);
        const int damage = old_maximum - unit.hit_points;
        unit.kind = UnitKind::arbalester;
        unit.hit_points = std::max(
            0,
            maximum_hit_points(unit) - damage
        );
    }
    for (Building& building : buildings_) {
        if (building.owner != player) {
            continue;
        }
        for (ProductionOrder& order : building.production_queue) {
            if (order.kind == UnitKind::archer ||
                order.kind == UnitKind::crossbowman) {
                order.kind = UnitKind::arbalester;
            }
        }
    }
}

void Simulation::apply_elite_skirmisher_upgrade(Player player) {
    for (Unit& unit : units_) {
        if (unit.owner != player ||
            unit.kind != UnitKind::skirmisher) {
            continue;
        }
        const int old_maximum = maximum_hit_points(unit);
        const int damage = old_maximum - unit.hit_points;
        unit.kind = UnitKind::elite_skirmisher;
        unit.hit_points = std::max(
            0,
            maximum_hit_points(unit) - damage
        );
    }
    for (Building& building : buildings_) {
        if (building.owner != player) {
            continue;
        }
        for (ProductionOrder& order : building.production_queue) {
            if (order.kind == UnitKind::skirmisher) {
                order.kind = UnitKind::elite_skirmisher;
            }
        }
    }
}

void Simulation::apply_pikeman_upgrade(Player player) {
    const int upgraded_hit_points = rules_for(UnitKind::pikeman).hit_points;
    for (Unit& unit : units_) {
        if (unit.owner == player && unit.kind == UnitKind::spearman) {
            unit.kind = UnitKind::pikeman;
            unit.hit_points = std::min(
                upgraded_hit_points,
                unit.hit_points +
                    upgraded_hit_points -
                    rules_for(UnitKind::spearman).hit_points
            );
        }
    }
    for (Building& building : buildings_) {
        if (building.owner != player) {
            continue;
        }
        for (ProductionOrder& order : building.production_queue) {
            if (order.kind == UnitKind::spearman) {
                order.kind = UnitKind::pikeman;
            }
        }
    }
}

void Simulation::apply_long_swordsman_upgrade(Player player) {
    const int upgraded_hit_points =
        rules_for(UnitKind::long_swordsman).hit_points;
    for (Unit& unit : units_) {
        if (unit.owner == player &&
            (unit.kind == UnitKind::militia ||
             unit.kind == UnitKind::man_at_arms)) {
            const int old_hit_points = rules_for(unit.kind).hit_points;
            unit.kind = UnitKind::long_swordsman;
            unit.hit_points = std::min(
                upgraded_hit_points,
                unit.hit_points + upgraded_hit_points - old_hit_points
            );
        }
    }
    for (Building& building : buildings_) {
        if (building.owner != player) {
            continue;
        }
        for (ProductionOrder& order : building.production_queue) {
            if (order.kind == UnitKind::militia ||
                order.kind == UnitKind::man_at_arms) {
                order.kind = UnitKind::long_swordsman;
            }
        }
    }
}

void Simulation::apply_two_handed_swordsman_upgrade(Player player) {
    for (Unit& unit : units_) {
        if (unit.owner != player ||
            (unit.kind != UnitKind::militia &&
             unit.kind != UnitKind::man_at_arms &&
             unit.kind != UnitKind::long_swordsman)) {
            continue;
        }
        const int old_maximum = maximum_hit_points(unit);
        const int damage = old_maximum - unit.hit_points;
        unit.kind = UnitKind::two_handed_swordsman;
        unit.hit_points = std::max(
            0,
            maximum_hit_points(unit) - damage
        );
    }
    for (Building& building : buildings_) {
        if (building.owner != player) {
            continue;
        }
        for (ProductionOrder& order : building.production_queue) {
            if (order.kind == UnitKind::militia ||
                order.kind == UnitKind::man_at_arms ||
                order.kind == UnitKind::long_swordsman) {
                order.kind = UnitKind::two_handed_swordsman;
            }
        }
    }
}

void Simulation::apply_champion_upgrade(Player player) {
    for (Unit& unit : units_) {
        if (unit.owner != player ||
            (unit.kind != UnitKind::militia &&
             unit.kind != UnitKind::man_at_arms &&
             unit.kind != UnitKind::long_swordsman &&
             unit.kind != UnitKind::two_handed_swordsman)) {
            continue;
        }
        const int old_maximum = maximum_hit_points(unit);
        const int damage = old_maximum - unit.hit_points;
        unit.kind = UnitKind::champion;
        unit.hit_points = std::max(
            0,
            maximum_hit_points(unit) - damage
        );
    }
    for (Building& building : buildings_) {
        if (building.owner != player) {
            continue;
        }
        for (ProductionOrder& order : building.production_queue) {
            if (order.kind == UnitKind::militia ||
                order.kind == UnitKind::man_at_arms ||
                order.kind == UnitKind::long_swordsman ||
                order.kind == UnitKind::two_handed_swordsman) {
                order.kind = UnitKind::champion;
            }
        }
    }
}

void Simulation::apply_cavalier_upgrade(Player player) {
    for (Unit& unit : units_) {
        if (unit.owner != player || unit.kind != UnitKind::knight) {
            continue;
        }
        const int old_maximum = maximum_hit_points(unit);
        const int damage = old_maximum - unit.hit_points;
        unit.kind = UnitKind::cavalier;
        unit.hit_points = std::max(
            0,
            maximum_hit_points(unit) - damage
        );
    }
    for (Building& building : buildings_) {
        if (building.owner != player) {
            continue;
        }
        for (ProductionOrder& order : building.production_queue) {
            if (order.kind == UnitKind::knight) {
                order.kind = UnitKind::cavalier;
            }
        }
    }
}

void Simulation::apply_paladin_upgrade(Player player) {
    for (Unit& unit : units_) {
        if (unit.owner != player ||
            (unit.kind != UnitKind::knight &&
             unit.kind != UnitKind::cavalier)) {
            continue;
        }
        const int old_maximum = maximum_hit_points(unit);
        const int damage = old_maximum - unit.hit_points;
        unit.kind = UnitKind::paladin;
        unit.hit_points = std::max(
            0,
            maximum_hit_points(unit) - damage
        );
    }
    for (Building& building : buildings_) {
        if (building.owner != player) {
            continue;
        }
        for (ProductionOrder& order : building.production_queue) {
            if (order.kind == UnitKind::knight ||
                order.kind == UnitKind::cavalier) {
                order.kind = UnitKind::paladin;
            }
        }
    }
}

void Simulation::apply_light_cavalry_upgrade(Player player) {
    for (Unit& unit : units_) {
        if (unit.owner != player ||
            unit.kind != UnitKind::scout_cavalry) {
            continue;
        }
        const int old_maximum = maximum_hit_points(unit);
        const int damage = old_maximum - unit.hit_points;
        unit.kind = UnitKind::light_cavalry;
        unit.hit_points = std::max(
            0,
            maximum_hit_points(unit) - damage
        );
    }
    for (Building& building : buildings_) {
        if (building.owner != player) {
            continue;
        }
        for (ProductionOrder& order : building.production_queue) {
            if (order.kind == UnitKind::scout_cavalry) {
                order.kind = UnitKind::light_cavalry;
            }
        }
    }
}

void Simulation::apply_hussar_upgrade(Player player) {
    for (Unit& unit : units_) {
        if (unit.owner != player ||
            (unit.kind != UnitKind::scout_cavalry &&
             unit.kind != UnitKind::light_cavalry)) {
            continue;
        }
        const int old_maximum = maximum_hit_points(unit);
        const int damage = old_maximum - unit.hit_points;
        unit.kind = UnitKind::hussar;
        unit.hit_points = std::max(
            0,
            maximum_hit_points(unit) - damage
        );
    }
    for (Building& building : buildings_) {
        if (building.owner != player) {
            continue;
        }
        for (ProductionOrder& order : building.production_queue) {
            if (order.kind == UnitKind::scout_cavalry ||
                order.kind == UnitKind::light_cavalry) {
                order.kind = UnitKind::hussar;
            }
        }
    }
}

void Simulation::apply_loom_upgrade(Player player) {
    for (Unit& unit : units_) {
        if (unit.owner == player && unit.kind == UnitKind::villager) {
            unit.hit_points = std::min(40, unit.hit_points + 15);
        }
    }
}

void Simulation::apply_bloodlines_upgrade(Player player) {
    for (Unit& unit : units_) {
        if (unit.owner == player && is_cavalry(unit.kind)) {
            unit.hit_points = std::min(
                rules_for(unit.kind).hit_points + 20,
                unit.hit_points + 20
            );
        }
    }
}

void Simulation::apply_horse_collar_upgrade(Player player) {
    for (Building& building : buildings_) {
        if (building.owner == player &&
            building.kind == BuildingKind::farm &&
            building.resource_amount > 0) {
            building.resource_amount =
                std::min(250, building.resource_amount + 75);
        }
    }
}

void Simulation::apply_fortified_wall_upgrade(Player player) {
    for (Building& building : buildings_) {
        if (building.owner != player) {
            continue;
        }
        if (building.kind == BuildingKind::stone_wall) {
            building.hit_points = std::min(3000, building.hit_points + 1200);
            building.kind = BuildingKind::fortified_wall;
        } else if (building.kind == BuildingKind::stone_gate_x ||
                   building.kind == BuildingKind::stone_gate_y) {
            building.hit_points = std::min(4000, building.hit_points + 1250);
            building.kind = building.kind == BuildingKind::stone_gate_x
                ? BuildingKind::fortified_gate_x
                : BuildingKind::fortified_gate_y;
        }
    }
}

void Simulation::apply_guard_tower_upgrade(Player player) {
    for (Building& building : buildings_) {
        if (building.owner == player &&
            building.kind == BuildingKind::watch_tower) {
            building.hit_points =
                std::min(1500, building.hit_points + 480);
            building.kind = BuildingKind::guard_tower;
        }
    }
}

void Simulation::apply_keep_upgrade(Player player) {
    for (Building& building : buildings_) {
        if (building.owner == player &&
            (building.kind == BuildingKind::watch_tower ||
             building.kind == BuildingKind::guard_tower)) {
            building.hit_points =
                std::min(2250, building.hit_points + 750);
            building.kind = BuildingKind::keep;
        }
    }
}

void Simulation::refresh_unit_attacks(Player player) {
    for (Unit& unit : units_) {
        if (unit.owner != player) {
            continue;
        }
        unit.attack = rules_for(unit.kind).attack;
        if (rules_for(unit.kind).attack_range > 1 &&
            unit.kind != UnitKind::hand_cannoneer &&
            unit.kind != UnitKind::bombard_cannon &&
            has_technology(player, Technology::chemistry)) {
            ++unit.attack;
        }
        if (unit.kind == UnitKind::scout_cavalry &&
            age(player) >= Age::feudal) {
            unit.attack += 2;
        }
        if (is_archer(unit.kind) &&
            unit.kind != UnitKind::janissary &&
            unit.kind != UnitKind::elite_janissary &&
            unit.kind != UnitKind::hand_cannoneer) {
            unit.attack += has_technology(
                player, Technology::fletching
            ) ? 1 : 0;
            unit.attack += has_technology(
                player, Technology::bodkin_arrow
            ) ? 1 : 0;
            unit.attack += has_technology(
                player, Technology::bracer
            ) ? 1 : 0;
        }
        if ((unit.kind == UnitKind::chu_ko_nu ||
             unit.kind == UnitKind::elite_chu_ko_nu) &&
            has_technology(player, Technology::rocketry)) {
            unit.attack += 2;
        }
        if ((unit.kind == UnitKind::scorpion ||
             unit.kind == UnitKind::heavy_scorpion) &&
            has_technology(player, Technology::rocketry)) {
            unit.attack += 4;
        }
        if (unit.kind == UnitKind::villager &&
            has_technology(player, Technology::supremacy)) {
            unit.attack += 6;
        }
        if (unit.kind == UnitKind::knight ||
             unit.kind == UnitKind::cavalier ||
             unit.kind == UnitKind::paladin ||
             unit.kind == UnitKind::light_cavalry ||
             unit.kind == UnitKind::hussar ||
             unit.kind == UnitKind::scout_cavalry ||
             unit.kind == UnitKind::militia ||
             unit.kind == UnitKind::man_at_arms ||
             unit.kind == UnitKind::long_swordsman ||
             unit.kind == UnitKind::two_handed_swordsman ||
             unit.kind == UnitKind::champion ||
             unit.kind == UnitKind::spearman ||
             unit.kind == UnitKind::pikeman ||
             unit.kind == UnitKind::halberdier ||
             unit.kind == UnitKind::throwing_axeman ||
             unit.kind == UnitKind::elite_throwing_axeman ||
             unit.kind == UnitKind::huskarl ||
             unit.kind == UnitKind::elite_huskarl ||
             unit.kind == UnitKind::teutonic_knight ||
             unit.kind == UnitKind::elite_teutonic_knight ||
             unit.kind == UnitKind::samurai ||
             unit.kind == UnitKind::elite_samurai ||
             unit.kind == UnitKind::cataphract ||
             unit.kind == UnitKind::elite_cataphract ||
             unit.kind == UnitKind::war_elephant ||
             unit.kind == UnitKind::elite_war_elephant ||
             unit.kind == UnitKind::mameluke ||
             unit.kind == UnitKind::elite_mameluke ||
             unit.kind == UnitKind::berserk ||
             unit.kind == UnitKind::elite_berserk ||
             unit.kind == UnitKind::jaguar_warrior ||
             unit.kind == UnitKind::elite_jaguar_warrior ||
             unit.kind == UnitKind::tarkan ||
             unit.kind == UnitKind::elite_tarkan ||
             unit.kind == UnitKind::camel_rider ||
             unit.kind == UnitKind::heavy_camel) {
            unit.attack += has_technology(
                player, Technology::forging
            ) ? 1 : 0;
            unit.attack += has_technology(
                player, Technology::iron_casting
            ) ? 1 : 0;
            unit.attack += has_technology(
                player, Technology::blast_furnace
            ) ? 2 : 0;
        }
    }
}

bool Simulation::automatic_splash_target_is_safe(
    const Unit& attacker,
    TilePosition impact
) const {
    if (attacker.kind != UnitKind::mangonel &&
        attacker.kind != UnitKind::onager &&
        attacker.kind != UnitKind::siege_onager) {
        return true;
    }
    const int radius = rules_for(attacker.kind).splash_radius;
    const auto friendly = [this, &attacker](EntityOwner owner) {
        return owner == attacker.owner || is_ally(owner, attacker.owner);
    };
    for (const Unit& candidate : units_) {
        if (candidate.id == attacker.id || candidate.hit_points <= 0 ||
            candidate.garrisoned_in != 0 || !friendly(candidate.owner)) {
            continue;
        }
        if (std::abs(candidate.position.x - impact.x) +
                std::abs(candidate.position.y - impact.y) <= radius) {
            return false;
        }
    }
    for (const Building& candidate : buildings_) {
        if (candidate.hit_points > 0 && friendly(candidate.owner) &&
            distance_to_building(impact, candidate) <= radius) {
            return false;
        }
    }
    return true;
}

bool Simulation::acquire_nearby_target(
    Unit& unit,
    bool allow_chase
) {
    if (rules_for(unit.kind).attack <= 0 ||
        unit.trebuchet_transform_ticks_remaining > 0 ||
        unit.kind == UnitKind::packed_trebuchet) {
        return false;
    }
    const int vision = effective_unit_vision_range(unit);
    const auto within_vision = [&unit, vision](TilePosition position) {
        const int x = position.x - unit.position.x;
        const int y = position.y - unit.position.y;
        return x * x + y * y <= vision * vision;
    };
    const int distance_bound =
        map_.width() + map_.height() + 1;
    int nearest_distance_squared = distance_bound * distance_bound;
    EntityId target_id{};
    std::uint8_t target_owner_id{255};
    bool target_is_building = false;
    TilePosition target_position{};

    if (!is_ram(unit.kind)) {
        for (const Unit& candidate : units_) {
            if (candidate.id == unit.id ||
                candidate.garrisoned_in != 0 ||
                !is_enemy(candidate.owner, unit.owner) ||
                is_relic(candidate.kind) ||
                candidate.hit_points <= 0 ||
                !within_vision(candidate.position)) {
                continue;
            }
            if (is_ship(unit.kind) && !is_ship(candidate.kind)) {
                continue;
            }
            if (!automatic_splash_target_is_safe(
                    unit, candidate.position
                )) {
                continue;
            }
            const int distance_squared = combat_distance_squared(
                unit.position, candidate.position
            );
            const int minimum_range =
                effective_minimum_attack_range(unit);
            const bool earlier_tie =
                candidate.owner.stable_id() < target_owner_id ||
                (candidate.owner.stable_id() == target_owner_id &&
                 candidate.id < target_id);
            if (distance_squared > minimum_range * minimum_range &&
                (distance_squared < nearest_distance_squared ||
                 (distance_squared == nearest_distance_squared &&
                  earlier_tie))) {
                nearest_distance_squared = distance_squared;
                target_id = candidate.id;
                target_owner_id = candidate.owner.stable_id();
                target_is_building = false;
                target_position = candidate.position;
            }
        }
    }
    for (const Building& candidate : buildings_) {
        const TilePosition nearest =
            nearest_point_on_building(unit.position, candidate);
        if (!is_enemy(candidate.owner, unit.owner) ||
            candidate.hit_points <= 0 ||
            !within_vision(nearest)) {
            continue;
        }
        if (!automatic_splash_target_is_safe(unit, nearest)) {
            continue;
        }
        const int distance_squared =
            combat_distance_squared(unit.position, candidate);
        const int minimum_range =
            effective_minimum_attack_range(unit);
        const bool earlier_tie =
            candidate.owner.stable_id() < target_owner_id ||
            (candidate.owner.stable_id() == target_owner_id &&
             candidate.id < target_id);
        if (distance_squared > minimum_range * minimum_range &&
            (distance_squared < nearest_distance_squared ||
             (distance_squared == nearest_distance_squared &&
              earlier_tie))) {
            nearest_distance_squared = distance_squared;
            target_id = candidate.id;
            target_owner_id = candidate.owner.stable_id();
            target_is_building = true;
            target_position = nearest;
        }
    }
    if (target_id == 0 ||
        (!allow_chase &&
         nearest_distance_squared >
             effective_attack_range(unit) *
                 effective_attack_range(unit))) {
        return false;
    }
    if (allow_chase) {
        if (!route_unit(unit, target_position)) {
            return false;
        }
    } else {
        unit.previous_position = unit.position;
        unit.destination = unit.position;
        unit.moving = false;
        unit.path.clear();
        unit.next_path_step = 0;
    }
    unit.attack_target_id = target_id;
    unit.attack_target_is_building = target_is_building;
    unit.attack_target_auto = true;
    return true;
}

void Simulation::update_match_outcome() {
    if (outcome_ != MatchOutcome::ongoing) return;
    const auto finish = [this](MatchOutcome outcome) {
        outcome_ = outcome;
        for (Unit& unit : units_) {
            unit.previous_position = unit.position;
            unit.destination = unit.position;
            unit.moving = false;
            unit.path.clear();
            unit.next_path_step = 0;
            unit.waypoints.clear();
            unit.formation_waypoints.clear();
            unit.attack_target_id = 0;
            unit.attack_target_is_building = false;
            unit.attack_target_auto = false;
            unit.attack_moving = false;
            unit.patrolling = false;
            unit.guard_target_id = 0;
            unit.guard_target_is_building = false;
            unit.has_resource_target = false;
            unit.resource_building_id = 0;
            unit.resource_unit_id = 0;
            unit.returning_resource = false;
        }
    };
    const auto has_player_entity = [this](Player player) {
        return std::ranges::any_of(
                   units_,
                   [player](const Unit& unit) {
                       return unit.owner == player &&
                           !is_animal(unit.kind) &&
                           !is_relic(unit.kind);
                   }
               ) ||
               std::ranges::any_of(
                   buildings_,
                   [player](const Building& building) {
                       return building.owner == player;
                   }
               );
    };

    const bool blue_alive = has_player_entity(Player::blue);
    const bool red_alive = has_player_entity(Player::red);
    const bool allied = blue_red_diplomacy_ == Diplomacy::ally;

    bool blue_wins = false;
    bool red_wins = false;
    if (match_rules_.conquest_enabled && !allied) {
        blue_wins = blue_alive && !red_alive;
        red_wins = red_alive && !blue_alive;
        if (!blue_alive && !red_alive) {
            finish(MatchOutcome::draw);
            return;
        }
    }
    if (match_rules_.regicide_enabled && !allied) {
        const Unit* blue_king = find_unit(match_rules_.blue_king);
        const Unit* red_king = find_unit(match_rules_.red_king);
        const bool blue_king_alive = blue_king != nullptr &&
            blue_king->owner == Player::blue && blue_king->hit_points > 0;
        const bool red_king_alive = red_king != nullptr &&
            red_king->owner == Player::red && red_king->hit_points > 0;
        blue_wins = blue_king_alive && !red_king_alive;
        red_wins = red_king_alive && !blue_king_alive;
        if (!blue_king_alive && !red_king_alive) {
            finish(MatchOutcome::draw);
            return;
        }
    }

    const auto completed_wonder = [this](Player player) {
        const auto found = std::ranges::find_if(
            buildings_, [player](const Building& building) {
                return building.owner == player &&
                    building.kind == BuildingKind::wonder &&
                    building.completed() && building.hit_points > 0;
            }
        );
        return found != buildings_.end();
    };
    const auto relic_total = [this](Player player) {
        int total = 0;
        for (const Building& building : buildings_) {
            if (building.owner == player &&
                building.kind == BuildingKind::monastery &&
                building.completed() && building.hit_points > 0) {
                total += building.relic_count;
            }
        }
        return total;
    };
    const auto update_countdown = [&](
        bool has_wonder, int relics,
        int& countdown, VictoryCountdownKind& kind,
        std::uint64_t& last_tick
    ) {
        const bool wonder_qualifies =
            match_rules_.wonder_enabled && has_wonder;
        const bool relic_qualifies =
            match_rules_.relic_enabled &&
            relics >= match_rules_.relics_required;
        if (!wonder_qualifies && !relic_qualifies) {
            countdown = 0;
            kind = VictoryCountdownKind::none;
            last_tick = tick_number_;
            return false;
        }
        const bool active_qualifies =
            (kind == VictoryCountdownKind::wonder &&
             wonder_qualifies) ||
            (kind == VictoryCountdownKind::relic &&
             relic_qualifies);
        if (!active_qualifies) {
            // Bounded precedence: Wonder wins only when starting fresh.
            // Active relic countdown is not replaced by a new Wonder.
            kind = wonder_qualifies
                ? VictoryCountdownKind::wonder
                : VictoryCountdownKind::relic;
            countdown = kind == VictoryCountdownKind::wonder
                ? match_rules_.wonder_countdown_ticks
                : match_rules_.relic_countdown_ticks;
        }
        if (last_tick != tick_number_) {
            --countdown;
            last_tick = tick_number_;
        }
        return countdown <= 0;
    };
    const bool blue_wonder = completed_wonder(Player::blue);
    const bool red_wonder = completed_wonder(Player::red);
    const int blue_relics = relic_total(Player::blue);
    const int red_relics = relic_total(Player::red);
    const bool team_wonder = allied && (blue_wonder || red_wonder);
    const int team_relics = allied ? blue_relics + red_relics : 0;
    blue_wins = blue_wins || update_countdown(
        allied ? team_wonder : blue_wonder,
        allied ? team_relics : blue_relics, player_states_[0].victory_countdown,
        player_states_[0].countdown_kind, player_states_[0].countdown_last_tick
    );
    red_wins = red_wins || update_countdown(
        allied ? team_wonder : red_wonder,
        allied ? team_relics : red_relics, player_states_[1].victory_countdown,
        player_states_[1].countdown_kind, player_states_[1].countdown_last_tick
    );

    const int blue_score = score(Player::blue);
    const int red_score = score(Player::red);
    if (match_rules_.score_limit > 0) {
        if (allied) {
            const bool team_reached =
                blue_score + red_score >= match_rules_.score_limit;
            blue_wins = blue_wins || team_reached;
            red_wins = red_wins || team_reached;
        } else {
            blue_wins =
                blue_wins || blue_score >= match_rules_.score_limit;
            red_wins =
                red_wins || red_score >= match_rules_.score_limit;
        }
    }
    if (match_rules_.time_limit_ticks > 0 &&
        tick_number_ >= match_rules_.time_limit_ticks) {
        if (allied) {
            finish(MatchOutcome::allied_victory);
            return;
        }
        if (blue_score == red_score) {
            finish(MatchOutcome::draw);
            return;
        }
        blue_wins = blue_score > red_score;
        red_wins = red_score > blue_score;
    }
    if (allied && (blue_wins || red_wins)) {
        finish(MatchOutcome::allied_victory);
    } else if (blue_wins && red_wins) {
        finish(MatchOutcome::draw);
    } else if (blue_wins) {
        finish(MatchOutcome::blue_victory);
    } else if (red_wins) {
        finish(MatchOutcome::red_victory);
    }
}

}  // namespace aoe
