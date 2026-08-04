#include "aoe/commercial_multiplayer_protocol.hpp"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void require_at(bool condition, int line) {
    if (!condition) {
        throw std::runtime_error(
            "commercial multiplayer protocol test failed at " +
            std::to_string(line)
        );
    }
}
#define require(condition) require_at((condition), __LINE__)

template <typename Callable>
void require_rejected(Callable&& callable) {
    try {
        callable();
    } catch (const std::exception&) {
        return;
    }
    throw std::runtime_error("invalid commercial record was accepted");
}

}  // namespace

int main() {
    constexpr std::uint64_t steam_id = 0x1122334455667788ULL;
    const std::string request = aoe::encode_commercial_peer_request({steam_id});
    const std::string request_golden{
        "\xf5\x01\x00\x00\x88\x77\x66\x55\x44\x33\x22\x11", 12
    };
    require(request == request_golden);
    require(aoe::decode_commercial_peer_request(request).steam_id == steam_id);
    const std::string auth_complete_golden{"\xf8\x01\x00\x00", 4};
    require(aoe::encode_commercial_auth_complete() ==
        auth_complete_golden);
    aoe::decode_commercial_auth_complete(
        aoe::encode_commercial_auth_complete()
    );

    const aoe::CommercialAuthTicket source{
        steam_id, {0xde, 0xad, 0xbe, 0xef}
    };
    const std::string encoded = aoe::encode_commercial_auth_ticket(source);
    require(encoded.size() == aoe::commercial_auth_ticket_record_size);
    const std::string ticket_prefix_golden{
        "\x59\x02\x00\x00\x04\x00\x00\x00"
        "\xde\xad\xbe\xef", 12
    };
    const std::string ticket_sender_golden{
        "\x88\x77\x66\x55\x44\x33\x22\x11", 8
    };
    require(encoded.substr(0, 12) == ticket_prefix_golden);
    require(encoded.substr(0x408, 8) == ticket_sender_golden);
    const auto decoded = aoe::decode_commercial_auth_ticket(encoded);
    require(decoded.sender_steam_id == steam_id);
    require(decoded.ticket == source.ticket);

    std::string bad_length = encoded;
    bad_length[4] = '\x01';
    bad_length[5] = '\x04';
    require_rejected([&] {
        (void)aoe::decode_commercial_auth_ticket(bad_length);
    });
    require_rejected([&] {
        (void)aoe::decode_commercial_auth_ticket(encoded.substr(0, 0x40f));
    });
    for (std::size_t size = 0; size < encoded.size(); ++size) {
        require_rejected([&] {
            (void)aoe::decode_commercial_auth_ticket(
                std::string_view{encoded}.substr(0, size)
            );
        });
    }
    require_rejected([&] {
        (void)aoe::decode_commercial_auth_ticket(
            encoded + std::string(1, '\0')
        );
    });
    require_rejected([&] {
        (void)aoe::decode_commercial_peer_request(
            std::string{"\xf5\x01\x00\x00", 4}
        );
    });
    require_rejected([] {
        aoe::CommercialAuthTicket too_large;
        too_large.ticket.resize(1025);
        (void)aoe::encode_commercial_auth_ticket(too_large);
    });
    const std::string empty_ticket =
        aoe::encode_commercial_auth_ticket({steam_id, {}});
    require(aoe::decode_commercial_auth_ticket(empty_ticket).ticket.empty());

    const auto& keys = aoe::commercial_lobby_metadata_keys();
    require(keys.size() == 16);
    require(std::find(keys.begin(), keys.end(), "SlotCount") != keys.end());
    require(std::find(keys.begin(), keys.end(), "SlotsFilled") != keys.end());
    require(std::find(keys.begin(), keys.end(), "LobbyFull") != keys.end());

    std::cout << "commercial multiplayer protocol tests passed\n";
}
