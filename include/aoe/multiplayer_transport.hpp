#pragma once

#include <cstddef>
#include <chrono>
#include <map>
#include <cstdint>
#include <optional>
#include <set>
#include <string>

#include "aoe/multiplayer.hpp"
#include "aoe/multiplayer_checkpoint.hpp"
#include "aoe/multiplayer_runtime.hpp"

namespace aoe {

enum class TcpPollStatus {
    no_data,
    frame,
    disconnected,
};

enum class TcpConnectStatus {
    connecting,
    connected,
    failed,
};

struct TcpFramePoll {
    TcpPollStatus status{TcpPollStatus::no_data};
    std::optional<LockstepFrame> frame;
};

class TcpFrameStream {
public:
    TcpFrameStream() = default;
    ~TcpFrameStream();
    TcpFrameStream(TcpFrameStream&& other) noexcept;
    TcpFrameStream& operator=(TcpFrameStream&& other) noexcept;
    TcpFrameStream(const TcpFrameStream&) = delete;
    TcpFrameStream& operator=(const TcpFrameStream&) = delete;

    void send_frame(const LockstepFrame& frame);
    void send_frame_fragmented(
        const LockstepFrame& frame,
        std::size_t maximum_fragment
    );
    std::optional<LockstepFrame> receive_frame();
    void set_nonblocking();
    void queue_frame(const LockstepFrame& frame);
    bool flush_queued();
    TcpFramePoll poll_frame();
    TcpConnectStatus connect_status() const;
    void close();
    [[nodiscard]] bool open() const { return descriptor_ >= 0; }

private:
    explicit TcpFrameStream(int descriptor) : descriptor_(descriptor) {}
    void send_bytes(const std::string& bytes, std::size_t fragment);

    int descriptor_{-1};
    bool nonblocking_{};
    std::string receive_buffer_;
    std::string send_buffer_;

    friend class TcpFrameListener;
    friend TcpFrameStream connect_localhost(std::uint16_t port);
    friend std::optional<TcpFrameStream> try_connect_localhost(
        std::uint16_t port
    );
    friend TcpFrameStream begin_connect_localhost(std::uint16_t port);
};

class TcpFrameListener {
public:
    explicit TcpFrameListener(std::uint16_t port = 0);
    ~TcpFrameListener();
    TcpFrameListener(TcpFrameListener&& other) noexcept;
    TcpFrameListener& operator=(TcpFrameListener&& other) noexcept;
    TcpFrameListener(const TcpFrameListener&) = delete;
    TcpFrameListener& operator=(const TcpFrameListener&) = delete;

    [[nodiscard]] std::uint16_t port() const { return port_; }
    TcpFrameStream accept();
    void set_nonblocking();
    std::optional<TcpFrameStream> try_accept();

private:
    int descriptor_{-1};
    std::uint16_t port_{};
    bool nonblocking_{};
};

TcpFrameStream connect_localhost(std::uint16_t port);
std::optional<TcpFrameStream> try_connect_localhost(std::uint16_t port);
TcpFrameStream begin_connect_localhost(std::uint16_t port);

// Configured multi-peer localhost harness. It deliberately provides no
// discovery, authentication, lobby, or Internet transport.
class LocalhostMultiPeerHost {
public:
    LocalhostMultiPeerHost(
        std::uint16_t port,
        LockstepSessionConfig config,
        std::uint64_t timeout_steps = 50,
        std::uint64_t hash_interval = 50
    );

    void accept_peer(PlayerSlotId slot);
    bool send(
        LockstepFrame frame,
        const Simulation& simulation,
        std::size_t maximum_fragment = 0
    );
    TcpFramePoll pump_one(const Simulation& simulation);
    bool advance(Simulation& simulation) {
        return session_.advance(simulation);
    }
    void close_peer(PlayerSlotId slot);

    [[nodiscard]] std::uint16_t port() const { return listener_.port(); }
    [[nodiscard]] const LockstepSession& session() const { return session_; }
    [[nodiscard]] PlayerSlotId host_slot() const { return host_slot_; }
    [[nodiscard]] std::size_t accepted_peer_count() const {
        return streams_.size();
    }
    [[nodiscard]] const std::vector<LockstepMapSignal>& signal_log() const {
        return signal_log_;
    }

private:
    void broadcast(
        const LockstepFrame& frame,
        std::optional<PlayerSlotId> except = std::nullopt,
        std::size_t maximum_fragment = 0
    );

    TcpFrameListener listener_;
    LockstepSessionConfig config_;
    PlayerSlotId host_slot_;
    std::map<std::uint8_t, TcpFrameStream> streams_;
    std::vector<LockstepMapSignal> signal_log_;
    LockstepSession session_;
};

class LocalhostMultiPeerClient {
public:
    LocalhostMultiPeerClient(
        std::uint16_t port,
        LockstepSessionConfig config,
        PlayerSlotId local_slot,
        std::uint64_t timeout_steps = 50,
        std::uint64_t hash_interval = 50
    );

    bool send(
        LockstepFrame frame,
        const Simulation& simulation,
        std::size_t maximum_fragment = 0
    );
    TcpFramePoll pump_one(const Simulation& simulation);
    bool advance(Simulation& simulation) {
        return session_.advance(simulation);
    }
    void close() { stream_.close(); }

    [[nodiscard]] const LockstepSession& session() const { return session_; }
    [[nodiscard]] PlayerSlotId local_slot() const { return local_slot_; }
    [[nodiscard]] const std::vector<LockstepMapSignal>& signal_log() const {
        return signal_log_;
    }

private:
    TcpFrameStream stream_;
    LockstepSessionConfig config_;
    PlayerSlotId local_slot_;
    std::vector<LockstepMapSignal> signal_log_;
    LockstepSession session_;
};

class LocalhostLockstepDriver {
public:
    LocalhostLockstepDriver(
        TcpFrameStream stream,
        std::string scenario_digest,
        Player local_slot,
        bool host,
        std::uint64_t timeout_steps = 50,
        std::uint64_t hash_interval = 50
    );
    LocalhostLockstepDriver(
        TcpFrameStream stream,
        LockstepSessionConfig config,
        Player local_slot,
        bool host,
        std::uint64_t timeout_steps = 50,
        std::uint64_t hash_interval = 50
    );

    bool send_hello(const Simulation& simulation);
    bool send_ready(const Simulation& simulation);
    bool send_start(const Simulation& simulation);
    bool submit_turn(
        const Simulation& simulation,
        std::vector<GameCommand> commands = {}
    );
    bool submit_turn_at(
        const Simulation& simulation,
        std::uint64_t execution_tick,
        std::vector<GameCommand> commands = {}
    );
    bool pump_one(const Simulation& simulation);
    TcpFramePoll pump_nonblocking(const Simulation& simulation);
    bool flush_outbound();
    bool advance(Simulation& simulation);
    bool send_chat(std::string text, ChatAudience audience);
    bool send_signal(TilePosition tile, ChatAudience audience);
    bool request_save_barrier(std::uint64_t target_tick);
    bool submit_save_hash(const Simulation& simulation);
    bool propose_pause(bool paused, std::uint64_t barrier_tick);
    bool propose_speed(GameSpeed speed, std::uint64_t barrier_tick);
    bool drop_peer();
    bool send_disconnect();
    void maintain_heartbeat(
        std::chrono::steady_clock::time_point now =
            std::chrono::steady_clock::now()
    );
    void update_control_barrier() { apply_committed_control(); }
    void update_reliability(
        std::chrono::steady_clock::time_point now =
            std::chrono::steady_clock::now()
    );
    void set_managed_drop_flow(bool enabled) {
        managed_drop_flow_ = enabled;
    }
    void elapse() { session_.elapse(); }
    void close();
    void set_disconnect_grace_polls(std::size_t polls) {
        disconnect_grace_polls_ = polls;
    }

    [[nodiscard]] LockstepStatus status() const {
        return session_.status();
    }
    [[nodiscard]] Player local_slot() const { return local_slot_; }
    [[nodiscard]] const LockstepSession& session() const { return session_; }
    [[nodiscard]] const std::vector<LockstepChatMessage>& chat_log() const {
        return chat_log_;
    }
    [[nodiscard]] const std::vector<LockstepMapSignal>& signal_log() const {
        return signal_log_;
    }
    [[nodiscard]] const LockstepSaveBarrier& save_barrier() const {
        return save_barrier_;
    }
    [[nodiscard]] NetworkTimingMetrics network_metrics(
        std::chrono::steady_clock::time_point now =
            std::chrono::steady_clock::now()
    ) const;
    [[nodiscard]] bool paused() const { return paused_; }
    [[nodiscard]] GameSpeed game_speed() const { return game_speed_; }
    [[nodiscard]] bool control_barrier_waiting() const {
        return pending_control_ &&
            session_.current_tick() >=
                pending_control_->barrier_tick &&
            !control_committed_;
    }
    [[nodiscard]] MultiplayerReliabilityStatus reliability_status() const {
        return reliability_status_;
    }
    [[nodiscard]] MultiplayerReliabilityReason reliability_reason() const {
        return reliability_reason_;
    }

private:
    LockstepFrame control_frame(LockstepFrameKind kind) const;
    bool receive_chat(const LockstepFrame& frame);
    bool receive_signal(const LockstepFrame& frame);
    bool receive_save_control(const LockstepFrame& frame);
    bool receive_heartbeat(
        const LockstepFrame& frame,
        std::chrono::steady_clock::time_point now
    );
    bool receive_control(const LockstepFrame& frame);
    bool receive_drop(const LockstepFrame& frame);
    bool propose_control(SessionControlMessage message);
    void apply_committed_control();

    TcpFrameStream stream_;
    std::string scenario_digest_;
    Player local_slot_;
    Player remote_slot_;
    bool host_{};
    std::size_t disconnect_grace_polls_{};
    std::size_t remote_eof_polls_{};
    std::uint64_t next_chat_sequence_{1};
    std::uint64_t last_chat_sequence_{};
    std::vector<LockstepChatMessage> chat_log_;
    std::uint64_t next_signal_sequence_{1};
    std::uint64_t last_signal_sequence_{};
    std::vector<LockstepMapSignal> signal_log_;
    std::vector<std::chrono::steady_clock::time_point> signal_times_;
    LockstepSaveBarrier save_barrier_;
    bool save_hash_sent_{};
    std::uint64_t next_heartbeat_sequence_{1};
    std::optional<std::uint64_t> round_trip_ms_;
    std::chrono::steady_clock::time_point last_peer_traffic_{
        std::chrono::steady_clock::now()
    };
    std::chrono::steady_clock::time_point last_heartbeat_sent_{};
    std::map<
        std::uint64_t,
        std::chrono::steady_clock::time_point
    > pending_heartbeats_;
    std::uint64_t next_control_proposal_id_{1};
    std::optional<SessionControlMessage> pending_control_;
    bool control_acknowledged_{};
    bool control_committed_{};
    bool paused_{};
    GameSpeed game_speed_{GameSpeed::normal};
    bool managed_drop_flow_{};
    bool transport_lost_{};
    MultiplayerReliabilityStatus reliability_status_{
        MultiplayerReliabilityStatus::active
    };
    MultiplayerReliabilityReason reliability_reason_{
        MultiplayerReliabilityReason::none
    };
    LockstepSession session_;
};

class LocalhostMultiplayerRuntime final : public MultiplayerRuntime {
public:
    static LocalhostMultiplayerRuntime host(
        std::uint16_t port,
        std::string scenario_digest,
        std::uint64_t timeout_steps = 50,
        std::uint64_t hash_interval = 50
    );
    static LocalhostMultiplayerRuntime join(
        std::uint16_t port,
        std::string scenario_digest,
        std::uint64_t timeout_steps = 50,
        std::uint64_t hash_interval = 50
    );
    static LocalhostMultiplayerRuntime host(
        std::uint16_t port,
        LockstepSessionConfig config,
        std::uint64_t timeout_steps = 50,
        std::uint64_t hash_interval = 50
    );
    static LocalhostMultiplayerRuntime join(
        std::uint16_t port,
        LockstepSessionConfig config,
        std::uint64_t timeout_steps = 50,
        std::uint64_t hash_interval = 50
    );

    bool queue_command(GameCommand command) override;
    void poll_transport(Simulation& simulation) override;
    void pump(Simulation& simulation) override;
    void disconnect() override;
    bool send_chat(std::string text, ChatAudience audience) override;
    bool send_signal(TilePosition tile, ChatAudience audience) override;
    bool request_save_barrier(std::uint64_t target_tick) override;
    bool propose_pause(bool paused, std::uint64_t barrier_tick) override;
    bool propose_speed(GameSpeed speed, std::uint64_t barrier_tick) override;
    bool drop_peer() override;
    void set_ready(bool ready = true) override { ready_requested_ = ready; }
    void request_start() override { start_requested_ = true; }

    [[nodiscard]] LockstepStatus status() const override;
    [[nodiscard]] bool connected() const override { return driver_.has_value(); }
    [[nodiscard]] std::uint16_t port() const override { return port_; }
    [[nodiscard]] std::uint64_t current_tick() const override;
    [[nodiscard]] bool waiting_for_turn() const override;
    [[nodiscard]] PlayerControllerState local_controller_state() const override {
        return local_controller_state_;
    }
    [[nodiscard]] bool peer_ready(Player player) const override {
        return driver_ && driver_->session().ready(player);
    }
    [[nodiscard]] const LockstepSessionConfig& session_config() const override {
        return config_;
    }
    [[nodiscard]] const std::vector<LockstepChatMessage>& chat_log() const override;
    [[nodiscard]] const std::vector<LockstepMapSignal>& signal_log() const override;
    [[nodiscard]] const LockstepSaveBarrier* save_barrier() const override {
        return driver_ ? &driver_->save_barrier() : nullptr;
    }
    [[nodiscard]] NetworkTimingMetrics network_metrics() const override {
        return driver_ ? driver_->network_metrics()
                       : NetworkTimingMetrics{};
    }
    [[nodiscard]] bool paused() const override {
        return driver_ && driver_->paused();
    }
    [[nodiscard]] GameSpeed game_speed() const override {
        return driver_ ? driver_->game_speed() : GameSpeed::normal;
    }
    [[nodiscard]] int effective_tick_cadence_ms() const override {
        const int base = config_.tick_cadence_ms;
        return game_speed() == GameSpeed::slow
            ? base * 2
            : game_speed() == GameSpeed::fast
                ? (base / 2 > 0 ? base / 2 : 1) : base;
    }
    [[nodiscard]] MultiplayerReliabilityStatus reliability_status() const override {
        return driver_
            ? driver_->reliability_status()
            : MultiplayerReliabilityStatus::active;
    }
    [[nodiscard]] MultiplayerReliabilityReason reliability_reason() const override {
        return driver_
            ? driver_->reliability_reason()
            : MultiplayerReliabilityReason::none;
    }
    [[nodiscard]] std::string transport_name() const override {
        return "LOCALHOST TCP";
    }
    [[nodiscard]] std::string transport_detail() const override {
        return "PORT " + std::to_string(port_);
    }
    [[nodiscard]] std::string public_match_reference() const override {
        return {};
    }
    [[nodiscard]] std::string local_identity() const override {
        return host_ ? config_.blue.peer_id : config_.red.peer_id;
    }

private:
    LocalhostMultiplayerRuntime(
        bool host,
        std::uint16_t port,
        std::string scenario_digest,
        std::uint64_t timeout_steps,
        std::uint64_t hash_interval,
        bool automatic_flow
    );
    LocalhostMultiplayerRuntime(
        bool host,
        std::uint16_t port,
        LockstepSessionConfig config,
        std::uint64_t timeout_steps,
        std::uint64_t hash_interval,
        bool automatic_flow
    );
    void attach(TcpFrameStream stream);
    void pump_impl(Simulation& simulation, bool allow_turn);

    bool host_{};
    std::uint16_t port_{};
    std::string scenario_digest_;
    LockstepSessionConfig config_;
    std::uint64_t timeout_steps_{};
    std::uint64_t hash_interval_{};
    std::optional<TcpFrameListener> listener_;
    std::optional<TcpFrameStream> pending_connection_;
    std::optional<LocalhostLockstepDriver> driver_;
    std::vector<GameCommand> pending_commands_;
    PlayerControllerState local_controller_state_{
        PlayerControllerState::active
    };
    bool local_resignation_pending_{};
    bool hello_sent_{};
    bool ready_sent_{};
    bool start_sent_{};
    bool automatic_flow_{};
    bool ready_requested_{};
    bool start_requested_{};
    std::set<std::uint64_t> submitted_ticks_;
    std::uint64_t next_submission_tick_{};
};

}  // namespace aoe
