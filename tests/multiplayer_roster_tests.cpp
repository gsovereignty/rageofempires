#include "aoe/multiplayer.hpp"
#include "aoe/multiplayer_checkpoint.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {

void require_at(bool condition, int line) {
    if (!condition) {
        throw std::runtime_error(
            "multiplayer roster test failed at " + std::to_string(line)
        );
    }
}
#define require(condition) require_at((condition), __LINE__)

aoe::PlayerSlotId slot(std::size_t index) {
    return *aoe::PlayerSlotId::from_index(index);
}

struct Fixture {
    aoe::Simulation simulation{aoe::GameMap(12, 8)};
    aoe::LockstepSessionConfig config;
    aoe::EntityId blue{};
    aoe::EntityId red{};
    aoe::EntityId green{};

    Fixture() {
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
        auto diplomacy = aoe::RosterDiplomacy::create(*roster);
        require(diplomacy.has_value());
        require(diplomacy->set_stance(
            slot(2), slot(1), aoe::Diplomacy::neutral
        ));
        simulation.replace_roster(*roster, *diplomacy);
        auto green_state = simulation.player_state(slot(2));
        green_state.civilization = aoe::Civilization::mongols;
        green_state.controller = aoe::PlayerControllerState::active;
        simulation.replace_player_state(slot(2), green_state);
        blue = simulation.add_unit(
            aoe::UnitKind::militia, slot(0), {2, 4}
        );
        red = simulation.add_unit(
            aoe::UnitKind::militia, slot(1), {6, 4}
        );
        green = simulation.add_unit(
            aoe::UnitKind::militia, slot(2), {9, 4}
        );
        config.scenario_digest = "native-roster";
        config.native_roster = *roster;
        config.native_diplomacy = *diplomacy;
        config.native_civilizations[2] =
            aoe::Civilization::mongols;
        config.host_slot = slot(0);
    }
};

aoe::LockstepFrame frame(
    aoe::LockstepFrameKind kind,
    aoe::PlayerSlotId source,
    const std::string& digest
) {
    aoe::LockstepFrame result;
    result.kind = kind;
    result.source = source;
    result.player =
        aoe::player_slot_to_legacy(source).value_or(aoe::Player::neutral);
    result.scenario_digest = digest;
    return result;
}

void handshake(
    aoe::LockstepSession& session,
    const Fixture& fixture
) {
    for (std::size_t index = 0; index < 3; ++index) {
        auto hello = frame(
            aoe::LockstepFrameKind::hello,
            slot(index), fixture.config.scenario_digest
        );
        hello.config = fixture.config;
        hello.config_digest =
            aoe::lockstep_config_digest(fixture.config);
        require(session.receive(hello, fixture.simulation));
    }
    for (std::size_t index = 0; index < 2; ++index) {
        require(session.receive(
            frame(
                aoe::LockstepFrameKind::ready,
                slot(index), fixture.config.scenario_digest
            ),
            fixture.simulation
        ));
    }
    require(session.status() == aoe::LockstepStatus::handshaking);
    require(session.receive(
        frame(
            aoe::LockstepFrameKind::ready,
            slot(2), fixture.config.scenario_digest
        ),
        fixture.simulation
    ));
    require(session.status() == aoe::LockstepStatus::ready);
    require(session.receive(
        frame(
            aoe::LockstepFrameKind::start,
            slot(0), fixture.config.scenario_digest
        ),
        fixture.simulation
    ));
}

}  // namespace

int main() {
    Fixture fixture;
    const std::string canonical =
        aoe::canonical_lockstep_config(fixture.config);
    require(canonical.starts_with("lockstep-config-v2 "));
    auto hello = frame(
        aoe::LockstepFrameKind::hello, slot(2),
        fixture.config.scenario_digest
    );
    hello.config = fixture.config;
    hello.config_digest =
        aoe::lockstep_config_digest(fixture.config);
    const aoe::LockstepFrame decoded =
        aoe::decode_lockstep_frame(aoe::encode_lockstep_frame(hello));
    require(decoded.source == slot(2));
    require(decoded.config.has_value());
    require(aoe::canonical_lockstep_config(*decoded.config) == canonical);

    aoe::LockstepSession session(fixture.config, 10, 1);
    handshake(session, fixture);
    require(session.status() == aoe::LockstepStatus::running);
    const std::string hash =
        aoe::deterministic_state_hash(fixture.simulation);
    auto blue = frame(
        aoe::LockstepFrameKind::turn, slot(0),
        fixture.config.scenario_digest
    );
    blue.state_hash = hash;
    blue.commands = {aoe::StopUnitCommand{fixture.blue}};
    auto red = frame(
        aoe::LockstepFrameKind::turn, slot(1),
        fixture.config.scenario_digest
    );
    red.state_hash = hash;
    red.commands = {aoe::StopUnitCommand{fixture.red}};
    auto green = frame(
        aoe::LockstepFrameKind::turn, slot(2),
        fixture.config.scenario_digest
    );
    green.state_hash = hash;
    green.commands = {aoe::StopUnitCommand{fixture.green}};
    require(session.receive(green, fixture.simulation));
    require(session.receive(blue, fixture.simulation));
    require(!session.advance(fixture.simulation));
    require(session.current_tick() == 0);
    require(session.receive(red, fixture.simulation));
    if (!session.advance(fixture.simulation)) {
        throw std::runtime_error(
            "advance failed status " +
            std::to_string(static_cast<int>(session.status()))
        );
    }
    require(session.current_tick() == 1);
    require(session.replay().commands().size() == 3);
    require(session.replay().commands()[0].source == slot(0));
    require(session.replay().commands()[1].source == slot(1));
    require(session.replay().commands()[2].source == slot(2));

    auto invalid = frame(
        aoe::LockstepFrameKind::turn, slot(7),
        fixture.config.scenario_digest
    );
    invalid.tick = 1;
    invalid.sequence = 1;
    require(!session.receive(invalid, fixture.simulation));
    require(!session.connected(slot(7)));

    bool malformed{};
    try {
        std::string bytes = aoe::encode_lockstep_frame(hello);
        const auto body = bytes.find(':') + 1;
        const auto source = bytes.find(" 2 ", body);
        bytes.replace(source, 3, " 8 ");
        const std::string payload = bytes.substr(body);
        bytes = std::to_string(payload.size()) + ":" + payload;
        (void)aoe::decode_lockstep_frame(bytes);
    } catch (const std::runtime_error&) {
        malformed = true;
    }
    require(malformed);

    Fixture observer;
    auto observer_state = observer.simulation.player_state(slot(2));
    observer_state.controller = aoe::PlayerControllerState::observer;
    observer.simulation.replace_player_state(slot(2), observer_state);
    aoe::LockstepSession observer_session(observer.config, 10, 1);
    handshake(observer_session, observer);
    const std::string observer_hash =
        aoe::deterministic_state_hash(observer.simulation);
    auto observer_blue = frame(
        aoe::LockstepFrameKind::turn, slot(0),
        observer.config.scenario_digest
    );
    observer_blue.state_hash = observer_hash;
    auto observer_red = frame(
        aoe::LockstepFrameKind::turn, slot(1),
        observer.config.scenario_digest
    );
    observer_red.state_hash = observer_hash;
    auto observer_green = frame(
        aoe::LockstepFrameKind::turn, slot(2),
        observer.config.scenario_digest
    );
    observer_green.state_hash = observer_hash;
    observer_green.commands = {
        aoe::StopUnitCommand{observer.green},
    };
    require(observer_session.receive(
        observer_blue, observer.simulation
    ));
    require(observer_session.receive(
        observer_red, observer.simulation
    ));
    require(observer_session.receive(
        observer_green, observer.simulation
    ));
    require(!observer_session.advance(observer.simulation));
    require(observer_session.status() ==
        aoe::LockstepStatus::invalid_command);

    aoe::LockstepSaveBarrier barrier;
    require(barrier.begin(
        fixture.simulation.tick_number(),
        fixture.simulation.tick_number(),
        *fixture.config.native_roster
    ));
    const std::string checkpoint_hash =
        aoe::deterministic_state_hash(fixture.simulation);
    require(barrier.submit(
        slot(2),
        {fixture.simulation.tick_number(), checkpoint_hash, 32}
    ));
    require(barrier.submit(
        slot(0),
        {fixture.simulation.tick_number(), checkpoint_hash, 30}
    ));
    require(barrier.status() ==
        aoe::SaveBarrierStatus::collecting);
    require(barrier.submit(
        slot(1),
        {fixture.simulation.tick_number(), checkpoint_hash, 31}
    ));
    require(barrier.status() == aoe::SaveBarrierStatus::matched);
    const auto save_path = std::filesystem::temp_directory_path() /
        "aoe-native-lockstep-checkpoint.save";
    const auto envelope_path = std::filesystem::temp_directory_path() /
        "aoe-native-lockstep-checkpoint.envelope";
    aoe::write_multiplayer_checkpoint_atomic(
        fixture.simulation, fixture.config, barrier,
        save_path, envelope_path
    );
    const aoe::ResumedMultiplayerCheckpoint checkpoint =
        aoe::load_multiplayer_checkpoint(
            save_path, envelope_path, fixture.config
        );
    require(checkpoint.envelope.active_bundle_slots[2]);
    require(checkpoint.envelope.last_bundle_sequences[2] == 32);
    require(checkpoint.envelope.config.native_roster.has_value());
    require(checkpoint.envelope.config.native_roster->slot(
        slot(2)
    ).occupied);
    std::filesystem::remove(save_path);
    std::filesystem::remove(envelope_path);

    std::cout << "multiplayer roster tests passed\n";
}
