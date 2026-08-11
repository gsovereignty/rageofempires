#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace aoe {

constexpr int nostr_match_protocol_version = 1;
constexpr int nostr_lobby_event_kind = 30078;
constexpr int nostr_match_event_kind = 78;
constexpr std::size_t nostr_max_future_sender_sequences = 128;

struct NostrLogicalEvent {
    std::uint64_t sequence{};
    std::string previous_event_id;
    std::string event_id;
    std::string content;
};

enum class NostrSequenceAccept {
    buffered,
    duplicate,
    conflict,
    out_of_bounds,
};

struct NostrSequenceDrain {
    std::vector<NostrLogicalEvent> contiguous;
    bool conflict{};
    std::string reason;
};

// Relay delivery order is not authoritative. This stream accepts bounded
// future input, detects conflicting logical identities, and releases only the
// contiguous previous-event chain.
class NostrSenderSequence {
public:
    NostrSequenceAccept accept(NostrLogicalEvent event);
    NostrSequenceDrain drain();

    [[nodiscard]] std::uint64_t highest_contiguous_sequence() const {
        return highest_contiguous_sequence_;
    }
    [[nodiscard]] const std::string& last_event_id() const {
        return last_event_id_;
    }
    [[nodiscard]] std::vector<std::pair<std::uint64_t, std::uint64_t>>
        missing_ranges() const;
    [[nodiscard]] bool conflicted() const { return conflicted_; }

private:
    static bool same_event(
        const NostrLogicalEvent& left,
        const NostrLogicalEvent& right
    );

    std::uint64_t highest_contiguous_sequence_{};
    std::string last_event_id_;
    std::map<std::uint64_t, NostrLogicalEvent> future_;
    std::map<std::uint64_t, NostrLogicalEvent> accepted_;
    bool conflicted_{};
};

}  // namespace aoe
