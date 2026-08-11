#pragma once

#include <memory>

#include "aoe/multiplayer_runtime.hpp"

namespace aoe {

class NostrMultiplayerRuntime final : public MultiplayerRuntime {
public:
    explicit NostrMultiplayerRuntime(MultiplayerLaunchConfig launch);
    ~NostrMultiplayerRuntime() override;
    NostrMultiplayerRuntime(NostrMultiplayerRuntime&&) noexcept;
    NostrMultiplayerRuntime& operator=(NostrMultiplayerRuntime&&) noexcept;
    NostrMultiplayerRuntime(const NostrMultiplayerRuntime&) = delete;
    NostrMultiplayerRuntime& operator=(const NostrMultiplayerRuntime&) = delete;

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
    void set_ready(bool ready = true) override;
    void request_start() override;

    [[nodiscard]] LockstepStatus status() const override;
    [[nodiscard]] bool connected() const override;
    [[nodiscard]] std::uint16_t port() const override;
    [[nodiscard]] std::uint64_t current_tick() const override;
    [[nodiscard]] bool waiting_for_turn() const override;
    [[nodiscard]] PlayerControllerState local_controller_state()
        const override;
    [[nodiscard]] bool peer_ready(Player player) const override;
    [[nodiscard]] const LockstepSessionConfig& session_config()
        const override;
    [[nodiscard]] const std::vector<LockstepChatMessage>& chat_log()
        const override;
    [[nodiscard]] const std::vector<LockstepMapSignal>& signal_log()
        const override;
    [[nodiscard]] const LockstepSaveBarrier* save_barrier() const override;
    [[nodiscard]] NetworkTimingMetrics network_metrics() const override;
    [[nodiscard]] bool paused() const override;
    [[nodiscard]] GameSpeed game_speed() const override;
    [[nodiscard]] int effective_tick_cadence_ms() const override;
    [[nodiscard]] MultiplayerReliabilityStatus reliability_status()
        const override;
    [[nodiscard]] MultiplayerReliabilityReason reliability_reason()
        const override;
    [[nodiscard]] std::string transport_name() const override;
    [[nodiscard]] std::string transport_detail() const override;
    [[nodiscard]] std::string public_match_reference() const override;
    [[nodiscard]] std::string local_identity() const override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace aoe
