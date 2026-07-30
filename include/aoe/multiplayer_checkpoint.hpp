#pragma once

#include <cstdint>
#include <array>
#include <filesystem>
#include <optional>
#include <string>

#include "aoe/multiplayer.hpp"

namespace aoe {

enum class SaveBarrierStatus {
    idle,
    collecting,
    matched,
    hash_mismatch,
};

struct SaveBarrierSubmission {
    std::uint64_t tick{};
    std::string state_hash;
    std::uint64_t last_bundle_sequence{};
};

class LockstepSaveBarrier {
public:
    bool begin(std::uint64_t target_tick, std::uint64_t committed_tick);
    bool begin(
        std::uint64_t target_tick,
        std::uint64_t committed_tick,
        const MatchRoster& roster
    );
    bool submit(Player player, SaveBarrierSubmission submission);
    bool submit(PlayerSlotId player, SaveBarrierSubmission submission);

    [[nodiscard]] SaveBarrierStatus status() const { return status_; }
    [[nodiscard]] std::uint64_t target_tick() const { return target_tick_; }
    [[nodiscard]] bool should_pause(std::uint64_t committed_tick) const;
    [[nodiscard]] const SaveBarrierSubmission& blue() const;
    [[nodiscard]] const SaveBarrierSubmission& red() const;
    [[nodiscard]] const SaveBarrierSubmission& submission(
        PlayerSlotId player
    ) const;
    [[nodiscard]] const std::array<bool, 8>& required() const {
        return required_;
    }

private:
    std::uint64_t target_tick_{};
    SaveBarrierStatus status_{SaveBarrierStatus::idle};
    std::array<bool, 8> required_{
        true, true, false, false, false, false, false, false,
    };
    std::array<std::optional<SaveBarrierSubmission>, 8> submissions_;
};

struct MultiplayerSaveEnvelope {
    LockstepSessionConfig config;
    std::string config_digest;
    std::string save_digest;
    std::uint64_t barrier_tick{};
    std::string state_hash;
    std::uint64_t blue_last_bundle_sequence{};
    std::uint64_t red_last_bundle_sequence{};
    std::array<bool, 8> active_bundle_slots{};
    std::array<std::uint64_t, 8> last_bundle_sequences{};
    bool paused{};
    GameSpeed game_speed{GameSpeed::normal};
};

struct ResumedMultiplayerCheckpoint {
    Simulation simulation;
    MultiplayerSaveEnvelope envelope;
};

std::string multiplayer_save_file_digest(
    const std::filesystem::path& save_path
);
void write_multiplayer_checkpoint_atomic(
    const Simulation& simulation,
    const LockstepSessionConfig& config,
    const LockstepSaveBarrier& barrier,
    const std::filesystem::path& save_path,
    const std::filesystem::path& envelope_path,
    bool paused = false,
    GameSpeed game_speed = GameSpeed::normal
);
ResumedMultiplayerCheckpoint load_multiplayer_checkpoint(
    const std::filesystem::path& save_path,
    const std::filesystem::path& envelope_path,
    const LockstepSessionConfig& expected_config
);

}  // namespace aoe
