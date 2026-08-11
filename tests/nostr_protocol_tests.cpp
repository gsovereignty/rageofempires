#include "aoe/nostr_protocol.hpp"

#include <iostream>

namespace {

int failures{};

void expect(bool value, const char* message) {
    if (!value) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

aoe::NostrLogicalEvent event(
    std::uint64_t sequence,
    std::string previous,
    std::string id,
    std::string content = "turn"
) {
    return {sequence, std::move(previous), std::move(id), std::move(content)};
}

}  // namespace

int main() {
    aoe::NostrSenderSequence reordered;
    expect(
        reordered.accept(event(2, "event-1", "event-2")) ==
            aoe::NostrSequenceAccept::buffered,
        "future sender event buffered"
    );
    expect(
        reordered.drain().contiguous.empty(),
        "future sender event waits for gap"
    );
    expect(
        reordered.missing_ranges() ==
            std::vector<std::pair<std::uint64_t, std::uint64_t>>{{1, 1}},
        "missing sequence range reported"
    );
    expect(
        reordered.accept(event(1, "", "event-1")) ==
            aoe::NostrSequenceAccept::buffered,
        "missing sender event accepted"
    );
    const aoe::NostrSequenceDrain contiguous = reordered.drain();
    expect(
        !contiguous.conflict && contiguous.contiguous.size() == 2 &&
            contiguous.contiguous[0].sequence == 1 &&
            contiguous.contiguous[1].sequence == 2 &&
            reordered.highest_contiguous_sequence() == 2 &&
            reordered.last_event_id() == "event-2",
        "reordered events drain through previous-event chain"
    );
    expect(
        reordered.accept(event(1, "", "event-1")) ==
            aoe::NostrSequenceAccept::duplicate,
        "exact accepted duplicate ignored"
    );

    aoe::NostrSenderSequence duplicate_conflict;
    expect(
        duplicate_conflict.accept(event(1, "", "event-a")) ==
            aoe::NostrSequenceAccept::buffered &&
        duplicate_conflict.accept(event(1, "", "event-b")) ==
            aoe::NostrSequenceAccept::conflict &&
        duplicate_conflict.conflicted(),
        "same sequence with different event conflicts"
    );

    aoe::NostrSenderSequence chain_conflict;
    expect(
        chain_conflict.accept(event(1, "wrong", "event-1")) ==
            aoe::NostrSequenceAccept::buffered,
        "chain candidate buffered"
    );
    const aoe::NostrSequenceDrain rejected_chain = chain_conflict.drain();
    expect(
        rejected_chain.conflict && chain_conflict.conflicted(),
        "incorrect previous-event link conflicts during drain"
    );

    aoe::NostrSenderSequence bounded;
    expect(
        bounded.accept(event(
            aoe::nostr_max_future_sender_sequences + 1,
            "", "too-far"
        )) == aoe::NostrSequenceAccept::out_of_bounds,
        "far-future sequence rejected by memory bound"
    );

    if (failures == 0) std::cout << "Nostr protocol tests passed\n";
    return failures == 0 ? 0 : 1;
}
