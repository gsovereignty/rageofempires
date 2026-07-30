#include "aoe/multiplayer_transport.hpp"

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {

void require_at(bool condition, int line) {
    if (!condition) {
        throw std::runtime_error(
            "multiplayer transport roster test failed at " +
            std::to_string(line)
        );
    }
}
#define require(condition) require_at((condition), __LINE__)

aoe::PlayerSlotId slot(std::size_t index) {
    return *aoe::PlayerSlotId::from_index(index);
}

aoe::LockstepFrame make_frame(
    aoe::LockstepFrameKind kind,
    aoe::PlayerSlotId source,
    const aoe::LockstepSessionConfig& config
) {
    aoe::LockstepFrame frame;
    frame.kind = kind;
    frame.source = source;
    frame.player =
        aoe::player_slot_to_legacy(source).value_or(aoe::Player::neutral);
    frame.scenario_digest = config.scenario_digest;
    return frame;
}

template <typename Pump>
void pump_frames(Pump&& pump, int expected) {
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(5);
    int received{};
    while (received < expected &&
           std::chrono::steady_clock::now() < deadline) {
        if (pump().status == aoe::TcpPollStatus::frame) {
            ++received;
        } else {
            std::this_thread::yield();
        }
    }
    require(received == expected);
}

}  // namespace

int main() {
    const auto roster = aoe::MatchRoster::create({
        {
            slot(0), true, *aoe::TeamId::numbered(1), false,
            {{"host", aoe::RosterControllerKind::human}},
        },
        {
            slot(1), true, *aoe::TeamId::numbered(2), false,
            {{"red", aoe::RosterControllerKind::human}},
        },
        {
            slot(2), true, *aoe::TeamId::numbered(1), false,
            {{"green", aoe::RosterControllerKind::human}},
        },
    });
    require(roster.has_value());
    const auto diplomacy = aoe::RosterDiplomacy::create(*roster);
    require(diplomacy.has_value());
    aoe::LockstepSessionConfig config;
    config.scenario_digest = "localhost-three-peer";
    config.native_roster = *roster;
    config.native_diplomacy = *diplomacy;
    config.host_slot = slot(0);

    aoe::Simulation base(aoe::GameMap(12, 8));
    base.replace_roster(*roster, *diplomacy);
    auto green_state = base.player_state(slot(2));
    green_state.controller = aoe::PlayerControllerState::active;
    base.replace_player_state(slot(2), green_state);
    const aoe::EntityId blue_unit =
        base.add_unit(aoe::UnitKind::militia, slot(0), {2, 4});
    const aoe::EntityId red_unit =
        base.add_unit(aoe::UnitKind::militia, slot(1), {6, 4});
    const aoe::EntityId green_unit =
        base.add_unit(aoe::UnitKind::militia, slot(2), {9, 4});
    aoe::Simulation host_simulation = base;
    aoe::Simulation red_simulation = base;
    aoe::Simulation green_simulation = base;

    aoe::LocalhostMultiPeerHost host(0, config, 20, 1);
    aoe::LocalhostMultiPeerClient red(
        host.port(), config, slot(1), 20, 1
    );
    host.accept_peer(slot(1));
    aoe::LocalhostMultiPeerClient green(
        host.port(), config, slot(2), 20, 1
    );
    host.accept_peer(slot(2));
    require(host.accepted_peer_count() == 2);

    auto host_hello = make_frame(
        aoe::LockstepFrameKind::hello, slot(0), config
    );
    host_hello.config = config;
    host_hello.config_digest = aoe::lockstep_config_digest(config);
    require(host.send(host_hello, host_simulation));
    auto red_hello = make_frame(
        aoe::LockstepFrameKind::hello, slot(1), config
    );
    red_hello.config = config;
    red_hello.config_digest = aoe::lockstep_config_digest(config);
    require(red.send(red_hello, red_simulation, 5));
    pump_frames(
        [&] { return host.pump_one(host_simulation); }, 1
    );
    auto green_hello = make_frame(
        aoe::LockstepFrameKind::hello, slot(2), config
    );
    green_hello.config = config;
    green_hello.config_digest = aoe::lockstep_config_digest(config);
    require(green.send(green_hello, green_simulation));
    pump_frames(
        [&] { return host.pump_one(host_simulation); }, 1
    );
    pump_frames([&] { return red.pump_one(red_simulation); }, 2);
    pump_frames([&] { return green.pump_one(green_simulation); }, 2);

    const auto ready = [&](aoe::PlayerSlotId source) {
        return make_frame(aoe::LockstepFrameKind::ready, source, config);
    };
    require(host.send(ready(slot(0)), host_simulation));
    require(red.send(ready(slot(1)), red_simulation));
    pump_frames(
        [&] { return host.pump_one(host_simulation); }, 1
    );
    require(green.send(ready(slot(2)), green_simulation));
    pump_frames(
        [&] { return host.pump_one(host_simulation); }, 1
    );
    pump_frames([&] { return red.pump_one(red_simulation); }, 2);
    pump_frames([&] { return green.pump_one(green_simulation); }, 2);
    require(host.session().status() == aoe::LockstepStatus::ready);
    require(red.session().status() == aoe::LockstepStatus::ready);
    require(green.session().status() == aoe::LockstepStatus::ready);

    require(host.send(
        make_frame(aoe::LockstepFrameKind::start, slot(0), config),
        host_simulation
    ));
    pump_frames([&] { return red.pump_one(red_simulation); }, 1);
    pump_frames([&] { return green.pump_one(green_simulation); }, 1);

    const std::string hash =
        aoe::deterministic_state_hash(host_simulation);
    auto blue_turn = make_frame(
        aoe::LockstepFrameKind::turn, slot(0), config
    );
    blue_turn.state_hash = hash;
    blue_turn.commands = {aoe::StopUnitCommand{blue_unit}};
    auto red_turn = make_frame(
        aoe::LockstepFrameKind::turn, slot(1), config
    );
    red_turn.state_hash = hash;
    red_turn.commands = {aoe::StopUnitCommand{red_unit}};
    auto green_turn = make_frame(
        aoe::LockstepFrameKind::turn, slot(2), config
    );
    green_turn.state_hash = hash;
    green_turn.commands = {aoe::StopUnitCommand{green_unit}};
    require(green.send(green_turn, green_simulation, 3));
    pump_frames(
        [&] { return host.pump_one(host_simulation); }, 1
    );
    require(host.send(blue_turn, host_simulation));
    require(red.send(red_turn, red_simulation));
    // Identical duplicate survives TCP relay but never duplicates command.
    require(red.send(red_turn, red_simulation));
    pump_frames(
        [&] { return host.pump_one(host_simulation); }, 2
    );
    pump_frames([&] { return red.pump_one(red_simulation); }, 2);
    pump_frames([&] { return green.pump_one(green_simulation); }, 3);
    require(host.advance(host_simulation));
    require(red.advance(red_simulation));
    require(green.advance(green_simulation));
    require(aoe::deterministic_state_hash(host_simulation) ==
        aoe::deterministic_state_hash(red_simulation));
    require(aoe::deterministic_state_hash(host_simulation) ==
        aoe::deterministic_state_hash(green_simulation));
    require(host.session().replay().commands().size() == 3);
    require(host.session().replay().commands()[0].source == slot(0));
    require(host.session().replay().commands()[1].source == slot(1));
    require(host.session().replay().commands()[2].source == slot(2));

    green.close();
    const auto disconnect_deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (host.session().status() != aoe::LockstepStatus::disconnected &&
           std::chrono::steady_clock::now() < disconnect_deadline) {
        (void)host.pump_one(host_simulation);
        std::this_thread::yield();
    }
    require(host.session().status() == aoe::LockstepStatus::disconnected);
    pump_frames([&] { return red.pump_one(red_simulation); }, 1);
    require(red.session().status() ==
        aoe::LockstepStatus::disconnected);

    std::cout << "multiplayer transport roster tests passed\n";
}
