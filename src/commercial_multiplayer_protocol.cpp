#include "aoe/commercial_multiplayer_protocol.hpp"

#include <stdexcept>

namespace aoe {
namespace {

void append_u32(std::string& output, std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8) {
        output.push_back(static_cast<char>((value >> shift) & 0xffU));
    }
}

void append_u64(std::string& output, std::uint64_t value) {
    for (int shift = 0; shift < 64; shift += 8) {
        output.push_back(static_cast<char>((value >> shift) & 0xffU));
    }
}

std::uint32_t read_u32(std::string_view bytes, std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 4) {
        throw std::runtime_error("truncated commercial multiplayer record");
    }
    std::uint32_t value{};
    for (std::size_t index = 0; index < 4; ++index) {
        value |= static_cast<std::uint32_t>(
            static_cast<unsigned char>(bytes[offset + index])
        ) << (index * 8);
    }
    return value;
}

std::uint64_t read_u64(std::string_view bytes, std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 8) {
        throw std::runtime_error("truncated commercial multiplayer record");
    }
    std::uint64_t value{};
    for (std::size_t index = 0; index < 8; ++index) {
        value |= static_cast<std::uint64_t>(
            static_cast<unsigned char>(bytes[offset + index])
        ) << (index * 8);
    }
    return value;
}

void require_kind(
    std::string_view bytes,
    std::uint32_t expected,
    std::size_t expected_size
) {
    if (bytes.size() != expected_size || read_u32(bytes, 0) != expected) {
        throw std::runtime_error("invalid commercial multiplayer record");
    }
}

}  // namespace

std::string encode_commercial_peer_request(
    const CommercialPeerRequest& request
) {
    std::string output;
    output.reserve(12);
    append_u32(output, commercial_peer_request_kind);
    append_u64(output, request.steam_id);
    return output;
}

CommercialPeerRequest decode_commercial_peer_request(
    std::string_view bytes
) {
    require_kind(bytes, commercial_peer_request_kind, 12);
    return {read_u64(bytes, 4)};
}

std::string encode_commercial_auth_complete() {
    std::string output;
    append_u32(output, commercial_auth_complete_kind);
    return output;
}

void decode_commercial_auth_complete(std::string_view bytes) {
    require_kind(bytes, commercial_auth_complete_kind, 4);
}

std::string encode_commercial_auth_ticket(
    const CommercialAuthTicket& ticket
) {
    if (ticket.ticket.size() > commercial_auth_ticket_capacity) {
        throw std::invalid_argument("commercial auth ticket exceeds 1024 bytes");
    }
    std::string output;
    output.reserve(commercial_auth_ticket_record_size);
    append_u32(output, commercial_auth_ticket_kind);
    append_u32(output, static_cast<std::uint32_t>(ticket.ticket.size()));
    if (!ticket.ticket.empty()) {
        output.append(
            reinterpret_cast<const char*>(ticket.ticket.data()),
            ticket.ticket.size()
        );
    }
    output.resize(8 + commercial_auth_ticket_capacity, '\0');
    append_u64(output, ticket.sender_steam_id);
    return output;
}

CommercialAuthTicket decode_commercial_auth_ticket(
    std::string_view bytes
) {
    require_kind(
        bytes,
        commercial_auth_ticket_kind,
        commercial_auth_ticket_record_size
    );
    const std::uint32_t size = read_u32(bytes, 4);
    if (size > commercial_auth_ticket_capacity) {
        throw std::runtime_error("invalid commercial auth ticket length");
    }
    CommercialAuthTicket result;
    result.sender_steam_id = read_u64(bytes, 0x408);
    result.ticket.assign(
        bytes.begin() + 8,
        bytes.begin() + 8 + size
    );
    return result;
}

const std::vector<std::string_view>& commercial_lobby_metadata_keys() {
    static const std::vector<std::string_view> keys{
        "name_salt", "title_salt", "SlotCount", "SlotsFilled",
        "QuickMatchType", "QuickMatchSize", "SkillAverage", "GameType",
        "MapFileType", "MapType", "MapSize", "Resource", "Victory",
        "CheatsEnabled", "LatencyRegion", "LobbyFull",
    };
    return keys;
}

}  // namespace aoe
