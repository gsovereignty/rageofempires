#pragma once

#include <array>
#include <map>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

#include "aoe/game_map.hpp"
#include "aoe/match_statistics.hpp"
#include "aoe/player_roster.hpp"
#include "aoe/roster_diplomacy.hpp"

namespace aoe {

void validate_trigger_runtime_semantics(
    const GameMap& map,
    const std::vector<Unit>& units,
    const std::vector<Building>& buildings,
    const std::vector<ObjectiveState>& objectives,
    const std::vector<TriggerState>& triggers,
    std::uint64_t current_tick,
    bool require_initial_entity_references
);

class Simulation {
public:
    struct BuildingMemory {
        Building building;
        Age owner_age{Age::dark};
        Civilization owner_civilization{Civilization::generic};
        int maximum_hit_points{};
        int topology_frame{};
    };

    struct PlayerState {
        Economy economy{100, 200, 200, 200};
        FormationKind formation{FormationKind::compact};
        PlayerControllerState controller{PlayerControllerState::active};
        std::vector<bool> explored;
        // Per-viewer stale images. std::map keeps save/hash ordering stable.
        std::map<EntityId, BuildingMemory> remembered_buildings;
        // Enemy mobile attackers temporarily exposed to this viewer. Values
        // are exclusive simulation-tick expiries and remain deterministic.
        std::map<EntityId, std::uint64_t> attack_reveal_expiries;
        Age age{Age::dark};
        std::array<bool, technology_count> technologies{};
        Civilization civilization{Civilization::generic};
        int farm_reseed_queue{};
        int mayan_resource_remainder{};
        int aztec_relic_gold_remainder{};
        int victory_countdown{};
        VictoryCountdownKind countdown_kind{VictoryCountdownKind::none};
        std::uint64_t countdown_last_tick{};
    };

    static constexpr int maximum_farm_reseed_queue = 15;
    explicit Simulation(GameMap map);

    static Simulation create_demo();

    [[nodiscard]] const GameMap& map() const { return map_; }
    [[nodiscard]] const std::vector<Unit>& units() const { return units_; }
    [[nodiscard]] const std::vector<Building>& buildings() const {
        return buildings_;
    }
    [[nodiscard]] const std::vector<Projectile>& projectiles() const {
        return projectiles_;
    }
    [[nodiscard]] const std::vector<ImpactEffect>& impact_effects() const {
        return impact_effects_;
    }
    [[nodiscard]] const std::vector<UnitDeathEffect>& death_effects() const {
        return death_effects_;
    }
    [[nodiscard]] const std::vector<BuildingRubbleEffect>& rubble_effects()
        const {
        return rubble_effects_;
    }
    [[nodiscard]] const Economy& economy(Player player) const;
    [[nodiscard]] const Economy& economy(PlayerSlotId player) const;
    [[nodiscard]] const Economy& economy(EntityOwner player) const;
    [[nodiscard]] std::optional<EntityId> selected_unit() const {
        return selected_unit_;
    }
    [[nodiscard]] const std::vector<EntityId>& selected_units() const {
        return selected_units_;
    }
    [[nodiscard]] bool is_unit_selected(EntityId id) const;
    [[nodiscard]] std::optional<EntityId> selected_building() const {
        return selected_building_;
    }
    [[nodiscard]] std::uint64_t tick_number() const { return tick_number_; }
    [[nodiscard]] std::uint32_t commercial_random_state() const {
        return commercial_random_state_;
    }
    void seed_commercial_random(std::uint32_t seed) {
        commercial_random_state_ = seed;
    }
    int consume_commercial_random();
    [[nodiscard]] TilePosition render_previous_elevation_position(
        const Unit& unit
    ) const;
    [[nodiscard]] TilePosition render_current_elevation_position(
        const Unit& unit
    ) const;
    [[nodiscard]] EntityId next_entity_id() const { return next_id_; }
    [[nodiscard]] std::uint64_t next_formation_group_id() const {
        return next_formation_group_id_;
    }
    [[nodiscard]] MatchOutcome outcome() const { return outcome_; }
    [[nodiscard]] RosterMatchOutcome roster_outcome() const;
    [[nodiscard]] std::optional<MatchOutcome> legacy_roster_outcome() const;
    [[nodiscard]] PlayerControllerState controller_state(
        Player player
    ) const;
    [[nodiscard]] PlayerControllerState controller_state(
        PlayerSlotId player
    ) const;
    [[nodiscard]] bool player_commands_allowed(Player player) const {
        return controller_state(player) == PlayerControllerState::active;
    }
    [[nodiscard]] bool player_commands_allowed(PlayerSlotId player) const {
        const auto index = player.index();
        return index && roster_.slot(player).occupied &&
            controller_state(player) == PlayerControllerState::active;
    }
    [[nodiscard]] bool observer_perspective(Player player) const {
        return controller_state(player) != PlayerControllerState::active;
    }
    [[nodiscard]] bool is_visible_to_controller(
        Player player, TilePosition position
    ) const;
    [[nodiscard]] bool is_unit_visible(
        Player player, const Unit& unit
    ) const;
    [[nodiscard]] bool is_unit_visible(
        EntityOwner player, const Unit& unit
    ) const;
    [[nodiscard]] bool is_unit_visible_to_controller(
        Player player, const Unit& unit
    ) const;
    [[nodiscard]] bool is_explored_to_controller(
        Player player, TilePosition position
    ) const;
    [[nodiscard]] const MatchRules& match_rules() const {
        return match_rules_;
    }
    [[nodiscard]] const std::vector<ObjectiveState>& objectives() const {
        return objectives_;
    }
    [[nodiscard]] const std::vector<TriggerState>& triggers() const {
        return triggers_;
    }
    [[nodiscard]] const std::vector<ScenarioMessage>& scenario_messages() const {
        return scenario_messages_;
    }
    void set_match_rules(MatchRules rules);
    void replace_match_state(
        MatchRules rules, MatchOutcome outcome,
        int blue_countdown, int red_countdown,
        VictoryCountdownKind blue_kind,
        VictoryCountdownKind red_kind
    );
    void replace_controller_states(
        PlayerControllerState blue,
        PlayerControllerState red
    );
    [[nodiscard]] int victory_countdown(Player player) const;
    [[nodiscard]] int victory_countdown(PlayerSlotId player) const {
        return player_state(player).victory_countdown;
    }
    [[nodiscard]] VictoryCountdownKind countdown_kind(Player player) const {
        return player == Player::blue
            ? player_states_[0].countdown_kind
            : player == Player::red
                ? player_states_[1].countdown_kind
                : VictoryCountdownKind::none;
    }
    [[nodiscard]] VictoryCountdownKind countdown_kind(
        PlayerSlotId player
    ) const {
        return player_state(player).countdown_kind;
    }
    [[nodiscard]] int score(Player player) const;
    [[nodiscard]] MatchStatistics match_statistics() const;
    [[nodiscard]] LegacyMatchStatistics legacy_match_statistics() const;
    [[nodiscard]] const PlayerStatistics& player_statistics(
        PlayerSlotId player
    ) const;
    [[nodiscard]] int population(Player player) const;
    [[nodiscard]] int population_capacity(Player player) const;
    [[nodiscard]] int farm_capacity(Player player) const;
    [[nodiscard]] int effective_building_wood_cost(
        Player player,
        BuildingKind kind
    ) const;
    [[nodiscard]] int effective_building_stone_cost(
        Player player,
        BuildingKind kind
    ) const;
    [[nodiscard]] int farm_reseed_queue(Player player) const {
        return player == Player::blue
            ? player_states_[0].farm_reseed_queue
            : player_states_[1].farm_reseed_queue;
    }
    [[nodiscard]] int effective_carry_capacity(const Unit& unit) const;
    [[nodiscard]] int mayan_resource_remainder(Player player) const {
        return player == Player::blue
            ? player_states_[0].mayan_resource_remainder
            : player == Player::red
                ? player_states_[1].mayan_resource_remainder : 0;
    }
    [[nodiscard]] int aztec_relic_gold_remainder(Player player) const {
        return player == Player::blue
            ? player_states_[0].aztec_relic_gold_remainder
            : player == Player::red
                ? player_states_[1].aztec_relic_gold_remainder : 0;
    }
    [[nodiscard]] int effective_ship_movement_numerator(
        const Unit& unit
    ) const;
    [[nodiscard]] Age age(Player player) const;
    [[nodiscard]] Age age(PlayerSlotId player) const;
    [[nodiscard]] Age age(EntityOwner player) const;
    [[nodiscard]] bool has_technology(
        Player player,
        Technology technology
    ) const;
    [[nodiscard]] bool has_technology(
        PlayerSlotId player,
        Technology technology
    ) const;
    [[nodiscard]] bool has_technology(
        EntityOwner player,
        Technology technology
    ) const;
    [[nodiscard]] int maximum_hit_points(const Unit& unit) const;
    [[nodiscard]] int maximum_hit_points(const Building& building) const;
    [[nodiscard]] int melee_armor(const Unit& unit) const;
    [[nodiscard]] int melee_armor(const Building& building) const;
    [[nodiscard]] int pierce_armor(const Unit& unit) const;
    [[nodiscard]] int pierce_armor(const Building& building) const;
    [[nodiscard]] int building_class_11_armor(
        const Building& building
    ) const;
    [[nodiscard]] int sappers_attack_bonus(
        Player player, BuildingKind target
    ) const;
    [[nodiscard]] int defensive_ship_bonus(
        Player player, BuildingKind source
    ) const;
    [[nodiscard]] int effective_attack_range(const Unit& unit) const;
    [[nodiscard]] int effective_minimum_attack_range(
        const Unit& unit
    ) const;
    [[nodiscard]] int effective_attack_interval(const Unit& unit) const;
    [[nodiscard]] int unique_unit_movement_numerator(
        const Unit& unit
    ) const;
    [[nodiscard]] int effective_siege_movement_numerator(
        const Unit& unit
    ) const;
    [[nodiscard]] int berserk_regeneration_per_three_ticks(
        const Unit& unit
    ) const;
    [[nodiscard]] int effective_unit_vision_range(const Unit& unit) const;
    [[nodiscard]] int effective_building_attack(
        const Building& building
    ) const;
    [[nodiscard]] int effective_building_attack_range(
        const Building& building
    ) const;
    [[nodiscard]] bool is_visible(
        Player player,
        TilePosition position
    ) const;
    [[nodiscard]] bool is_visible(
        PlayerSlotId player,
        TilePosition position
    ) const;
    [[nodiscard]] bool is_visible(
        EntityOwner player,
        TilePosition position
    ) const;
    [[nodiscard]] bool is_building_visible(
        Player player,
        const Building& building
    ) const;
    [[nodiscard]] bool is_building_visible(
        EntityOwner player,
        const Building& building
    ) const;
    [[nodiscard]] bool is_explored(
        Player player,
        TilePosition position
    ) const;
    [[nodiscard]] bool is_explored(
        PlayerSlotId player,
        TilePosition position
    ) const;
    [[nodiscard]] std::vector<TilePosition> explored_tiles(
        Player player
    ) const;
    [[nodiscard]] const std::map<EntityId, BuildingMemory>&
    remembered_buildings(Player player) const;
    [[nodiscard]] std::vector<EntityId> idle_villagers(
        Player player
    ) const;
    [[nodiscard]] std::vector<EntityId> idle_military(
        Player player
    ) const;

    EntityId add_unit(UnitKind kind, Player owner, TilePosition position);
    EntityId add_unit(
        UnitKind kind, EntityOwner owner, TilePosition position
    );
    EntityId add_unit(
        UnitKind kind, PlayerSlotId owner, TilePosition position
    );
    EntityId add_building(
        BuildingKind kind,
        Player owner,
        TilePosition position
    );
    EntityId add_building(
        BuildingKind kind,
        EntityOwner owner,
        TilePosition position
    );
    EntityId add_building(
        BuildingKind kind,
        PlayerSlotId owner,
        TilePosition position
    );
    bool select_unit_at(TilePosition position, Player player);
    bool select_units_in_area(
        TilePosition first,
        TilePosition second,
        Player player
    );
    bool select_units(
        const std::vector<EntityId>& ids,
        Player player
    );
    [[nodiscard]] std::vector<TilePosition> formation_destinations(
        const std::vector<EntityId>& unit_ids,
        TilePosition center
    ) const;
    [[nodiscard]] std::vector<TilePosition> formation_destinations(
        const std::vector<EntityId>& unit_ids,
        TilePosition center,
        FormationKind kind,
        std::optional<TilePosition> facing = std::nullopt
    ) const;
    bool set_formation_kind(Player player, FormationKind kind);
    bool set_formation_kind(PlayerSlotId player, FormationKind kind);
    [[nodiscard]] FormationKind formation_kind(Player player) const;
    [[nodiscard]] FormationKind formation_kind(PlayerSlotId player) const;
    bool command_formation(
        const std::vector<EntityId>& unit_ids,
        TilePosition center,
        FormationKind kind
    );
    bool command_formation_order(
        const std::vector<EntityId>& unit_ids,
        TilePosition center, FormationKind kind,
        FormationOrderKind order,
        EntityId guard_target = 0,
        bool guard_target_is_building = false
    );
    bool select_building_at(TilePosition position, Player player);
    bool command_unit(EntityId unit_id, TilePosition destination);
    bool command_gather_unit(EntityId villager_id, EntityId herdable_id);
    bool command_attack_move(EntityId unit_id, TilePosition destination);
    bool command_attack_ground(EntityId unit_id, TilePosition destination);
    bool command_convert(EntityId monk_id, EntityId target_id);
    bool command_heal(EntityId monk_id, EntityId target_id);
    bool command_collect_relic(EntityId monk_id, EntityId relic_id);
    bool command_deposit_relic(EntityId monk_id, EntityId monastery_id);
    bool buy_resource(Player player, MarketResource resource);
    bool sell_resource(Player player, MarketResource resource);
    bool tribute_resource(
        Player from,
        Player to,
        ResourceKind resource,
        int amount
    );
    bool command_trade_route(EntityId cart, EntityId market);
    bool command_embark(EntityId unit, EntityId transport);
    bool command_disembark(EntityId transport, TilePosition shore);
    bool restore_garrison(EntityId unit, EntityId building);
    void validate_loaded_state() const;
    bool set_diplomacy(Player player, Player other, Diplomacy relation);
    [[nodiscard]] Diplomacy diplomacy(Player player, Player other) const;
    [[nodiscard]] Diplomacy diplomacy(
        EntityOwner player, EntityOwner other
    ) const;
    [[nodiscard]] Diplomacy diplomacy(Player player, EntityOwner other) const;
    [[nodiscard]] Diplomacy diplomacy(EntityOwner player, Player other) const;
    [[nodiscard]] Diplomacy diplomacy(
        PlayerSlotId player, PlayerSlotId other
    ) const;
    [[nodiscard]] bool is_enemy(Player player, Player other) const;
    [[nodiscard]] bool is_enemy(
        EntityOwner player, EntityOwner other
    ) const;
    [[nodiscard]] bool is_enemy(EntityOwner player, Player other) const;
    [[nodiscard]] bool is_enemy(Player player, EntityOwner other) const;
    [[nodiscard]] bool is_enemy(PlayerSlotId player, EntityOwner other) const;
    [[nodiscard]] bool is_enemy(EntityOwner player, PlayerSlotId other) const;
    [[nodiscard]] bool is_ally(Player player, Player other) const;
    [[nodiscard]] bool is_ally(
        EntityOwner player, EntityOwner other
    ) const;
    [[nodiscard]] bool is_ally(EntityOwner player, Player other) const;
    [[nodiscard]] bool is_ally(Player player, EntityOwner other) const;
    [[nodiscard]] bool is_ally(PlayerSlotId player, EntityOwner other) const;
    [[nodiscard]] bool is_ally(EntityOwner player, PlayerSlotId other) const;
    [[nodiscard]] Civilization civilization(Player player) const;
    [[nodiscard]] Civilization civilization(PlayerSlotId player) const;
    [[nodiscard]] Civilization civilization(EntityOwner player) const;
    [[nodiscard]] bool team_has_civilization(
        Player player,
        Civilization civilization
    ) const;
    [[nodiscard]] bool team_has_civilization(
        EntityOwner player,
        Civilization civilization
    ) const;
    bool set_civilization(Player player, Civilization civilization);
    void replace_player_state(PlayerSlotId player, PlayerState state);
    [[nodiscard]] const PlayerState& player_state(PlayerSlotId player) const;
    [[nodiscard]] const MatchRoster& roster() const { return roster_; }
    [[nodiscard]] const RosterDiplomacy& roster_diplomacy() const {
        return roster_diplomacy_;
    }
    void replace_roster(MatchRoster roster, RosterDiplomacy diplomacy);
    [[nodiscard]] int market_buy_price(MarketResource resource) const;
    [[nodiscard]] int market_sell_price(MarketResource resource) const;
    [[nodiscard]] int market_buy_price(
        Player player,
        MarketResource resource
    ) const;
    [[nodiscard]] int market_sell_price(
        Player player,
        MarketResource resource
    ) const;
    [[nodiscard]] int market_base_price(MarketResource resource) const;
    bool command_patrol(EntityId unit_id, TilePosition destination);
    bool command_guard(
        EntityId unit_id,
        EntityId target_id,
        bool target_is_building
    );
    bool queue_waypoint(EntityId unit_id, TilePosition destination);
    bool set_unit_stance(EntityId unit_id, UnitStance stance);
    bool command_pack_trebuchet(EntityId unit_id, bool pack);
    bool delete_unit(EntityId unit_id);
    bool delete_building(EntityId building_id);
    bool command_selected(TilePosition destination);
    bool stop_unit(EntityId unit_id);
    bool construct_building_at(
        EntityId builder_id,
        BuildingKind kind,
        TilePosition position
    );
    bool construct_building(
        BuildingKind kind,
        TilePosition position
    );
    bool queue_unit_at(EntityId building_id, UnitKind kind);
    bool queue_unit(UnitKind kind);
    bool cancel_production_at(EntityId building_id);
    bool set_rally_point(EntityId building_id, TilePosition position);
    bool reseed_farm(EntityId building_id);
    bool reseed_farm_immediately(EntityId building_id);
    bool consume_farm_reseed(EntityId building_id);
    bool ungarrison_at(EntityId building_id);
    [[nodiscard]] int garrison_count(EntityId building_id) const;
    bool resign(Player player);
    bool resign(PlayerSlotId player);
    bool advance_age_at(EntityId building_id);
    bool research_technology_at(
        EntityId building_id,
        Technology technology
    );
    void update();

    void replace_state(
        std::vector<Unit> units,
        std::vector<Building> buildings,
        Economy blue,
        Economy red,
        std::uint64_t tick_number
    );
    void merge_exploration(
        Player player,
        const std::vector<TilePosition>& explored
    );
    void replace_projectiles(std::vector<Projectile> projectiles);
    void replace_impact_effects(std::vector<ImpactEffect> effects);
    void replace_death_effects(std::vector<UnitDeathEffect> effects);
    void replace_rubble_effects(std::vector<BuildingRubbleEffect> effects);
    void replace_ages(Age blue, Age red);
    void replace_technologies(
        Player player,
        const std::vector<Technology>& technologies
    );
    void replace_market_prices(int food, int wood, int stone);
    void replace_diplomacy(Diplomacy relation);
    void replace_civilizations(Civilization blue, Civilization red);
    void replace_farm_reseed_queues(int blue, int red);
    void replace_mayan_resource_remainders(int blue, int red);
    void replace_aztec_relic_gold_remainders(int blue, int red);
    void replace_scenario_runtime(
        std::vector<ObjectiveState> objectives,
        std::vector<TriggerState> triggers,
        std::vector<ScenarioMessage> messages = {}
    );
    void replace_match_statistics(MatchStatistics statistics);
    void replace_commercial_random_state(std::uint32_t state) {
        commercial_random_state_ = state;
    }

private:
    struct RenderElevationPositions {
        TilePosition previous{};
        TilePosition current{};
    };

    void initialize_unit_render_elevation(const Unit& unit);
    void prune_unit_render_elevations();
    void refresh_unit_render_subtile(Unit& unit);
    bool evaluate_scenario_triggers();
    [[nodiscard]] bool trigger_condition_met(
        const TriggerState& trigger
    ) const;
    void apply_trigger_effect(const TriggerEffect& effect);
    [[nodiscard]] std::pair<int, int> formation_pace(
        const std::vector<EntityId>& unit_ids
    ) const;
    Unit* find_unit(EntityId id);
    Building* find_building(EntityId id);
    Unit* unit_at(TilePosition position);
    Building* building_at(TilePosition position);
    [[nodiscard]] TilePosition nearest_point_on_building(
        TilePosition source,
        const Building& building
    ) const;
    [[nodiscard]] int distance_to_building(
        TilePosition source,
        const Building& building
    ) const;
    [[nodiscard]] int combat_distance_squared(
        TilePosition first,
        TilePosition second
    ) const;
    [[nodiscard]] int combat_distance_squared(
        TilePosition source,
        const Building& building
    ) const;
    [[nodiscard]] int combat_distance_squared(
        const Building& first,
        const Building& second
    ) const;
    [[nodiscard]] int projectile_travel_ticks(
        int distance_squared,
        int projectile_speed_tenths
    ) const;
    [[nodiscard]] int distance_between_buildings(
        const Building& first,
        const Building& second
    ) const;
    Building* nearest_drop_off(const Unit& unit);
    bool route_to_resource_interaction(
        Unit& unit,
        TilePosition resource_target
    );
    bool route_to_nearest_resource(
        Unit& unit,
        ResourceKind resource
    );
    bool occupied(
        TilePosition position,
        EntityId except,
        std::optional<EntityOwner> mover = std::nullopt,
        bool plan_owned_gate = false
    ) const;
    bool footprint_available(
        BuildingKind kind,
        TilePosition position,
        EntityId except
    ) const;
    void detach_builder(EntityId unit_id);
    bool route_unit(Unit& unit, TilePosition destination);
    std::optional<TilePosition> spawn_position(const Building& building) const;
    void perform_attack(Unit& attacker, Unit& defender);
    void perform_attack(Unit& attacker, Building& defender);
    void detonate_demolition_ship(Unit& attacker);
    void detonate_petard(
        Unit& attacker,
        TilePosition center,
        EntityId primary_target
    );
    void launch_projectile(Unit& attacker, const Unit& defender);
    void launch_projectile(Unit& attacker, const Building& defender);
    void launch_ground_projectile(
        Unit& attacker,
        TilePosition destination
    );
    void update_building_defenses();
    void update_projectiles();
    void reveal_attacker_to(
        const Unit& attacker,
        EntityOwner victim,
        int minimum_duration_ticks = 1
    );
    void reveal_attacker_to_ground_victims(
        const Unit& attacker,
        TilePosition center,
        int radius,
        int minimum_duration_ticks
    );
    [[nodiscard]] bool has_attack_reveal(
        EntityOwner viewer, EntityId attacker
    ) const;
    void prune_attack_reveals();
    void gather(Unit& unit);
    std::pair<int, int> finite_resource_yield(
        Player player, int available, int requested
    );
    [[nodiscard]] ResourceKind work_resource(const Unit& unit) const;
    [[nodiscard]] int work_resource_amount(const Unit& unit) const;
    void update_production();
    void update_match_outcome();
    void update_exploration();
    void update_gate_states();
    void credit_gathered(
        Player player, ResourceKind resource, int amount
    );
    void sample_match_statistics();
    [[nodiscard]] PlayerStatistics& mutable_statistics(Player player);
    [[nodiscard]] PlayerStatistics& mutable_statistics(EntityOwner player);
    [[nodiscard]] int committed_population(Player player) const;
    [[nodiscard]] bool has_age_prerequisites(
        Player player,
        Age target
    ) const;
    [[nodiscard]] int effective_building_vision_range(
        const Building& building
    ) const;
    [[nodiscard]] int cavalry_movement_numerator(const Unit& unit) const;
    [[nodiscard]] int ship_movement_numerator(const Unit& unit) const;
    [[nodiscard]] int transport_capacity(Player player) const;
    [[nodiscard]] int garrison_capacity(BuildingKind building) const;
    [[nodiscard]] int carry_capacity(const Unit& unit) const;
    [[nodiscard]] bool can_garrison(
        const Unit& unit,
        const Building& building
    ) const;
    void apply_man_at_arms_upgrade(Player player);
    void apply_crossbowman_upgrade(Player player);
    void apply_pikeman_upgrade(Player player);
    void apply_long_swordsman_upgrade(Player player);
    void apply_cavalier_upgrade(Player player);
    void apply_paladin_upgrade(Player player);
    void apply_light_cavalry_upgrade(Player player);
    void apply_hussar_upgrade(Player player);
    void apply_two_handed_swordsman_upgrade(Player player);
    void apply_champion_upgrade(Player player);
    void apply_arbalester_upgrade(Player player);
    void apply_elite_skirmisher_upgrade(Player player);
    void apply_loom_upgrade(Player player);
    void apply_bloodlines_upgrade(Player player);
    void apply_horse_collar_upgrade(Player player);
    void apply_fortified_wall_upgrade(Player player);
    void apply_guard_tower_upgrade(Player player);
    void apply_keep_upgrade(Player player);
    void refresh_unit_attacks(Player player);
    [[nodiscard]] bool automatic_splash_target_is_safe(
        const Unit& attacker,
        TilePosition impact
    ) const;
    bool acquire_nearby_target(Unit& unit, bool allow_chase = true);
    [[nodiscard]] std::size_t map_index(TilePosition position) const;

    GameMap map_;
    std::vector<Unit> units_;
    std::unordered_map<EntityId, RenderElevationPositions>
        unit_render_elevations_;
    std::vector<Building> buildings_;
    std::vector<Projectile> projectiles_;
    std::vector<ImpactEffect> impact_effects_;
    std::vector<UnitDeathEffect> death_effects_;
    std::vector<BuildingRubbleEffect> rubble_effects_;
    std::array<PlayerState, 8> player_states_{};
    MatchRoster roster_{MatchRoster::legacy_blue_red()};
    RosterDiplomacy roster_diplomacy_{
        RosterDiplomacy::legacy_blue_red()
    };
    std::optional<EntityId> selected_unit_;
    std::vector<EntityId> selected_units_;
    std::uint64_t next_formation_group_id_{1};
    std::optional<EntityId> selected_building_;
    EntityId next_id_{1};
    std::uint64_t tick_number_{};
    // MSVCRT global random state used by commercial gameplay paths. CRT rand
    // starts from seed 1 when srand has not been called.
    std::uint32_t commercial_random_state_{1};
    MatchOutcome outcome_{MatchOutcome::ongoing};
    MatchRules match_rules_{};
    int food_market_price_{100};
    int wood_market_price_{100};
    int stone_market_price_{100};
    Diplomacy blue_red_diplomacy_{Diplomacy::enemy};
    std::vector<ObjectiveState> objectives_;
    std::vector<TriggerState> triggers_;
    std::vector<ScenarioMessage> scenario_messages_;
    MatchStatistics match_statistics_;
};

}  // namespace aoe
