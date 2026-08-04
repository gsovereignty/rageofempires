#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace aoe {

// Outer Steam P2P control records recovered from AoK HD. These records are
// not the game-action protocol and do not replace Steam identity/auth APIs.
constexpr std::uint32_t commercial_peer_request_kind = 0x01f5;
constexpr std::uint32_t commercial_auth_complete_kind = 0x01f8;
constexpr std::uint32_t commercial_auth_ticket_kind = 0x0259;
constexpr std::size_t commercial_auth_ticket_capacity = 1024;
constexpr std::size_t commercial_auth_ticket_record_size = 0x410;

struct CommercialPeerRequest {
    std::uint64_t steam_id{};
};

struct CommercialAuthTicket {
    std::uint64_t sender_steam_id{};
    std::vector<std::uint8_t> ticket;
};

std::string encode_commercial_peer_request(
    const CommercialPeerRequest& request
);
CommercialPeerRequest decode_commercial_peer_request(
    std::string_view bytes
);

std::string encode_commercial_auth_complete();
void decode_commercial_auth_complete(std::string_view bytes);

std::string encode_commercial_auth_ticket(
    const CommercialAuthTicket& ticket
);
CommercialAuthTicket decode_commercial_auth_ticket(
    std::string_view bytes
);

// Exact lobby keys observed in SteamMatchmaking calls. Values remain owned by
// Steam lobby metadata; this list makes spelling/case testable.
const std::vector<std::string_view>& commercial_lobby_metadata_keys();

}  // namespace aoe
