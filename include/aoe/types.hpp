#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace aoe {

using EntityId = std::uint32_t;

struct TilePosition {
    int x{};
    int y{};

    auto operator<=>(const TilePosition&) const = default;
};

enum class Terrain {
    grass,
    water,
    forest,
    berry_bush,
    gold_mine,
    stone_mine,
    fish,
    beach,
    shallows,
};

enum class ResourceKind {
    none,
    wood,
    food,
    gold,
    stone,
};

enum class DamageClass {
    melee,
    pierce,
};

enum class Age {
    dark,
    feudal,
    castle,
    imperial,
};

enum class Technology {
    wheelbarrow,
    fletching,
    forging,
    murder_holes,
    man_at_arms,
    crossbowman,
    pikeman,
    long_swordsman,
    loom,
    double_bit_axe,
    horse_collar,
    fortified_wall,
    guard_tower,
    keep,
    bodkin_arrow,
    bracer,
    iron_casting,
    blast_furnace,
    scale_mail_armor,
    chain_mail_armor,
    plate_mail_armor,
    scale_barding_armor,
    chain_barding_armor,
    plate_barding_armor,
    padded_archer_armor,
    leather_archer_armor,
    ring_archer_armor,
    bloodlines,
    husbandry,
    cavalier,
    paladin,
    light_cavalry,
    hussar,
    two_handed_swordsman,
    champion,
    arbalester,
    elite_skirmisher,
    war_galley,
    galleon,
    fast_fire_ship,
    heavy_demolition_ship,
    cannon_galleon,
    elite_cannon_galleon,
    careening,
    dry_dock,
    shipwright,
    longboat,
    elite_longboat,
    turtle_ship,
    elite_turtle_ship,
    longbowman,
    elite_longbowman,
    throwing_axeman,
    elite_throwing_axeman,
    huskarl,
    elite_huskarl,
    teutonic_knight,
    elite_teutonic_knight,
    samurai,
    elite_samurai,
    chu_ko_nu,
    elite_chu_ko_nu,
    cataphract,
    elite_cataphract,
    war_elephant,
    elite_war_elephant,
    mameluke,
    elite_mameluke,
    janissary,
    elite_janissary,
    berserk,
    elite_berserk,
    mangudai,
    elite_mangudai,
    berserkergang,
    jaguar_warrior,
    elite_jaguar_warrior,
    plumed_archer,
    elite_plumed_archer,
    conquistador,
    elite_conquistador,
    tarkan,
    elite_tarkan,
    yeomen,
    bearded_axe,
    anarchy,
    crenellations,
    kataparuto,
    rocketry,
    logistica,
    mahouts,
    zealotry,
    artillery,
    drill,
    supremacy,
    atheism,
    shinkichon,
    el_dorado,
    elite_eagle_warrior,
    heavy_scorpion,
    onager,
    siege_onager,
    heavy_cavalry_archer,
    heavy_camel,
    capped_ram,
    siege_ram,
    halberdier,
    chemistry,
    hand_cannoneer_gate,
    bombard_cannon_gate,
    siege_engineers,
    conscription,
    petard_gate,
    bombard_tower,
    sanctity,
    fervor,
    redemption,
    atonement,
    illumination,
    block_printing,
    faith,
    theocracy,
    heresy,
    heavy_plow,
    crop_rotation,
    bow_saw,
    two_man_saw,
    gold_mining,
    gold_shaft_mining,
    stone_mining,
    stone_shaft_mining,
    hand_cart,
    fish_trap_gate,
    coinage,
    banking,
    cartography,
    caravan,
    guilds,
    trade_cog_gate,
    outpost_gate,
    town_watch,
    town_patrol,
    masonry,
    architecture,
    ballistics,
    heated_shot,
    hoardings,
    sappers,
    wonder_plans,
    thumb_ring,
    parthian_tactics,
    squires,
    tracking,
    herbal_medicine,
    stone_cutting,
    spy_technology,
    woad_raider,
    elite_woad_raider,
};

inline constexpr std::size_t technology_count =
    static_cast<std::size_t>(Technology::elite_woad_raider) + 1;

enum class UnitKind {
    villager,
    knight,
    archer,
    scout_cavalry,
    militia,
    spearman,
    battering_ram,
    skirmisher,
    mangonel,
    man_at_arms,
    crossbowman,
    pikeman,
    long_swordsman,
    cavalier,
    paladin,
    light_cavalry,
    hussar,
    two_handed_swordsman,
    champion,
    arbalester,
    elite_skirmisher,
    sheep,
    deer,
    boar,
    monk,
    relic,
    trade_cart,
    fishing_ship,
    galley,
    war_galley,
    galleon,
    transport_ship,
    fire_ship,
    fast_fire_ship,
    demolition_ship,
    heavy_demolition_ship,
    cannon_galleon,
    elite_cannon_galleon,
    longboat,
    elite_longboat,
    turtle_ship,
    elite_turtle_ship,
    longbowman,
    elite_longbowman,
    throwing_axeman,
    elite_throwing_axeman,
    huskarl,
    elite_huskarl,
    teutonic_knight,
    elite_teutonic_knight,
    samurai,
    elite_samurai,
    chu_ko_nu,
    elite_chu_ko_nu,
    cataphract,
    elite_cataphract,
    war_elephant,
    elite_war_elephant,
    mameluke,
    elite_mameluke,
    janissary,
    elite_janissary,
    berserk,
    elite_berserk,
    mangudai,
    elite_mangudai,
    jaguar_warrior,
    elite_jaguar_warrior,
    plumed_archer,
    elite_plumed_archer,
    conquistador,
    elite_conquistador,
    tarkan,
    elite_tarkan,
    eagle_warrior,
    elite_eagle_warrior,
    scorpion,
    heavy_scorpion,
    onager,
    siege_onager,
    packed_trebuchet,
    trebuchet,
    cavalry_archer,
    heavy_cavalry_archer,
    camel_rider,
    heavy_camel,
    capped_ram,
    siege_ram,
    halberdier,
    hand_cannoneer,
    bombard_cannon,
    petard,
    missionary,
    trade_cog,
    woad_raider,
    elite_woad_raider,
    king,
};

inline constexpr std::size_t unit_kind_count =
    static_cast<std::size_t>(UnitKind::king) + 1;

enum class Player {
    blue,
    red,
    neutral,
};

// Stable in-memory entity ownership. Playable IDs are roster slots 0..7;
// neutral is 8. Legacy Player conversion is checked and never aliases slots
// 2..7 to red or blue.
class EntityOwner {
public:
    static constexpr std::uint8_t neutral_stable_id = 8;

    constexpr EntityOwner() = default;
    constexpr EntityOwner(Player player) :
        stable_id_(player == Player::blue ? 0 :
                   player == Player::red ? 1 : neutral_stable_id) {}

    [[nodiscard]] static constexpr std::optional<EntityOwner> from_stable_id(
        int stable_id
    ) noexcept {
        return stable_id >= 0 && stable_id <= neutral_stable_id
            ? std::optional<EntityOwner>{
                  EntityOwner{static_cast<std::uint8_t>(stable_id)}
              }
            : std::nullopt;
    }

    [[nodiscard]] constexpr std::uint8_t stable_id() const noexcept {
        return stable_id_;
    }
    [[nodiscard]] constexpr bool is_neutral() const noexcept {
        return stable_id_ == neutral_stable_id;
    }
    [[nodiscard]] constexpr std::optional<std::size_t> slot_index()
        const noexcept {
        return is_neutral()
            ? std::nullopt
            : std::optional<std::size_t>{stable_id_};
    }
    [[nodiscard]] constexpr std::optional<Player> legacy_player()
        const noexcept {
        if (stable_id_ == 0) return Player::blue;
        if (stable_id_ == 1) return Player::red;
        if (is_neutral()) return Player::neutral;
        return std::nullopt;
    }

    operator Player() const {
        const auto legacy = legacy_player();
        if (!legacy) {
            throw std::invalid_argument(
                "entity owner is not representable as legacy Player"
            );
        }
        return *legacy;
    }

    friend constexpr bool operator==(
        EntityOwner, EntityOwner
    ) noexcept = default;
    friend constexpr bool operator==(
        EntityOwner owner, Player player
    ) noexcept {
        return owner.legacy_player() == player;
    }
    friend constexpr bool operator==(
        Player player, EntityOwner owner
    ) noexcept {
        return owner == player;
    }

private:
    explicit constexpr EntityOwner(std::uint8_t stable_id) :
        stable_id_(stable_id) {}
    std::uint8_t stable_id_{};
};

enum class PlayerControllerState {
    active,
    resigned,
    observer,
};

enum class Diplomacy {
    ally,
    neutral,
    enemy,
};

enum class Civilization {
    generic,
    britons,
    franks,
    teutons,
    goths,
    celts,
    vikings,
    byzantines,
    japanese,
    chinese,
    persians,
    saracens,
    turks,
    mongols,
    spanish,
    huns,
    koreans,
    aztecs,
    mayans,
};

enum class MatchOutcome {
    ongoing,
    blue_victory,
    red_victory,
    allied_victory,
    draw,
};

enum class RosterOutcomeStatus {
    ongoing,
    draw,
    victory,
};

struct RosterMatchOutcome {
    RosterOutcomeStatus status{RosterOutcomeStatus::ongoing};
    std::optional<int> winning_team;
    std::vector<std::uint8_t> winning_slots;

    auto operator<=>(const RosterMatchOutcome&) const = default;
};

struct MatchRules {
    bool conquest_enabled{true};
    bool wonder_enabled{true};
    bool relic_enabled{true};
    int wonder_countdown_ticks{200};
    int relic_countdown_ticks{200};
    int relics_required{5};
    int score_limit{};
    std::uint64_t time_limit_ticks{};
    bool regicide_enabled{};
    EntityId blue_king{};
    EntityId red_king{};
};

enum class VictoryCountdownKind {
    none,
    wonder,
    relic,
};

enum class UnitStance {
    aggressive,
    defensive,
    stand_ground,
    passive,
};

enum class FormationKind {
    compact,
    line,
    box,
    staggered,
    flank,
};
enum class FormationOrderKind {
    move,
    attack_move,
    patrol,
    guard,
    queued_waypoint,
};

struct FormationWaypoint {
    TilePosition destination{};
    TilePosition anchor{};
    TilePosition slot{};
    FormationKind kind{FormationKind::compact};
    std::uint64_t group_id{};
    int move_interval{};
    int speed_numerator{};
};

enum class BuildingKind {
    town_center,
    barracks,
    archery_range,
    house,
    mill,
    lumber_camp,
    mining_camp,
    farm,
    stable,
    blacksmith,
    castle,
    university,
    siege_workshop,
    palisade_wall,
    watch_tower,
    stone_wall,
    palisade_gate_x,
    palisade_gate_y,
    stone_gate_x,
    stone_gate_y,
    monastery,
    market,
    dock,
    bombard_tower,
    fish_trap,
    outpost,
    wonder,
    guard_tower,
    keep,
    fortified_wall,
    fortified_gate_x,
    fortified_gate_y,
};

inline constexpr std::size_t building_kind_count =
    static_cast<std::size_t>(BuildingKind::fortified_gate_y) + 1;

enum class TriggerConditionKind {
    elapsed_ticks, unit_exists, unit_destroyed, building_exists,
    building_destroyed, resource_at_least, area_presence,
    object_hit_points_at_least,
};

enum class TriggerEffectKind {
    message, complete_objective, add_resource, create_unit,
    create_building, diplomacy, victory, defeat, research, tribute,
    remove_object, set_objective_state, activate_trigger,
    deactivate_trigger,
};

struct TriggerCondition {
    TriggerConditionKind kind{TriggerConditionKind::elapsed_ticks};
    Player player{Player::blue};
    ResourceKind resource{ResourceKind::none};
    EntityId entity{};
    int amount{};
    TilePosition first{};
    TilePosition second{};
};

struct TriggerEffect {
    TriggerEffectKind kind{TriggerEffectKind::message};
    Player player{Player::blue};
    ResourceKind resource{ResourceKind::none};
    UnitKind unit{UnitKind::villager};
    BuildingKind building{BuildingKind::house};
    Diplomacy diplomacy{Diplomacy::enemy};
    EntityId entity{};
    int objective_id{};
    int trigger_id{};
    Player target_player{Player::red};
    Technology technology{Technology::wheelbarrow};
    bool state{};
    int amount{};
    TilePosition position{};
    std::string text;
    std::string audio_file;
};

struct ObjectiveState {
    int id{};
    Player player{Player::blue};
    bool required{true};
    bool hidden{};
    bool completed{};
    std::string description;

    bool operator==(const ObjectiveState&) const = default;
};

struct ScenarioMessage {
    std::string text;
    Player player{Player::blue};
    std::uint64_t expires_tick{};
    std::string audio_file;

    bool operator==(const ScenarioMessage&) const = default;
};

struct TriggerState {
    int id{};
    int priority{};
    bool enabled{true};
    bool looping{};
    bool executable{};
    std::uint64_t activation_tick{};
    std::uint64_t last_fired_tick{};
    std::uint64_t fired_count{};
    std::vector<TriggerCondition> conditions;
    std::vector<TriggerEffect> effects;
    TriggerCondition condition;
    TriggerEffect effect;
};

enum class MarketResource {
    food,
    wood,
    stone,
};

struct Unit {
    EntityId id{};
    UnitKind kind{UnitKind::villager};
    EntityOwner owner{Player::blue};
    TilePosition position{};
    TilePosition previous_position{};
    TilePosition render_previous_subtile{};
    TilePosition render_current_subtile{};
    bool render_subtile_initialized{};
    TilePosition destination{};
    int hit_points{25};
    int attack{3};
    int attack_cooldown{};
    int movement_cooldown{};
    int movement_speed_remainder{};
    int formation_move_interval{};
    int formation_speed_numerator{};
    std::uint64_t formation_group_id{};
    TilePosition formation_anchor{-1, -1};
    TilePosition formation_slot{-1, -1};
    std::vector<FormationWaypoint> formation_waypoints;
    int gather_work_remainder{};
    std::uint64_t last_move_tick{};
    EntityId attack_target_id{};
    bool attack_target_is_building{};
    bool attack_target_auto{};
    EntityId repair_target_id{};
    int repair_wood_remainder{};
    int repair_stone_remainder{};
    bool moving{};
    std::vector<TilePosition> path;
    std::size_t next_path_step{};
    std::vector<TilePosition> waypoints;
    TilePosition resource_target{-1, -1};
    bool has_resource_target{};
    bool returning_resource{};
    ResourceKind carried_resource{ResourceKind::none};
    int carried_amount{};
    EntityId resource_building_id{};
    EntityId resource_unit_id{};
    int food_remaining{};
    int food_decay_remainder{};
    EntityId garrison_target_id{};
    EntityId garrisoned_in{};
    TilePosition attack_move_destination{-1, -1};
    bool attack_moving{};
    TilePosition patrol_origin{-1, -1};
    TilePosition patrol_destination{-1, -1};
    bool patrolling{};
    TilePosition attack_ground_target{-1, -1};
    bool attacking_ground{};
    EntityId guard_target_id{};
    bool guard_target_is_building{};
    UnitStance stance{UnitStance::aggressive};
    TilePosition stance_anchor{};
    bool returning_to_stance{};
    EntityId conversion_target_id{};
    int conversion_progress{};
    int conversion_cooldown{};
    EntityId healing_target_id{};
    bool carrying_relic{};
    EntityId relic_target_id{};
    EntityId relic_deposit_target_id{};
    EntityId trade_home_market_id{};
    EntityId trade_target_market_id{};
    bool trade_returning{};
    bool trade_waiting{};
    int trade_work_ticks_remaining{};
    int trebuchet_transform_ticks_remaining{};
    bool trebuchet_transform_to_packed{};
    std::optional<EntityOwner> last_damage_owner;
};

struct ProductionOrder {
    UnitKind kind{UnitKind::villager};
    int ticks_remaining{};
    int paid_wood{};
    int paid_food{};
    int paid_gold{};
    int work_remainder{};
};

struct Building {
    EntityId id{};
    BuildingKind kind{BuildingKind::town_center};
    EntityOwner owner{Player::blue};
    TilePosition position{};
    int hit_points{500};
    std::vector<ProductionOrder> production_queue;
    int relic_count{};
    int construction_ticks_remaining{};
    EntityId builder_id{};
    std::vector<EntityId> builder_ids;
    int construction_work_remainder{};
    int resource_amount{};
    Age age_research_target{Age::dark};
    int age_research_ticks_remaining{};
    Technology technology_research_target{Technology::wheelbarrow};
    int technology_research_ticks_remaining{};
    int attack_cooldown{};
    TilePosition rally_point{};
    bool has_rally_point{};
    bool gate_open{};
    std::optional<EntityOwner> last_damage_owner;

    [[nodiscard]] bool completed() const {
        return construction_ticks_remaining == 0;
    }
};

struct Projectile {
    EntityOwner owner{Player::blue};
    EntityId target{};
    bool target_is_building{};
    TilePosition origin{};
    TilePosition destination{};
    int damage{};
    DamageClass damage_class{DamageClass::melee};
    int ticks_remaining{};
    int total_ticks{};
    int visual_lane{};
    int splash_radius{};
    UnitKind source_kind{UnitKind::villager};
    int splash_radius_half_tiles{};
    bool source_is_building{};
    BuildingKind source_building_kind{BuildingKind::town_center};
    int projectile_speed_tenths{};
    EntityId source_entity_id{};
};

struct ImpactEffect {
    TilePosition position{};
    bool splash{};
    int ticks_remaining{};
    int total_ticks{};
    UnitKind source_kind{UnitKind::villager};
    bool source_is_building{};
    BuildingKind source_building_kind{BuildingKind::town_center};
    EntityId source_entity_id{};
};

struct UnitDeathEffect {
    TilePosition position{};
    UnitKind kind{UnitKind::villager};
    EntityOwner owner{Player::blue};
    int ticks_remaining{};
    int total_ticks{};
    EntityId entity_id{};
    TilePosition previous_position{};
};

struct BuildingRubbleEffect {
    TilePosition position{};
    BuildingKind kind{BuildingKind::house};
    EntityOwner owner{Player::blue};
    int ticks_remaining{};
    int total_ticks{};
    EntityId entity_id{};
};

struct Economy {
    int wood{};
    int food{};
    int gold{};
    int stone{};
};

std::string_view name(UnitKind kind);
std::string_view name(BuildingKind kind);
std::string_view name(Player player);
std::string_view name(MatchOutcome outcome);
std::string_view name(ResourceKind resource);
std::string_view name(Age age);
std::string_view name(Technology technology);
std::string_view name(UnitStance stance);
std::string_view name(Civilization civilization);

}  // namespace aoe
