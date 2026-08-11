#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "aoe/multiplayer.hpp"
#include "aoe/multiplayer_checkpoint.hpp"

namespace aoe {

enum class LatencyBand { unknown, green, yellow, red };

enum class MultiplayerReliabilityStatus {
    active,
    waiting,
    suspended,
    dropped,
    disconnected,
};

enum class MultiplayerReliabilityReason {
    none,
    peer_silent,
    transport_lost,
    host_dropped_peer,
    peer_disconnected,
    relay_quorum_lost,
    protocol_conflict,
    backfill_incomplete,
};

struct NetworkTimingMetrics {
    std::optional<std::uint64_t> round_trip_ms;
    std::uint64_t milliseconds_since_peer_traffic{};
    bool waiting{};
    LatencyBand latency_band{LatencyBand::unknown};
};

LatencyBand latency_band_for_rtt(std::uint64_t milliseconds);

struct MultiplayerLaunchConfig {
    bool hosting{};
    std::uint16_t port{48192};
    LockstepSessionConfig session;
    std::string match_reference;
    std::vector<std::string> relays;
    bool one_relay_development{};
};

// Transport-neutral production surface consumed by SDL. Native builds create
// LocalhostMultiplayerRuntime; Emscripten builds create NostrMultiplayerRuntime.
class MultiplayerRuntime {
public:
    virtual ~MultiplayerRuntime() = default;

    virtual bool queue_command(GameCommand command) = 0;
    virtual void poll_transport(Simulation& simulation) = 0;
    virtual void pump(Simulation& simulation) = 0;
    virtual void disconnect() = 0;
    virtual bool send_chat(std::string text, ChatAudience audience) = 0;
    virtual bool send_signal(TilePosition tile, ChatAudience audience) = 0;
    virtual bool request_save_barrier(std::uint64_t target_tick) = 0;
    virtual bool propose_pause(bool paused, std::uint64_t barrier_tick) = 0;
    virtual bool propose_speed(GameSpeed speed, std::uint64_t barrier_tick) = 0;
    virtual bool drop_peer() = 0;
    virtual void set_ready(bool ready = true) = 0;
    virtual void request_start() = 0;

    [[nodiscard]] virtual LockstepStatus status() const = 0;
    [[nodiscard]] virtual bool connected() const = 0;
    [[nodiscard]] virtual std::uint16_t port() const = 0;
    [[nodiscard]] virtual std::uint64_t current_tick() const = 0;
    [[nodiscard]] virtual bool waiting_for_turn() const = 0;
    [[nodiscard]] virtual PlayerControllerState local_controller_state()
        const = 0;
    [[nodiscard]] virtual bool peer_ready(Player player) const = 0;
    [[nodiscard]] virtual const LockstepSessionConfig& session_config()
        const = 0;
    [[nodiscard]] virtual const std::vector<LockstepChatMessage>& chat_log()
        const = 0;
    [[nodiscard]] virtual const std::vector<LockstepMapSignal>& signal_log()
        const = 0;
    [[nodiscard]] virtual const LockstepSaveBarrier* save_barrier() const = 0;
    [[nodiscard]] virtual NetworkTimingMetrics network_metrics() const = 0;
    [[nodiscard]] virtual bool paused() const = 0;
    [[nodiscard]] virtual GameSpeed game_speed() const = 0;
    [[nodiscard]] virtual int effective_tick_cadence_ms() const = 0;
    [[nodiscard]] virtual MultiplayerReliabilityStatus reliability_status()
        const = 0;
    [[nodiscard]] virtual MultiplayerReliabilityReason reliability_reason()
        const = 0;
    [[nodiscard]] virtual std::string transport_name() const = 0;
    [[nodiscard]] virtual std::string transport_detail() const = 0;
    [[nodiscard]] virtual std::string public_match_reference() const = 0;
    [[nodiscard]] virtual std::string local_identity() const = 0;
};

// Common frame construction and LockstepSession acceptance seam. Transport
// adapters may only advance authoritative input through these methods.
class LockstepRuntimeCoordinator {
public:
    static LockstepFrame hello(
        const LockstepSession& session,
        Player local_slot
    );
    static LockstepFrame control(
        const LockstepSession& session,
        Player local_slot,
        LockstepFrameKind kind
    );
    static LockstepFrame turn(
        const LockstepSession& session,
        const Simulation& simulation,
        Player local_slot,
        std::uint64_t execution_tick,
        std::vector<GameCommand> commands
    );
    static bool receive(
        LockstepSession& session,
        const LockstepFrame& frame,
        const Simulation& simulation
    );
    static bool advance(
        LockstepSession& session,
        Simulation& simulation
    );
};

std::unique_ptr<MultiplayerRuntime> create_multiplayer_runtime(
    MultiplayerLaunchConfig launch
);

}  // namespace aoe
