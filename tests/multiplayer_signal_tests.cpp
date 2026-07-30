#include "aoe/multiplayer_transport.hpp"

#include <iostream>

namespace {
int failures{};
void expect(bool value, const char* message) {
    if (!value) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}
}

int main() {
    aoe::LockstepSessionConfig golden_config;
    golden_config.scenario_digest = "player-codec-golden";
    const std::string canonical =
        aoe::canonical_lockstep_config(golden_config);
    expect(
        canonical.find("\"blue-peer\" 0 0 1 \"red-peer\" 1 0 1") !=
            std::string::npos,
        "lockstep config preserves blue/red numeric wire IDs"
    );

    aoe::LockstepFrame codec;
    codec.kind = aoe::LockstepFrameKind::signal;
    codec.player = aoe::Player::blue;
    codec.scenario_digest = "signal-codec";
    codec.signal = aoe::LockstepMapSignal{
        7, aoe::Player::blue, aoe::ChatAudience::allies, {11, 9}
    };
    const std::string encoded = aoe::encode_lockstep_frame(codec);
    expect(
        encoded.find(" signal 3 0 ") != std::string::npos &&
            encoded.find(" 7 allies blue ") != std::string::npos,
        "lockstep frame preserves stable slot wire ID"
    );
    const aoe::LockstepFrame decoded =
        aoe::decode_lockstep_frame(encoded);
    expect(decoded.signal && decoded.signal->sequence == 7 &&
               decoded.signal->tile == aoe::TilePosition{11, 9},
           "signal codec");
    bool rejected_bad_tile{};
    try {
        codec.signal->tile = {-1, 9};
        (void)aoe::decode_lockstep_frame(
            aoe::encode_lockstep_frame(codec)
        );
    } catch (const std::exception&) {
        rejected_bad_tile = true;
    }
    expect(rejected_bad_tile, "codec rejects negative tile");

    aoe::Simulation simulation = aoe::Simulation::create_demo();
    aoe::LockstepSessionConfig config;
    config.scenario_digest = "signal-routing";
    config.blue.team = 1;
    config.red.team = 1;
    aoe::TcpFrameListener listener;
    aoe::LocalhostLockstepDriver join(
        aoe::connect_localhost(listener.port()),
        config, aoe::Player::red, false
    );
    aoe::LocalhostLockstepDriver host(
        listener.accept(), config, aoe::Player::blue, true
    );
    expect(host.send_hello(simulation) && join.send_hello(simulation),
           "hello send");
    expect(host.pump_one(simulation) && join.pump_one(simulation),
           "hello receive");
    expect(host.send_ready(simulation) && join.send_ready(simulation),
           "ready send");
    expect(host.pump_one(simulation) && join.pump_one(simulation),
           "ready receive");
    expect(host.send_start(simulation) && join.pump_one(simulation),
           "start");
    const std::string hash_before =
        aoe::deterministic_state_hash(simulation);
    expect(join.send_signal({4, 5}, aoe::ChatAudience::allies),
           "join signal send");
    expect(host.pump_one(simulation) && join.pump_one(simulation),
           "join signal route");
    expect(host.send_signal({6, 7}, aoe::ChatAudience::allies),
           "host signal send");
    expect(join.pump_one(simulation), "host signal route");
    expect(host.signal_log().size() == 2 &&
               join.signal_log().size() == 2 &&
               host.signal_log()[0].sequence == 1 &&
               host.signal_log()[1].sequence == 2 &&
               join.signal_log()[0].tile == aoe::TilePosition{4, 5} &&
               join.signal_log()[1].tile == aoe::TilePosition{6, 7},
           "ordered allied routing");
    expect(!host.send_signal({-1, 5}, aoe::ChatAudience::allies),
           "negative tile rejected");
    expect(host.send_signal({8, 7}, aoe::ChatAudience::allies) &&
               host.send_signal({9, 7}, aoe::ChatAudience::allies) &&
               host.send_signal({10, 7}, aoe::ChatAudience::allies) &&
               !host.send_signal({11, 7}, aoe::ChatAudience::allies),
           "rolling signal rate bound");
    expect(aoe::deterministic_state_hash(simulation) == hash_before &&
               host.session().replay().commands().empty(),
           "signal excluded from hash and replay");

    aoe::LockstepSessionConfig opposing_config;
    opposing_config.scenario_digest = "signal-opponents";
    opposing_config.blue.team = 1;
    opposing_config.red.team = 2;
    aoe::TcpFrameListener opposing_listener;
    aoe::LocalhostLockstepDriver opposing_join(
        aoe::connect_localhost(opposing_listener.port()),
        opposing_config, aoe::Player::red, false
    );
    aoe::LocalhostLockstepDriver opposing_host(
        opposing_listener.accept(),
        opposing_config, aoe::Player::blue, true
    );
    expect(opposing_host.send_hello(simulation) &&
               opposing_join.send_hello(simulation) &&
               opposing_host.pump_one(simulation) &&
               opposing_join.pump_one(simulation),
           "opposing peers hello");
    expect(
        !opposing_host.send_signal(
            {3, 3}, aoe::ChatAudience::allies
        ) &&
        !opposing_join.send_signal(
            {3, 3}, aoe::ChatAudience::allies
        ),
        "allies-only signal rejected for opponents"
    );
    if (failures == 0) std::cout << "multiplayer signal tests passed\n";
    return failures == 0 ? 0 : 1;
}
