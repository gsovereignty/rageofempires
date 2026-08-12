#include "aoe/nostr_browser_bridge.hpp"

#include <deque>
#include <utility>

#include <emscripten/emscripten.h>

namespace {

std::deque<std::string>& event_queue() {
    static std::deque<std::string> queue;
    return queue;
}

std::deque<std::string>& status_queue() {
    static std::deque<std::string> queue;
    return queue;
}

std::deque<std::string>& publish_queue() {
    static std::deque<std::string> queue;
    return queue;
}

void enqueue_bounded(
    std::deque<std::string>& queue,
    const char* bytes,
    std::size_t size
) {
    if (bytes == nullptr || size == 0 ||
        size > aoe::nostr_bridge_max_message_bytes ||
        queue.size() >= aoe::nostr_bridge_max_queued_messages) {
        return;
    }
    queue.emplace_back(bytes, size);
}

std::vector<std::string> drain(std::deque<std::string>& queue) {
    std::vector<std::string> result;
    result.reserve(queue.size());
    while (!queue.empty()) {
        result.push_back(std::move(queue.front()));
        queue.pop_front();
    }
    return result;
}

EM_JS(int, js_nostr_initialize, (const char* json), {
    try {
        if (!globalThis.AoeNostrRuntime) return 0;
        globalThis.AoeNostrRuntime.initialize(JSON.parse(UTF8ToString(json)));
        return 1;
    } catch (error) {
        console.error('Nostr initialization failed', error);
        return 0;
    }
});

EM_JS(int, js_nostr_publish, (const char* json), {
    try {
        if (!globalThis.AoeNostrRuntime) return 0;
        globalThis.AoeNostrRuntime.publish(JSON.parse(UTF8ToString(json)));
        return 1;
    } catch (error) {
        console.error('Nostr publication request failed', error);
        return 0;
    }
});

EM_JS(int, js_nostr_subscribe, (const char* json), {
    try {
        if (!globalThis.AoeNostrRuntime) return 0;
        globalThis.AoeNostrRuntime.subscribe(JSON.parse(UTF8ToString(json)));
        return 1;
    } catch (error) {
        console.error('Nostr subscription request failed', error);
        return 0;
    }
});

EM_JS(int, js_nostr_republish, (const char* id), {
    try {
        if (!globalThis.AoeNostrRuntime) return 0;
        globalThis.AoeNostrRuntime.republish(UTF8ToString(id));
        return 1;
    } catch (error) {
        console.error('Nostr republication request failed', error);
        return 0;
    }
});

EM_JS(int, js_nostr_refresh_subscriptions, (), {
    try {
        if (!globalThis.AoeNostrRuntime) return 0;
        globalThis.AoeNostrRuntime.refreshSubscriptions();
        return 1;
    } catch (error) {
        console.error('Nostr subscription refresh failed', error);
        return 0;
    }
});

EM_JS(int, js_nostr_update_diagnostics, (const char* json), {
    try {
        Module.browserNostrGameDiagnostics = JSON.parse(UTF8ToString(json));
        return 1;
    } catch (error) {
        console.error('Nostr diagnostics update failed', error);
        return 0;
    }
});

EM_JS(void, js_nostr_shutdown, (), {
    if (globalThis.AoeNostrRuntime) globalThis.AoeNostrRuntime.shutdown();
});

}  // namespace

extern "C" {

EMSCRIPTEN_KEEPALIVE void aoe_nostr_enqueue_event(
    const char* bytes,
    std::size_t size
) {
    enqueue_bounded(event_queue(), bytes, size);
}

EMSCRIPTEN_KEEPALIVE void aoe_nostr_enqueue_status(
    const char* bytes,
    std::size_t size
) {
    enqueue_bounded(status_queue(), bytes, size);
}

EMSCRIPTEN_KEEPALIVE void aoe_nostr_publish_result(
    const char* bytes,
    std::size_t size
) {
    enqueue_bounded(publish_queue(), bytes, size);
}

}  // extern "C"

namespace aoe {

bool nostr_bridge_initialize(const std::string& config_json) {
    return !config_json.empty() &&
        config_json.size() <= nostr_bridge_max_message_bytes &&
        js_nostr_initialize(config_json.c_str()) != 0;
}

bool nostr_bridge_publish(const std::string& intent_json) {
    return !intent_json.empty() &&
        intent_json.size() <= nostr_bridge_max_message_bytes &&
        js_nostr_publish(intent_json.c_str()) != 0;
}

bool nostr_bridge_subscribe(const std::string& filter_json) {
    return !filter_json.empty() &&
        filter_json.size() <= nostr_bridge_max_message_bytes &&
        js_nostr_subscribe(filter_json.c_str()) != 0;
}

bool nostr_bridge_republish(const std::string& event_id) {
    return !event_id.empty() && event_id.size() <= 64 &&
        js_nostr_republish(event_id.c_str()) != 0;
}

bool nostr_bridge_refresh_subscriptions() {
    return js_nostr_refresh_subscriptions() != 0;
}

bool nostr_bridge_update_diagnostics(const std::string& diagnostics_json) {
    return !diagnostics_json.empty() &&
        diagnostics_json.size() <= nostr_bridge_max_message_bytes &&
        js_nostr_update_diagnostics(diagnostics_json.c_str()) != 0;
}

void nostr_bridge_shutdown() {
    js_nostr_shutdown();
    event_queue().clear();
    status_queue().clear();
    publish_queue().clear();
}

std::vector<std::string> drain_nostr_bridge_events() {
    return drain(event_queue());
}

std::vector<std::string> drain_nostr_bridge_statuses() {
    return drain(status_queue());
}

std::vector<std::string> drain_nostr_bridge_publish_results() {
    return drain(publish_queue());
}

}  // namespace aoe
