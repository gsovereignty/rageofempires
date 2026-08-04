#include "aoe/commercial_multiplayer_service.hpp"
#include "aoe/commercial_multiplayer_adapter.h"

#include <dlfcn.h>
#include <algorithm>
#include <deque>
#include <map>
#include <stdexcept>

namespace aoe {

class MockCommercialNetwork {
public:
    struct Lobby {
        CommercialLobbySnapshot snapshot;
    };
    std::uint64_t next_lobby{1};
    std::map<CommercialLobbyId, Lobby> lobbies;
    std::map<CommercialUserId, std::map<int, std::deque<CommercialPacket>>> queues;
    std::map<CommercialUserId, std::string> names;
};

namespace {

class DynamicAdapter final : public CommercialMultiplayerService {
public:
    DynamicAdapter(void* library, AoeCommercialMultiplayerAdapterV1* api)
        : library_(library), api_(api) {}
    ~DynamicAdapter() override {
        if (api_ && api_->destroy) api_->destroy(api_->context);
        if (library_) dlclose(library_);
    }
    CommercialIdentity identity() const override {
        return {api_->user_id(api_->context), api_->display_name(api_->context)};
    }
    std::vector<std::uint8_t> authentication_ticket() override {
        const std::size_t size = api_->ticket(api_->context, nullptr, 0);
        std::vector<std::uint8_t> result(size);
        if (size && api_->ticket(api_->context, result.data(), size) != size) {
            throw std::runtime_error("commercial adapter ticket changed size");
        }
        return result;
    }
    bool authenticate(CommercialUserId peer, const std::vector<std::uint8_t>& ticket) override {
        return api_->authenticate(api_->context, peer, ticket.data(), ticket.size()) != 0;
    }
    CommercialLobbyId create_lobby(std::size_t capacity) override {
        return api_->create_lobby(api_->context, capacity);
    }
    std::vector<CommercialLobbySnapshot> discover_lobbies() const override {
        throw std::runtime_error("commercial adapter ABI v1 lacks lobby enumeration");
    }
    bool join_lobby(CommercialLobbyId lobby) override {
        return api_->join_lobby(api_->context, lobby) != 0;
    }
    void leave_lobby(CommercialLobbyId lobby) override {
        api_->leave_lobby(api_->context, lobby);
    }
    bool set_lobby_metadata(CommercialLobbyId lobby, std::string key, std::string value) override {
        return api_->set_metadata(api_->context, lobby, key.c_str(), value.c_str()) != 0;
    }
    std::optional<CommercialLobbySnapshot> lobby(CommercialLobbyId) const override {
        throw std::runtime_error("commercial adapter ABI v1 lacks lobby snapshots");
    }
    bool transfer_lobby_owner(CommercialLobbyId lobby, CommercialUserId owner) override {
        return api_->transfer_owner(api_->context, lobby, owner) != 0;
    }
    bool send_reliable(CommercialUserId peer, std::string bytes, int channel) override {
        return api_->send(api_->context, peer, bytes.data(), bytes.size(), channel) != 0;
    }
    std::optional<CommercialPacket> receive(int channel) override {
        CommercialUserId sender{};
        const std::size_t size = api_->receive(api_->context, &sender, nullptr, 0, channel);
        if (!size) return std::nullopt;
        std::string bytes(size, '\0');
        if (api_->receive(api_->context, &sender, bytes.data(), size, channel) != size) {
            throw std::runtime_error("commercial adapter packet changed size");
        }
        return CommercialPacket{sender, std::move(bytes)};
    }
private:
    void* library_{};
    AoeCommercialMultiplayerAdapterV1* api_{};
};

class MockService final : public CommercialMultiplayerService {
public:
    MockService(std::shared_ptr<MockCommercialNetwork> network, CommercialIdentity identity)
        : network_(std::move(network)), identity_(std::move(identity)) {
        if (!identity_.user_id || identity_.display_name.empty()) {
            throw std::invalid_argument("mock commercial identity is invalid");
        }
        if (!network_->names.emplace(identity_.user_id, identity_.display_name).second) {
            throw std::invalid_argument("duplicate mock commercial identity");
        }
    }
    ~MockService() override { network_->names.erase(identity_.user_id); }
    CommercialIdentity identity() const override { return identity_; }
    std::vector<std::uint8_t> authentication_ticket() override {
        std::vector<std::uint8_t> ticket(16);
        for (std::size_t i = 0; i < 8; ++i) {
            ticket[i] = static_cast<std::uint8_t>(identity_.user_id >> (i * 8));
            ticket[i + 8] = static_cast<std::uint8_t>(ticket[i] ^ 0xa5U);
        }
        return ticket;
    }
    bool authenticate(CommercialUserId peer, const std::vector<std::uint8_t>& ticket) override {
        if (!network_->names.contains(peer) || ticket.size() != 16) return false;
        for (std::size_t i = 0; i < 8; ++i) {
            const auto value = static_cast<std::uint8_t>(peer >> (i * 8));
            if (ticket[i] != value || ticket[i + 8] != static_cast<std::uint8_t>(value ^ 0xa5U)) return false;
        }
        return true;
    }
    CommercialLobbyId create_lobby(std::size_t capacity) override {
        if (capacity == 0 || capacity > 8) throw std::invalid_argument("commercial lobby capacity must be 1..8");
        const auto id = network_->next_lobby++;
        CommercialLobbySnapshot snapshot{id, identity_.user_id, capacity, {identity_.user_id}, {}};
        network_->lobbies.emplace(id, MockCommercialNetwork::Lobby{std::move(snapshot)});
        return id;
    }
    std::vector<CommercialLobbySnapshot> discover_lobbies() const override {
        std::vector<CommercialLobbySnapshot> result;
        for (const auto& [id, lobby] : network_->lobbies) {
            (void)id;
            if (lobby.snapshot.members.size() < lobby.snapshot.capacity) result.push_back(lobby.snapshot);
        }
        return result;
    }
    bool join_lobby(CommercialLobbyId id) override {
        auto found = network_->lobbies.find(id);
        if (found == network_->lobbies.end()) return false;
        auto& members = found->second.snapshot.members;
        if (std::find(members.begin(), members.end(), identity_.user_id) != members.end()) return true;
        if (members.size() >= found->second.snapshot.capacity) return false;
        members.push_back(identity_.user_id);
        return true;
    }
    void leave_lobby(CommercialLobbyId id) override {
        auto found = network_->lobbies.find(id);
        if (found == network_->lobbies.end()) return;
        auto& snapshot = found->second.snapshot;
        snapshot.members.erase(std::remove(snapshot.members.begin(), snapshot.members.end(), identity_.user_id), snapshot.members.end());
        if (snapshot.members.empty()) { network_->lobbies.erase(found); return; }
        if (snapshot.owner == identity_.user_id) snapshot.owner = snapshot.members.front();
    }
    bool set_lobby_metadata(CommercialLobbyId id, std::string key, std::string value) override {
        auto found = network_->lobbies.find(id);
        if (found == network_->lobbies.end() || found->second.snapshot.owner != identity_.user_id) return false;
        found->second.snapshot.metadata[std::move(key)] = std::move(value);
        return true;
    }
    std::optional<CommercialLobbySnapshot> lobby(CommercialLobbyId id) const override {
        auto found = network_->lobbies.find(id);
        return found == network_->lobbies.end() ? std::nullopt : std::optional{found->second.snapshot};
    }
    bool transfer_lobby_owner(CommercialLobbyId id, CommercialUserId owner) override {
        auto found = network_->lobbies.find(id);
        if (found == network_->lobbies.end() || found->second.snapshot.owner != identity_.user_id ||
            std::find(found->second.snapshot.members.begin(), found->second.snapshot.members.end(), owner) == found->second.snapshot.members.end()) return false;
        found->second.snapshot.owner = owner;
        return true;
    }
    bool send_reliable(CommercialUserId peer, std::string bytes, int channel) override {
        if (!network_->names.contains(peer) || channel < 0) return false;
        network_->queues[peer][channel].push_back({identity_.user_id, std::move(bytes)});
        return true;
    }
    std::optional<CommercialPacket> receive(int channel) override {
        auto& queue = network_->queues[identity_.user_id][channel];
        if (queue.empty()) return std::nullopt;
        CommercialPacket packet = std::move(queue.front());
        queue.pop_front();
        return packet;
    }
private:
    std::shared_ptr<MockCommercialNetwork> network_;
    CommercialIdentity identity_;
};

void require_api(const AoeCommercialMultiplayerAdapterV1& api) {
    if (api.abi_version != 1 || !api.destroy || !api.user_id ||
        !api.display_name || !api.ticket || !api.authenticate ||
        !api.create_lobby || !api.join_lobby || !api.leave_lobby ||
        !api.set_metadata || !api.transfer_owner || !api.send || !api.receive) {
        throw std::runtime_error("invalid commercial multiplayer adapter ABI");
    }
}

}  // namespace

std::unique_ptr<CommercialMultiplayerService>
load_commercial_multiplayer_adapter(const std::string& library_path) {
    void* library = dlopen(library_path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!library) throw std::runtime_error(dlerror());
    auto create = reinterpret_cast<AoeCreateCommercialMultiplayerAdapterV1>(
        dlsym(library, "aoe_commercial_multiplayer_adapter_v1")
    );
    if (!create) {
        dlclose(library);
        throw std::runtime_error("commercial adapter entry point missing");
    }
    AoeCommercialMultiplayerAdapterV1* api = create();
    try {
        if (!api) throw std::runtime_error("commercial adapter creation failed");
        require_api(*api);
    } catch (...) {
        if (api && api->destroy) api->destroy(api->context);
        dlclose(library);
        throw;
    }
    return std::make_unique<DynamicAdapter>(library, api);
}

std::shared_ptr<MockCommercialNetwork> make_mock_commercial_network() {
    return std::make_shared<MockCommercialNetwork>();
}

std::unique_ptr<CommercialMultiplayerService> make_mock_commercial_service(
    std::shared_ptr<MockCommercialNetwork> network,
    CommercialIdentity identity
) {
    if (!network) throw std::invalid_argument("mock commercial network is null");
    return std::make_unique<MockService>(std::move(network), std::move(identity));
}

}  // namespace aoe
