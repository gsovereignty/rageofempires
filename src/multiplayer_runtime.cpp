#include "aoe/multiplayer_runtime.hpp"

#include <utility>

#if defined(__EMSCRIPTEN__)
#include "aoe/nostr_multiplayer_runtime.hpp"
#else
#include "aoe/multiplayer_transport.hpp"
#endif

namespace aoe {

LockstepFrame LockstepRuntimeCoordinator::control(
    const LockstepSession& session,
    Player local_slot,
    LockstepFrameKind kind
) {
    LockstepFrame frame;
    frame.kind = kind;
    frame.player = local_slot;
    frame.scenario_digest = session.config().scenario_digest;
    return frame;
}

LockstepFrame LockstepRuntimeCoordinator::hello(
    const LockstepSession& session,
    Player local_slot
) {
    LockstepFrame frame = control(
        session, local_slot, LockstepFrameKind::hello
    );
    frame.config = session.config();
    frame.config_digest = lockstep_config_digest(session.config());
    return frame;
}

LockstepFrame LockstepRuntimeCoordinator::turn(
    const LockstepSession& session,
    const Simulation& simulation,
    Player local_slot,
    std::uint64_t execution_tick,
    std::vector<GameCommand> commands
) {
    LockstepFrame frame = control(
        session, local_slot, LockstepFrameKind::turn
    );
    frame.tick = execution_tick;
    frame.sequence = execution_tick;
    if (execution_tick == session.current_tick() &&
        execution_tick % session.hash_interval() == 0) {
        frame.state_hash = deterministic_state_hash(simulation);
    }
    frame.commands = std::move(commands);
    return frame;
}

bool LockstepRuntimeCoordinator::receive(
    LockstepSession& session,
    const LockstepFrame& frame,
    const Simulation& simulation
) {
    return session.receive(frame, simulation);
}

bool LockstepRuntimeCoordinator::advance(
    LockstepSession& session,
    Simulation& simulation
) {
    return session.advance(simulation);
}

std::unique_ptr<MultiplayerRuntime> create_multiplayer_runtime(
    MultiplayerLaunchConfig launch
) {
#if defined(__EMSCRIPTEN__)
    return std::make_unique<NostrMultiplayerRuntime>(std::move(launch));
#else
    if (launch.hosting) {
        return std::make_unique<LocalhostMultiplayerRuntime>(
            LocalhostMultiplayerRuntime::host(
                launch.port, std::move(launch.session)
            )
        );
    }
    return std::make_unique<LocalhostMultiplayerRuntime>(
        LocalhostMultiplayerRuntime::join(
            launch.port, std::move(launch.session)
        )
    );
#endif
}

}  // namespace aoe
