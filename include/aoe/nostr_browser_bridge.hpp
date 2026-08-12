#pragma once

#include <string>
#include <vector>

namespace aoe {

constexpr std::size_t nostr_bridge_max_message_bytes = 1024 * 1024;
constexpr std::size_t nostr_bridge_max_queued_messages = 256;

bool nostr_bridge_initialize(const std::string& config_json);
bool nostr_bridge_publish(const std::string& intent_json);
bool nostr_bridge_subscribe(const std::string& filter_json);
bool nostr_bridge_republish(const std::string& event_id);
bool nostr_bridge_refresh_subscriptions();
bool nostr_bridge_update_diagnostics(const std::string& diagnostics_json);
void nostr_bridge_shutdown();

std::vector<std::string> drain_nostr_bridge_events();
std::vector<std::string> drain_nostr_bridge_statuses();
std::vector<std::string> drain_nostr_bridge_publish_results();

}  // namespace aoe
