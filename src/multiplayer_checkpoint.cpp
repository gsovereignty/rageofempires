#include "aoe/multiplayer_checkpoint.hpp"

#include <atomic>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <ranges>

#include "aoe/format_versions.hpp"
#include "aoe/save_game.hpp"

#if !defined(_WIN32)
#include <fcntl.h>
#include <unistd.h>
#endif

namespace aoe {
namespace {

std::filesystem::path temporary_peer(
    const std::filesystem::path& path
) {
    static std::atomic<std::uint64_t> sequence{};
    return path.parent_path() /
        (path.filename().string() + ".tmp." +
         std::to_string(sequence.fetch_add(1)));
}

void replace_file(
    const std::filesystem::path& temporary,
    const std::filesystem::path& target
) {
    std::error_code error;
    std::filesystem::rename(temporary, target, error);
    if (!error) return;
    std::filesystem::remove(target, error);
    error.clear();
    std::filesystem::rename(temporary, target, error);
    if (error) {
        std::filesystem::remove(temporary);
        throw std::runtime_error("cannot atomically replace checkpoint");
    }
}

void sync_path(const std::filesystem::path& path, bool directory = false) {
#if !defined(_WIN32)
    const int descriptor = ::open(
        path.c_str(), directory ? O_RDONLY : O_RDONLY
    );
    if (descriptor < 0 || ::fsync(descriptor) != 0) {
        if (descriptor >= 0) ::close(descriptor);
        throw std::runtime_error("cannot durably flush checkpoint");
    }
    ::close(descriptor);
#else
    (void)path;
    (void)directory;
#endif
}

MultiplayerSaveEnvelope read_envelope(
    const std::filesystem::path& path
) {
    std::ifstream input(path);
    std::string magic;
    int version{};
    std::string canonical;
    MultiplayerSaveEnvelope envelope;
    std::string speed;
    input >> magic >> version >> std::quoted(canonical) >>
        std::quoted(envelope.config_digest) >>
        std::quoted(envelope.save_digest) >>
        envelope.barrier_tick >> std::quoted(envelope.state_hash);
    if (!input || magic != "aoe-multiplayer-checkpoint" ||
        (version != 2 && version != 3)) {
        throw std::runtime_error("invalid multiplayer checkpoint envelope");
    }
    if (version == 2) {
        input >> envelope.blue_last_bundle_sequence >>
            envelope.red_last_bundle_sequence;
        envelope.active_bundle_slots[0] = true;
        envelope.active_bundle_slots[1] = true;
        envelope.last_bundle_sequences[0] =
            envelope.blue_last_bundle_sequence;
        envelope.last_bundle_sequences[1] =
            envelope.red_last_bundle_sequence;
    } else {
        std::size_t count{};
        input >> count;
        if (count < 2 || count > 8) {
            throw std::runtime_error(
                "invalid multiplayer checkpoint envelope"
            );
        }
        for (std::size_t index = 0; index < count; ++index) {
            int stable_id{};
            std::uint64_t sequence{};
            input >> stable_id >> sequence;
            const auto slot = decode_player_slot_id(stable_id);
            if (!slot || slot->is_neutral() ||
                envelope.active_bundle_slots[*slot->index()]) {
                throw std::runtime_error(
                    "invalid multiplayer checkpoint envelope"
                );
            }
            envelope.active_bundle_slots[*slot->index()] = true;
            envelope.last_bundle_sequences[*slot->index()] = sequence;
        }
        envelope.blue_last_bundle_sequence =
            envelope.last_bundle_sequences[0];
        envelope.red_last_bundle_sequence =
            envelope.last_bundle_sequences[1];
    }
    input >> envelope.paused >> speed;
    if (speed == "slow") {
        envelope.game_speed = GameSpeed::slow;
    } else if (speed == "normal") {
        envelope.game_speed = GameSpeed::normal;
    } else if (speed == "fast") {
        envelope.game_speed = GameSpeed::fast;
    } else {
        throw std::runtime_error("invalid checkpoint speed");
    }
    input >> std::ws;
    if (input.peek() != std::char_traits<char>::eof()) {
        throw std::runtime_error("trailing multiplayer checkpoint data");
    }
    // Reuse the strict frame codec as the sole canonical config decoder.
    LockstepFrame frame;
    frame.kind = LockstepFrameKind::hello;
    frame.player = Player::blue;
    frame.scenario_digest = "checkpoint-config";
    frame.config_digest = envelope.config_digest;
    const std::string marker = "aoe-lockstep 3 hello ";
    (void)marker;
    // Canonical config is parsed by embedding it in a normal hello frame.
    std::ostringstream payload;
    payload << "aoe-lockstep 3 hello " << lockstep_protocol_version
            << " 0 " << std::quoted(std::string{"checkpoint-config"})
            << ' ' << std::quoted(canonical) << ' '
            << std::quoted(envelope.config_digest)
            << " 0 allies blue \"\""
            << " 0 allies blue 0 0"
            << " 0 0 pause normal 0 0 \"\" 0";
    const std::string body = payload.str();
    frame = decode_lockstep_frame(
        std::to_string(body.size()) + ":" + body
    );
    if (!frame.config) {
        throw std::runtime_error("missing checkpoint config");
    }
    envelope.config = std::move(*frame.config);
    return envelope;
}

}  // namespace

bool LockstepSaveBarrier::begin(
    std::uint64_t target_tick,
    std::uint64_t committed_tick
) {
    if (status_ != SaveBarrierStatus::idle ||
        target_tick < committed_tick) {
        return false;
    }
    target_tick_ = target_tick;
    submissions_ = {};
    status_ = SaveBarrierStatus::collecting;
    return true;
}

bool LockstepSaveBarrier::begin(
    std::uint64_t target_tick,
    std::uint64_t committed_tick,
    const MatchRoster& roster
) {
    if (!begin(target_tick, committed_tick)) return false;
    required_.fill(false);
    std::size_t count{};
    for (std::size_t index = 0; index < 8; ++index) {
        const PlayerSlotId slot = *PlayerSlotId::from_index(index);
        required_[index] = roster.slot(slot).occupied;
        if (required_[index]) ++count;
    }
    if (count < 2) {
        status_ = SaveBarrierStatus::idle;
        return false;
    }
    return true;
}

bool LockstepSaveBarrier::submit(
    Player player,
    SaveBarrierSubmission submission
) {
    const auto slot = player_slot_from_legacy(player);
    return slot && !slot->is_neutral() &&
        submit(*slot, std::move(submission));
}

bool LockstepSaveBarrier::submit(
    PlayerSlotId player,
    SaveBarrierSubmission submission
) {
    const auto player_index = player.index();
    if (status_ != SaveBarrierStatus::collecting ||
        submission.tick != target_tick_ ||
        submission.state_hash.empty() ||
        !player_index || !required_[*player_index]) {
        return false;
    }
    auto& destination = submissions_[*player_index];
    if (destination) {
        return destination->tick == submission.tick &&
            destination->state_hash == submission.state_hash &&
            destination->last_bundle_sequence ==
                submission.last_bundle_sequence;
    }
    destination = std::move(submission);
    if (std::ranges::all_of(
            std::views::iota(std::size_t{0}, std::size_t{8}),
            [&](std::size_t index) {
                return !required_[index] ||
                    submissions_[index].has_value();
            }
        )) {
        const auto first = std::ranges::find(required_, true);
        const std::string& expected =
            submissions_[std::distance(required_.begin(), first)]
                ->state_hash;
        status_ = std::ranges::all_of(
            std::views::iota(std::size_t{0}, std::size_t{8}),
            [&](std::size_t index) {
                return !required_[index] ||
                    submissions_[index]->state_hash == expected;
            }
        ) ? SaveBarrierStatus::matched
          : SaveBarrierStatus::hash_mismatch;
    }
    return true;
}

bool LockstepSaveBarrier::should_pause(
    std::uint64_t committed_tick
) const {
    return status_ != SaveBarrierStatus::idle &&
        committed_tick >= target_tick_;
}

const SaveBarrierSubmission& LockstepSaveBarrier::blue() const {
    return submission(*PlayerSlotId::from_index(0));
}

const SaveBarrierSubmission& LockstepSaveBarrier::red() const {
    return submission(*PlayerSlotId::from_index(1));
}

const SaveBarrierSubmission& LockstepSaveBarrier::submission(
    PlayerSlotId player
) const {
    const auto index = player.index();
    if (!index || !submissions_[*index]) {
        throw std::logic_error("missing barrier submission");
    }
    return *submissions_[*index];
}

std::string multiplayer_save_file_digest(
    const std::filesystem::path& save_path
) {
    std::ifstream input(save_path, std::ios::binary);
    if (!input) throw std::runtime_error("cannot read checkpoint save");
    std::uint64_t hash = 14695981039346656037ULL;
    char byte{};
    while (input.get(byte)) {
        hash ^= static_cast<unsigned char>(byte);
        hash *= 1099511628211ULL;
    }
    std::ostringstream output;
    output << "save" << reconstruction_save_version
           << "-fnv1a64:" << std::hex << std::setfill('0')
           << std::setw(16) << hash;
    return output.str();
}

void write_multiplayer_checkpoint_atomic(
    const Simulation& simulation,
    const LockstepSessionConfig& config,
    const LockstepSaveBarrier& barrier,
    const std::filesystem::path& save_path,
    const std::filesystem::path& envelope_path,
    bool paused,
    GameSpeed game_speed
) {
    const auto first = std::ranges::find(barrier.required(), true);
    if (barrier.status() != SaveBarrierStatus::matched ||
        first == barrier.required().end() ||
        barrier.submission(*PlayerSlotId::from_index(
            std::distance(barrier.required().begin(), first)
        )).state_hash != deterministic_state_hash(simulation)) {
        throw std::invalid_argument("unmatched checkpoint barrier");
    }
    for (std::size_t index = 0; index < 8; ++index) {
        const PlayerSlotId slot = *PlayerSlotId::from_index(index);
        const bool configured = config.native_roster
            ? config.native_roster->slot(slot).occupied
            : index < 2;
        if (barrier.required()[index] != configured) {
            throw std::invalid_argument(
                "checkpoint barrier roster mismatch"
            );
        }
    }
    const auto temporary_save = temporary_peer(save_path);
    const auto temporary_envelope = temporary_peer(envelope_path);
    try {
        save_game(simulation, temporary_save);
        sync_path(temporary_save);
        const std::string save_digest =
            multiplayer_save_file_digest(temporary_save);
        std::ofstream output(temporary_envelope);
        if (!output) throw std::runtime_error("cannot write checkpoint");
        output << "aoe-multiplayer-checkpoint 3 "
               << std::quoted(canonical_lockstep_config(config)) << ' '
               << std::quoted(lockstep_config_digest(config)) << ' '
               << std::quoted(save_digest) << ' '
               << barrier.target_tick() << ' '
               << std::quoted(barrier.submission(
                      *PlayerSlotId::from_index(
                          std::distance(
                              barrier.required().begin(), first
                          )
                      )
                  ).state_hash);
        const std::size_t participant_count = std::ranges::count(
            barrier.required(), true
        );
        output << ' ' << participant_count;
        for (std::size_t index = 0; index < 8; ++index) {
            if (!barrier.required()[index]) continue;
            output << ' ' << index << ' '
                   << barrier.submission(
                          *PlayerSlotId::from_index(index)
                      ).last_bundle_sequence;
        }
        output << ' ' << paused << ' '
               << (game_speed == GameSpeed::slow
                       ? "slow"
                       : game_speed == GameSpeed::fast
                           ? "fast" : "normal") << '\n';
        output.close();
        if (!output) throw std::runtime_error("cannot flush checkpoint");
        sync_path(temporary_envelope);
        replace_file(temporary_save, save_path);
        // Envelope is the commit marker and is replaced last.
        replace_file(temporary_envelope, envelope_path);
        sync_path(envelope_path.parent_path().empty()
            ? std::filesystem::path{"."}
            : envelope_path.parent_path(), true);
    } catch (...) {
        std::filesystem::remove(temporary_save);
        std::filesystem::remove(temporary_envelope);
        throw;
    }
}

ResumedMultiplayerCheckpoint load_multiplayer_checkpoint(
    const std::filesystem::path& save_path,
    const std::filesystem::path& envelope_path,
    const LockstepSessionConfig& expected_config
) {
    MultiplayerSaveEnvelope envelope = read_envelope(envelope_path);
    for (std::size_t index = 0; index < 8; ++index) {
        const PlayerSlotId slot = *PlayerSlotId::from_index(index);
        const bool expected = expected_config.native_roster
            ? expected_config.native_roster->slot(slot).occupied
            : index < 2;
        if (envelope.active_bundle_slots[index] != expected) {
            throw std::runtime_error(
                "multiplayer checkpoint roster mismatch"
            );
        }
    }
    if (canonical_lockstep_config(envelope.config) !=
            canonical_lockstep_config(expected_config) ||
        envelope.config_digest !=
            lockstep_config_digest(expected_config) ||
        envelope.save_digest !=
            multiplayer_save_file_digest(save_path)) {
        throw std::runtime_error("multiplayer checkpoint digest mismatch");
    }
    Simulation simulation = load_game(save_path);
    if (deterministic_state_hash(simulation) != envelope.state_hash) {
        throw std::runtime_error("multiplayer checkpoint state mismatch");
    }
    return {std::move(simulation), std::move(envelope)};
}

}  // namespace aoe
