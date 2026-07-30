#include <filesystem>
#include <iostream>
#include <source_location>

#include "aoe/format_versions.hpp"
#include "aoe/game_command.hpp"
#include "aoe/multiplayer_transport.hpp"
#include "aoe/save_game.hpp"

namespace {

void require(
    bool value,
    const std::source_location location = std::source_location::current()
) {
    if (!value) {
        std::cerr << "Requirement failed at " << location.file_name() << ':'
                  << location.line() << '\n';
        std::abort();
    }
}

void perspective_unlocks_only_after_resignation() {
    aoe::Simulation simulation(aoe::GameMap(20, 20));
    simulation.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {1, 1}
    );
    simulation.add_unit(
        aoe::UnitKind::villager, aoe::Player::red, {18, 18}
    );
    const aoe::TilePosition hidden{18, 18};
    require(!simulation.is_visible(aoe::Player::blue, hidden));
    require(!simulation.is_visible_to_controller(
        aoe::Player::blue, hidden
    ));
    require(!simulation.is_explored_to_controller(
        aoe::Player::blue, hidden
    ));
    require(simulation.resign(aoe::Player::blue));
    require(
        simulation.controller_state(aoe::Player::blue) ==
        aoe::PlayerControllerState::resigned
    );
    require(!simulation.is_visible(aoe::Player::blue, hidden));
    require(simulation.is_visible_to_controller(
        aoe::Player::blue, hidden
    ));
    require(simulation.is_explored_to_controller(
        aoe::Player::blue, hidden
    ));
}

void replay_rejects_every_post_resign_command() {
    aoe::Simulation simulation(aoe::GameMap(10, 8));
    const auto villager = simulation.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {1, 1}
    );
    simulation.add_unit(
        aoe::UnitKind::villager, aoe::Player::red, {8, 6}
    );
    aoe::Replay replay;
    replay.record(0, aoe::ResignCommand{aoe::Player::blue});
    replay.record(0, aoe::MoveUnitCommand{villager, {4, 4}});
    replay.record(
        0,
        aoe::TributeResourceCommand{
            aoe::Player::blue, aoe::Player::red,
            aoe::ResourceKind::food, 10
        }
    );
    bool rejected{};
    try {
        replay.apply_current_tick(simulation);
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    require(rejected);
    require(simulation.units().front().position == aoe::TilePosition(1, 1));
    require(!simulation.units().front().moving);
    require(simulation.economy(aoe::Player::blue).food == 200);
    require(!aoe::execute(
        simulation, aoe::ResignCommand{aoe::Player::blue}
    ));
}

void save106_persists_controller_state() {
    aoe::Simulation simulation(aoe::GameMap(8, 8));
    simulation.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {1, 1}
    );
    simulation.add_unit(
        aoe::UnitKind::villager, aoe::Player::red, {6, 6}
    );
    require(simulation.resign(aoe::Player::blue));
    const auto path = std::filesystem::temp_directory_path() /
        "aoe-current-resigned-controller.save";
    aoe::save_game(simulation, path);
    aoe::Simulation restored = aoe::load_game(path);
    std::filesystem::remove(path);
    require(aoe::reconstruction_save_version >= 106);
    require(
        restored.controller_state(aoe::Player::blue) ==
        aoe::PlayerControllerState::resigned
    );
    require(
        restored.controller_state(aoe::Player::red) ==
        aoe::PlayerControllerState::active
    );
    require(restored.observer_perspective(aoe::Player::blue));
}

void allied_resignation_uses_surviving_team_outcome() {
    aoe::Simulation simulation(aoe::GameMap(10, 8));
    simulation.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {1, 1}
    );
    simulation.add_unit(
        aoe::UnitKind::villager, aoe::Player::red, {8, 6}
    );
    simulation.replace_diplomacy(aoe::Diplomacy::ally);
    require(simulation.resign(aoe::Player::blue));
    require(simulation.outcome() == aoe::MatchOutcome::red_victory);
    require(
        simulation.controller_state(aoe::Player::red) ==
        aoe::PlayerControllerState::active
    );
}

void multiplayer_runtime_becomes_read_only_observer() {
    auto runtime = aoe::LocalhostMultiplayerRuntime::host(
        0, std::string{"observer-test"}
    );
    aoe::Simulation simulation(aoe::GameMap(8, 8));
    simulation.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {1, 1}
    );
    simulation.add_unit(
        aoe::UnitKind::villager, aoe::Player::red, {6, 6}
    );
    require(runtime.queue_command(
        aoe::ResignCommand{aoe::Player::blue}
    ));
    require(!runtime.queue_command(
        aoe::MoveUnitCommand{simulation.units().front().id, {2, 2}}
    ));
    require(simulation.resign(aoe::Player::blue));
    runtime.poll_transport(simulation);
    require(
        runtime.local_controller_state() ==
        aoe::PlayerControllerState::observer
    );
    require(!runtime.queue_command(
        aoe::ResignCommand{aoe::Player::blue}
    ));
    require(!runtime.send_chat("after resign", aoe::ChatAudience::all));
}

}  // namespace

int main() {
    perspective_unlocks_only_after_resignation();
    replay_rejects_every_post_resign_command();
    save106_persists_controller_state();
    allied_resignation_uses_surviving_team_outcome();
    multiplayer_runtime_becomes_read_only_observer();
    std::cout << "All resignation observer tests passed\n";
}
