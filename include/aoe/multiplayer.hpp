#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "aoe/format_versions.hpp"
#include "aoe/game_command.hpp"

namespace aoe {

constexpr int lockstep_protocol_version = 3;

struct LockstepPlayerConfig {
    std::string peer_id;
    Player slot{Player::blue};
    Civilization civilization{Civilization::generic};
    int team{1};
};

struct LockstepSessionConfig {
    std::string build_id{"reconstruction-dev"};
    int command_schema_version{reconstruction_command_schema_version};
    int save_version{reconstruction_save_version};
    int scenario_version{reconstruction_scenario_version};
    std::string scenario_digest;
    std::string content_rules_digest{"reconstruction-rules-v1"};
    int tick_cadence_ms{200};
    int input_delay_ticks{};
    std::uint64_t deterministic_seed{};
    LockstepPlayerConfig blue{"blue-peer", Player::blue};
    LockstepPlayerConfig red{"red-peer", Player::red};
    std::optional<MatchRoster> native_roster;
    std::optional<RosterDiplomacy> native_diplomacy;
    std::array<Civilization, 8> native_civilizations{};
    std::optional<PlayerSlotId> host_slot;
};

std::string canonical_lockstep_config(
    const LockstepSessionConfig& config
);
std::string lockstep_config_digest(
    const LockstepSessionConfig& config
);
std::string lockstep_compatibility_digest(
    LockstepSessionConfig config
);

enum class LockstepFrameKind {
    hello,
    ready,
    start,
    turn,
    disconnect,
    chat,
    signal,
    save_barrier,
    save_hash,
    heartbeat_ping,
    heartbeat_pong,
    control_proposal,
    control_ack,
    control_commit,
    peer_drop,
};

enum class GameSpeed { slow, normal, fast };
enum class SessionControlKind { pause, resume, speed };

struct SessionControlMessage {
    std::uint64_t proposal_id{};
    std::uint64_t barrier_tick{};
    SessionControlKind kind{SessionControlKind::pause};
    GameSpeed speed{GameSpeed::normal};
};

enum class ChatAudience { all, allies };

struct LockstepChatMessage {
    std::uint64_t sequence{};
    Player sender{Player::blue};
    ChatAudience audience{ChatAudience::all};
    std::string text;
};

struct LockstepMapSignal {
    std::uint64_t sequence{};
    Player sender{Player::blue};
    ChatAudience audience{ChatAudience::allies};
    TilePosition tile{};
};

struct LockstepFrame {
    LockstepFrameKind kind{LockstepFrameKind::hello};
    int protocol_version{lockstep_protocol_version};
    Player player{Player::blue};
    std::string scenario_digest;
    std::uint64_t tick{};
    std::uint64_t sequence{};
    std::string state_hash;
    std::vector<GameCommand> commands;
    std::optional<LockstepSessionConfig> config;
    std::string config_digest;
    std::optional<LockstepChatMessage> chat;
    std::optional<LockstepMapSignal> signal;
    std::optional<SessionControlMessage> control;
    std::optional<PlayerSlotId> source;
};

enum class LockstepStatus {
    handshaking,
    ready,
    running,
    desync,
    timed_out,
    disconnected,
    protocol_mismatch,
    build_mismatch,
    schema_mismatch,
    scenario_mismatch,
    content_mismatch,
    settings_mismatch,
    roster_mismatch,
    invalid_command,
};

std::string encode_lockstep_frame(const LockstepFrame& frame);
LockstepFrame decode_lockstep_frame(const std::string& bytes);
std::string deterministic_state_hash(const Simulation& simulation);

class LockstepSession {
public:
    explicit LockstepSession(
        std::string scenario_digest,
        std::uint64_t timeout_steps = 50,
        std::uint64_t hash_interval = 50
    );
    explicit LockstepSession(
        LockstepSessionConfig config,
        std::uint64_t timeout_steps = 50,
        std::uint64_t hash_interval = 50
    );

    bool receive(const LockstepFrame& frame, const Simulation& simulation);
    bool advance(Simulation& simulation);
    void elapse();
    void disconnect(Player player);
    void disconnect(PlayerSlotId player);

    [[nodiscard]] LockstepStatus status() const { return status_; }
    [[nodiscard]] std::uint64_t current_tick() const { return current_tick_; }
    [[nodiscard]] std::uint64_t hash_interval() const {
        return hash_interval_;
    }
    [[nodiscard]] const Replay& replay() const { return replay_; }
    [[nodiscard]] const LockstepSessionConfig& config() const {
        return config_;
    }
    [[nodiscard]] bool connected(Player player) const;
    [[nodiscard]] bool connected(PlayerSlotId player) const;
    [[nodiscard]] bool ready(Player player) const {
        return ready(*player_slot_from_legacy(player));
    }
    [[nodiscard]] bool ready(PlayerSlotId player) const {
        return peer(player).ready;
    }

private:
    struct Peer {
        bool hello{};
        bool ready{};
        bool connected{};
    };
    struct TurnBundle {
        std::array<std::optional<LockstepFrame>, 8> slots;
    };

    bool validate_command_owner(
        const Simulation& simulation,
        PlayerSlotId player,
        const GameCommand& command
    ) const;
    Peer& peer(PlayerSlotId player);
    const Peer& peer(PlayerSlotId player) const;
    [[nodiscard]] std::vector<PlayerSlotId> required_slots() const;
    [[nodiscard]] PlayerSlotId source_of(
        const LockstepFrame& frame
    ) const;

    std::string scenario_digest_;
    LockstepSessionConfig config_;
    bool allow_legacy_config_{};
    std::uint64_t timeout_steps_;
    std::uint64_t hash_interval_;
    std::uint64_t idle_steps_{};
    std::uint64_t current_tick_{};
    LockstepStatus status_{LockstepStatus::handshaking};
    std::array<Peer, 8> peers_{};
    std::map<std::uint64_t, TurnBundle> turns_;
    Replay replay_;
};

}  // namespace aoe
