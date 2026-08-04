#include "aoe/save_game.hpp"
#include "aoe/format_versions.hpp"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>

#include "aoe/game_rules.hpp"
#include "aoe/player_codec.hpp"

namespace aoe {
namespace {

int encode(UnitKind kind) {
    switch (kind) {
        case UnitKind::villager:
            return 0;
        case UnitKind::knight:
            return 1;
        case UnitKind::archer:
            return 2;
        case UnitKind::scout_cavalry:
            return 3;
        case UnitKind::militia:
            return 4;
        case UnitKind::spearman:
            return 5;
        case UnitKind::battering_ram:
            return 6;
        case UnitKind::skirmisher:
            return 7;
        case UnitKind::mangonel:
            return 8;
        case UnitKind::man_at_arms:
            return 9;
        case UnitKind::crossbowman:
            return 10;
        case UnitKind::pikeman:
            return 11;
        case UnitKind::long_swordsman:
            return 12;
        case UnitKind::cavalier:
            return 13;
        case UnitKind::paladin:
            return 14;
        case UnitKind::light_cavalry:
            return 15;
        case UnitKind::hussar:
            return 16;
        case UnitKind::two_handed_swordsman:
            return 17;
        case UnitKind::champion:
            return 18;
        case UnitKind::arbalester:
            return 19;
        case UnitKind::elite_skirmisher:
            return 20;
        case UnitKind::sheep:
            return 21;
        case UnitKind::deer:
            return 22;
        case UnitKind::boar:
            return 23;
        case UnitKind::monk:
            return 24;
        case UnitKind::relic:
            return 25;
        case UnitKind::trade_cart:
            return 26;
        case UnitKind::fishing_ship:
            return 27;
        case UnitKind::galley: return 28;
        case UnitKind::war_galley: return 29;
        case UnitKind::galleon: return 30;
        case UnitKind::transport_ship: return 31;
        case UnitKind::fire_ship: return 32;
        case UnitKind::fast_fire_ship: return 33;
        case UnitKind::demolition_ship: return 34;
        case UnitKind::heavy_demolition_ship: return 35;
        case UnitKind::cannon_galleon: return 36;
        case UnitKind::elite_cannon_galleon: return 37;
        case UnitKind::longboat: return 38;
        case UnitKind::elite_longboat: return 39;
        case UnitKind::turtle_ship: return 40;
        case UnitKind::elite_turtle_ship: return 41;
        case UnitKind::longbowman: return 42;
        case UnitKind::elite_longbowman: return 43;
        case UnitKind::throwing_axeman: return 44;
        case UnitKind::elite_throwing_axeman: return 45;
        case UnitKind::huskarl: return 46;
        case UnitKind::elite_huskarl: return 47;
        case UnitKind::teutonic_knight: return 48;
        case UnitKind::elite_teutonic_knight: return 49;
        case UnitKind::samurai: return 50;
        case UnitKind::elite_samurai: return 51;
        case UnitKind::chu_ko_nu: return 52;
        case UnitKind::elite_chu_ko_nu: return 53;
        case UnitKind::cataphract: return 54;
        case UnitKind::elite_cataphract: return 55;
        case UnitKind::war_elephant: return 56;
        case UnitKind::elite_war_elephant: return 57;
        case UnitKind::mameluke: return 58;
        case UnitKind::elite_mameluke: return 59;
        case UnitKind::janissary: return 60;
        case UnitKind::elite_janissary: return 61;
        case UnitKind::berserk: return 62;
        case UnitKind::elite_berserk: return 63;
        case UnitKind::mangudai: return 64;
        case UnitKind::elite_mangudai: return 65;
        case UnitKind::jaguar_warrior: return 66;
        case UnitKind::elite_jaguar_warrior: return 67;
        case UnitKind::plumed_archer: return 68;
        case UnitKind::elite_plumed_archer: return 69;
        case UnitKind::conquistador: return 70;
        case UnitKind::elite_conquistador: return 71;
        case UnitKind::tarkan: return 72;
        case UnitKind::elite_tarkan: return 73;
        case UnitKind::eagle_warrior: return 74;
        case UnitKind::elite_eagle_warrior: return 75;
        case UnitKind::scorpion: return 76;
        case UnitKind::heavy_scorpion: return 77;
        case UnitKind::onager: return 78;
        case UnitKind::siege_onager: return 79;
        case UnitKind::packed_trebuchet: return 80;
        case UnitKind::trebuchet: return 81;
        case UnitKind::cavalry_archer: return 82;
        case UnitKind::heavy_cavalry_archer: return 83;
        case UnitKind::camel_rider: return 84;
        case UnitKind::heavy_camel: return 85;
        case UnitKind::capped_ram: return 86;
        case UnitKind::siege_ram: return 87;
        case UnitKind::halberdier: return 88;
        case UnitKind::hand_cannoneer: return 89;
        case UnitKind::bombard_cannon: return 90;
        case UnitKind::petard: return 91;
        case UnitKind::missionary: return 92;
        case UnitKind::trade_cog: return 93;
        case UnitKind::woad_raider: return 94;
        case UnitKind::elite_woad_raider: return 95;
        case UnitKind::king: return 96;
    }
    return 0;
}

int encode(BuildingKind kind) {
    switch (kind) {
        case BuildingKind::town_center:
            return 0;
        case BuildingKind::barracks:
            return 1;
        case BuildingKind::archery_range:
            return 2;
        case BuildingKind::house:
            return 3;
        case BuildingKind::mill:
            return 4;
        case BuildingKind::lumber_camp:
            return 5;
        case BuildingKind::mining_camp:
            return 6;
        case BuildingKind::farm:
            return 7;
        case BuildingKind::stable:
            return 8;
        case BuildingKind::blacksmith:
            return 9;
        case BuildingKind::castle:
            return 10;
        case BuildingKind::university:
            return 11;
        case BuildingKind::siege_workshop:
            return 12;
        case BuildingKind::palisade_wall:
            return 13;
        case BuildingKind::watch_tower:
            return 14;
        case BuildingKind::stone_wall:
            return 15;
        case BuildingKind::palisade_gate_x:
            return 16;
        case BuildingKind::palisade_gate_y:
            return 17;
        case BuildingKind::stone_gate_x:
            return 18;
        case BuildingKind::stone_gate_y:
            return 19;
        case BuildingKind::monastery:
            return 20;
        case BuildingKind::market:
            return 21;
        case BuildingKind::dock:
            return 22;
        case BuildingKind::bombard_tower:
            return 23;
        case BuildingKind::fish_trap:
            return 24;
        case BuildingKind::outpost:
            return 25;
        case BuildingKind::wonder:
            return 26;
        case BuildingKind::guard_tower: return 27;
        case BuildingKind::keep: return 28;
        case BuildingKind::fortified_wall: return 29;
        case BuildingKind::fortified_gate_x: return 30;
        case BuildingKind::fortified_gate_y: return 31;
    }
    return 0;
}

int encode(Terrain terrain) {
    switch (terrain) {
        case Terrain::grass:
            return 0;
        case Terrain::grass2: return 9;
        case Terrain::dirt: return 10;
        case Terrain::dirt2: return 11;
        case Terrain::dirt3: return 12;
        case Terrain::road: return 13;
        case Terrain::snow: return 14;
        case Terrain::ice: return 15;
        case Terrain::water:
            return 1;
        case Terrain::deep_water: return 16;
        case Terrain::forest:
            return 2;
        case Terrain::pine_forest: return 17;
        case Terrain::oak_forest: return 18;
        case Terrain::bamboo_forest: return 19;
        case Terrain::palm_forest: return 20;
        case Terrain::jungle_forest: return 21;
        case Terrain::berry_bush:
            return 3;
        case Terrain::gold_mine:
            return 4;
        case Terrain::stone_mine:
            return 5;
        case Terrain::fish:
            return 6;
        case Terrain::fish_shore: return 22;
        case Terrain::fish_deep: return 23;
        case Terrain::beach:
            return 7;
        case Terrain::shallows:
            return 8;
    }
    return 0;
}

int encode(ResourceKind resource) {
    return static_cast<int>(resource);
}

}  // namespace

void save_game(const Simulation& simulation, const std::filesystem::path& path) {
    std::ofstream output(path);
    if (!output) {
        throw std::runtime_error("could not open save file");
    }
    if (simulation.formation_kind(Player::blue) < FormationKind::compact ||
        simulation.formation_kind(Player::blue) > FormationKind::flank ||
        simulation.formation_kind(Player::red) < FormationKind::compact ||
        simulation.formation_kind(Player::red) > FormationKind::flank) {
        throw std::runtime_error("invalid formation kind");
    }

    output << "AOE-ARCHAEOLOGY-SAVE "
           << reconstruction_save_version << '\n';
    output << "tick " << simulation.tick_number() << '\n';
    output << "commercial-random-state "
           << simulation.commercial_random_state() << '\n';
    output << "controllers "
           << static_cast<int>(
                simulation.controller_state(Player::blue)
              ) << ' '
           << static_cast<int>(
                simulation.controller_state(Player::red)
              ) << '\n';
    output << "formations "
           << static_cast<int>(
                simulation.formation_kind(Player::blue)
              ) << ' '
           << static_cast<int>(
                simulation.formation_kind(Player::red)
              ) << '\n';
    output << "farm-reseed-queues "
           << simulation.farm_reseed_queue(Player::blue) << ' '
           << simulation.farm_reseed_queue(Player::red) << '\n';
    const MatchStatistics statistics = simulation.match_statistics();
    for (std::size_t player = 0; player < statistics.players.size();
         ++player) {
        const PlayerStatistics& value = statistics.players[player];
        output << "match-statistics " << player << ' '
               << value.gathered.food << ' ' << value.gathered.wood << ' '
               << value.gathered.gold << ' ' << value.gathered.stone << ' '
               << value.tribute_sent.food << ' '
               << value.tribute_sent.wood << ' '
               << value.tribute_sent.gold << ' '
               << value.tribute_sent.stone << ' '
               << value.tribute_received.food << ' '
               << value.tribute_received.wood << ' '
               << value.tribute_received.gold << ' '
               << value.tribute_received.stone << ' '
               << value.units_created << ' ' << value.units_lost << ' '
               << value.units_killed << ' ' << value.buildings_built << ' '
               << value.buildings_lost << ' ' << value.buildings_razed << ' '
               << value.conversions << ' ' << value.relics_collected << ' '
               << value.technologies_researched << ' '
               << value.wonders_built << ' '
               << (value.age_times.feudal
                       ? static_cast<std::int64_t>(*value.age_times.feudal)
                       : -1) << ' '
               << (value.age_times.castle
                       ? static_cast<std::int64_t>(*value.age_times.castle)
                       : -1) << ' '
               << (value.age_times.imperial
                       ? static_cast<std::int64_t>(*value.age_times.imperial)
                       : -1) << '\n';
    }
    for (const StatisticsTimelineSample& sample :
         statistics.timeline) {
        output << "statistics-sample " << sample.tick;
        for (int value : sample.score) output << ' ' << value;
        for (int value : sample.population) output << ' ' << value;
        for (const ResourceStatistics& gathered : sample.gathered) {
            output << ' ' << gathered.food << ' ' << gathered.wood
                   << ' ' << gathered.gold << ' ' << gathered.stone;
        }
        output << '\n';
    }
    const RosterDiplomacyRules diplomacy_rules =
        simulation.roster_diplomacy().rules();
    output << "roster-rules " << diplomacy_rules.allied_victory << ' '
           << diplomacy_rules.shared_vision << '\n';
    for (std::size_t index = 0; index < 8; ++index) {
        const PlayerSlotId slot = *PlayerSlotId::from_index(index);
        const MatchRosterSlot& roster = simulation.roster().slot(slot);
        output << "roster-slot " << index << ' ' << roster.occupied << ' '
               << roster.team.number() << ' '
               << roster.cooperative_control << ' '
               << roster.controllers.size();
        for (const RosterController& controller : roster.controllers) {
            output << ' ' << static_cast<int>(controller.kind) << ' '
                   << std::quoted(controller.id);
        }
        output << '\n';
        const Simulation::PlayerState& state =
            simulation.player_state(slot);
        std::string technologies;
        technologies.reserve(technology_count);
        for (bool known : state.technologies) {
            technologies.push_back(known ? '1' : '0');
        }
        std::string explored;
        explored.reserve(state.explored.size());
        for (bool visible : state.explored) {
            explored.push_back(visible ? '1' : '0');
        }
        output << "player-state " << index << ' '
               << state.economy.wood << ' ' << state.economy.food << ' '
               << state.economy.gold << ' ' << state.economy.stone << ' '
               << static_cast<int>(state.age) << ' '
               << static_cast<int>(state.civilization) << ' '
               << static_cast<int>(state.formation) << ' '
               << static_cast<int>(state.controller) << ' '
               << state.farm_reseed_queue << ' '
               << state.mayan_resource_remainder << ' '
               << state.aztec_relic_gold_remainder << ' '
               << state.victory_countdown << ' '
               << static_cast<int>(state.countdown_kind) << ' '
               << state.countdown_last_tick << ' '
               << std::quoted(technologies) << ' '
               << std::quoted(explored) << '\n';
    }
    for (std::size_t from = 0; from < 8; ++from) {
        const PlayerSlotId source = *PlayerSlotId::from_index(from);
        if (!simulation.roster().slot(source).occupied) continue;
        for (std::size_t to = 0; to < 8; ++to) {
            const PlayerSlotId target = *PlayerSlotId::from_index(to);
            if (from == to ||
                !simulation.roster().slot(target).occupied) continue;
            output << "roster-diplomacy " << from << ' ' << to << ' '
                   << static_cast<int>(
                          simulation.roster_diplomacy().stance(
                              source, target
                          )
                      ) << '\n';
        }
    }
    for (const ObjectiveState& objective : simulation.objectives()) {
        output << "scenario-objective " << objective.id << ' '
               << static_cast<int>(objective.player) << ' '
               << objective.required << ' ' << objective.hidden << ' '
               << objective.completed << ' '
               << std::quoted(objective.description) << '\n';
    }
    for (const TriggerState& trigger : simulation.triggers()) {
        output << "scenario-trigger " << trigger.id << ' '
               << trigger.priority << ' ' << trigger.enabled << ' '
               << trigger.looping << ' ' << trigger.executable << ' '
               << trigger.activation_tick << ' '
               << trigger.last_fired_tick << ' '
               << trigger.fired_count << ' ' << trigger.conditions.size();
        for (const TriggerCondition& condition : trigger.conditions) {
            output << ' ' << static_cast<int>(condition.kind) << ' '
                   << static_cast<int>(condition.player) << ' '
                   << static_cast<int>(condition.resource) << ' '
                   << condition.entity << ' ' << condition.amount << ' '
                   << condition.first.x << ' ' << condition.first.y << ' '
                   << condition.second.x << ' ' << condition.second.y;
        }
        output << ' ' << trigger.effects.size();
        for (const TriggerEffect& effect : trigger.effects) {
            output << ' ' << static_cast<int>(effect.kind) << ' '
                   << static_cast<int>(effect.player) << ' '
                   << static_cast<int>(effect.target_player) << ' '
                   << static_cast<int>(effect.resource) << ' '
                   << static_cast<int>(effect.unit) << ' '
                   << static_cast<int>(effect.building) << ' '
                   << static_cast<int>(effect.diplomacy) << ' '
                   << static_cast<int>(effect.technology) << ' '
                   << effect.entity << ' ' << effect.objective_id << ' '
                   << effect.trigger_id << ' ' << effect.state << ' '
                   << effect.amount << ' ' << effect.position.x << ' '
                   << effect.position.y << ' ' << std::quoted(effect.text)
                   << ' ' << std::quoted(effect.audio_file);
        }
        output << '\n';
    }
    for (const ScenarioMessage& message : simulation.scenario_messages()) {
        output << "scenario-message "
               << static_cast<int>(message.player) << ' '
               << message.expires_tick << ' '
               << std::quoted(message.text) << ' '
               << std::quoted(message.audio_file) << '\n';
    }
    const MatchRules& match = simulation.match_rules();
    output << "match " << match.conquest_enabled << ' '
           << match.wonder_enabled << ' ' << match.relic_enabled << ' '
           << match.wonder_countdown_ticks << ' '
           << match.relic_countdown_ticks << ' '
           << match.relics_required << ' ' << match.score_limit << ' '
           << match.time_limit_ticks << ' '
           << static_cast<int>(simulation.outcome()) << ' '
           << simulation.victory_countdown(Player::blue) << ' '
           << simulation.victory_countdown(Player::red) << ' '
           << static_cast<int>(
                simulation.countdown_kind(Player::blue)
              ) << ' '
           << static_cast<int>(
                simulation.countdown_kind(Player::red)
              ) << ' ' << match.regicide_enabled << ' '
           << match.blue_king << ' ' << match.red_king << '\n';
    output << "ages " << static_cast<int>(simulation.age(Player::blue))
           << ' ' << static_cast<int>(simulation.age(Player::red)) << '\n';
    output << "market-prices "
           << simulation.market_base_price(MarketResource::food)
           << ' '
           << simulation.market_base_price(MarketResource::wood)
           << ' '
           << simulation.market_base_price(MarketResource::stone)
           << '\n';
    output << "diplomacy "
           << static_cast<int>(
                simulation.diplomacy(Player::blue, Player::red)
              ) << '\n';
    output << "civilizations "
           << static_cast<int>(simulation.civilization(Player::blue))
           << ' '
           << static_cast<int>(simulation.civilization(Player::red))
           << '\n';
    output << "mayan-resource-remainders "
           << simulation.mayan_resource_remainder(Player::blue) << ' '
           << simulation.mayan_resource_remainder(Player::red) << '\n';
    output << "aztec-relic-gold-remainders "
           << simulation.aztec_relic_gold_remainder(Player::blue) << ' '
           << simulation.aztec_relic_gold_remainder(Player::red) << '\n';
    for (Player player : {Player::blue, Player::red}) {
        for (Technology technology : {
                 Technology::wheelbarrow,
                 Technology::fletching,
                 Technology::forging,
                 Technology::murder_holes,
                 Technology::man_at_arms,
                 Technology::crossbowman,
                 Technology::pikeman,
                 Technology::long_swordsman,
                 Technology::loom,
                 Technology::double_bit_axe,
                 Technology::horse_collar,
                 Technology::fortified_wall,
                 Technology::guard_tower,
                 Technology::keep,
                 Technology::bodkin_arrow,
                 Technology::bracer,
                 Technology::iron_casting,
                 Technology::blast_furnace,
                 Technology::scale_mail_armor,
                 Technology::chain_mail_armor,
                 Technology::plate_mail_armor,
                 Technology::scale_barding_armor,
                 Technology::chain_barding_armor,
                 Technology::plate_barding_armor,
                 Technology::padded_archer_armor,
                 Technology::leather_archer_armor,
                 Technology::ring_archer_armor,
                 Technology::bloodlines,
                 Technology::husbandry,
                 Technology::cavalier,
                 Technology::paladin,
                 Technology::light_cavalry,
                 Technology::hussar,
                 Technology::two_handed_swordsman,
                 Technology::champion,
                 Technology::arbalester,
                 Technology::elite_skirmisher,
                 Technology::war_galley,
                 Technology::galleon,
                 Technology::fast_fire_ship,
                 Technology::heavy_demolition_ship,
                 Technology::cannon_galleon,
                 Technology::elite_cannon_galleon,
                 Technology::careening,
                 Technology::dry_dock,
                 Technology::shipwright,
                 Technology::longboat,
                 Technology::elite_longboat,
                 Technology::turtle_ship,
                 Technology::elite_turtle_ship,
                 Technology::longbowman,
                 Technology::elite_longbowman,
                 Technology::throwing_axeman,
                 Technology::elite_throwing_axeman,
                 Technology::huskarl,
                 Technology::elite_huskarl,
                 Technology::teutonic_knight,
                 Technology::elite_teutonic_knight,
                 Technology::samurai,
                 Technology::elite_samurai,
                 Technology::chu_ko_nu,
                 Technology::elite_chu_ko_nu,
                 Technology::cataphract,
                 Technology::elite_cataphract,
                 Technology::war_elephant,
                 Technology::elite_war_elephant,
                 Technology::mameluke,
                 Technology::elite_mameluke,
                 Technology::janissary,
                 Technology::elite_janissary,
                 Technology::berserk,
                 Technology::elite_berserk,
                 Technology::mangudai,
                 Technology::elite_mangudai,
                 Technology::berserkergang,
                 Technology::jaguar_warrior,
                 Technology::elite_jaguar_warrior,
                 Technology::plumed_archer,
                 Technology::elite_plumed_archer,
                 Technology::conquistador,
                 Technology::elite_conquistador,
                 Technology::tarkan,
                 Technology::elite_tarkan,
                 Technology::yeomen,
                 Technology::bearded_axe,
                 Technology::anarchy,
                 Technology::crenellations,
                 Technology::kataparuto,
                 Technology::rocketry,
                 Technology::logistica,
                 Technology::mahouts,
                 Technology::zealotry,
                 Technology::artillery,
                 Technology::drill,
                 Technology::supremacy,
                 Technology::atheism,
                 Technology::shinkichon,
                 Technology::el_dorado,
                 Technology::elite_eagle_warrior,
                 Technology::heavy_scorpion,
                 Technology::onager,
                 Technology::siege_onager,
                 Technology::heavy_cavalry_archer,
                 Technology::heavy_camel,
                 Technology::capped_ram,
                 Technology::siege_ram,
                 Technology::halberdier,
                 Technology::chemistry,
                 Technology::hand_cannoneer_gate,
                 Technology::bombard_cannon_gate,
                 Technology::siege_engineers,
                 Technology::conscription,
                 Technology::petard_gate,
                 Technology::bombard_tower,
                 Technology::sanctity,
                 Technology::fervor,
                 Technology::redemption,
                 Technology::atonement,
                 Technology::illumination,
                 Technology::block_printing,
                 Technology::faith,
                 Technology::theocracy,
                 Technology::heresy,
                 Technology::heavy_plow,
                 Technology::crop_rotation,
                 Technology::bow_saw,
                 Technology::two_man_saw,
                 Technology::gold_mining,
                 Technology::gold_shaft_mining,
                 Technology::stone_mining,
                 Technology::stone_shaft_mining,
                 Technology::hand_cart,
                 Technology::fish_trap_gate,
                 Technology::coinage,
                 Technology::banking,
                 Technology::cartography,
                 Technology::caravan,
                 Technology::guilds,
                 Technology::trade_cog_gate,
                 Technology::town_watch,
                 Technology::town_patrol,
                 Technology::masonry,
                 Technology::architecture,
                 Technology::ballistics,
                 Technology::heated_shot,
                 Technology::hoardings,
                 Technology::sappers,
                 Technology::tracking,
                 Technology::squires,
                 Technology::parthian_tactics,
                 Technology::thumb_ring,
                 Technology::herbal_medicine,
                 Technology::stone_cutting,
                 Technology::spy_technology,
                 Technology::woad_raider,
                 Technology::elite_woad_raider,
                 Technology::wonder_plans,
             }) {
            if (simulation.has_technology(player, technology)) {
                output << "technology " << encode_player_wire(player) << ' '
                       << static_cast<int>(technology) << '\n';
            }
        }
    }
    output << "map " << simulation.map().width() << ' '
           << simulation.map().height() << '\n';
    for (int y = 0; y < simulation.map().height(); ++y) {
        for (int x = 0; x < simulation.map().width(); ++x) {
            const TilePosition position{x, y};
            output << "tile " << x << ' ' << y << ' '
                   << encode(simulation.map().terrain_at(position)) << ' '
                   << simulation.map().resource_amount_at(position) << ' '
                   << simulation.map().elevation_at(position) << ' '
                   << simulation.map().cliff_at(position) << '\n';
        }
    }
    for (Player player : {Player::blue, Player::red}) {
        for (TilePosition position : simulation.explored_tiles(player)) {
            output << "explored " << encode_player_wire(player) << ' '
                   << position.x << ' ' << position.y << '\n';
        }
    }
    output << "blue " << simulation.economy(Player::blue).wood << ' '
           << simulation.economy(Player::blue).food << ' '
           << simulation.economy(Player::blue).gold << ' '
           << simulation.economy(Player::blue).stone << '\n';
    output << "red " << simulation.economy(Player::red).wood << ' '
           << simulation.economy(Player::red).food << ' '
           << simulation.economy(Player::red).gold << ' '
           << simulation.economy(Player::red).stone << '\n';
    for (const Unit& unit : simulation.units()) {
        output << "unit " << unit.id << ' ' << encode(unit.kind) << ' '
               << static_cast<int>(unit.owner.stable_id()) << ' '
               << unit.position.x << ' '
               << unit.position.y << ' ' << unit.previous_position.x << ' '
               << unit.previous_position.y << ' ' << unit.destination.x << ' '
               << unit.destination.y << ' ' << unit.hit_points << ' '
               << unit.attack << ' ' << unit.attack_cooldown << ' '
               << unit.movement_cooldown << ' '
               << unit.movement_speed_remainder << ' '
               << unit.gather_work_remainder << ' '
               << unit.last_move_tick << ' '
               << unit.attack_target_id << ' '
               << unit.attack_target_is_building << ' '
               << unit.repair_target_id << ' '
               << unit.repair_wood_remainder << ' '
               << unit.repair_stone_remainder << ' '
               << unit.moving << ' '
               << unit.resource_target.x << ' ' << unit.resource_target.y << ' '
               << unit.has_resource_target << ' '
               << unit.returning_resource << ' '
               << encode(unit.carried_resource) << ' '
               << unit.carried_amount << ' '
               << unit.resource_building_id << ' '
               << unit.garrison_target_id << ' '
               << unit.garrisoned_in << ' '
               << unit.attack_move_destination.x << ' '
               << unit.attack_move_destination.y << ' '
               << unit.attack_moving << ' '
               << unit.patrol_origin.x << ' '
               << unit.patrol_origin.y << ' '
               << unit.patrol_destination.x << ' '
               << unit.patrol_destination.y << ' '
               << unit.patrolling << ' '
               << unit.attack_ground_target.x << ' '
               << unit.attack_ground_target.y << ' '
               << unit.attacking_ground << ' '
               << unit.guard_target_id << ' '
               << unit.guard_target_is_building << ' '
               << static_cast<int>(unit.stance) << ' '
               << unit.stance_anchor.x << ' '
               << unit.stance_anchor.y << ' '
               << unit.returning_to_stance << ' '
               << unit.attack_target_auto << ' '
               << unit.waypoints.size();
        for (TilePosition waypoint : unit.waypoints) {
            output << ' ' << waypoint.x << ' ' << waypoint.y;
        }
        output << ' ' << unit.resource_unit_id
               << ' ' << unit.food_remaining
               << ' ' << unit.conversion_target_id
               << ' ' << unit.conversion_progress
               << ' ' << unit.conversion_cooldown
               << ' ' << unit.healing_target_id
               << ' ' << unit.carrying_relic
               << ' ' << unit.trade_home_market_id
               << ' ' << unit.trade_target_market_id
               << ' ' << unit.trade_returning
               << ' ' << unit.trebuchet_transform_ticks_remaining
               << ' ' << unit.trebuchet_transform_to_packed
               << ' ' << unit.trade_waiting
               << ' ' << unit.trade_work_ticks_remaining
               << ' ' << unit.formation_move_interval
               << ' ' << unit.formation_speed_numerator
               << ' ' << unit.formation_group_id
               << ' ' << unit.formation_anchor.x
               << ' ' << unit.formation_anchor.y
               << ' ' << unit.formation_slot.x
               << ' ' << unit.formation_slot.y
               << ' ' << unit.formation_waypoints.size();
        for (const FormationWaypoint& waypoint :
             unit.formation_waypoints) {
            output << ' ' << waypoint.destination.x
                   << ' ' << waypoint.destination.y
                   << ' ' << waypoint.anchor.x
                   << ' ' << waypoint.anchor.y
                   << ' ' << waypoint.slot.x
                   << ' ' << waypoint.slot.y
                   << ' ' << static_cast<int>(waypoint.kind)
                   << ' ' << waypoint.group_id
                   << ' ' << waypoint.move_interval
                   << ' ' << waypoint.speed_numerator;
        }
        output << ' ' << unit.relic_target_id
               << ' ' << unit.relic_deposit_target_id
               << ' ' << unit.food_decay_remainder
               << ' ' << unit.unconvertible;
        output << '\n';
    }
    for (const Building& building : simulation.buildings()) {
        output << "building " << building.id << ' ' << encode(building.kind)
               << ' ' << static_cast<int>(building.owner.stable_id()) << ' '
               << building.position.x
               << ' ' << building.position.y << ' ' << building.hit_points
               << ' ' << building.construction_ticks_remaining << ' '
               << building.builder_id << ' ' << building.resource_amount << ' '
               << static_cast<int>(building.age_research_target) << ' '
               << building.age_research_ticks_remaining << ' '
               << static_cast<int>(
                    building.technology_research_target
                  ) << ' '
               << building.technology_research_ticks_remaining << ' '
               << building.attack_cooldown << ' '
               << building.rally_point.x << ' '
               << building.rally_point.y << ' '
               << building.has_rally_point << ' '
               << building.gate_open << ' '
               << building.construction_work_remainder << ' '
               << building.builder_ids.size();
        for (EntityId builder_id : building.builder_ids) {
            output << ' ' << builder_id;
        }
        output << ' ' << building.relic_count;
        output << ' ' << building.production_queue.size();
        for (const ProductionOrder& order : building.production_queue) {
            output << ' ' << encode(order.kind) << ' '
                   << order.ticks_remaining << ' '
                   << order.paid_wood << ' '
                   << order.paid_food << ' '
                   << order.paid_gold << ' '
                   << order.work_remainder;
        }
        output << '\n';
    }
    for (const Projectile& projectile : simulation.projectiles()) {
        output << "projectile "
               << static_cast<int>(projectile.owner.stable_id()) << ' '
               << projectile.target << ' ' << projectile.target_is_building
               << ' ' << projectile.origin.x << ' ' << projectile.origin.y
               << ' ' << projectile.destination.x << ' '
               << projectile.destination.y << ' ' << projectile.damage << ' '
               << static_cast<int>(projectile.damage_class) << ' '
               << projectile.ticks_remaining << ' '
               << projectile.total_ticks << ' '
               << projectile.visual_lane << ' '
               << projectile.splash_radius << ' '
               << encode(projectile.source_kind) << ' '
               << projectile.splash_radius_half_tiles << ' '
               << projectile.source_is_building << ' '
               << encode(projectile.source_building_kind) << ' '
               << projectile.projectile_speed_tenths << ' '
               << projectile.source_entity_id << '\n';
    }
    for (const ImpactEffect& effect : simulation.impact_effects()) {
        output << "impact " << effect.position.x << ' '
               << effect.position.y << ' ' << effect.splash << ' '
               << effect.ticks_remaining << ' ' << effect.total_ticks << ' '
               << encode(effect.source_kind) << ' '
               << effect.source_is_building << ' '
               << encode(effect.source_building_kind) << ' '
               << effect.source_entity_id
               << '\n';
    }
    for (const UnitDeathEffect& effect : simulation.death_effects()) {
        output << "death " << effect.position.x << ' '
               << effect.position.y << ' ' << encode(effect.kind) << ' '
               << static_cast<int>(effect.owner.stable_id()) << ' '
               << effect.ticks_remaining
               << ' ' << effect.total_ticks << ' '
               << effect.entity_id << ' '
               << effect.previous_position.x << ' '
               << effect.previous_position.y << '\n';
    }
    for (const BuildingRubbleEffect& effect : simulation.rubble_effects()) {
        output << "rubble " << effect.position.x << ' '
               << effect.position.y << ' ' << encode(effect.kind) << ' '
               << static_cast<int>(effect.owner.stable_id()) << ' '
               << effect.ticks_remaining
               << ' ' << effect.total_ticks << ' '
               << effect.entity_id << '\n';
    }
}

Simulation load_game(const std::filesystem::path& path) {
    std::ifstream input(path);
    std::string magic;
    int version{};
    input >> magic >> version;
    if (!input || magic != "AOE-ARCHAEOLOGY-SAVE" ||
        (version < 1 || version > reconstruction_save_version)) {
        throw std::runtime_error("unsupported or corrupt save file");
    }

    std::uint64_t tick{};
    std::uint32_t commercial_random_state{1};
    Economy blue;
    Economy red;
    Age blue_age{Age::dark};
    Age red_age{Age::dark};
    int food_market_price{100};
    int wood_market_price{100};
    int stone_market_price{100};
    Diplomacy blue_red_diplomacy{Diplomacy::enemy};
    MatchRules match_rules;
    MatchOutcome match_outcome{MatchOutcome::ongoing};
    PlayerControllerState blue_controller{
        PlayerControllerState::active
    };
    PlayerControllerState red_controller{
        PlayerControllerState::active
    };
    int blue_countdown{};
    int red_countdown{};
    VictoryCountdownKind blue_countdown_kind{
        VictoryCountdownKind::none
    };
    VictoryCountdownKind red_countdown_kind{
        VictoryCountdownKind::none
    };
    FormationKind blue_formation{FormationKind::compact};
    FormationKind red_formation{FormationKind::compact};
    Civilization blue_civilization{Civilization::generic};
    Civilization red_civilization{Civilization::generic};
    int blue_farm_reseed_queue{};
    int red_farm_reseed_queue{};
    int blue_mayan_resource_remainder{};
    int red_mayan_resource_remainder{};
    int blue_aztec_relic_gold_remainder{};
    int red_aztec_relic_gold_remainder{};
    std::vector<Technology> blue_technologies;
    std::vector<Technology> red_technologies;
    std::vector<Unit> units;
    std::vector<Building> buildings;
    std::vector<Projectile> projectiles;
    std::vector<ImpactEffect> impact_effects;
    std::vector<UnitDeathEffect> death_effects;
    std::vector<BuildingRubbleEffect> rubble_effects;
    std::vector<TilePosition> blue_explored;
    std::vector<TilePosition> red_explored;
    std::vector<ObjectiveState> objectives;
    std::vector<TriggerState> triggers;
    std::vector<ScenarioMessage> scenario_messages;
    MatchStatistics match_statistics;
    std::array<std::optional<MatchRosterSlot>, 8> native_roster_slots;
    std::array<std::optional<Simulation::PlayerState>, 8>
        native_player_states;
    std::vector<std::tuple<PlayerSlotId, PlayerSlotId, Diplomacy>>
        native_diplomacy;
    RosterDiplomacyRules native_diplomacy_rules;
    bool native_roster_rules_seen{};
    std::optional<GameMap> map;
    std::string record;

    while (input >> record) {
        if (record == "tick") {
            input >> tick;
        } else if (record == "commercial-random-state" && version >= 113) {
            input >> commercial_random_state;
        } else if (record == "controllers" && version >= 106) {
            int blue_value{};
            int red_value{};
            input >> blue_value >> red_value;
            if (blue_value < static_cast<int>(
                    PlayerControllerState::active
                ) ||
                blue_value > static_cast<int>(
                    PlayerControllerState::observer
                ) ||
                red_value < static_cast<int>(
                    PlayerControllerState::active
                ) ||
                red_value > static_cast<int>(
                    PlayerControllerState::observer
                )) {
                throw std::runtime_error(
                    "invalid controller state in save"
                );
            }
            blue_controller =
                static_cast<PlayerControllerState>(blue_value);
            red_controller =
                static_cast<PlayerControllerState>(red_value);
        } else if (record == "match-statistics" && version >= 105) {
            std::size_t player{};
            std::int64_t feudal{};
            std::int64_t castle{};
            std::int64_t imperial{};
            input >> player;
            const std::size_t player_count = version >= 109 ? 8 : 2;
            if (player >= player_count) {
                throw std::runtime_error(
                    "invalid match statistics player"
                );
            }
            PlayerStatistics& value = match_statistics.players[player];
            input >> value.gathered.food >> value.gathered.wood >>
                value.gathered.gold >> value.gathered.stone >>
                value.tribute_sent.food >> value.tribute_sent.wood >>
                value.tribute_sent.gold >> value.tribute_sent.stone >>
                value.tribute_received.food >>
                value.tribute_received.wood >>
                value.tribute_received.gold >>
                value.tribute_received.stone >>
                value.units_created >> value.units_lost >>
                value.units_killed >> value.buildings_built >>
                value.buildings_lost >> value.buildings_razed >>
                value.conversions >> value.relics_collected >>
                value.technologies_researched >> value.wonders_built >>
                feudal >> castle >> imperial;
            if (feudal >= 0) value.age_times.feudal = feudal;
            if (castle >= 0) value.age_times.castle = castle;
            if (imperial >= 0) value.age_times.imperial = imperial;
        } else if (record == "statistics-sample" && version >= 105) {
            StatisticsTimelineSample sample;
            input >> sample.tick;
            const std::size_t player_count = version >= 109 ? 8 : 2;
            for (std::size_t index = 0; index < player_count; ++index) {
                input >> sample.score[index];
            }
            for (std::size_t index = 0; index < player_count; ++index) {
                input >> sample.population[index];
            }
            for (std::size_t index = 0; index < player_count; ++index) {
                input >> sample.gathered[index].food >>
                    sample.gathered[index].wood >>
                    sample.gathered[index].gold >>
                    sample.gathered[index].stone;
            }
            if (match_statistics.timeline.size() >= 100000 ||
                (!match_statistics.timeline.empty() &&
                 sample.tick <= match_statistics.timeline.back().tick)) {
                throw std::runtime_error(
                    "invalid statistics timeline in save"
                );
            }
            match_statistics.timeline.push_back(sample);
        } else if (record == "roster-rules" && version >= 109) {
            if (native_roster_rules_seen) {
                throw std::runtime_error("duplicate roster rules in save");
            }
            input >> native_diplomacy_rules.allied_victory >>
                native_diplomacy_rules.shared_vision;
            native_roster_rules_seen = true;
        } else if (record == "roster-slot" && version >= 109) {
            int stable_id{};
            bool occupied{};
            int team_number{};
            bool cooperative{};
            std::size_t controller_count{};
            input >> stable_id >> occupied >> team_number >> cooperative >>
                controller_count;
            const auto slot = decode_player_slot_id(stable_id);
            if (!slot || slot->is_neutral() ||
                native_roster_slots[*slot->index()] ||
                controller_count > 8) {
                throw std::runtime_error("invalid roster slot in save");
            }
            MatchRosterSlot value;
            value.slot = *slot;
            value.occupied = occupied;
            value.cooperative_control = cooperative;
            if (team_number != 0) {
                const auto team = TeamId::numbered(team_number);
                if (!team) {
                    throw std::runtime_error("invalid roster team in save");
                }
                value.team = *team;
            }
            for (std::size_t index = 0; index < controller_count; ++index) {
                int kind{};
                std::string id;
                input >> kind >> std::quoted(id);
                if (kind < static_cast<int>(RosterControllerKind::human) ||
                    kind > static_cast<int>(
                        RosterControllerKind::computer
                    )) {
                    throw std::runtime_error(
                        "invalid roster controller in save"
                    );
                }
                value.controllers.push_back({
                    std::move(id),
                    static_cast<RosterControllerKind>(kind),
                });
            }
            native_roster_slots[*slot->index()] = std::move(value);
        } else if (record == "player-state" && version >= 109) {
            int stable_id{};
            int age{};
            int civilization{};
            int formation{};
            int controller{};
            int countdown_kind{};
            std::string technologies;
            std::string explored;
            Simulation::PlayerState state;
            input >> stable_id >>
                state.economy.wood >> state.economy.food >>
                state.economy.gold >> state.economy.stone >>
                age >> civilization >> formation >> controller >>
                state.farm_reseed_queue >>
                state.mayan_resource_remainder >>
                state.aztec_relic_gold_remainder >>
                state.victory_countdown >> countdown_kind >>
                state.countdown_last_tick >>
                std::quoted(technologies) >> std::quoted(explored);
            const auto slot = decode_player_slot_id(stable_id);
            const auto all_bits = [](const std::string& bits) {
                return std::ranges::all_of(
                    bits, [](char bit) { return bit == '0' || bit == '1'; }
                );
            };
            if (!slot || slot->is_neutral() ||
                native_player_states[*slot->index()] ||
                state.economy.wood < 0 || state.economy.food < 0 ||
                state.economy.gold < 0 || state.economy.stone < 0 ||
                age < static_cast<int>(Age::dark) ||
                age > static_cast<int>(Age::imperial) ||
                civilization < static_cast<int>(Civilization::generic) ||
                civilization > static_cast<int>(Civilization::mayans) ||
                formation < static_cast<int>(FormationKind::compact) ||
                formation > static_cast<int>(FormationKind::flank) ||
                controller < static_cast<int>(
                    PlayerControllerState::active
                ) ||
                controller > static_cast<int>(
                    PlayerControllerState::observer
                ) ||
                state.farm_reseed_queue < 0 ||
                state.farm_reseed_queue >
                    Simulation::maximum_farm_reseed_queue ||
                state.mayan_resource_remainder < 0 ||
                state.mayan_resource_remainder >= 115 ||
                state.aztec_relic_gold_remainder < 0 ||
                state.aztec_relic_gold_remainder >= 100 ||
                state.victory_countdown < 0 ||
                countdown_kind < static_cast<int>(
                    VictoryCountdownKind::none
                ) ||
                countdown_kind > static_cast<int>(
                    VictoryCountdownKind::relic
                ) ||
                technologies.size() !=
                    (version >= 110 ? technology_count
                                    : technology_count - 2) ||
                !all_bits(technologies) || !all_bits(explored)) {
                throw std::runtime_error("invalid player state in save");
            }
            state.age = static_cast<Age>(age);
            state.civilization =
                static_cast<Civilization>(civilization);
            state.formation = static_cast<FormationKind>(formation);
            state.controller =
                static_cast<PlayerControllerState>(controller);
            state.countdown_kind =
                static_cast<VictoryCountdownKind>(countdown_kind);
            for (std::size_t index = 0; index < technologies.size(); ++index) {
                state.technologies[index] = technologies[index] == '1';
            }
            state.explored.reserve(explored.size());
            for (char bit : explored) {
                state.explored.push_back(bit == '1');
            }
            native_player_states[*slot->index()] = std::move(state);
        } else if (record == "roster-diplomacy" && version >= 109) {
            int from{};
            int to{};
            int relation{};
            input >> from >> to >> relation;
            const auto source = decode_player_slot_id(from);
            const auto target = decode_player_slot_id(to);
            if (!source || source->is_neutral() ||
                !target || target->is_neutral() ||
                *source == *target ||
                relation < static_cast<int>(Diplomacy::ally) ||
                relation > static_cast<int>(Diplomacy::enemy)) {
                throw std::runtime_error(
                    "invalid roster diplomacy in save"
                );
            }
            native_diplomacy.emplace_back(
                *source, *target, static_cast<Diplomacy>(relation)
            );
        } else if (record == "formations" && version >= 95) {
            int blue{};
            int red{};
            input >> blue >> red;
            if (blue < static_cast<int>(FormationKind::compact) ||
                blue > static_cast<int>(FormationKind::flank) ||
                red < static_cast<int>(FormationKind::compact) ||
                red > static_cast<int>(FormationKind::flank)) {
                throw std::runtime_error("invalid formation kind in save");
            }
            blue_formation = static_cast<FormationKind>(blue);
            red_formation = static_cast<FormationKind>(red);
        } else if (record == "farm-reseed-queues" && version >= 99) {
            input >> blue_farm_reseed_queue >> red_farm_reseed_queue;
            if (blue_farm_reseed_queue < 0 ||
                red_farm_reseed_queue < 0 ||
                blue_farm_reseed_queue >
                    Simulation::maximum_farm_reseed_queue ||
                red_farm_reseed_queue >
                    Simulation::maximum_farm_reseed_queue) {
                throw std::runtime_error(
                    "invalid farm reseed queue in save"
                );
            }
        } else if (
            record == "mayan-resource-remainders" &&
            version >= 104
        ) {
            input >> blue_mayan_resource_remainder >>
                red_mayan_resource_remainder;
            if (blue_mayan_resource_remainder < 0 ||
                blue_mayan_resource_remainder >= 115 ||
                red_mayan_resource_remainder < 0 ||
                red_mayan_resource_remainder >= 115) {
                throw std::runtime_error(
                    "invalid Mayan resource remainder in save"
                );
            }
        } else if (
            record == "aztec-relic-gold-remainders" &&
            version >= 108
        ) {
            input >> blue_aztec_relic_gold_remainder >>
                red_aztec_relic_gold_remainder;
            if (blue_aztec_relic_gold_remainder < 0 ||
                blue_aztec_relic_gold_remainder >= 100 ||
                red_aztec_relic_gold_remainder < 0 ||
                red_aztec_relic_gold_remainder >= 100) {
                throw std::runtime_error(
                    "invalid Aztec relic gold remainder in save"
                );
            }
        } else if (record == "scenario-objective" && version >= 100) {
            ObjectiveState objective;
            int player{};
            input >> objective.id >> player >> objective.required >>
                objective.hidden >> objective.completed >>
                std::quoted(objective.description);
            if (objective.id <= 0 || player < 0 || player > 2 ||
                objective.description.empty() ||
                std::ranges::any_of(
                    objectives,
                    [id = objective.id](const ObjectiveState& existing) {
                        return existing.id == id;
                    }
                )) {
                throw std::runtime_error("invalid scenario objective in save");
            }
            objective.player = static_cast<Player>(player);
            objectives.push_back(std::move(objective));
        } else if (record == "scenario-trigger" && version >= 100) {
            TriggerState trigger;
            input >> trigger.id >> trigger.priority >> trigger.enabled >>
                trigger.looping >> trigger.executable;
            if (version >= 101) {
                input >> trigger.activation_tick >>
                    trigger.last_fired_tick >> trigger.fired_count;
            }
            if (version >= 107) {
                int condition_count{};
                int effect_count{};
                input >> condition_count;
                if (condition_count < 0 || condition_count > 256 ||
                    (trigger.executable && condition_count == 0)) {
                    throw std::runtime_error(
                        "invalid scenario trigger condition count in save"
                    );
                }
                for (int index = 0; index < condition_count; ++index) {
                    TriggerCondition condition;
                    int kind{};
                    int player{};
                    int resource{};
                    input >> kind >> player >> resource >> condition.entity >>
                        condition.amount >> condition.first.x >>
                        condition.first.y >> condition.second.x >>
                        condition.second.y;
                    if (kind < 0 || kind > 7 || player < 0 || player > 2 ||
                        resource < 0 || resource > 4) {
                        throw std::runtime_error(
                            "invalid scenario trigger condition in save"
                        );
                    }
                    condition.kind =
                        static_cast<TriggerConditionKind>(kind);
                    condition.player = static_cast<Player>(player);
                    condition.resource = static_cast<ResourceKind>(resource);
                    trigger.conditions.push_back(condition);
                }
                input >> effect_count;
                if (effect_count < 0 || effect_count > 256 ||
                    (trigger.executable && effect_count == 0)) {
                    throw std::runtime_error(
                        "invalid scenario trigger effect count in save"
                    );
                }
                for (int index = 0; index < effect_count; ++index) {
                    TriggerEffect effect;
                    int kind{};
                    int player{};
                    int target{};
                    int resource{};
                    int unit{};
                    int building{};
                    int diplomacy{};
                    int technology{};
                    int state{};
                    input >> kind >> player >> target >> resource >> unit >>
                        building >> diplomacy >> technology >> effect.entity >>
                        effect.objective_id >> effect.trigger_id >>
                        state >> effect.amount >> effect.position.x >>
                        effect.position.y >> std::quoted(effect.text);
                    if (version >= 112) {
                        input >> std::quoted(effect.audio_file);
                    }
                    if (kind < 0 || kind > 13 || player < 0 || player > 2 ||
                        target < 0 || target > 2 || resource < 0 ||
                        resource > 4 || unit < 0 ||
                        unit > (version >= 110 ? 95 : 93) ||
                        building < 0 || building > 26 || diplomacy < 0 ||
                        diplomacy > 2 || technology < 0 || state < 0 ||
                        state > 1 ||
                        technology >= static_cast<int>(technology_count)) {
                        throw std::runtime_error(
                            "invalid scenario trigger effect in save"
                        );
                    }
                    effect.kind = static_cast<TriggerEffectKind>(kind);
                    effect.player = static_cast<Player>(player);
                    effect.target_player = static_cast<Player>(target);
                    effect.resource = static_cast<ResourceKind>(resource);
                    effect.unit = static_cast<UnitKind>(unit);
                    effect.building = static_cast<BuildingKind>(building);
                    effect.diplomacy = static_cast<Diplomacy>(diplomacy);
                    effect.technology = static_cast<Technology>(technology);
                    effect.state = state != 0;
                    trigger.effects.push_back(std::move(effect));
                }
                if (!input || trigger.id <= 0 ||
                    trigger.priority < -100000 ||
                    trigger.priority > 100000 ||
                    trigger.activation_tick > tick ||
                    trigger.last_fired_tick > tick ||
                    std::ranges::any_of(
                        triggers,
                        [id = trigger.id](const TriggerState& existing) {
                            return existing.id == id;
                        }
                    )) {
                    throw std::runtime_error(
                        "invalid scenario trigger in save"
                    );
                }
                if (!trigger.conditions.empty()) {
                    trigger.condition = trigger.conditions.front();
                }
                if (!trigger.effects.empty()) {
                    trigger.effect = trigger.effects.front();
                }
                triggers.push_back(std::move(trigger));
                continue;
            }
            int condition_kind{};
            int condition_player{};
            int condition_resource{};
            int effect_kind{};
            int effect_player{};
            int effect_resource{};
            int unit{};
            int building{};
            int diplomacy{};
            input >> condition_kind >> condition_player >> condition_resource >>
                trigger.condition.entity >> trigger.condition.amount >>
                trigger.condition.first.x >> trigger.condition.first.y >>
                trigger.condition.second.x >> trigger.condition.second.y >>
                effect_kind >> effect_player >> effect_resource >> unit >>
                building >> diplomacy >> trigger.effect.amount >>
                trigger.effect.position.x >> trigger.effect.position.y >>
                std::quoted(trigger.effect.text);
            if (trigger.id <= 0 || trigger.priority < -100000 ||
                trigger.priority > 100000 || condition_kind < 0 ||
                condition_kind > 6 || effect_kind < 0 || effect_kind > 7 ||
                condition_player < 0 || condition_player > 2 ||
                effect_player < 0 || effect_player > 2 ||
                condition_resource < 0 || condition_resource > 4 ||
                effect_resource < 0 || effect_resource > 4 ||
                unit < 0 || unit > (version >= 110 ? 95 : 93) ||
                building < 0 || building > 26 ||
                diplomacy < 0 || diplomacy > 2 ||
                trigger.activation_tick > tick ||
                trigger.last_fired_tick > tick ||
                (trigger.fired_count == 0 &&
                 trigger.last_fired_tick != 0) ||
                std::ranges::any_of(
                    triggers,
                    [id = trigger.id](const TriggerState& existing) {
                        return existing.id == id;
                    }
                )) {
                throw std::runtime_error("invalid scenario trigger in save");
            }
            trigger.condition.kind =
                static_cast<TriggerConditionKind>(condition_kind);
            trigger.condition.player = static_cast<Player>(condition_player);
            trigger.condition.resource =
                static_cast<ResourceKind>(condition_resource);
            trigger.effect.kind = static_cast<TriggerEffectKind>(effect_kind);
            trigger.effect.player = static_cast<Player>(effect_player);
            trigger.effect.resource =
                static_cast<ResourceKind>(effect_resource);
            trigger.effect.unit = static_cast<UnitKind>(unit);
            trigger.effect.building = static_cast<BuildingKind>(building);
            trigger.effect.diplomacy = static_cast<Diplomacy>(diplomacy);
            trigger.conditions.push_back(trigger.condition);
            trigger.effects.push_back(trigger.effect);
            triggers.push_back(std::move(trigger));
        } else if (record == "scenario-message" && version >= 100) {
            ScenarioMessage message;
            if (version >= 102) {
                int player{};
                input >> player >> message.expires_tick >>
                    std::quoted(message.text);
                if (version >= 112) {
                    input >> std::quoted(message.audio_file);
                }
                if (player < 0 || player > 1) {
                    throw std::runtime_error(
                        "invalid scenario message player in save"
                    );
                }
                message.player = static_cast<Player>(player);
            } else if (version >= 101) {
                input >> message.expires_tick >> std::quoted(message.text);
            } else {
                input >> std::quoted(message.text);
                message.expires_tick = tick + 30;
            }
            if (message.text.empty() || message.text.size() > 4096 ||
                message.expires_tick <= tick) {
                throw std::runtime_error("invalid scenario message in save");
            }
            scenario_messages.push_back(std::move(message));
        } else if (record == "match" && version >= 93) {
            int outcome{};
            input >> match_rules.conquest_enabled >>
                match_rules.wonder_enabled >> match_rules.relic_enabled >>
                match_rules.wonder_countdown_ticks >>
                match_rules.relic_countdown_ticks >>
                match_rules.relics_required >> match_rules.score_limit >>
                match_rules.time_limit_ticks >> outcome >>
                blue_countdown >> red_countdown;
            int blue_kind{};
            int red_kind{};
            input >> blue_kind >> red_kind;
            if (version >= 115) {
                input >> match_rules.regicide_enabled >>
                    match_rules.blue_king >> match_rules.red_king;
            }
            if (version >= 94) {
                blue_countdown_kind =
                    static_cast<VictoryCountdownKind>(blue_kind);
                red_countdown_kind =
                    static_cast<VictoryCountdownKind>(red_kind);
            } else {
                blue_countdown_kind = blue_countdown > 0
                    ? (blue_kind != 0
                        ? VictoryCountdownKind::wonder
                        : VictoryCountdownKind::relic)
                    : VictoryCountdownKind::none;
                red_countdown_kind = red_countdown > 0
                    ? (red_kind != 0
                        ? VictoryCountdownKind::wonder
                        : VictoryCountdownKind::relic)
                    : VictoryCountdownKind::none;
            }
            match_outcome = static_cast<MatchOutcome>(outcome);
        } else if (record == "ages" && version >= 11) {
            int blue_value{};
            int red_value{};
            input >> blue_value >> red_value;
            blue_age = static_cast<Age>(blue_value);
            red_age = static_cast<Age>(red_value);
        } else if (record == "market-prices" && version >= 63) {
            input >> food_market_price >> wood_market_price >>
                stone_market_price;
        } else if (record == "diplomacy" && version >= 64) {
            int relation{};
            input >> relation;
            blue_red_diplomacy = static_cast<Diplomacy>(relation);
        } else if (record == "civilizations" && version >= 65) {
            int blue_value{};
            int red_value{};
            input >> blue_value >> red_value;
            blue_civilization = static_cast<Civilization>(blue_value);
            red_civilization = static_cast<Civilization>(red_value);
        } else if (record == "technology" && version >= 12) {
            int player{};
            int technology{};
            input >> player >> technology;
            (player == 0 ? blue_technologies : red_technologies)
                .push_back(static_cast<Technology>(technology));
        } else if (record == "map" && version >= 3) {
            int width{};
            int height{};
            input >> width >> height;
            map.emplace(width, height);
        } else if (record == "tile" && version >= 3) {
            if (!map) {
                throw std::runtime_error("tile record before map record");
            }
            TilePosition position;
            int terrain{};
            int resources{};
            input >> position.x >> position.y >> terrain >> resources;
            int elevation{};
            if (version >= 103) input >> elevation;
            bool cliff{};
            if (version >= 116) input >> cliff;
            const Terrain decoded =
                terrain == 1 ? Terrain::water :
                terrain == 2 ? Terrain::forest :
                terrain == 3 ? Terrain::berry_bush :
                terrain == 4 ? Terrain::gold_mine :
                terrain == 5 ? Terrain::stone_mine :
                terrain == 6 && version >= 66 ? Terrain::fish :
                terrain == 7 && version >= 98 ? Terrain::beach :
                terrain == 8 && version >= 98 ? Terrain::shallows :
                terrain == 9 && version >= 116 ? Terrain::grass2 :
                terrain == 10 && version >= 116 ? Terrain::dirt :
                terrain == 11 && version >= 116 ? Terrain::dirt2 :
                terrain == 12 && version >= 116 ? Terrain::dirt3 :
                terrain == 13 && version >= 116 ? Terrain::road :
                terrain == 14 && version >= 116 ? Terrain::snow :
                terrain == 15 && version >= 116 ? Terrain::ice :
                terrain == 16 && version >= 116 ? Terrain::deep_water :
                terrain == 17 && version >= 116 ? Terrain::pine_forest :
                terrain == 18 && version >= 116 ? Terrain::oak_forest :
                terrain == 19 && version >= 116 ? Terrain::bamboo_forest :
                terrain == 20 && version >= 116 ? Terrain::palm_forest :
                terrain == 21 && version >= 116 ? Terrain::jungle_forest :
                terrain == 22 && version >= 116 ? Terrain::fish_shore :
                terrain == 23 && version >= 116 ? Terrain::fish_deep :
                Terrain::grass;
            map->set_terrain(position, decoded);
            map->set_resource_amount(position, resources);
            map->set_elevation(position, elevation);
            map->set_cliff(position, cliff);
        } else if (record == "explored" && version >= 5) {
            int player{};
            TilePosition position;
            input >> player >> position.x >> position.y;
            (player == 0 ? blue_explored : red_explored)
                .push_back(position);
        } else if (record == "blue") {
            input >> blue.wood >> blue.food;
            if (version >= 8) {
                input >> blue.gold >> blue.stone;
            }
        } else if (record == "red") {
            input >> red.wood >> red.food;
            if (version >= 8) {
                input >> red.gold >> red.stone;
            }
        } else if (record == "unit") {
            Unit unit;
            int kind{};
            int owner{};
            input >> unit.id >> kind >> owner >>
                unit.position.x >> unit.position.y;
            if (version >= 16) {
                input >> unit.previous_position.x >>
                    unit.previous_position.y;
            } else {
                unit.previous_position = unit.position;
            }
            input >> unit.destination.x >> unit.destination.y >>
                unit.hit_points >> unit.attack;
            if (version >= 6) {
                input >> unit.attack_cooldown;
            }
            if (version >= 15) {
                input >> unit.movement_cooldown;
            }
            if (version >= 47) {
                input >> unit.movement_speed_remainder;
            }
            if (version >= 48) {
                input >> unit.gather_work_remainder;
            }
            if (version >= 16) {
                input >> unit.last_move_tick;
            }
            if (version >= 17) {
                input >> unit.attack_target_id >>
                    unit.attack_target_is_building;
            }
            if (version >= 24) {
                input >> unit.repair_target_id >>
                    unit.repair_wood_remainder >>
                    unit.repair_stone_remainder;
            }
            input >> unit.moving;
            if (version >= 3) {
                input >> unit.resource_target.x >> unit.resource_target.y >>
                    unit.has_resource_target >> unit.returning_resource;
                if (version >= 8) {
                    int resource{};
                    input >> resource >> unit.carried_amount;
                    unit.carried_resource =
                        static_cast<ResourceKind>(resource);
                } else {
                    input >> unit.carried_amount;
                    if (unit.carried_amount > 0) {
                        unit.carried_resource = ResourceKind::wood;
                    }
                }
                if (version >= 9) {
                    input >> unit.resource_building_id;
                }
                if (version >= 34) {
                    input >> unit.garrison_target_id >>
                        unit.garrisoned_in;
                }
                if (version >= 37) {
                    input >> unit.attack_move_destination.x >>
                        unit.attack_move_destination.y >>
                        unit.attack_moving;
                }
                if (version >= 38) {
                    input >> unit.patrol_origin.x >>
                        unit.patrol_origin.y >>
                        unit.patrol_destination.x >>
                        unit.patrol_destination.y >>
                        unit.patrolling;
                }
                if (version >= 43) {
                    input >> unit.attack_ground_target.x >>
                        unit.attack_ground_target.y >>
                        unit.attacking_ground;
                }
                if (version >= 39) {
                    input >> unit.guard_target_id >>
                        unit.guard_target_is_building;
                }
                if (version >= 41) {
                    int stance{};
                    input >> stance >>
                        unit.stance_anchor.x >>
                        unit.stance_anchor.y >>
                        unit.returning_to_stance >>
                        unit.attack_target_auto;
                    unit.stance = static_cast<UnitStance>(stance);
                } else {
                    unit.stance_anchor = unit.position;
                }
                if (version >= 40) {
                    std::size_t waypoint_count{};
                    input >> waypoint_count;
                    for (std::size_t index = 0;
                         index < waypoint_count;
                         ++index) {
                        TilePosition waypoint;
                        input >> waypoint.x >> waypoint.y;
                        unit.waypoints.push_back(waypoint);
                    }
                }
            if (version >= 59) {
                input >> unit.resource_unit_id >>
                    unit.food_remaining;
            }
            if (version >= 61) {
                input >> unit.conversion_target_id >>
                    unit.conversion_progress >>
                    unit.conversion_cooldown;
            }
            if (version >= 62) {
                input >> unit.healing_target_id >>
                    unit.carrying_relic;
            }
            if (version >= 64) {
                input >> unit.trade_home_market_id >>
                    unit.trade_target_market_id >>
                    unit.trade_returning;
            }
            if (version >= 79) {
                input >> unit.trebuchet_transform_ticks_remaining >>
                    unit.trebuchet_transform_to_packed;
            }
            if (version >= 91) {
                input >> unit.trade_waiting >>
                    unit.trade_work_ticks_remaining;
            }
            if (version >= 95) {
                input >> unit.formation_move_interval >>
                    unit.formation_speed_numerator >>
                    unit.formation_group_id >>
                    unit.formation_anchor.x >>
                    unit.formation_anchor.y >>
                    unit.formation_slot.x >>
                    unit.formation_slot.y;
            }
            if (version >= 96) {
                std::size_t formation_waypoint_count{};
                input >> formation_waypoint_count;
                if (formation_waypoint_count > 20) {
                    throw std::runtime_error(
                        "invalid formation waypoint count in save"
                    );
                }
                for (std::size_t index = 0;
                     index < formation_waypoint_count; ++index) {
                    FormationWaypoint waypoint;
                    int formation_kind{};
                    input >> waypoint.destination.x >>
                        waypoint.destination.y >>
                        waypoint.anchor.x >> waypoint.anchor.y >>
                        waypoint.slot.x >> waypoint.slot.y >>
                        formation_kind >> waypoint.group_id >>
                        waypoint.move_interval >>
                        waypoint.speed_numerator;
                    if (formation_kind <
                            static_cast<int>(FormationKind::compact) ||
                        formation_kind >
                            static_cast<int>(FormationKind::flank) ||
                        waypoint.group_id == 0 ||
                        waypoint.move_interval <= 0 ||
                        waypoint.speed_numerator <= 0) {
                        throw std::runtime_error(
                            "invalid formation waypoint in save"
                        );
                    }
                    waypoint.kind =
                        static_cast<FormationKind>(formation_kind);
                    unit.formation_waypoints.push_back(waypoint);
                }
            }
            if (version >= 97) {
                input >> unit.relic_target_id >>
                    unit.relic_deposit_target_id;
            }
            if (version >= 99) {
                input >> unit.food_decay_remainder;
                if (unit.food_decay_remainder < 0 ||
                    unit.food_decay_remainder >= 500) {
                    throw std::runtime_error(
                        "invalid food decay remainder in save"
                    );
                }
            }
            if (version >= 117) {
                input >> unit.unconvertible;
            }
            }
            unit.kind =
                kind == 0 ? UnitKind::villager :
                kind == 2 ? UnitKind::archer :
                kind == 4 && version >= 14 ? UnitKind::militia :
                kind == 5 && version >= 14 ? UnitKind::spearman :
                kind == 6 && version >= 23 ? UnitKind::battering_ram :
                kind == 7 && version >= 28 ? UnitKind::skirmisher :
                kind == 8 && version >= 29 ? UnitKind::mangonel :
                kind == 9 && version >= 30 ? UnitKind::man_at_arms :
                kind == 10 && version >= 31 ? UnitKind::crossbowman :
                kind == 11 && version >= 32 ? UnitKind::pikeman :
                kind == 12 && version >= 33 ? UnitKind::long_swordsman :
                kind == 13 && version >= 51 ? UnitKind::cavalier :
                kind == 14 && version >= 52 ? UnitKind::paladin :
                kind == 15 && version >= 53 ? UnitKind::light_cavalry :
                kind == 16 && version >= 54 ? UnitKind::hussar :
                kind == 17 && version >= 55
                    ? UnitKind::two_handed_swordsman :
                kind == 18 && version >= 56 ? UnitKind::champion :
                kind == 19 && version >= 57 ? UnitKind::arbalester :
                kind == 20 && version >= 58
                    ? UnitKind::elite_skirmisher :
                kind == 21 && version >= 59
                    ? UnitKind::sheep :
                kind == 22 && version >= 60
                    ? UnitKind::deer :
                kind == 23 && version >= 60
                    ? UnitKind::boar :
                kind == 24 && version >= 61
                    ? UnitKind::monk :
                kind == 25 && version >= 62
                    ? UnitKind::relic :
                kind == 26 && version >= 64
                    ? UnitKind::trade_cart :
                kind == 27 && version >= 66
                    ? UnitKind::fishing_ship :
                kind == 28 && version >= 67 ? UnitKind::galley :
                kind == 29 && version >= 67 ? UnitKind::war_galley :
                kind == 30 && version >= 67 ? UnitKind::galleon :
                kind == 31 && version >= 67 ? UnitKind::transport_ship :
                kind == 32 && version >= 68 ? UnitKind::fire_ship :
                kind == 33 && version >= 68 ? UnitKind::fast_fire_ship :
                kind == 34 && version >= 68 ? UnitKind::demolition_ship :
                kind == 35 && version >= 68
                    ? UnitKind::heavy_demolition_ship :
                kind == 36 && version >= 69 ? UnitKind::cannon_galleon :
                kind == 37 && version >= 69
                    ? UnitKind::elite_cannon_galleon :
                kind == 38 && version >= 70 ? UnitKind::longboat :
                kind == 39 && version >= 70 ? UnitKind::elite_longboat :
                kind == 40 && version >= 70 ? UnitKind::turtle_ship :
                kind == 41 && version >= 70
                    ? UnitKind::elite_turtle_ship :
                kind == 42 && version >= 71 ? UnitKind::longbowman :
                kind == 43 && version >= 71
                    ? UnitKind::elite_longbowman :
                kind == 44 && version >= 71 ? UnitKind::throwing_axeman :
                kind == 45 && version >= 71
                    ? UnitKind::elite_throwing_axeman :
                kind == 46 && version >= 71 ? UnitKind::huskarl :
                kind == 47 && version >= 71 ? UnitKind::elite_huskarl :
                kind == 48 && version >= 71
                    ? UnitKind::teutonic_knight :
                kind == 49 && version >= 71
                    ? UnitKind::elite_teutonic_knight :
                kind == 50 && version >= 72 ? UnitKind::samurai :
                kind == 51 && version >= 72 ? UnitKind::elite_samurai :
                kind == 52 && version >= 72 ? UnitKind::chu_ko_nu :
                kind == 53 && version >= 72 ? UnitKind::elite_chu_ko_nu :
                kind == 54 && version >= 72 ? UnitKind::cataphract :
                kind == 55 && version >= 72 ? UnitKind::elite_cataphract :
                kind == 56 && version >= 72 ? UnitKind::war_elephant :
                kind == 57 && version >= 72
                    ? UnitKind::elite_war_elephant :
                kind == 58 && version >= 73 ? UnitKind::mameluke :
                kind == 59 && version >= 73 ? UnitKind::elite_mameluke :
                kind == 60 && version >= 73 ? UnitKind::janissary :
                kind == 61 && version >= 73 ? UnitKind::elite_janissary :
                kind == 62 && version >= 73 ? UnitKind::berserk :
                kind == 63 && version >= 73 ? UnitKind::elite_berserk :
                kind == 64 && version >= 73 ? UnitKind::mangudai :
                kind == 65 && version >= 73 ? UnitKind::elite_mangudai :
                kind == 66 && version >= 74 ? UnitKind::jaguar_warrior :
                kind == 67 && version >= 74
                    ? UnitKind::elite_jaguar_warrior :
                kind == 68 && version >= 74 ? UnitKind::plumed_archer :
                kind == 69 && version >= 74
                    ? UnitKind::elite_plumed_archer :
                kind == 70 && version >= 74 ? UnitKind::conquistador :
                kind == 71 && version >= 74
                    ? UnitKind::elite_conquistador :
                kind == 72 && version >= 74 ? UnitKind::tarkan :
                kind == 73 && version >= 74 ? UnitKind::elite_tarkan :
                kind == 74 && version >= 79 ? UnitKind::eagle_warrior :
                kind == 75 && version >= 79
                    ? UnitKind::elite_eagle_warrior :
                kind == 76 && version >= 79 ? UnitKind::scorpion :
                kind == 77 && version >= 79 ? UnitKind::heavy_scorpion :
                kind == 78 && version >= 79 ? UnitKind::onager :
                kind == 79 && version >= 79 ? UnitKind::siege_onager :
                kind == 80 && version >= 79 ? UnitKind::packed_trebuchet :
                kind == 81 && version >= 79 ? UnitKind::trebuchet :
                kind == 82 && version >= 80 ? UnitKind::cavalry_archer :
                kind == 83 && version >= 80
                    ? UnitKind::heavy_cavalry_archer :
                kind == 84 && version >= 81 ? UnitKind::camel_rider :
                kind == 85 && version >= 81 ? UnitKind::heavy_camel :
                kind == 86 && version >= 82 ? UnitKind::capped_ram :
                kind == 87 && version >= 82 ? UnitKind::siege_ram :
                kind == 88 && version >= 83 ? UnitKind::halberdier :
                kind == 89 && version >= 84 ? UnitKind::hand_cannoneer :
                kind == 90 && version >= 84 ? UnitKind::bombard_cannon :
                kind == 91 && version >= 86 ? UnitKind::petard :
                kind == 92 && version >= 88 ? UnitKind::missionary :
                kind == 93 && version >= 90 ? UnitKind::trade_cog :
                kind == 94 && version >= 110 ? UnitKind::woad_raider :
                kind == 95 && version >= 110 ? UnitKind::elite_woad_raider :
                kind == 96 && version >= 115 ? UnitKind::king :
                kind == 3 && version >= 13
                    ? UnitKind::scout_cavalry
                    : UnitKind::knight;
            if (version >= 109) {
                const auto decoded = EntityOwner::from_stable_id(owner);
                if (!decoded) {
                    throw std::runtime_error("invalid unit owner in save");
                }
                unit.owner = *decoded;
            } else {
                unit.owner = owner == 0 ? Player::blue :
                    owner == 1 ? Player::red : Player::neutral;
            }
            units.push_back(unit);
        } else if (record == "building" && version >= 2) {
            Building building;
            int kind{};
            int owner{};
            std::size_t queue_size{};
            input >> building.id >> kind >> owner >>
                building.position.x >> building.position.y >>
                building.hit_points;
            if (version >= 4) {
                input >> building.construction_ticks_remaining >>
                    building.builder_id;
            }
            if (version >= 9) {
                input >> building.resource_amount;
            }
            if (version >= 11) {
                int age_target{};
                input >> age_target >>
                    building.age_research_ticks_remaining;
                building.age_research_target =
                    static_cast<Age>(age_target);
            }
            if (version >= 12) {
                int technology_target{};
                input >> technology_target >>
                    building.technology_research_ticks_remaining;
                building.technology_research_target =
                    static_cast<Technology>(technology_target);
            }
            if (version >= 20) {
                input >> building.attack_cooldown;
            }
            if (version >= 35) {
                input >> building.rally_point.x >>
                    building.rally_point.y >>
                    building.has_rally_point;
            }
            if (version >= 49) {
                input >> building.gate_open;
            }
            if (version >= 42) {
                std::size_t builder_count{};
                input >> building.construction_work_remainder >>
                    builder_count;
                for (std::size_t index = 0;
                     index < builder_count;
                     ++index) {
                    EntityId builder_id{};
                    input >> builder_id;
                    building.builder_ids.push_back(builder_id);
                }
            } else if (building.builder_id != 0) {
                building.builder_ids.push_back(building.builder_id);
            }
            if (version >= 62) {
                input >> building.relic_count;
            }
            input >> queue_size;
            if (!input || queue_size > 5) {
                throw std::runtime_error("invalid production queue in save");
            }
            building.kind =
                kind == 0 ? BuildingKind::town_center :
                kind == 1 ? BuildingKind::barracks :
                kind == 2 ? BuildingKind::archery_range :
                kind == 3 ? BuildingKind::house :
                kind == 4 ? BuildingKind::mill :
                kind == 5 ? BuildingKind::lumber_camp :
                kind == 6 ? BuildingKind::mining_camp :
                kind == 10 && version >= 19
                    ? BuildingKind::castle :
                kind == 11 && version >= 22
                    ? BuildingKind::university :
                kind == 12 && version >= 23
                    ? BuildingKind::siege_workshop :
                kind == 13 && version >= 25
                    ? BuildingKind::palisade_wall :
                kind == 14 && version >= 26
                    ? BuildingKind::watch_tower :
                kind == 15 && version >= 27
                    ? BuildingKind::stone_wall :
                kind == 16 && version >= 49
                    ? BuildingKind::palisade_gate_x :
                kind == 17 && version >= 49
                    ? BuildingKind::palisade_gate_y :
                kind == 18 && version >= 50
                    ? BuildingKind::stone_gate_x :
                kind == 19 && version >= 50
                    ? BuildingKind::stone_gate_y :
                kind == 20 && version >= 61
                    ? BuildingKind::monastery :
                kind == 21 && version >= 63
                    ? BuildingKind::market :
                kind == 22 && version >= 66
                    ? BuildingKind::dock :
                kind == 23 && version >= 87
                    ? BuildingKind::bombard_tower :
                kind == 24 && version >= 90
                    ? BuildingKind::fish_trap :
                kind == 25 && version >= 92
                    ? BuildingKind::outpost :
                kind == 26 && version >= 93
                    ? BuildingKind::wonder :
                kind == 27 && version >= 114
                    ? BuildingKind::guard_tower :
                kind == 28 && version >= 114
                    ? BuildingKind::keep :
                kind == 29 && version >= 114
                    ? BuildingKind::fortified_wall :
                kind == 30 && version >= 114
                    ? BuildingKind::fortified_gate_x :
                kind == 31 && version >= 114
                    ? BuildingKind::fortified_gate_y :
                kind == 9 && version >= 18
                    ? BuildingKind::blacksmith :
                kind == 8 && version >= 13
                    ? BuildingKind::stable
                    : BuildingKind::farm;
            if (version >= 109) {
                const auto decoded = EntityOwner::from_stable_id(owner);
                if (!decoded) {
                    throw std::runtime_error(
                        "invalid building owner in save"
                    );
                }
                building.owner = *decoded;
            } else {
                building.owner = owner == 0 ? Player::blue : Player::red;
            }
            for (std::size_t index = 0; index < queue_size; ++index) {
                int unit_kind{};
                ProductionOrder order;
                input >> unit_kind >> order.ticks_remaining;
                order.kind =
                    unit_kind == 0 ? UnitKind::villager :
                    unit_kind == 2 ? UnitKind::archer :
                    unit_kind == 4 && version >= 14
                        ? UnitKind::militia :
                    unit_kind == 5 && version >= 14
                        ? UnitKind::spearman :
                    unit_kind == 6 && version >= 23
                        ? UnitKind::battering_ram :
                    unit_kind == 7 && version >= 28
                        ? UnitKind::skirmisher :
                    unit_kind == 8 && version >= 29
                        ? UnitKind::mangonel :
                    unit_kind == 9 && version >= 30
                        ? UnitKind::man_at_arms :
                    unit_kind == 10 && version >= 31
                        ? UnitKind::crossbowman :
                    unit_kind == 11 && version >= 32
                        ? UnitKind::pikeman :
                    unit_kind == 12 && version >= 33
                        ? UnitKind::long_swordsman :
                    unit_kind == 13 && version >= 51
                        ? UnitKind::cavalier :
                    unit_kind == 14 && version >= 52
                        ? UnitKind::paladin :
                    unit_kind == 15 && version >= 53
                        ? UnitKind::light_cavalry :
                    unit_kind == 16 && version >= 54
                        ? UnitKind::hussar :
                    unit_kind == 17 && version >= 55
                        ? UnitKind::two_handed_swordsman :
                    unit_kind == 18 && version >= 56
                        ? UnitKind::champion :
                    unit_kind == 19 && version >= 57
                        ? UnitKind::arbalester :
                    unit_kind == 20 && version >= 58
                        ? UnitKind::elite_skirmisher :
                    unit_kind == 24 && version >= 61
                        ? UnitKind::monk :
                    unit_kind == 26 && version >= 64
                        ? UnitKind::trade_cart :
                    unit_kind == 27 && version >= 66
                        ? UnitKind::fishing_ship :
                    unit_kind == 28 && version >= 67
                        ? UnitKind::galley :
                    unit_kind == 29 && version >= 67
                        ? UnitKind::war_galley :
                    unit_kind == 30 && version >= 67
                        ? UnitKind::galleon :
                    unit_kind == 31 && version >= 67
                        ? UnitKind::transport_ship :
                    unit_kind == 32 && version >= 68
                        ? UnitKind::fire_ship :
                    unit_kind == 33 && version >= 68
                        ? UnitKind::fast_fire_ship :
                    unit_kind == 34 && version >= 68
                        ? UnitKind::demolition_ship :
                    unit_kind == 35 && version >= 68
                        ? UnitKind::heavy_demolition_ship :
                    unit_kind == 36 && version >= 69
                        ? UnitKind::cannon_galleon :
                    unit_kind == 37 && version >= 69
                        ? UnitKind::elite_cannon_galleon :
                    unit_kind == 38 && version >= 70
                        ? UnitKind::longboat :
                    unit_kind == 39 && version >= 70
                        ? UnitKind::elite_longboat :
                    unit_kind == 40 && version >= 70
                        ? UnitKind::turtle_ship :
                    unit_kind == 41 && version >= 70
                        ? UnitKind::elite_turtle_ship :
                    unit_kind == 42 && version >= 71
                        ? UnitKind::longbowman :
                    unit_kind == 43 && version >= 71
                        ? UnitKind::elite_longbowman :
                    unit_kind == 44 && version >= 71
                        ? UnitKind::throwing_axeman :
                    unit_kind == 45 && version >= 71
                        ? UnitKind::elite_throwing_axeman :
                    unit_kind == 46 && version >= 71
                        ? UnitKind::huskarl :
                    unit_kind == 47 && version >= 71
                        ? UnitKind::elite_huskarl :
                    unit_kind == 48 && version >= 71
                        ? UnitKind::teutonic_knight :
                    unit_kind == 49 && version >= 71
                        ? UnitKind::elite_teutonic_knight :
                    unit_kind == 50 && version >= 72
                        ? UnitKind::samurai :
                    unit_kind == 51 && version >= 72
                        ? UnitKind::elite_samurai :
                    unit_kind == 52 && version >= 72
                        ? UnitKind::chu_ko_nu :
                    unit_kind == 53 && version >= 72
                        ? UnitKind::elite_chu_ko_nu :
                    unit_kind == 54 && version >= 72
                        ? UnitKind::cataphract :
                    unit_kind == 55 && version >= 72
                        ? UnitKind::elite_cataphract :
                    unit_kind == 56 && version >= 72
                        ? UnitKind::war_elephant :
                    unit_kind == 57 && version >= 72
                        ? UnitKind::elite_war_elephant :
                    unit_kind == 58 && version >= 73
                        ? UnitKind::mameluke :
                    unit_kind == 59 && version >= 73
                        ? UnitKind::elite_mameluke :
                    unit_kind == 60 && version >= 73
                        ? UnitKind::janissary :
                    unit_kind == 61 && version >= 73
                        ? UnitKind::elite_janissary :
                    unit_kind == 62 && version >= 73
                        ? UnitKind::berserk :
                    unit_kind == 63 && version >= 73
                        ? UnitKind::elite_berserk :
                    unit_kind == 64 && version >= 73
                        ? UnitKind::mangudai :
                    unit_kind == 65 && version >= 73
                        ? UnitKind::elite_mangudai :
                    unit_kind == 66 && version >= 74
                        ? UnitKind::jaguar_warrior :
                    unit_kind == 67 && version >= 74
                        ? UnitKind::elite_jaguar_warrior :
                    unit_kind == 68 && version >= 74
                        ? UnitKind::plumed_archer :
                    unit_kind == 69 && version >= 74
                        ? UnitKind::elite_plumed_archer :
                    unit_kind == 70 && version >= 74
                        ? UnitKind::conquistador :
                    unit_kind == 71 && version >= 74
                        ? UnitKind::elite_conquistador :
                    unit_kind == 72 && version >= 74
                        ? UnitKind::tarkan :
                    unit_kind == 73 && version >= 74
                        ? UnitKind::elite_tarkan :
                    unit_kind == 74 && version >= 79
                        ? UnitKind::eagle_warrior :
                    unit_kind == 75 && version >= 79
                        ? UnitKind::elite_eagle_warrior :
                    unit_kind == 76 && version >= 79
                        ? UnitKind::scorpion :
                    unit_kind == 77 && version >= 79
                        ? UnitKind::heavy_scorpion :
                    unit_kind == 78 && version >= 79
                        ? UnitKind::onager :
                    unit_kind == 79 && version >= 79
                        ? UnitKind::siege_onager :
                    unit_kind == 80 && version >= 79
                        ? UnitKind::packed_trebuchet :
                    unit_kind == 81 && version >= 79
                        ? UnitKind::trebuchet :
                    unit_kind == 82 && version >= 80
                        ? UnitKind::cavalry_archer :
                    unit_kind == 83 && version >= 80
                        ? UnitKind::heavy_cavalry_archer :
                    unit_kind == 84 && version >= 81
                        ? UnitKind::camel_rider :
                    unit_kind == 85 && version >= 81
                        ? UnitKind::heavy_camel :
                    unit_kind == 86 && version >= 82
                        ? UnitKind::capped_ram :
                    unit_kind == 87 && version >= 82
                        ? UnitKind::siege_ram :
                    unit_kind == 88 && version >= 83
                        ? UnitKind::halberdier :
                    unit_kind == 89 && version >= 84
                        ? UnitKind::hand_cannoneer :
                    unit_kind == 90 && version >= 84
                        ? UnitKind::bombard_cannon :
                    unit_kind == 91 && version >= 86
                        ? UnitKind::petard :
                    unit_kind == 92 && version >= 88
                        ? UnitKind::missionary :
                    unit_kind == 93 && version >= 90
                        ? UnitKind::trade_cog :
                    unit_kind == 94 && version >= 110
                        ? UnitKind::woad_raider :
                    unit_kind == 95 && version >= 110
                        ? UnitKind::elite_woad_raider :
                    unit_kind == 3 && version >= 13
                        ? UnitKind::scout_cavalry
                        : UnitKind::knight;
                if (version >= 36) {
                    input >> order.paid_wood >>
                        order.paid_food >>
                        order.paid_gold;
                } else {
                    const UnitRules& rules = rules_for(order.kind);
                    order.paid_wood = rules.wood_cost;
                    order.paid_food = rules.food_cost;
                    order.paid_gold = rules.gold_cost;
                }
                if (version >= 85) {
                    input >> order.work_remainder;
                }
                const UnitRules& queued_rules = rules_for(order.kind);
                if (!input || unit_kind != encode(order.kind) ||
                    order.ticks_remaining <= 0 ||
                    order.ticks_remaining > 1'000'000 ||
                    order.paid_wood < 0 ||
                    order.paid_wood > 1'000'000 ||
                    order.paid_food < 0 ||
                    order.paid_food > 1'000'000 ||
                    order.paid_gold < 0 ||
                    order.paid_gold > 1'000'000 ||
                    order.work_remainder < 0 ||
                    order.work_remainder >= 100) {
                    throw std::runtime_error(
                        "invalid production order in save"
                    );
                }
                const auto owner_has = [&](
                    Technology technology
                ) {
                    if (version >= 109) {
                        const auto slot =
                            entity_owner_slot(building.owner);
                        if (!slot || slot->is_neutral() ||
                            !native_player_states[*slot->index()]) {
                            return false;
                        }
                        return (*native_player_states[*slot->index()])
                            .technologies[static_cast<std::size_t>(
                                technology
                            )];
                    }
                    const auto& owner_technologies =
                        building.owner == Player::blue
                            ? blue_technologies : red_technologies;
                    return std::ranges::find(
                        owner_technologies, technology
                    ) != owner_technologies.end();
                };
                const bool alternate_producer =
                    (building.kind == BuildingKind::barracks &&
                     (order.kind == UnitKind::huskarl ||
                      order.kind == UnitKind::elite_huskarl) &&
                     owner_has(Technology::anarchy));
                if (queued_rules.trained_at != building.kind &&
                    !alternate_producer) {
                    throw std::runtime_error(
                        "production order has invalid producer in save: " +
                        std::to_string(unit_kind) + " at " +
                        std::to_string(kind)
                    );
                }
                building.production_queue.push_back(order);
            }
            buildings.push_back(std::move(building));
        } else if (record == "projectile" && version >= 7) {
            Projectile projectile;
            int owner{};
            input >> owner >> projectile.target >>
                projectile.target_is_building >>
                projectile.origin.x >> projectile.origin.y >>
                projectile.destination.x >> projectile.destination.y >>
                projectile.damage;
            if (version >= 10) {
                int damage_class{};
                input >> damage_class;
                projectile.damage_class =
                    static_cast<DamageClass>(damage_class);
            } else {
                projectile.damage_class = DamageClass::pierce;
            }
            input >> projectile.ticks_remaining >>
                projectile.total_ticks;
            if (version >= 21) {
                input >> projectile.visual_lane;
            }
            if (version >= 29) {
                input >> projectile.splash_radius;
            }
            if (version >= 79) {
                int source_kind{};
                input >> source_kind;
                projectile.source_kind =
                    static_cast<UnitKind>(source_kind);
            }
            if (version >= 84) {
                input >> projectile.splash_radius_half_tiles;
            }
            if (version >= 87) {
                int source_building_kind{};
                input >> projectile.source_is_building >>
                    source_building_kind >>
                    projectile.projectile_speed_tenths;
                projectile.source_building_kind =
                    static_cast<BuildingKind>(source_building_kind);
            }
            if (version >= 111) {
                input >> projectile.source_entity_id;
            }
            if (version >= 109) {
                const auto decoded = EntityOwner::from_stable_id(owner);
                if (!decoded) {
                    throw std::runtime_error(
                        "invalid projectile owner in save"
                    );
                }
                projectile.owner = *decoded;
            } else {
                projectile.owner =
                    owner == 0 ? Player::blue : Player::red;
            }
            projectiles.push_back(projectile);
        } else if (record == "impact" && version >= 44) {
            ImpactEffect effect;
            input >> effect.position.x >> effect.position.y >>
                effect.splash >> effect.ticks_remaining >>
                effect.total_ticks;
            if (version >= 79) {
                int source_kind{};
                input >> source_kind;
                effect.source_kind = static_cast<UnitKind>(source_kind);
            }
            if (version >= 87) {
                int source_building_kind{};
                input >> effect.source_is_building >>
                    source_building_kind;
                effect.source_building_kind =
                    static_cast<BuildingKind>(source_building_kind);
            }
            if (version >= 111) {
                input >> effect.source_entity_id;
            }
            impact_effects.push_back(effect);
        } else if (record == "death" && version >= 45) {
            UnitDeathEffect effect;
            int kind{};
            int owner{};
            input >> effect.position.x >> effect.position.y >> kind >> owner >>
                effect.ticks_remaining >> effect.total_ticks;
            effect.kind =
                kind == 0 ? UnitKind::villager :
                kind == 2 ? UnitKind::archer :
                kind == 4 ? UnitKind::militia :
                kind == 5 ? UnitKind::spearman :
                kind == 6 ? UnitKind::battering_ram :
                kind == 7 ? UnitKind::skirmisher :
                kind == 8 ? UnitKind::mangonel :
                kind == 9 ? UnitKind::man_at_arms :
                kind == 10 ? UnitKind::crossbowman :
                kind == 11 ? UnitKind::pikeman :
                kind == 12 ? UnitKind::long_swordsman :
                kind == 13 && version >= 51 ? UnitKind::cavalier :
                kind == 14 && version >= 52 ? UnitKind::paladin :
                kind == 15 && version >= 53 ? UnitKind::light_cavalry :
                kind == 16 && version >= 54 ? UnitKind::hussar :
                kind == 17 && version >= 55
                    ? UnitKind::two_handed_swordsman :
                kind == 18 && version >= 56 ? UnitKind::champion :
                kind == 19 && version >= 57 ? UnitKind::arbalester :
                kind == 20 && version >= 58
                    ? UnitKind::elite_skirmisher :
                kind == 21 && version >= 59
                    ? UnitKind::sheep :
                kind == 22 && version >= 60
                    ? UnitKind::deer :
                kind == 23 && version >= 60
                    ? UnitKind::boar :
                kind == 24 && version >= 61
                    ? UnitKind::monk :
                kind == 86 && version >= 82
                    ? UnitKind::capped_ram :
                kind == 87 && version >= 82
                    ? UnitKind::siege_ram :
                kind == 88 && version >= 83
                    ? UnitKind::halberdier :
                kind == 89 && version >= 84
                    ? UnitKind::hand_cannoneer :
                kind == 90 && version >= 84
                    ? UnitKind::bombard_cannon :
                kind == 91 && version >= 86
                    ? UnitKind::petard :
                kind == 92 && version >= 88
                    ? UnitKind::missionary :
                kind == 93 && version >= 90
                    ? UnitKind::trade_cog :
                kind == 94 && version >= 110
                    ? UnitKind::woad_raider :
                kind == 95 && version >= 110
                    ? UnitKind::elite_woad_raider :
                kind == 3 ? UnitKind::scout_cavalry :
                UnitKind::knight;
            if (version >= 109) {
                const auto decoded = EntityOwner::from_stable_id(owner);
                if (!decoded) {
                    throw std::runtime_error(
                        "invalid death effect owner in save"
                    );
                }
                effect.owner = *decoded;
            } else {
                effect.owner = owner == 0 ? Player::blue : Player::red;
            }
            if (version >= 111) {
                input >> effect.entity_id >>
                    effect.previous_position.x >>
                    effect.previous_position.y;
            } else {
                effect.previous_position = effect.position;
            }
            death_effects.push_back(effect);
        } else if (record == "rubble" && version >= 46) {
            BuildingRubbleEffect effect;
            int kind{};
            int owner{};
            input >> effect.position.x >> effect.position.y >> kind >> owner >>
                effect.ticks_remaining >> effect.total_ticks;
            effect.kind = static_cast<BuildingKind>(kind);
            if (version >= 109) {
                const auto decoded = EntityOwner::from_stable_id(owner);
                if (!decoded) {
                    throw std::runtime_error(
                        "invalid rubble effect owner in save"
                    );
                }
                effect.owner = *decoded;
            } else {
                effect.owner = owner == 0 ? Player::blue : Player::red;
            }
            if (version >= 111) {
                input >> effect.entity_id;
            }
            rubble_effects.push_back(effect);
        } else {
            throw std::runtime_error("unknown save record: " + record);
        }
        if (!input) {
            throw std::runtime_error("malformed save record: " + record);
        }
    }

    if (!map) {
        map = GameMap::create_demo_map();
    }
    std::optional<MatchRoster> native_roster;
    std::optional<RosterDiplomacy> native_roster_diplomacy;
    if (version >= 109) {
        if (!native_roster_rules_seen ||
            std::ranges::any_of(
                native_roster_slots,
                [](const auto& slot) { return !slot.has_value(); }
            ) ||
            std::ranges::any_of(
                native_player_states,
                [](const auto& state) { return !state.has_value(); }
            )) {
            throw std::runtime_error(
                "incomplete native roster in save"
            );
        }
        std::vector<MatchRosterSlot> slots;
        slots.reserve(8);
        for (auto& slot : native_roster_slots) {
            slots.push_back(std::move(*slot));
        }
        native_roster = MatchRoster::create(std::move(slots));
        if (!native_roster) {
            throw std::runtime_error("invalid native roster in save");
        }
        native_roster_diplomacy =
            RosterDiplomacy::create(
                *native_roster, native_diplomacy_rules
            );
        if (!native_roster_diplomacy) {
            throw std::runtime_error(
                "invalid native diplomacy in save"
            );
        }
        std::size_t expected_diplomacy{};
        for (std::size_t from = 0; from < 8; ++from) {
            const PlayerSlotId source =
                *PlayerSlotId::from_index(from);
            if (!native_roster->slot(source).occupied) continue;
            for (std::size_t to = 0; to < 8; ++to) {
                const PlayerSlotId target =
                    *PlayerSlotId::from_index(to);
                if (from != to &&
                    native_roster->slot(target).occupied) {
                    ++expected_diplomacy;
                }
            }
        }
        if (native_diplomacy.size() != expected_diplomacy) {
            throw std::runtime_error(
                "incomplete native diplomacy in save"
            );
        }
        std::array<std::array<bool, 8>, 8> seen_diplomacy{};
        for (const auto& [from, to, relation] : native_diplomacy) {
            const std::size_t from_index = *from.index();
            const std::size_t to_index = *to.index();
            if (seen_diplomacy[from_index][to_index] ||
                !native_roster->slot(from).occupied ||
                !native_roster->slot(to).occupied ||
                !native_roster_diplomacy->set_stance(
                    from, to, relation
                )) {
                throw std::runtime_error(
                    "invalid native diplomacy in save"
                );
            }
            seen_diplomacy[from_index][to_index] = true;
        }
        const std::size_t exploration_size =
            static_cast<std::size_t>(map->width() * map->height());
        for (const auto& state : native_player_states) {
            if ((*state).explored.size() != exploration_size) {
                throw std::runtime_error(
                    "invalid player exploration in save"
                );
            }
        }
        const auto owner_valid = [&native_roster](EntityOwner owner) {
            const auto slot = entity_owner_slot(owner);
            return slot &&
                (slot->is_neutral() ||
                 native_roster->slot(*slot).occupied);
        };
        if (std::ranges::any_of(
                units,
                [&](const Unit& unit) {
                    return !owner_valid(unit.owner);
                }
            ) ||
            std::ranges::any_of(
                buildings,
                [&](const Building& building) {
                    return building.owner.is_neutral() ||
                        !owner_valid(building.owner);
                }
            ) ||
            std::ranges::any_of(
                projectiles,
                [&](const Projectile& projectile) {
                    return projectile.owner.is_neutral() ||
                        !owner_valid(projectile.owner);
                }
            )) {
            throw std::runtime_error(
                "entity owner is not occupied in save"
            );
        }
    }
    for (const TriggerState& trigger : triggers) {
        if (!trigger.executable) continue;
        if (trigger.conditions.empty() || trigger.conditions.size() > 256 ||
            trigger.effects.empty() || trigger.effects.size() > 256) {
            throw std::runtime_error(
                "invalid scenario trigger vector bounds in save"
            );
        }
        for (const TriggerCondition& value : trigger.conditions) {
            if ((value.kind == TriggerConditionKind::elapsed_ticks &&
                 value.amount < 0) ||
                (value.kind ==
                    TriggerConditionKind::object_hit_points_at_least &&
                 (value.entity == 0 || value.amount < 0 ||
                  (std::ranges::none_of(
                       units, [id = value.entity](const Unit& unit) {
                           return unit.id == id;
                       }
                   ) &&
                   std::ranges::none_of(
                       buildings,
                       [id = value.entity](const Building& building) {
                           return building.id == id;
                       }
                   )))) ||
                (value.kind == TriggerConditionKind::area_presence &&
                 (!map->contains(value.first) ||
                  !map->contains(value.second) ||
                  value.first.x > value.second.x ||
                  value.first.y > value.second.y ||
                  value.amount < 0))) {
                throw std::runtime_error(
                    "invalid scenario trigger condition in save"
                );
            }
        }
        for (const TriggerEffect& value : trigger.effects) {
            if ((value.kind == TriggerEffectKind::tribute &&
                 (value.player == Player::neutral ||
                  value.target_player == Player::neutral ||
                  value.player == value.target_player ||
                  value.resource == ResourceKind::none ||
                  value.amount < 0)) ||
                (value.kind == TriggerEffectKind::research &&
                 value.player == Player::neutral) ||
                (value.kind == TriggerEffectKind::remove_object &&
                 (value.entity == 0 ||
                  (std::ranges::none_of(
                       units, [id = value.entity](const Unit& unit) {
                           return unit.id == id;
                       }
                   ) &&
                   std::ranges::none_of(
                       buildings,
                       [id = value.entity](const Building& building) {
                           return building.id == id;
                       }
                   )))) ||
                (value.kind == TriggerEffectKind::set_objective_state &&
                 std::ranges::none_of(
                     objectives,
                     [id = value.objective_id](
                         const ObjectiveState& objective
                     ) { return objective.id == id; }
                 )) ||
                ((value.kind == TriggerEffectKind::activate_trigger ||
                  value.kind == TriggerEffectKind::deactivate_trigger) &&
                 std::ranges::none_of(
                     triggers,
                     [id = value.trigger_id](const TriggerState& candidate) {
                         return candidate.id == id;
                     }
                 ))) {
                throw std::runtime_error(
                    "invalid scenario trigger effect in save"
                );
            }
        }
        const TriggerCondition& condition = trigger.condition;
        const TriggerEffect& effect = trigger.effect;
        if ((condition.kind == TriggerConditionKind::elapsed_ticks &&
             condition.amount < 0) ||
            (condition.kind == TriggerConditionKind::resource_at_least &&
             (condition.player == Player::neutral ||
              condition.resource == ResourceKind::none ||
              condition.amount < 0)) ||
            ((effect.kind == TriggerEffectKind::add_resource ||
              effect.kind == TriggerEffectKind::victory ||
              effect.kind == TriggerEffectKind::defeat ||
              effect.kind == TriggerEffectKind::message) &&
             effect.player == Player::neutral) ||
            (effect.kind == TriggerEffectKind::add_resource &&
             effect.resource == ResourceKind::none) ||
            (effect.kind == TriggerEffectKind::message &&
             (effect.amount <= 0 || effect.text.empty() ||
              effect.audio_file.size() > 4096)) ||
            (effect.kind == TriggerEffectKind::complete_objective &&
             std::ranges::none_of(
                 objectives,
                 [id = effect.amount](const ObjectiveState& objective) {
                     return objective.id == id;
                 }
             )) ||
            ((effect.kind == TriggerEffectKind::create_unit ||
              effect.kind == TriggerEffectKind::create_building) &&
             !map->contains(effect.position)) ||
            (condition.kind == TriggerConditionKind::area_presence &&
             (!map->contains(condition.first) ||
              !map->contains(condition.second) ||
              condition.first.x > condition.second.x ||
              condition.first.y > condition.second.y ||
              condition.amount < 0)) ||
            ((condition.kind == TriggerConditionKind::unit_exists ||
              condition.kind == TriggerConditionKind::unit_destroyed) &&
             std::ranges::none_of(
                 units,
                 [id = condition.entity](const Unit& unit) {
                     return unit.id == id;
                 }
             )) ||
            ((condition.kind == TriggerConditionKind::building_exists ||
              condition.kind == TriggerConditionKind::building_destroyed) &&
             std::ranges::none_of(
                 buildings,
                 [id = condition.entity](const Building& building) {
                     return building.id == id;
                 }
             ))) {
            throw std::runtime_error(
                "invalid scenario trigger semantics in save"
            );
        }
    }
    Simulation simulation(std::move(*map));
    if (version < 114) {
        const auto owned_technology = [&](const Building& building,
                                          Technology technology) {
            const auto legacy = building.owner.legacy_player();
            if (!legacy) return false;
            const auto& technologies = *legacy == Player::blue
                ? blue_technologies : red_technologies;
            return std::ranges::find(technologies, technology) !=
                technologies.end();
        };
        for (Building& building : buildings) {
            if (building.kind == BuildingKind::watch_tower) {
                building.kind = owned_technology(building, Technology::keep)
                    ? BuildingKind::keep
                    : owned_technology(building, Technology::guard_tower)
                        ? BuildingKind::guard_tower : building.kind;
            } else if (owned_technology(
                    building, Technology::fortified_wall)) {
                building.kind = building.kind == BuildingKind::stone_wall
                    ? BuildingKind::fortified_wall
                    : building.kind == BuildingKind::stone_gate_x
                        ? BuildingKind::fortified_gate_x
                        : building.kind == BuildingKind::stone_gate_y
                            ? BuildingKind::fortified_gate_y : building.kind;
            }
        }
    }
    if (native_roster && native_roster_diplomacy) {
        simulation.replace_roster(
            *native_roster, *native_roster_diplomacy
        );
    }
    simulation.replace_state(
        std::move(units),
        std::move(buildings),
        blue,
        red,
        tick
    );
    simulation.replace_commercial_random_state(commercial_random_state);
    if (version >= 109) {
        for (std::size_t index = 0; index < 8; ++index) {
            simulation.replace_player_state(
                *PlayerSlotId::from_index(index),
                *native_player_states[index]
            );
        }
    }
    simulation.replace_farm_reseed_queues(
        blue_farm_reseed_queue, red_farm_reseed_queue
    );
    simulation.replace_mayan_resource_remainders(
        blue_mayan_resource_remainder,
        red_mayan_resource_remainder
    );
    simulation.replace_aztec_relic_gold_remainders(
        blue_aztec_relic_gold_remainder,
        red_aztec_relic_gold_remainder
    );
    simulation.validate_loaded_state();
    simulation.merge_exploration(Player::blue, blue_explored);
    simulation.merge_exploration(Player::red, red_explored);
    simulation.replace_projectiles(std::move(projectiles));
    simulation.replace_impact_effects(std::move(impact_effects));
    simulation.replace_death_effects(std::move(death_effects));
    simulation.replace_rubble_effects(std::move(rubble_effects));
    simulation.replace_ages(blue_age, red_age);
    simulation.replace_technologies(
        Player::blue,
        blue_technologies
    );
    simulation.replace_technologies(
        Player::red,
        red_technologies
    );
    simulation.replace_market_prices(
        food_market_price,
        wood_market_price,
        stone_market_price
    );
    simulation.replace_diplomacy(blue_red_diplomacy);
    simulation.set_formation_kind(Player::blue, blue_formation);
    simulation.set_formation_kind(Player::red, red_formation);
    simulation.replace_match_state(
        match_rules, match_outcome, blue_countdown, red_countdown,
        blue_countdown_kind, red_countdown_kind
    );
    simulation.replace_controller_states(
        blue_controller, red_controller
    );
    simulation.replace_civilizations(
        blue_civilization,
        red_civilization
    );
    simulation.replace_scenario_runtime(
        std::move(objectives),
        std::move(triggers),
        std::move(scenario_messages)
    );
    simulation.replace_match_statistics(std::move(match_statistics));
    if (version >= 109) {
        for (std::size_t index = 0; index < 8; ++index) {
            simulation.replace_player_state(
                *PlayerSlotId::from_index(index),
                std::move(*native_player_states[index])
            );
        }
    }
    return simulation;
}

}  // namespace aoe
