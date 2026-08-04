#include "aoe/commercial_multiplayer_service.hpp"

#include <iostream>
#include <stdexcept>

namespace {
void require_at(bool value, int line) {
    if (!value) throw std::runtime_error("commercial service test failed at " + std::to_string(line));
}
#define require(value) require_at((value), __LINE__)
}

int main() {
    auto network = aoe::make_mock_commercial_network();
    std::vector<std::unique_ptr<aoe::CommercialMultiplayerService>> peers;
    for (std::uint64_t id = 1; id <= 8; ++id) {
        peers.push_back(aoe::make_mock_commercial_service(network, {id, "peer-" + std::to_string(id)}));
    }
    const auto lobby = peers[0]->create_lobby(8);
    require(peers[0]->set_lobby_metadata(lobby, "SlotCount", "8"));
    require(peers[0]->set_lobby_metadata(lobby, "SlotsFilled", "1"));
    require(peers[1]->discover_lobbies().size() == 1);
    for (std::size_t index = 1; index < peers.size(); ++index) {
        require(peers[index]->join_lobby(lobby));
        require(peers[0]->authenticate(index + 1, peers[index]->authentication_ticket()));
    }
    require(peers[0]->lobby(lobby)->members.size() == 8);
    require(peers[1]->discover_lobbies().empty());
    require(!peers[1]->set_lobby_metadata(lobby, "SlotCount", "2"));
    require(peers[0]->send_reliable(8, "start", 0));
    const auto packet = peers[7]->receive(0);
    require(packet && packet->sender == 1 && packet->bytes == "start");
    require(!peers[7]->receive(0));

    // Explicit owner transfer and automatic migration on owner departure.
    require(peers[0]->transfer_lobby_owner(lobby, 2));
    require(peers[1]->lobby(lobby)->owner == 2);
    peers[1]->leave_lobby(lobby);
    require(peers[2]->lobby(lobby)->owner == 1);
    peers[1].reset();
    peers[1] = aoe::make_mock_commercial_service(network, {2, "peer-2"});
    require(peers[1]->join_lobby(lobby)); // process loss/reconnect keeps account
    require(peers[0]->authenticate(2, peers[1]->authentication_ticket()));
    require(peers[1]->lobby(lobby)->members.size() == 8);

    auto bad = peers[2]->authentication_ticket();
    bad[0] ^= 1;
    require(!peers[0]->authenticate(3, bad));
    require(!peers[0]->send_reliable(99, "missing", 0));

    std::cout << "commercial multiplayer service tests passed\n";
}
