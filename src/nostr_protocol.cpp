#include "aoe/nostr_protocol.hpp"

#include <algorithm>

namespace aoe {

bool NostrSenderSequence::same_event(
    const NostrLogicalEvent& left,
    const NostrLogicalEvent& right
) {
    return left.event_id == right.event_id &&
        left.previous_event_id == right.previous_event_id &&
        left.content == right.content;
}

NostrSequenceAccept NostrSenderSequence::accept(
    NostrLogicalEvent event
) {
    if (conflicted_ || event.sequence == 0 || event.event_id.empty()) {
        return NostrSequenceAccept::conflict;
    }
    if (event.sequence <= highest_contiguous_sequence_) {
        const auto found = accepted_.find(event.sequence);
        if (found != accepted_.end() && same_event(found->second, event)) {
            return NostrSequenceAccept::duplicate;
        }
        conflicted_ = true;
        return NostrSequenceAccept::conflict;
    }
    if (event.sequence > highest_contiguous_sequence_ +
            nostr_max_future_sender_sequences) {
        return NostrSequenceAccept::out_of_bounds;
    }
    const auto found = future_.find(event.sequence);
    if (found == future_.end()) {
        future_.emplace(event.sequence, std::move(event));
        return NostrSequenceAccept::buffered;
    }
    if (same_event(found->second, event)) {
        return NostrSequenceAccept::duplicate;
    }
    conflicted_ = true;
    return NostrSequenceAccept::conflict;
}

NostrSequenceDrain NostrSenderSequence::drain() {
    NostrSequenceDrain result;
    if (conflicted_) {
        result.conflict = true;
        result.reason = "sender stream already conflicted";
        return result;
    }
    while (true) {
        const std::uint64_t expected = highest_contiguous_sequence_ + 1;
        const auto found = future_.find(expected);
        if (found == future_.end()) break;
        const std::string expected_previous =
            highest_contiguous_sequence_ == 0 ? std::string{} : last_event_id_;
        if (found->second.previous_event_id != expected_previous) {
            conflicted_ = true;
            result.conflict = true;
            result.reason = "sender previous-event chain mismatch";
            return result;
        }
        NostrLogicalEvent accepted = std::move(found->second);
        future_.erase(found);
        highest_contiguous_sequence_ = accepted.sequence;
        last_event_id_ = accepted.event_id;
        accepted_[accepted.sequence] = accepted;
        result.contiguous.push_back(std::move(accepted));
        while (accepted_.size() > nostr_max_future_sender_sequences * 2) {
            accepted_.erase(accepted_.begin());
        }
    }
    return result;
}

std::vector<std::pair<std::uint64_t, std::uint64_t>>
NostrSenderSequence::missing_ranges() const {
    std::vector<std::pair<std::uint64_t, std::uint64_t>> ranges;
    if (future_.empty()) return ranges;
    std::uint64_t cursor = highest_contiguous_sequence_ + 1;
    for (const auto& [sequence, event] : future_) {
        static_cast<void>(event);
        if (cursor < sequence) ranges.emplace_back(cursor, sequence - 1);
        cursor = sequence + 1;
    }
    return ranges;
}

}  // namespace aoe
