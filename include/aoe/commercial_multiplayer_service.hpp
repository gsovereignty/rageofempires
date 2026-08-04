#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aoe {

using CommercialUserId = std::uint64_t;
using CommercialLobbyId = std::uint64_t;

struct CommercialIdentity {
    CommercialUserId user_id{};
    std::string display_name;
};

struct CommercialPacket {
    CommercialUserId sender{};
    std::string bytes;
};

struct CommercialLobbySnapshot {
    CommercialLobbyId lobby_id{};
    CommercialUserId owner{};
    std::size_t capacity{};
    std::vector<CommercialUserId> members;
    std::map<std::string, std::string> metadata;
};

// Steam-shaped boundary used by commercial multiplayer. Implementations may
// use Steamworks, another licensed service adapter, or the hermetic mock.
class CommercialMultiplayerService {
public:
    virtual ~CommercialMultiplayerService() = default;
    virtual CommercialIdentity identity() const = 0;
    virtual std::vector<std::uint8_t> authentication_ticket() = 0;
    virtual bool authenticate(
        CommercialUserId peer,
        const std::vector<std::uint8_t>& ticket
    ) = 0;
    virtual CommercialLobbyId create_lobby(std::size_t capacity) = 0;
    virtual std::vector<CommercialLobbySnapshot> discover_lobbies() const = 0;
    virtual bool join_lobby(CommercialLobbyId lobby) = 0;
    virtual void leave_lobby(CommercialLobbyId lobby) = 0;
    virtual bool set_lobby_metadata(
        CommercialLobbyId lobby,
        std::string key,
        std::string value
    ) = 0;
    virtual std::optional<CommercialLobbySnapshot> lobby(
        CommercialLobbyId lobby
    ) const = 0;
    virtual bool transfer_lobby_owner(
        CommercialLobbyId lobby,
        CommercialUserId new_owner
    ) = 0;
    virtual bool send_reliable(
        CommercialUserId peer,
        std::string bytes,
        int channel = 0
    ) = 0;
    virtual std::optional<CommercialPacket> receive(int channel = 0) = 0;
};

class MockCommercialNetwork;

// Shared deterministic service used for multi-client protocol tests. Each
// service instance represents one process/account on same mock backend.
std::shared_ptr<MockCommercialNetwork> make_mock_commercial_network();
std::unique_ptr<CommercialMultiplayerService> make_mock_commercial_service(
    std::shared_ptr<MockCommercialNetwork> network,
    CommercialIdentity identity
);

// Loads an opt-in adapter implementing the stable C ABI documented in
// docs/contracts/COMMERCIAL_MULTIPLAYER_ADAPTER.md. No SDK is linked or
// redistributed by this project.
std::unique_ptr<CommercialMultiplayerService>
load_commercial_multiplayer_adapter(const std::string& library_path);

}  // namespace aoe
