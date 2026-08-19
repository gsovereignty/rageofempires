#include "aoe/nostr_multiplayer_runtime.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <charconv>
#include <deque>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <variant>

#include "aoe/nostr_browser_bridge.hpp"
#include "aoe/nostr_protocol.hpp"
#include "aoe/runtime_paths.hpp"

namespace aoe {
namespace {

constexpr std::string_view application_tag = "aoe-reconstruction";
constexpr std::size_t maximum_event_content_bytes = 768 * 1024;
constexpr std::size_t maximum_event_tags = 64;
constexpr std::size_t maximum_tag_parts = 8;
constexpr std::size_t maximum_tag_part_bytes = 4096;
constexpr std::size_t maximum_turn_frames = 5;
constexpr std::uint64_t match_epoch = 1;
constexpr std::uint64_t lobby_lifetime_seconds = 2 * 60 * 60;

struct JsonValue {
    using Array = std::vector<JsonValue>;
    using Object = std::map<std::string, JsonValue>;
    std::variant<std::nullptr_t, bool, std::uint64_t, std::string, Array, Object>
        value;

    [[nodiscard]] const Object& object() const {
        return std::get<Object>(value);
    }
    [[nodiscard]] const Array& array() const {
        return std::get<Array>(value);
    }
    [[nodiscard]] const std::string& string() const {
        return std::get<std::string>(value);
    }
    [[nodiscard]] std::uint64_t number() const {
        return std::get<std::uint64_t>(value);
    }
    [[nodiscard]] bool boolean() const {
        return std::get<bool>(value);
    }
};

void append_utf8(std::string& output, std::uint32_t codepoint) {
    if (codepoint <= 0x7f) {
        output.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7ff) {
        output.push_back(static_cast<char>(0xc0 | (codepoint >> 6)));
        output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    } else if (codepoint <= 0xffff) {
        output.push_back(static_cast<char>(0xe0 | (codepoint >> 12)));
        output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
        output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    } else {
        output.push_back(static_cast<char>(0xf0 | (codepoint >> 18)));
        output.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f)));
        output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
        output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    }
}

class JsonParser {
public:
    explicit JsonParser(std::string_view input) : input_(input) {}

    JsonValue parse() {
        JsonValue result = parse_value(0);
        whitespace();
        if (position_ != input_.size()) fail("trailing JSON data");
        return result;
    }

private:
    [[noreturn]] void fail(std::string_view message) const {
        throw std::runtime_error(std::string{message});
    }

    void whitespace() {
        while (position_ < input_.size() &&
               (input_[position_] == ' ' || input_[position_] == '\n' ||
                input_[position_] == '\r' || input_[position_] == '\t')) {
            ++position_;
        }
    }

    bool consume(char character) {
        whitespace();
        if (position_ >= input_.size() || input_[position_] != character) {
            return false;
        }
        ++position_;
        return true;
    }

    void require(char character) {
        if (!consume(character)) fail("invalid JSON punctuation");
    }

    bool literal(std::string_view value) {
        whitespace();
        if (input_.substr(position_, value.size()) != value) return false;
        position_ += value.size();
        return true;
    }

    std::uint32_t hex4() {
        if (input_.size() - position_ < 4) fail("short JSON unicode escape");
        std::uint32_t value{};
        for (int index = 0; index < 4; ++index) {
            const char character = input_[position_++];
            value <<= 4;
            if (character >= '0' && character <= '9') {
                value |= static_cast<std::uint32_t>(character - '0');
            } else if (character >= 'a' && character <= 'f') {
                value |= static_cast<std::uint32_t>(character - 'a' + 10);
            } else if (character >= 'A' && character <= 'F') {
                value |= static_cast<std::uint32_t>(character - 'A' + 10);
            } else {
                fail("invalid JSON unicode escape");
            }
        }
        return value;
    }

    std::string parse_string() {
        whitespace();
        if (position_ >= input_.size() || input_[position_++] != '"') {
            fail("expected JSON string");
        }
        std::string output;
        while (position_ < input_.size()) {
            const unsigned char character =
                static_cast<unsigned char>(input_[position_++]);
            if (character == '"') return output;
            if (character < 0x20) fail("control byte in JSON string");
            if (character != '\\') {
                output.push_back(static_cast<char>(character));
                continue;
            }
            if (position_ >= input_.size()) fail("short JSON escape");
            const char escaped = input_[position_++];
            switch (escaped) {
                case '"': output.push_back('"'); break;
                case '\\': output.push_back('\\'); break;
                case '/': output.push_back('/'); break;
                case 'b': output.push_back('\b'); break;
                case 'f': output.push_back('\f'); break;
                case 'n': output.push_back('\n'); break;
                case 'r': output.push_back('\r'); break;
                case 't': output.push_back('\t'); break;
                case 'u': {
                    std::uint32_t codepoint = hex4();
                    if (codepoint >= 0xd800 && codepoint <= 0xdbff) {
                        if (input_.size() - position_ < 6 ||
                            input_[position_] != '\\' ||
                            input_[position_ + 1] != 'u') {
                            fail("unpaired JSON unicode surrogate");
                        }
                        position_ += 2;
                        const std::uint32_t low = hex4();
                        if (low < 0xdc00 || low > 0xdfff) {
                            fail("invalid JSON unicode surrogate");
                        }
                        codepoint = 0x10000 +
                            ((codepoint - 0xd800) << 10) +
                            (low - 0xdc00);
                    } else if (codepoint >= 0xdc00 && codepoint <= 0xdfff) {
                        fail("unpaired JSON unicode surrogate");
                    }
                    append_utf8(output, codepoint);
                    break;
                }
                default: fail("invalid JSON escape");
            }
            if (output.size() > nostr_bridge_max_message_bytes) {
                fail("JSON string exceeds bound");
            }
        }
        fail("unterminated JSON string");
    }

    JsonValue parse_number() {
        whitespace();
        const std::size_t start = position_;
        if (position_ >= input_.size() ||
            input_[position_] < '0' || input_[position_] > '9') {
            fail("invalid JSON number");
        }
        if (input_[position_] == '0') {
            ++position_;
            if (position_ < input_.size() &&
                std::isdigit(static_cast<unsigned char>(input_[position_]))) {
                fail("noncanonical JSON number");
            }
        } else {
            while (position_ < input_.size() &&
                   std::isdigit(static_cast<unsigned char>(input_[position_]))) {
                ++position_;
            }
        }
        std::uint64_t value{};
        const auto converted = std::from_chars(
            input_.data() + start, input_.data() + position_, value
        );
        if (converted.ec != std::errc{} ||
            converted.ptr != input_.data() + position_) {
            fail("JSON integer out of range");
        }
        return JsonValue{value};
    }

    JsonValue parse_value(std::size_t depth) {
        if (depth > 12) fail("JSON nesting exceeds bound");
        whitespace();
        if (position_ >= input_.size()) fail("missing JSON value");
        if (input_[position_] == '"') return JsonValue{parse_string()};
        if (input_[position_] == '{') {
            ++position_;
            JsonValue::Object object;
            if (consume('}')) return JsonValue{std::move(object)};
            do {
                std::string key = parse_string();
                require(':');
                auto [found, inserted] = object.emplace(
                    std::move(key), parse_value(depth + 1)
                );
                static_cast<void>(found);
                if (!inserted) fail("duplicate JSON object key");
                if (object.size() > 1024) fail("JSON object exceeds bound");
            } while (consume(','));
            require('}');
            return JsonValue{std::move(object)};
        }
        if (input_[position_] == '[') {
            ++position_;
            JsonValue::Array array;
            if (consume(']')) return JsonValue{std::move(array)};
            do {
                array.push_back(parse_value(depth + 1));
                if (array.size() > 4096) fail("JSON array exceeds bound");
            } while (consume(','));
            require(']');
            return JsonValue{std::move(array)};
        }
        if (literal("true")) return JsonValue{true};
        if (literal("false")) return JsonValue{false};
        if (literal("null")) return JsonValue{nullptr};
        return parse_number();
    }

    std::string_view input_;
    std::size_t position_{};
};

const JsonValue& required(
    const JsonValue::Object& object,
    std::string_view key
) {
    const auto found = object.find(std::string{key});
    if (found == object.end()) {
        throw std::runtime_error("missing JSON field: " + std::string{key});
    }
    return found->second;
}

std::string string_field(
    const JsonValue::Object& object,
    std::string_view key,
    std::size_t maximum = nostr_bridge_max_message_bytes
) {
    const std::string& value = required(object, key).string();
    if (value.size() > maximum) {
        throw std::runtime_error("JSON string field exceeds bound");
    }
    return value;
}

std::string optional_string_field(
    const JsonValue::Object& object,
    std::string_view key,
    std::size_t maximum = nostr_bridge_max_message_bytes
) {
    const auto found = object.find(std::string{key});
    if (found == object.end()) return {};
    const std::string& value = found->second.string();
    if (value.size() > maximum) {
        throw std::runtime_error("JSON string field exceeds bound");
    }
    return value;
}

std::uint64_t number_field(
    const JsonValue::Object& object,
    std::string_view key
) {
    return required(object, key).number();
}

bool bool_field(const JsonValue::Object& object, std::string_view key) {
    return required(object, key).boolean();
}

std::vector<std::string> string_array_field(
    const JsonValue::Object& object,
    std::string_view key,
    std::size_t maximum_count,
    std::size_t maximum_size
) {
    const auto& array = required(object, key).array();
    if (array.size() > maximum_count) {
        throw std::runtime_error("JSON string array exceeds bound");
    }
    std::vector<std::string> result;
    result.reserve(array.size());
    for (const JsonValue& value : array) {
        const std::string& string = value.string();
        if (string.size() > maximum_size) {
            throw std::runtime_error("JSON string array item exceeds bound");
        }
        result.push_back(string);
    }
    return result;
}

std::vector<std::string> default_public_relays() {
    const auto path = runtime_resources_directory() / "nostr-relays.json";
    std::ifstream input{path};
    if (!input) {
        throw std::runtime_error(
            "cannot open canonical Nostr relay configuration: " +
            path.string()
        );
    }
    std::ostringstream contents;
    contents << input.rdbuf();
    const auto relays = string_array_field(
        JsonParser(contents.str()).parse().object(), "relays", 20, 256
    );
    if (relays.size() != 20 ||
        std::ranges::any_of(relays, [](const std::string& relay) {
            return !relay.starts_with("wss://");
        }) || std::set<std::string>{relays.begin(), relays.end()}.size() !=
            relays.size()) {
        throw std::runtime_error(
            "canonical Nostr relay configuration must contain 20 unique "
            "wss URLs"
        );
    }
    return relays;
}

std::vector<std::pair<std::uint64_t, std::uint64_t>> range_array_field(
    const JsonValue::Object& object,
    std::string_view key
) {
    const auto& array = required(object, key).array();
    if (array.size() > 32) {
        throw std::runtime_error("missing-range array exceeds bound");
    }
    std::vector<std::pair<std::uint64_t, std::uint64_t>> result;
    result.reserve(array.size());
    for (const JsonValue& value : array) {
        const auto& range = value.array();
        if (range.size() != 2) {
            throw std::runtime_error("invalid missing sequence range");
        }
        const std::uint64_t first = range[0].number();
        const std::uint64_t last = range[1].number();
        if (first == 0 || last < first ||
            last - first >= nostr_max_future_sender_sequences) {
            throw std::runtime_error("missing sequence range exceeds bound");
        }
        result.emplace_back(first, last);
    }
    return result;
}

std::string json_string(std::string_view input) {
    std::ostringstream output;
    output << '"';
    for (const unsigned char character : input) {
        switch (character) {
            case '"': output << "\\\""; break;
            case '\\': output << "\\\\"; break;
            case '\b': output << "\\b"; break;
            case '\f': output << "\\f"; break;
            case '\n': output << "\\n"; break;
            case '\r': output << "\\r"; break;
            case '\t': output << "\\t"; break;
            default:
                if (character < 0x20) {
                    output << "\\u" << std::hex << std::setfill('0')
                           << std::setw(4) << static_cast<int>(character)
                           << std::dec;
                } else {
                    output << static_cast<char>(character);
                }
        }
    }
    output << '"';
    return output.str();
}

std::string json_string_array(const std::vector<std::string>& values) {
    std::ostringstream output;
    output << '[';
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index != 0) output << ',';
        output << json_string(values[index]);
    }
    output << ']';
    return output.str();
}

bool hex64(std::string_view value) {
    return value.size() == 64 && std::ranges::all_of(value, [](char character) {
        return (character >= '0' && character <= '9') ||
            (character >= 'a' && character <= 'f');
    });
}

std::uint64_t unix_time_seconds() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count()
    );
}

int slot_number(Player player) {
    if (player == Player::blue) return 0;
    if (player == Player::red) return 1;
    throw std::invalid_argument("unsupported Nostr player slot");
}

std::string compatibility_digest(LockstepSessionConfig config) {
    return lockstep_compatibility_digest(std::move(config));
}

bool compatible_configuration(
    const LockstepSessionConfig& local,
    const LockstepSessionConfig& remote
) {
    return compatibility_digest(local) == compatibility_digest(remote);
}

LockstepFrame hello_for_config(
    const LockstepSessionConfig& config,
    Player player
) {
    LockstepFrame frame;
    frame.kind = LockstepFrameKind::hello;
    frame.player = player;
    frame.scenario_digest = config.scenario_digest;
    frame.config = config;
    frame.config_digest = lockstep_config_digest(config);
    return frame;
}

struct BrowserEvent {
    std::string relay;
    std::string event_id;
    std::string pubkey;
    int kind{};
    std::uint64_t created_at{};
    std::vector<std::vector<std::string>> tags;
    std::string content;
};

BrowserEvent parse_browser_event(const std::string& json) {
    if (json.empty() || json.size() > nostr_bridge_max_message_bytes) {
        throw std::runtime_error("Nostr event envelope exceeds bridge bound");
    }
    const JsonValue parsed = JsonParser(json).parse();
    const auto& object = parsed.object();
    BrowserEvent event;
    event.relay = string_field(object, "relay", 512);
    event.event_id = string_field(object, "event_id", 64);
    event.pubkey = string_field(object, "pubkey", 64);
    const std::uint64_t kind = number_field(object, "kind");
    if (kind > static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
        throw std::runtime_error("Nostr event kind out of range");
    }
    event.kind = static_cast<int>(kind);
    event.created_at = number_field(object, "created_at");
    event.content = string_field(object, "content", maximum_event_content_bytes);
    const auto& tags = required(object, "tags").array();
    if (tags.size() > maximum_event_tags) {
        throw std::runtime_error("Nostr event tag count exceeds bound");
    }
    for (const JsonValue& tag_value : tags) {
        const auto& parts = tag_value.array();
        if (parts.size() < 2 || parts.size() > maximum_tag_parts) {
            throw std::runtime_error("invalid Nostr event tag");
        }
        std::vector<std::string> tag;
        for (const JsonValue& part : parts) {
            const std::string& string = part.string();
            if (string.size() > maximum_tag_part_bytes) {
                throw std::runtime_error("Nostr tag part exceeds bound");
            }
            tag.push_back(string);
        }
        event.tags.push_back(std::move(tag));
    }
    if (!hex64(event.event_id) || !hex64(event.pubkey) ||
        (event.kind != nostr_lobby_event_kind &&
         event.kind != nostr_match_event_kind)) {
        throw std::runtime_error("invalid Nostr application event envelope");
    }
    return event;
}

bool has_tag(
    const BrowserEvent& event,
    std::string_view name,
    std::string_view value
) {
    return std::ranges::any_of(event.tags, [&](const auto& tag) {
        return tag.size() >= 2 && tag[0] == name && tag[1] == value;
    });
}

}  // namespace

class NostrMultiplayerRuntime::Impl {
public:
    explicit Impl(MultiplayerLaunchConfig launch)
        : config_(std::move(launch.session)),
          match_reference_(std::move(launch.match_reference)),
          hosting_(launch.hosting),
          local_slot_(launch.hosting ? Player::blue : Player::red),
          one_relay_development_(launch.one_relay_development),
          relays_(std::move(launch.relays)),
          last_peer_traffic_(std::chrono::steady_clock::now()),
          last_peer_probe_at_(last_peer_traffic_) {
        if (config_.input_delay_ticks < 3) config_.input_delay_ticks = 3;
        config_.host_slot = *PlayerSlotId::from_index(0);
        if (relays_.empty() && hosting_) {
            relays_ = default_public_relays();
        }
        std::ostringstream initialize;
        initialize << "{\"role\":"
                   << json_string(hosting_ ? "host" : "join")
                   << ",\"relays\":" << json_string_array(relays_)
                   << ",\"compatibility_digest\":"
                   << json_string(compatibility_digest(config_))
                   << ",\"one_relay_development\":"
                   << (one_relay_development_ ? "true" : "false");
        if (!match_reference_.empty()) {
            initialize << ",\"match_reference\":"
                       << json_string(match_reference_);
        }
        initialize << '}';
        if (!nostr_bridge_initialize(initialize.str())) {
            reliability_status_ = MultiplayerReliabilityStatus::suspended;
            reliability_reason_ = MultiplayerReliabilityReason::transport_lost;
            failure_ = "Applesauce browser runtime unavailable";
        }
    }

    ~Impl() { disconnect(); }

    bool queue_command(GameCommand command) {
        if (local_controller_state_ != PlayerControllerState::active ||
            !session_ || session_->status() != LockstepStatus::running ||
            pending_commands_.size() >= 256) {
            return false;
        }
        pending_commands_.push_back(std::move(command));
        return true;
    }

    void poll_transport(Simulation& simulation) {
        if (disconnected_) return;
        for (const std::string& json : drain_nostr_bridge_statuses()) {
            try {
                handle_status(json, simulation);
            } catch (const std::exception& error) {
                suspend(
                    MultiplayerReliabilityReason::protocol_conflict,
                    std::string{"invalid bridge status: "} + error.what()
                );
            }
        }
        for (const std::string& json : drain_nostr_bridge_publish_results()) {
            try {
                handle_publish_result(json);
            } catch (const std::exception& error) {
                suspend(
                    MultiplayerReliabilityReason::protocol_conflict,
                    std::string{"invalid publish result: "} + error.what()
                );
            }
        }
        for (const std::string& json : drain_nostr_bridge_events()) {
            try {
                handle_event(parse_browser_event(json), simulation);
            } catch (const std::exception& error) {
                suspend(
                    MultiplayerReliabilityReason::protocol_conflict,
                    std::string{"invalid match event: "} + error.what()
                );
            }
        }
        maybe_apply_start(simulation);
        maybe_publish_initial_lobby();
        maybe_publish_prerequisites();
        maybe_start();
        update_reliability();
        update_browser_diagnostics(simulation);
    }

    void pump(Simulation& simulation) {
        poll_transport(simulation);
        if (disconnected_) return;
        if (simulation.controller_state(local_slot_) !=
                PlayerControllerState::active) {
            local_controller_state_ = PlayerControllerState::observer;
            pending_commands_.clear();
        }
        if (simulation.outcome() != MatchOutcome::ongoing) {
            pending_commands_.clear();
            publish_terminal_result(simulation);
            return;
        }
        apply_control_if_due();
        if (!session_ || session_->status() != LockstepStatus::running ||
            reliability_status_ != MultiplayerReliabilityStatus::active ||
            paused_ || control_waiting()) {
            return;
        }
        if (save_barrier_.should_pause(session_->current_tick())) {
            submit_save_hash(simulation);
            return;
        }
        schedule_turns(simulation);
        if (LockstepRuntimeCoordinator::advance(*session_, simulation)) {
            last_advanced_at_ = std::chrono::steady_clock::now();
        }
        publish_terminal_result(simulation);
    }

    void disconnect() {
        if (disconnected_) return;
        disconnected_ = true;
        reliability_status_ = MultiplayerReliabilityStatus::disconnected;
        reliability_reason_ = MultiplayerReliabilityReason::peer_disconnected;
        nostr_bridge_shutdown();
    }

    bool send_chat(std::string text, ChatAudience audience) {
        if (text.empty() || text.size() > 4096 ||
            (!session_ && current_lobby_event_id_.empty())) return false;
        const std::uint64_t sequence = next_chat_sequence_++;
        std::ostringstream content;
        content << base_content("chat")
                << ",\"sender_slot\":" << slot_number(local_slot_)
                << ",\"sequence\":" << sequence
                << ",\"audience\":"
                << json_string(audience == ChatAudience::all ? "all" : "allies")
                << ",\"text\":" << json_string(text) << '}';
        return publish("chat", content.str(), false, false);
    }

    bool send_signal(TilePosition tile, ChatAudience audience) {
        if (tile.x < 0 || tile.y < 0 || tile.x > 32767 || tile.y > 32767 ||
            (!session_ && current_lobby_event_id_.empty())) return false;
        const std::uint64_t sequence = next_signal_sequence_++;
        std::ostringstream content;
        content << base_content("signal")
                << ",\"sender_slot\":" << slot_number(local_slot_)
                << ",\"sequence\":" << sequence
                << ",\"audience\":"
                << json_string(audience == ChatAudience::all ? "all" : "allies")
                << ",\"x\":" << tile.x << ",\"y\":" << tile.y << '}';
        return publish("signal", content.str(), false, false);
    }

    bool request_save_barrier(std::uint64_t target_tick) {
        if (!hosting_ || !session_ ||
            session_->status() != LockstepStatus::running ||
            target_tick < session_->current_tick()) return false;
        std::ostringstream content;
        content << base_content("save_barrier")
                << ",\"target_tick\":" << target_tick << '}';
        return publish("save_barrier", content.str(), false, true);
    }

    bool propose_pause(bool paused, std::uint64_t barrier_tick) {
        return propose_control(
            paused ? SessionControlKind::pause : SessionControlKind::resume,
            game_speed_, barrier_tick
        );
    }

    bool propose_speed(GameSpeed speed, std::uint64_t barrier_tick) {
        return propose_control(SessionControlKind::speed, speed, barrier_tick);
    }

    bool drop_peer() {
        if (!hosting_ || !session_ || drop_proposal_pending_) return false;
        drop_proposal_pending_ = true;
        drop_proposal_id_ = next_control_id_++;
        std::ostringstream content;
        content << base_content("drop")
                << ",\"stage\":\"proposal\",\"proposal_id\":"
                << drop_proposal_id_ << '}';
        if (publish("drop", content.str(), false, true)) return true;
        drop_proposal_pending_ = false;
        drop_proposal_id_ = 0;
        return false;
    }

    void set_ready(bool ready) {
        ready_requested_ = ready;
        if (!ready) local_ready_sent_for_.clear();
        maybe_publish_prerequisites();
    }

    void request_start() {
        start_requested_ = true;
        maybe_start();
    }

    [[nodiscard]] LockstepStatus status() const {
        if (session_) return session_->status();
        return LockstepStatus::handshaking;
    }

    [[nodiscard]] bool connected() const {
        return initialized_ && usable_relay_count() >= quorum_;
    }

    [[nodiscard]] std::uint64_t current_tick() const {
        return session_ ? session_->current_tick() : 0;
    }

    [[nodiscard]] bool waiting_for_turn() const {
        return reliability_status_ == MultiplayerReliabilityStatus::waiting ||
            (session_ && session_->status() == LockstepStatus::running &&
             last_advanced_at_ != std::chrono::steady_clock::time_point{} &&
             std::chrono::steady_clock::now() - last_advanced_at_ >=
                std::chrono::seconds(1));
    }

    [[nodiscard]] bool peer_ready(Player player) const {
        return ready_event_ids_[slot_number(player)].has_value();
    }

    [[nodiscard]] NetworkTimingMetrics network_metrics() const {
        NetworkTimingMetrics metrics;
        const auto now = std::chrono::steady_clock::now();
        if (now >= last_peer_traffic_) {
            metrics.milliseconds_since_peer_traffic =
                static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        now - last_peer_traffic_
                    ).count()
                );
        }
        metrics.waiting = waiting_for_turn();
        return metrics;
    }

    [[nodiscard]] int effective_tick_cadence_ms() const {
        const int base = config_.tick_cadence_ms;
        return game_speed_ == GameSpeed::slow ? base * 2 :
            game_speed_ == GameSpeed::fast ? std::max(base / 2, 1) : base;
    }

    [[nodiscard]] std::string transport_detail() const {
        std::ostringstream output;
        output << usable_relay_count() << '/' << relays_.size()
               << " RELAYS; QUORUM " << quorum_;
        if (!failure_.empty()) output << "; " << failure_;
        return output.str();
    }

    void update_browser_diagnostics(const Simulation& simulation) const {
        const Unit* blue_villager = nullptr;
        std::ostringstream blue_villagers_json;
        blue_villagers_json << '[';
        for (const Unit& unit : simulation.units()) {
            if (unit.owner == Player::blue &&
                unit.kind == UnitKind::villager && unit.hit_points > 0) {
                if (!blue_villager) blue_villager = &unit;
                if (blue_villagers_json.tellp() > 1) blue_villagers_json << ',';
                blue_villagers_json
                    << "{\"id\":" << unit.id
                    << ",\"x\":" << unit.position.x
                    << ",\"y\":" << unit.position.y << '}';
            }
        }
        blue_villagers_json << ']';
        std::ostringstream units_json;
        units_json << '[';
        for (const Unit& unit : simulation.units()) {
            if (units_json.tellp() > 1) units_json << ',';
            units_json << "{\"id\":" << unit.id
                       << ",\"kind\":" << static_cast<int>(unit.kind)
                       << ",\"owner\":"
                       << static_cast<int>(unit.owner.stable_id())
                       << ",\"x\":" << unit.position.x
                       << ",\"y\":" << unit.position.y
                       << ",\"destinationX\":" << unit.destination.x
                       << ",\"destinationY\":" << unit.destination.y
                       << ",\"hitPoints\":" << unit.hit_points
                       << ",\"moving\":"
                       << (unit.moving ? "true" : "false")
                       << ",\"animationState\":"
                       << static_cast<int>(unit.animation_state)
                       << ",\"facing\":"
                       << static_cast<int>(unit.facing)
                       << ",\"attackTargetId\":"
                       << unit.attack_target_id
                       << ",\"repairTargetId\":"
                       << unit.repair_target_id
                       << ",\"formationGroupId\":"
                       << unit.formation_group_id
                       << ",\"formationAnchorX\":"
                       << unit.formation_anchor.x
                       << ",\"formationAnchorY\":"
                       << unit.formation_anchor.y
                       << ",\"formationSlotX\":"
                       << unit.formation_slot.x
                       << ",\"formationSlotY\":"
                       << unit.formation_slot.y
                       << ",\"formationWaypointCount\":"
                       << unit.formation_waypoints.size()
                       << ",\"waypointCount\":" << unit.waypoints.size()
                       << ",\"patrolling\":"
                       << (unit.patrolling ? "true" : "false") << '}';
        }
        units_json << ']';
        std::ostringstream buildings_json;
        buildings_json << '[';
        for (const Building& building : simulation.buildings()) {
            if (buildings_json.tellp() > 1) buildings_json << ',';
            buildings_json
                << "{\"id\":" << building.id
                << ",\"kind\":" << static_cast<int>(building.kind)
                << ",\"owner\":"
                << static_cast<int>(building.owner.stable_id())
                << ",\"x\":" << building.position.x
                << ",\"y\":" << building.position.y
                << ",\"hitPoints\":" << building.hit_points
                << ",\"constructionTicksRemaining\":"
                << building.construction_ticks_remaining
                << ",\"productionQueueSize\":"
                << building.production_queue.size()
                << ",\"technologyResearchTicksRemaining\":"
                << building.technology_research_ticks_remaining << '}';
        }
        buildings_json << ']';
        const Economy& blue_economy = simulation.economy(Player::blue);
        const Economy& red_economy = simulation.economy(Player::red);
        std::ostringstream output;
        output << "{\"currentTick\":" << current_tick()
               << ",\"protocolVersion\":" << nostr_match_protocol_version
               << ",\"epoch\":" << match_epoch
               << ",\"localSlot\":" << slot_number(local_slot_)
               << ",\"hostPublicKey\":"
               << json_string(host_public_key_)
               << ",\"bluePublicKey\":"
               << json_string(config_.blue.peer_id)
               << ",\"redPublicKey\":"
               << json_string(config_.red.peer_id)
               << ",\"configDigest\":"
               << json_string(lockstep_config_digest(config_))
               << ",\"stateHash\":\""
               << deterministic_state_hash(simulation) << '"'
               << ",\"scenarioDigest\":"
               << json_string(config_.scenario_digest)
               << ",\"tickCadenceMs\":"
               << effective_tick_cadence_ms()
               << ",\"waiting\":"
               << (waiting_for_turn() ? "true" : "false")
               << ",\"reliabilityStatus\":"
               << static_cast<int>(reliability_status_)
               << ",\"reliabilityReason\":"
               << static_cast<int>(reliability_reason_)
               << ",\"usableRelayCount\":" << usable_relay_count()
               << ",\"eoseRelayCount\":" << eose_relay_count()
               << ",\"lobbyRevision\":" << accepted_lobby_revision_
               << ",\"lobbyEventId\":"
               << json_string(accepted_lobby_event_id_)
               << ",\"blueAckEventId\":"
               << json_string(
                      ack_event_ids_[0].value_or(std::string{})
                  )
               << ",\"redAckEventId\":"
               << json_string(
                      ack_event_ids_[1].value_or(std::string{})
                  )
               << ",\"blueReadyEventId\":"
               << json_string(
                      ready_event_ids_[0].value_or(std::string{})
                  )
               << ",\"redReadyEventId\":"
               << json_string(
                      ready_event_ids_[1].value_or(std::string{})
                  )
               << ",\"startRequested\":"
               << (start_requested_ ? "true" : "false")
               << ",\"startSent\":" << (start_sent_ ? "true" : "false")
               << ",\"blueAckObserved\":"
               << (ack_event_ids_[0]
                       ? observed_count(*ack_event_ids_[0]) : 0)
               << ",\"redAckObserved\":"
               << (ack_event_ids_[1]
                       ? observed_count(*ack_event_ids_[1]) : 0)
               << ",\"blueReadyObserved\":"
               << (ready_event_ids_[0]
                       ? observed_count(*ready_event_ids_[0]) : 0)
               << ",\"redReadyObserved\":"
               << (ready_event_ids_[1]
                       ? observed_count(*ready_event_ids_[1]) : 0)
               << ",\"sessionStatus\":" << static_cast<int>(status())
               << ",\"blueReady\":"
               << (peer_ready(Player::blue) ? "true" : "false")
               << ",\"redReady\":"
               << (peer_ready(Player::red) ? "true" : "false")
               << ",\"blueContiguous\":"
               << sender_streams_[0].highest_contiguous_sequence()
               << ",\"redContiguous\":"
               << sender_streams_[1].highest_contiguous_sequence()
               << ",\"blueMissing\":" << missing_json(sender_streams_[0])
               << ",\"redMissing\":" << missing_json(sender_streams_[1])
               << ",\"chatCount\":" << chat_log_.size()
               << ",\"signalCount\":" << signal_log_.size()
               << ",\"paused\":" << (paused_ ? "true" : "false")
               << ",\"gameSpeed\":" << static_cast<int>(game_speed_)
               << ",\"stateHashStatus\":"
               << static_cast<int>(save_barrier_.status())
               << ",\"outcome\":"
               << static_cast<int>(simulation.outcome())
               << ",\"blueVillagerId\":"
               << (blue_villager ? blue_villager->id : 0)
               << ",\"blueVillagerX\":"
               << (blue_villager ? blue_villager->position.x : -1)
               << ",\"blueVillagerY\":"
               << (blue_villager ? blue_villager->position.y : -1)
               << ",\"blueVillagers\":" << blue_villagers_json.str()
               << ",\"units\":" << units_json.str()
               << ",\"buildings\":" << buildings_json.str()
               << ",\"blueEconomy\":{\"wood\":" << blue_economy.wood
               << ",\"food\":" << blue_economy.food
               << ",\"gold\":" << blue_economy.gold
               << ",\"stone\":" << blue_economy.stone << "}"
               << ",\"redEconomy\":{\"wood\":" << red_economy.wood
               << ",\"food\":" << red_economy.food
               << ",\"gold\":" << red_economy.gold
               << ",\"stone\":" << red_economy.stone << "}"
               << ",\"bluePopulation\":"
               << simulation.population(Player::blue)
               << ",\"redPopulation\":"
               << simulation.population(Player::red)
               << ",\"blueControllerState\":"
               << static_cast<int>(
                      simulation.controller_state(Player::blue)
                  )
               << ",\"redControllerState\":"
               << static_cast<int>(
                      simulation.controller_state(Player::red)
                  )
               << ",\"resultCount\":"
               << static_cast<int>(result_fingerprints_[0].has_value()) +
                      static_cast<int>(result_fingerprints_[1].has_value())
               << ",\"terminalResultAgreement\":"
               << (result_fingerprints_[0] && result_fingerprints_[1] &&
                           result_fingerprints_[0] == result_fingerprints_[1]
                       ? "true" : "false")
               << ",\"terminalStateHash\":"
               << json_string(
                      simulation.outcome() == MatchOutcome::ongoing
                          ? std::string{}
                          : deterministic_state_hash(simulation)
                  ) << '}';
        (void)nostr_bridge_update_diagnostics(output.str());
    }

    LockstepSessionConfig config_;
    std::vector<LockstepChatMessage> chat_log_;
    std::vector<LockstepMapSignal> signal_log_;
    LockstepSaveBarrier save_barrier_;
    PlayerControllerState local_controller_state_{PlayerControllerState::active};
    bool paused_{};
    GameSpeed game_speed_{GameSpeed::normal};
    MultiplayerReliabilityStatus reliability_status_{
        MultiplayerReliabilityStatus::waiting
    };
    MultiplayerReliabilityReason reliability_reason_{
        MultiplayerReliabilityReason::backfill_incomplete
    };
    std::string match_reference_;
    std::string public_key_;

private:
    enum class IntentKind {
        lobby,
        join,
        acknowledge,
        ready,
        start,
        turn,
        other,
    };

    struct IntentState {
        IntentKind kind{IntentKind::other};
        std::uint64_t sender_sequence{};
    };

    struct RelayState {
        bool connected{};
        bool ready{};
        bool eose{};
        bool auth_required{};
    };

    struct PendingControl {
        std::uint64_t id{};
        std::uint64_t barrier_tick{};
        SessionControlKind kind{SessionControlKind::pause};
        GameSpeed speed{GameSpeed::normal};
        bool acknowledged{};
        bool committed{};
    };

    std::string base_content(std::string_view family) const {
        std::ostringstream content;
        content << "{\"protocol\":" << nostr_match_protocol_version
                << ",\"family\":" << json_string(family)
                << ",\"match_id\":" << json_string(match_id_)
                << ",\"epoch\":" << match_epoch;
        return content.str();
    }

    std::string next_intent_id(std::string_view family) {
        return std::string{family} + '-' + std::to_string(next_intent_id_++);
    }

    bool publish(
        std::string_view family,
        const std::string& content,
        bool lobby,
        bool cache,
        IntentKind kind = IntentKind::other,
        std::uint64_t sender_sequence = 0
    ) {
        if (!initialized_ || match_id_.empty() || content.empty() ||
            content.size() > maximum_event_content_bytes) return false;
        const std::string intent_id = next_intent_id(family);
        std::vector<std::vector<std::string>> tags{
            {"m", match_id_},
            {"t", std::string{application_tag}},
            {"v", std::to_string(nostr_match_protocol_version)},
        };
        if (lobby) {
            tags.push_back({"d", match_id_});
            tags.push_back({"expiration", std::to_string(lobby_expires_at_)});
        }
        if (!current_lobby_event_id_.empty() && !lobby) {
            tags.push_back({"e", current_lobby_event_id_});
        }
        if (!config_.blue.peer_id.empty()) tags.push_back({"p", config_.blue.peer_id});
        if (!config_.red.peer_id.empty() && config_.red.peer_id != "open") {
            tags.push_back({"p", config_.red.peer_id});
        }
        std::ostringstream json;
        json << "{\"intent_id\":" << json_string(intent_id)
             << ",\"kind\":"
             << (lobby ? nostr_lobby_event_kind : nostr_match_event_kind)
             << ",\"tags\":[";
        for (std::size_t index = 0; index < tags.size(); ++index) {
            if (index != 0) json << ',';
            json << json_string_array(tags[index]);
        }
        json << "],\"content\":" << json_string(content)
             << ",\"cache\":" << (cache ? "true" : "false") << '}';
        pending_intents_[intent_id] = {kind, sender_sequence};
        if (!nostr_bridge_publish(json.str())) {
            pending_intents_.erase(intent_id);
            return false;
        }
        return true;
    }

    bool publish_lobby() {
        LockstepFrame hello = hello_for_config(config_, Player::blue);
        std::ostringstream content;
        content << base_content("lobby")
                << ",\"revision\":" << lobby_revision_
                << ",\"host_pubkey\":" << json_string(host_public_key_)
                << ",\"config_digest\":"
                << json_string(lockstep_config_digest(config_))
                << ",\"compatibility_digest\":"
                << json_string(compatibility_digest(config_))
                << ",\"hello_frame\":"
                << json_string(encode_lockstep_frame(hello))
                << ",\"relays\":" << json_string_array(relays_)
                << ",\"status\":"
                << json_string(
                    config_.red.peer_id == "open" ? "open" : "full"
                )
                << ",\"expires_at\":" << lobby_expires_at_
                << ",\"open\":"
                << (config_.red.peer_id == "open" ? "true" : "false")
                << '}';
        reset_lobby_prerequisites();
        return publish(
            "lobby", content.str(), true, true, IntentKind::lobby
        );
    }

    void maybe_publish_initial_lobby() {
        if (!initial_lobby_publish_pending_ || !hosting_ || !initialized_ ||
            usable_relay_count() < quorum_ || eose_relay_count() < quorum_) {
            return;
        }
        if (publish_lobby()) initial_lobby_publish_pending_ = false;
    }

    void publish_join() {
        if (hosting_ || current_lobby_event_id_.empty() ||
            join_sent_for_ == current_lobby_event_id_) return;
        std::ostringstream content;
        content << base_content("join")
                << ",\"lobby_event_id\":"
                << json_string(current_lobby_event_id_)
                << ",\"requested_slot\":1,\"compatibility_digest\":"
                << json_string(compatibility_digest(config_)) << '}';
        if (publish("join", content.str(), false, true, IntentKind::join)) {
            join_sent_for_ = current_lobby_event_id_;
        }
    }

    void maybe_publish_prerequisites() {
        if (current_lobby_event_id_.empty() ||
            config_.blue.peer_id.empty() || config_.red.peer_id.empty() ||
            config_.red.peer_id == "open" ||
            observed_count(current_lobby_event_id_) < quorum_) return;
        if (local_ack_sent_for_ != current_lobby_event_id_) {
            std::ostringstream content;
            content << base_content("acknowledge")
                    << ",\"lobby_event_id\":"
                    << json_string(current_lobby_event_id_)
                    << ",\"sender_slot\":" << slot_number(local_slot_)
                    << ",\"config_digest\":"
                    << json_string(lockstep_config_digest(config_))
                    << ",\"observed_relay_count\":"
                    << observed_count(current_lobby_event_id_) << '}';
            if (publish(
                    "ack", content.str(), false, true,
                    IntentKind::acknowledge
                )) {
                local_ack_sent_for_ = current_lobby_event_id_;
            }
        }
        if (ready_requested_ &&
            local_ready_sent_for_ != current_lobby_event_id_) {
            std::ostringstream content;
            content << base_content("ready")
                    << ",\"lobby_event_id\":"
                    << json_string(current_lobby_event_id_)
                    << ",\"sender_slot\":" << slot_number(local_slot_)
                    << '}';
            if (publish(
                    "ready", content.str(), false, true,
                    IntentKind::ready
                )) {
                local_ready_sent_for_ = current_lobby_event_id_;
            }
        }
    }

    void maybe_start() {
        if (!hosting_ || !start_requested_ || start_sent_ ||
            current_lobby_event_id_.empty() ||
            !ack_event_ids_[0] || !ack_event_ids_[1] ||
            !ready_event_ids_[0] || !ready_event_ids_[1]) return;
        for (const auto& event_id : {
                 *ack_event_ids_[0], *ack_event_ids_[1],
                 *ready_event_ids_[0], *ready_event_ids_[1]}) {
            if (observed_count(event_id) < quorum_) return;
        }
        std::ostringstream content;
        content << base_content("start")
                << ",\"lobby_event_id\":"
                << json_string(current_lobby_event_id_)
                << ",\"blue_ready_id\":"
                << json_string(*ready_event_ids_[0])
                << ",\"red_ready_id\":"
                << json_string(*ready_event_ids_[1])
                << ",\"tick_zero\":0}";
        if (publish("start", content.str(), false, true, IntentKind::start)) {
            start_sent_ = true;
        }
    }

    void handle_status(const std::string& json, Simulation& simulation) {
        const JsonValue parsed = JsonParser(json).parse();
        const auto& object = parsed.object();
        const std::string type = string_field(object, "type", 64);
        if (type == "initialized") {
            public_key_ = string_field(object, "pubkey", 64);
            host_public_key_ = string_field(object, "host_pubkey", 64);
            match_id_ = string_field(object, "match_id", 64);
            match_reference_ = string_field(object, "match_reference", 4096);
            relays_ = string_array_field(object, "relays", 20, 512);
            quorum_ = static_cast<std::size_t>(number_field(object, "quorum"));
            if (!hex64(public_key_) || !hex64(host_public_key_) ||
                !hex64(match_id_) || quorum_ < 1 || quorum_ > relays_.size()) {
                throw std::runtime_error("invalid initialized identity");
            }
            initialized_ = true;
            if (hosting_) {
                if (public_key_ != host_public_key_) {
                    throw std::runtime_error("host signer identity mismatch");
                }
                config_.blue.peer_id = public_key_;
                config_.red.peer_id = "open";
                lobby_expires_at_ = unix_time_seconds() +
                    lobby_lifetime_seconds;
                lobby_revision_ = 1;
                initial_lobby_publish_pending_ = true;
            }
            return;
        }
        if (type == "fatal") {
            suspend(
                MultiplayerReliabilityReason::transport_lost,
                string_field(object, "message", 4096)
            );
            return;
        }
        if (type == "relay") {
            const std::string relay = string_field(object, "relay", 512);
            RelayState& state = relay_states_[relay];
            state.connected = bool_field(object, "connected");
            state.ready = bool_field(object, "ready");
            state.auth_required = bool_field(object, "auth_required");
            if (!state.connected) state.eose = false;
            update_reliability();
            return;
        }
        if (type == "relay_disabled") {
            const std::string relay = string_field(object, "relay", 512);
            RelayState& state = relay_states_[relay];
            state.connected = false;
            state.ready = false;
            state.eose = false;
            state.auth_required = optional_string_field(
                object, "reason", 128
            ) == "authentication required";
            update_reliability();
            return;
        }
        if (type == "relay_enabled") {
            const std::string relay = string_field(object, "relay", 512);
            RelayState& state = relay_states_[relay];
            state.connected = false;
            state.ready = false;
            state.eose = false;
            state.auth_required = false;
            update_reliability();
            return;
        }
        if (type == "backfill_open") {
            relay_states_[string_field(object, "relay", 512)].eose = false;
            update_reliability();
            return;
        }
        if (type == "eose") {
            relay_states_[string_field(object, "relay", 512)].eose = true;
            republish_failed_turns();
            update_reliability();
            return;
        }
        if (type == "event_observed") {
            const std::string relay = string_field(object, "relay", 512);
            const std::string event_id = string_field(object, "event_id", 64);
            if (!hex64(event_id)) throw std::runtime_error("invalid observed event ID");
            observations_[event_id].insert(relay);
            maybe_publish_prerequisites();
            maybe_start();
            maybe_apply_start(simulation);
            return;
        }
        if (type == "subscription_closed" ||
            type == "subscription_error" || type == "relay_status_error") {
            failure_ = optional_string_field(object, "message", 4096);
            if (failure_.empty()) failure_ = optional_string_field(object, "reason", 4096);
            update_reliability();
            return;
        }
        if (type == "republish_unavailable") {
            suspend(
                MultiplayerReliabilityReason::backfill_incomplete,
                "exact signed event unavailable for republication"
            );
        }
    }

    void handle_publish_result(const std::string& json) {
        const JsonValue parsed = JsonParser(json).parse();
        const auto& object = parsed.object();
        const std::string intent_id = string_field(object, "intent_id", 128);
        const bool ok = bool_field(object, "ok");
        const std::string event_id = optional_string_field(object, "event_id", 64);
        constexpr std::string_view republish_prefix{"republish:"};
        if (intent_id.starts_with(republish_prefix)) {
            const std::string requested_event_id = intent_id.substr(
                republish_prefix.size()
            );
            if (!hex64(requested_event_id) || event_id != requested_event_id) {
                throw std::runtime_error("invalid republication result");
            }
            turn_republish_outstanding_ids_.erase(event_id);
            if (ok) failed_turn_event_ids_.erase(event_id);
            update_reliability();
            return;
        }
        const auto found = pending_intents_.find(intent_id);
        if (found == pending_intents_.end()) return;
        const IntentState intent = found->second;
        pending_intents_.erase(found);
        if (!ok || !hex64(event_id)) {
            if (intent.kind == IntentKind::turn) {
                turn_publish_outstanding_ = false;
                if (hex64(event_id)) {
                    last_outbound_turn_event_id_ = event_id;
                    outbound_turn_event_ids_[intent.sender_sequence] = event_id;
                    failed_turn_event_ids_.insert(event_id);
                }
            }
            suspend(
                MultiplayerReliabilityReason::relay_quorum_lost,
                "relay publication failed for " + intent_id
            );
            return;
        }
        switch (intent.kind) {
            case IntentKind::lobby:
                current_lobby_event_id_ = event_id;
                break;
            case IntentKind::turn:
                last_outbound_turn_event_id_ = event_id;
                outbound_turn_event_ids_[intent.sender_sequence] = event_id;
                while (outbound_turn_event_ids_.size() >
                        nostr_max_future_sender_sequences * 2) {
                    outbound_turn_event_ids_.erase(
                        outbound_turn_event_ids_.begin()
                    );
                }
                turn_publish_outstanding_ = false;
                break;
            default: break;
        }
    }

    void validate_scope(const BrowserEvent& event) const {
        if (match_id_.empty() || !has_tag(event, "m", match_id_) ||
            !has_tag(event, "t", application_tag) ||
            !has_tag(event, "v", std::to_string(nostr_match_protocol_version))) {
            throw std::runtime_error("event outside match scope");
        }
        if (event.kind == nostr_lobby_event_kind &&
            !has_tag(event, "d", match_id_)) {
            throw std::runtime_error("lobby address mismatch");
        }
    }

    void handle_event(BrowserEvent event, Simulation& simulation) {
        validate_scope(event);
        const JsonValue parsed = JsonParser(event.content).parse();
        const auto& content = parsed.object();
        if (number_field(content, "protocol") != nostr_match_protocol_version ||
            string_field(content, "match_id", 64) != match_id_ ||
            number_field(content, "epoch") != match_epoch) {
            throw std::runtime_error("event content scope mismatch");
        }
        const std::string family = string_field(content, "family", 32);
        if ((family == "acknowledge" || family == "ready" ||
             family == "start") &&
            string_field(content, "lobby_event_id", 64) !=
                current_lobby_event_id_) {
            if (retired_lobby_event_ids_.contains(
                    string_field(content, "lobby_event_id", 64))) return;
            defer_handshake_event(std::move(event));
            return;
        }
        if (family == "turn_batch" && !session_) {
            defer_turn_event(std::move(event));
            return;
        }
        if (family == "lobby") {
            handle_lobby(event, content);
            replay_deferred_handshake(simulation);
        }
        else if (family == "join") handle_join(event, content);
        else if (family == "acknowledge") handle_acknowledge(event, content);
        else if (family == "ready") handle_ready(event, content);
        else if (family == "start") handle_start(event, content, simulation);
        else if (family == "turn_batch") handle_turn(event, content, simulation);
        else if (family == "chat") handle_chat(event, content);
        else if (family == "signal") handle_signal(event, content);
        else if (family == "control") handle_control(event, content);
        else if (family == "save_barrier") handle_save_barrier(event, content);
        else if (family == "save_hash") handle_save_hash(event, content);
        else if (family == "drop") handle_drop(event, content);
        else if (family == "receipt") handle_receipt(event, content);
        else if (family == "result") {
            handle_result(event, content);
        }
        else if (family == "checkpoint") {
            // Public diagnostic/agreement events have no direct simulation
            // mutation. Authoritative turns still use LockstepSession::receive.
        } else {
            throw std::runtime_error("unknown match event family");
        }
    }

    void defer_handshake_event(BrowserEvent event) {
        if (deferred_handshake_events_.size() >= 256) {
            throw std::runtime_error("deferred handshake event bound exceeded");
        }
        if (std::ranges::none_of(deferred_handshake_events_, [&](const auto& queued) {
                return queued.event_id == event.event_id;
            })) {
            deferred_handshake_events_.push_back(std::move(event));
        }
    }

    void replay_deferred_handshake(Simulation& simulation) {
        if (replaying_deferred_handshake_ || current_lobby_event_id_.empty()) return;
        replaying_deferred_handshake_ = true;
        std::vector<BrowserEvent> queued =
            std::move(deferred_handshake_events_);
        deferred_handshake_events_.clear();
        for (BrowserEvent& event : queued) {
            handle_event(std::move(event), simulation);
        }
        replaying_deferred_handshake_ = false;
    }

    void defer_turn_event(BrowserEvent event) {
        if (prestart_turn_events_.size() >=
                nostr_max_future_sender_sequences * 2) {
            throw std::runtime_error("pre-start turn event bound exceeded");
        }
        if (std::ranges::none_of(prestart_turn_events_, [&](const auto& queued) {
                return queued.event_id == event.event_id;
            })) {
            prestart_turn_events_.push_back(std::move(event));
        }
    }

    void handle_lobby(
        const BrowserEvent& event,
        const JsonValue::Object& content
    ) {
        if (event.kind != nostr_lobby_event_kind ||
            event.pubkey != host_public_key_ ||
            string_field(content, "host_pubkey", 64) != host_public_key_) {
            throw std::runtime_error("invalid lobby author");
        }
        const std::uint64_t now = unix_time_seconds();
        const std::uint64_t expires_at = number_field(content, "expires_at");
        const std::string lobby_status = string_field(content, "status", 16);
        if (event.created_at > now + 300 || expires_at <= now ||
            expires_at < event.created_at ||
            expires_at - event.created_at > lobby_lifetime_seconds + 300 ||
            (lobby_status != "open" && lobby_status != "full")) {
            throw std::runtime_error("expired or invalid lobby lifetime");
        }
        const std::uint64_t revision = number_field(content, "revision");
        if (revision < accepted_lobby_revision_) return;
        if (revision == accepted_lobby_revision_ &&
            !accepted_lobby_event_id_.empty() &&
            accepted_lobby_event_id_ != event.event_id) {
            throw std::runtime_error("conflicting lobby revision");
        }
        LockstepFrame hello = decode_lockstep_frame(
            string_field(content, "hello_frame", maximum_event_content_bytes)
        );
        if (!hello.config || hello.kind != LockstepFrameKind::hello ||
            hello.player != Player::blue ||
            string_field(content, "config_digest", 128) !=
                lockstep_config_digest(*hello.config) ||
            string_field(content, "compatibility_digest", 128) !=
                compatibility_digest(*hello.config) ||
            !compatible_configuration(config_, *hello.config)) {
            throw std::runtime_error("lobby compatibility mismatch");
        }
        const bool open = bool_field(content, "open");
        if (open != (hello.config->red.peer_id == "open") ||
            (lobby_status == "open") != open) {
            throw std::runtime_error("lobby status does not match roster");
        }
        const std::vector<std::string> lobby_relays = string_array_field(
            content, "relays", 20, 512
        );
        if (lobby_relays != relays_) {
            throw std::runtime_error("lobby relay set mismatch");
        }
        if (!current_lobby_event_id_.empty() &&
            current_lobby_event_id_ != event.event_id) {
            retired_lobby_event_ids_.insert(current_lobby_event_id_);
        }
        accepted_lobby_revision_ = revision;
        accepted_lobby_event_id_ = event.event_id;
        current_lobby_event_id_ = event.event_id;
        config_ = *hello.config;
        lobby_expires_at_ = expires_at;
        reset_lobby_prerequisites();
        if (!hosting_) {
            if (config_.red.peer_id == public_key_) {
                maybe_publish_prerequisites();
            } else if (bool_field(content, "open")) {
                publish_join();
            } else {
                throw std::runtime_error("local signer not accepted in lobby");
            }
        }
    }

    void handle_join(
        const BrowserEvent& event,
        const JsonValue::Object& content
    ) {
        if (!hosting_ || event.kind != nostr_match_event_kind ||
            config_.red.peer_id != "open" || event.pubkey == public_key_ ||
            string_field(content, "lobby_event_id", 64) !=
                current_lobby_event_id_ ||
            number_field(content, "requested_slot") != 1 ||
            string_field(content, "compatibility_digest", 128) !=
                compatibility_digest(config_)) {
            return;
        }
        config_.red.peer_id = event.pubkey;
        ++lobby_revision_;
        publish_lobby();
    }

    std::optional<Player> slot_for_pubkey(std::string_view pubkey) const {
        if (pubkey == config_.blue.peer_id) return Player::blue;
        if (pubkey == config_.red.peer_id) return Player::red;
        return std::nullopt;
    }

    Player validated_sender(
        const BrowserEvent& event,
        const JsonValue::Object& content
    ) const {
        const auto player = slot_for_pubkey(event.pubkey);
        if (!player || number_field(content, "sender_slot") !=
                static_cast<std::uint64_t>(slot_number(*player))) {
            throw std::runtime_error("event author/slot mismatch");
        }
        return *player;
    }

    void handle_acknowledge(
        const BrowserEvent& event,
        const JsonValue::Object& content
    ) {
        const Player sender = validated_sender(event, content);
        if (string_field(content, "lobby_event_id", 64) !=
                current_lobby_event_id_ ||
            string_field(content, "config_digest", 128) !=
                lockstep_config_digest(config_) ||
            number_field(content, "observed_relay_count") < quorum_) {
            return;
        }
        ack_event_ids_[slot_number(sender)] = event.event_id;
        maybe_start();
    }

    void handle_ready(
        const BrowserEvent& event,
        const JsonValue::Object& content
    ) {
        const Player sender = validated_sender(event, content);
        if (string_field(content, "lobby_event_id", 64) !=
                current_lobby_event_id_) {
            throw std::runtime_error("stale ready event");
        }
        ready_event_ids_[slot_number(sender)] = event.event_id;
        maybe_start();
    }

    void handle_start(
        const BrowserEvent& event,
        const JsonValue::Object& content,
        Simulation& simulation
    ) {
        if (event.pubkey != host_public_key_ ||
            string_field(content, "lobby_event_id", 64) !=
                current_lobby_event_id_ ||
            !hex64(string_field(content, "blue_ready_id", 64)) ||
            !hex64(string_field(content, "red_ready_id", 64)) ||
            number_field(content, "tick_zero") != 0) {
            throw std::runtime_error("invalid start commit");
        }
        if (pending_start_ && pending_start_->event_id != event.event_id) {
            throw std::runtime_error("conflicting start commits");
        }
        pending_start_ = event;
        maybe_apply_start(simulation);
    }

    void maybe_apply_start(Simulation& simulation) {
        if (!pending_start_ || session_ || !ready_event_ids_[0] ||
            !ready_event_ids_[1] ||
            observed_count(pending_start_->event_id) < quorum_) return;
        const JsonValue parsed = JsonParser(pending_start_->content).parse();
        const auto& content = parsed.object();
        if (string_field(content, "blue_ready_id", 64) !=
                *ready_event_ids_[0] ||
            string_field(content, "red_ready_id", 64) !=
                *ready_event_ids_[1]) {
            throw std::runtime_error("start ready references mismatch");
        }
        session_.emplace(config_, 150, 50);
        for (const Player player : {Player::blue, Player::red}) {
            const LockstepFrame hello = LockstepRuntimeCoordinator::hello(
                *session_, player
            );
            if (!LockstepRuntimeCoordinator::receive(
                    *session_, hello, simulation
                )) {
                throw std::runtime_error("start hello rejected");
            }
        }
        for (const Player player : {Player::blue, Player::red}) {
            const LockstepFrame ready = LockstepRuntimeCoordinator::control(
                *session_, player, LockstepFrameKind::ready
            );
            if (!LockstepRuntimeCoordinator::receive(
                    *session_, ready, simulation
                )) {
                throw std::runtime_error("start ready rejected");
            }
        }
        const LockstepFrame start = LockstepRuntimeCoordinator::control(
            *session_, Player::blue, LockstepFrameKind::start
        );
        if (!LockstepRuntimeCoordinator::receive(
                *session_, start, simulation
            )) {
            throw std::runtime_error("start commit rejected");
        }
        pending_start_.reset();
        next_submission_tick_ = 0;
        const auto started_at = std::chrono::steady_clock::now();
        last_peer_traffic_ = started_at;
        last_advanced_at_ = started_at;
        reliability_status_ = MultiplayerReliabilityStatus::active;
        reliability_reason_ = MultiplayerReliabilityReason::none;
        std::vector<BrowserEvent> queued = std::move(prestart_turn_events_);
        prestart_turn_events_.clear();
        for (BrowserEvent& event : queued) {
            const JsonValue turn_parsed = JsonParser(event.content).parse();
            handle_turn(event, turn_parsed.object(), simulation);
        }
    }

    void handle_turn(
        const BrowserEvent& event,
        const JsonValue::Object& content,
        Simulation& simulation
    ) {
        if (!session_ || session_->status() != LockstepStatus::running) return;
        const Player sender = validated_sender(event, content);
        if (number_field(content, "epoch") != match_epoch) {
            throw std::runtime_error("turn epoch mismatch");
        }
        const std::uint64_t sequence = number_field(content, "sender_sequence");
        const std::string previous = optional_string_field(
            content, "previous_event_id", 64
        );
        NostrSenderSequence& stream = sender_streams_[slot_number(sender)];
        const NostrSequenceAccept accepted = stream.accept({
            sequence, previous, event.event_id, event.content,
        });
        if (accepted == NostrSequenceAccept::conflict) {
            suspend(
                MultiplayerReliabilityReason::protocol_conflict,
                "conflicting logical turn input"
            );
            return;
        }
        if (accepted == NostrSequenceAccept::out_of_bounds) {
            suspend(
                MultiplayerReliabilityReason::backfill_incomplete,
                "future sender sequence exceeds bound"
            );
            return;
        }
        const NostrSequenceDrain drained = stream.drain();
        if (drained.conflict) {
            suspend(
                MultiplayerReliabilityReason::protocol_conflict,
                drained.reason
            );
            return;
        }
        for (const NostrLogicalEvent& logical : drained.contiguous) {
            const JsonValue parsed = JsonParser(logical.content).parse();
            const auto& batch = parsed.object();
            const std::vector<std::string> frames = string_array_field(
                batch, "frames", maximum_turn_frames, 1024 * 1024
            );
            if (frames.empty()) throw std::runtime_error("empty turn batch");
            const std::uint64_t first = number_field(batch, "first_tick");
            const std::uint64_t last = number_field(batch, "last_tick");
            if (last < first || last - first + 1 != frames.size() ||
                frames.size() > maximum_turn_frames) {
                throw std::runtime_error("invalid turn batch tick range");
            }
            for (std::size_t index = 0; index < frames.size(); ++index) {
                LockstepFrame frame = decode_lockstep_frame(frames[index]);
                if (frame.kind != LockstepFrameKind::turn ||
                    frame.player != sender || frame.tick != first + index ||
                    frame.commands.size() > 256 ||
                    !LockstepRuntimeCoordinator::receive(
                        *session_, frame, simulation
                    )) {
                    throw std::runtime_error("turn frame rejected by lockstep");
                }
            }
        }
        if (sender != local_slot_) {
            last_peer_traffic_ = std::chrono::steady_clock::now();
        }
        publish_receipt();
    }

    void schedule_turns(const Simulation& simulation) {
        if (!session_ || turn_publish_outstanding_) return;
        const std::uint64_t current = session_->current_tick();
        const std::uint64_t scheduled = current +
            static_cast<std::uint64_t>(config_.input_delay_ticks);
        bool checkpoint_blocked{};
        while (next_submission_tick_ <= scheduled &&
               outbound_frames_.size() < maximum_turn_frames) {
            if (next_submission_tick_ != current &&
                next_submission_tick_ % session_->hash_interval() == 0) {
                checkpoint_blocked = true;
                break;
            }
            std::vector<GameCommand> commands;
            if (next_submission_tick_ == scheduled) {
                commands = std::move(pending_commands_);
            }
            outbound_frames_.push_back(LockstepRuntimeCoordinator::turn(
                *session_, simulation, local_slot_, next_submission_tick_,
                std::move(commands)
            ));
            if (next_submission_tick_ == scheduled) pending_commands_.clear();
            ++next_submission_tick_;
        }
        if (outbound_frames_.size() >= 3 ||
            (checkpoint_blocked && !outbound_frames_.empty()) ||
            outbound_frames_.size() == maximum_turn_frames) {
            publish_turn_batch();
        }
    }

    void publish_turn_batch() {
        if (outbound_frames_.empty() || turn_publish_outstanding_) return;
        const std::uint64_t sender_sequence = next_sender_sequence_++;
        std::vector<std::string> encoded;
        encoded.reserve(outbound_frames_.size());
        for (const LockstepFrame& frame : outbound_frames_) {
            encoded.push_back(encode_lockstep_frame(frame));
        }
        std::ostringstream content;
        content << base_content("turn_batch")
                << ",\"sender_slot\":" << slot_number(local_slot_)
                << ",\"sender_sequence\":" << sender_sequence
                << ",\"previous_event_id\":"
                << json_string(last_outbound_turn_event_id_)
                << ",\"first_tick\":" << outbound_frames_.front().tick
                << ",\"last_tick\":" << outbound_frames_.back().tick
                << ",\"frames\":" << json_string_array(encoded) << '}';
        if (publish(
                "turn", content.str(), false, true,
                IntentKind::turn, sender_sequence
            )) {
            outbound_frames_.clear();
            turn_publish_outstanding_ = true;
        } else {
            --next_sender_sequence_;
        }
    }

    void publish_receipt(bool force = false) {
        if (!force && ++received_since_receipt_ < 4) return;
        received_since_receipt_ = 0;
        std::ostringstream content;
        content << base_content("receipt")
                << ",\"sender_slot\":" << slot_number(local_slot_)
                << ",\"blue_contiguous\":"
                << sender_streams_[0].highest_contiguous_sequence()
                << ",\"red_contiguous\":"
                << sender_streams_[1].highest_contiguous_sequence()
                << ",\"blue_missing\":"
                << missing_json(sender_streams_[0])
                << ",\"red_missing\":"
                << missing_json(sender_streams_[1]) << '}';
        (void)publish("receipt", content.str(), false, false);
    }

    void republish_failed_turns() {
        for (const std::string& event_id : failed_turn_event_ids_) {
            if (turn_republish_outstanding_ids_.contains(event_id)) continue;
            if (nostr_bridge_republish(event_id)) {
                turn_republish_outstanding_ids_.insert(event_id);
            }
        }
    }

    static std::string missing_json(const NostrSenderSequence& stream) {
        std::ostringstream output;
        output << '[';
        const auto ranges = stream.missing_ranges();
        for (std::size_t index = 0; index < ranges.size(); ++index) {
            if (index != 0) output << ',';
            output << '[' << ranges[index].first << ','
                   << ranges[index].second << ']';
        }
        output << ']';
        return output.str();
    }

    void handle_receipt(
        const BrowserEvent& event,
        const JsonValue::Object& content
    ) {
        const Player sender = validated_sender(event, content);
        if (sender == local_slot_) return;
        last_peer_traffic_ = std::chrono::steady_clock::now();
        (void)number_field(content, "blue_contiguous");
        (void)number_field(content, "red_contiguous");
        const auto missing = range_array_field(
            content,
            local_slot_ == Player::blue ? "blue_missing" : "red_missing"
        );
        for (const auto& [first, last] : missing) {
            for (std::uint64_t sequence = first; sequence <= last; ++sequence) {
                const auto found = outbound_turn_event_ids_.find(sequence);
                if (found == outbound_turn_event_ids_.end() ||
                    !nostr_bridge_republish(found->second)) {
                    suspend(
                        MultiplayerReliabilityReason::backfill_incomplete,
                        "missing exact signed turn event for sequence " +
                            std::to_string(sequence)
                    );
                    return;
                }
                if (sequence == last) break;
            }
        }
    }

    void handle_chat(
        const BrowserEvent& event,
        const JsonValue::Object& content
    ) {
        const Player sender = validated_sender(event, content);
        const std::uint64_t sender_sequence = number_field(content, "sequence");
        const std::uint64_t display_sequence =
            (static_cast<std::uint64_t>(slot_number(sender) + 1) << 56) |
            sender_sequence;
        if (!chat_sequences_.insert(display_sequence).second) return;
        const std::string audience = string_field(content, "audience", 16);
        const std::string text = string_field(content, "text", 4096);
        if (sender_sequence == 0 || text.empty() ||
            (audience != "all" && audience != "allies")) {
            throw std::runtime_error("invalid chat event");
        }
        const bool allied = config_.blue.team == config_.red.team;
        if (audience == "all" || allied) {
            chat_log_.push_back({
                display_sequence, sender,
                audience == "all" ? ChatAudience::all : ChatAudience::allies,
                text,
            });
            std::ranges::sort(chat_log_, {}, &LockstepChatMessage::sequence);
            if (chat_log_.size() > 128) chat_log_.erase(chat_log_.begin());
        }
    }

    void handle_signal(
        const BrowserEvent& event,
        const JsonValue::Object& content
    ) {
        const Player sender = validated_sender(event, content);
        const std::uint64_t sender_sequence = number_field(content, "sequence");
        const std::uint64_t display_sequence =
            (static_cast<std::uint64_t>(slot_number(sender) + 1) << 56) |
            sender_sequence;
        if (!signal_sequences_.insert(display_sequence).second) return;
        const std::string audience = string_field(content, "audience", 16);
        const int x = static_cast<int>(number_field(content, "x"));
        const int y = static_cast<int>(number_field(content, "y"));
        if (sender_sequence == 0 || x < 0 || y < 0 || x > 32767 || y > 32767 ||
            (audience != "all" && audience != "allies")) {
            throw std::runtime_error("invalid signal event");
        }
        const bool allied = config_.blue.team == config_.red.team;
        if (audience == "all" || allied) {
            signal_log_.push_back({
                display_sequence, sender,
                audience == "all" ? ChatAudience::all : ChatAudience::allies,
                {x, y},
            });
            std::ranges::sort(signal_log_, {}, &LockstepMapSignal::sequence);
            if (signal_log_.size() > 64) signal_log_.erase(signal_log_.begin());
        }
    }

    bool propose_control(
        SessionControlKind kind,
        GameSpeed speed,
        std::uint64_t barrier_tick
    ) {
        if (!hosting_ || !session_ || pending_control_ ||
            session_->status() != LockstepStatus::running ||
            barrier_tick < session_->current_tick()) return false;
        PendingControl pending;
        pending.id = next_control_id_++;
        pending.barrier_tick = std::max(
            barrier_tick,
            session_->current_tick() +
                static_cast<std::uint64_t>(config_.input_delay_ticks) + 1
        );
        if (paused_ && kind == SessionControlKind::resume) {
            pending.barrier_tick = session_->current_tick();
        }
        pending.kind = kind;
        pending.speed = speed;
        pending_control_ = pending;
        return publish_control_stage("proposal", pending);
    }

    bool publish_control_stage(
        std::string_view stage,
        const PendingControl& pending
    ) {
        const std::string kind =
            pending.kind == SessionControlKind::pause ? "pause" :
            pending.kind == SessionControlKind::resume ? "resume" : "speed";
        const std::string speed =
            pending.speed == GameSpeed::slow ? "slow" :
            pending.speed == GameSpeed::fast ? "fast" : "normal";
        std::ostringstream content;
        content << base_content("control")
                << ",\"stage\":" << json_string(stage)
                << ",\"proposal_id\":" << pending.id
                << ",\"barrier_tick\":" << pending.barrier_tick
                << ",\"control\":" << json_string(kind)
                << ",\"speed\":" << json_string(speed) << '}';
        return publish("control", content.str(), false, true);
    }

    PendingControl parse_control(const JsonValue::Object& content) const {
        PendingControl pending;
        pending.id = number_field(content, "proposal_id");
        pending.barrier_tick = number_field(content, "barrier_tick");
        const std::string kind = string_field(content, "control", 16);
        const std::string speed = string_field(content, "speed", 16);
        pending.kind = kind == "pause" ? SessionControlKind::pause :
            kind == "resume" ? SessionControlKind::resume :
            kind == "speed" ? SessionControlKind::speed :
            throw std::runtime_error("invalid control kind");
        pending.speed = speed == "slow" ? GameSpeed::slow :
            speed == "normal" ? GameSpeed::normal :
            speed == "fast" ? GameSpeed::fast :
            throw std::runtime_error("invalid control speed");
        if (pending.id == 0) throw std::runtime_error("invalid control ID");
        return pending;
    }

    void handle_control(
        const BrowserEvent& event,
        const JsonValue::Object& content
    ) {
        if (!session_) return;
        const std::string stage = string_field(content, "stage", 16);
        PendingControl incoming = parse_control(content);
        if (stage == "proposal") {
            if (hosting_ && event.pubkey == public_key_) return;
            if (event.pubkey != host_public_key_ || hosting_) {
                throw std::runtime_error("invalid control proposal");
            }
            if (incoming.id <= last_completed_control_id_) return;
            if (pending_control_ ||
                incoming.barrier_tick < session_->current_tick()) {
                throw std::runtime_error("invalid control proposal");
            }
            pending_control_ = incoming;
            pending_control_->acknowledged = true;
            (void)publish_control_stage("acknowledge", *pending_control_);
            return;
        }
        if (stage == "acknowledge") {
            if (!hosting_ && event.pubkey == public_key_) return;
            if (!hosting_ || event.pubkey != config_.red.peer_id) {
                throw std::runtime_error("invalid control acknowledgement");
            }
        } else if (stage == "commit") {
            if (event.pubkey != host_public_key_) {
                throw std::runtime_error("invalid control commit");
            }
        } else {
            throw std::runtime_error("invalid control stage");
        }
        if (incoming.id <= last_completed_control_id_) return;
        if (!pending_control_ || !same_control(*pending_control_, incoming)) {
            throw std::runtime_error("control event does not match proposal");
        }
        if (stage == "acknowledge") {
            pending_control_->acknowledged = true;
            pending_control_->committed = true;
            (void)publish_control_stage("commit", *pending_control_);
        } else {
            pending_control_->committed = true;
        }
        apply_control_if_due();
    }

    static bool same_control(
        const PendingControl& left,
        const PendingControl& right
    ) {
        return left.id == right.id && left.barrier_tick == right.barrier_tick &&
            left.kind == right.kind && left.speed == right.speed;
    }

    bool control_waiting() const {
        return pending_control_ && session_ &&
            session_->current_tick() >= pending_control_->barrier_tick &&
            !pending_control_->committed;
    }

    void apply_control_if_due() {
        if (!pending_control_ || !pending_control_->committed || !session_ ||
            session_->current_tick() < pending_control_->barrier_tick) return;
        if (pending_control_->kind == SessionControlKind::pause) paused_ = true;
        else if (pending_control_->kind == SessionControlKind::resume) paused_ = false;
        else game_speed_ = pending_control_->speed;
        last_completed_control_id_ = pending_control_->id;
        pending_control_.reset();
    }

    void handle_save_barrier(
        const BrowserEvent& event,
        const JsonValue::Object& content
    ) {
        if (!session_ || event.pubkey != host_public_key_ ||
            !save_barrier_.begin(
                number_field(content, "target_tick"), session_->current_tick()
            )) {
            throw std::runtime_error("invalid save barrier");
        }
        save_hash_sent_ = false;
        checkpoint_published_ = false;
    }

    void submit_save_hash(const Simulation& simulation) {
        if (save_hash_sent_ || !session_) return;
        const SaveBarrierSubmission submission{
            save_barrier_.target_tick(),
            deterministic_state_hash(simulation),
            save_barrier_.target_tick() == 0
                ? 0 : save_barrier_.target_tick() - 1,
        };
        std::ostringstream content;
        content << base_content("save_hash")
                << ",\"sender_slot\":" << slot_number(local_slot_)
                << ",\"target_tick\":" << submission.tick
                << ",\"last_bundle_sequence\":"
                << submission.last_bundle_sequence
                << ",\"state_hash\":"
                << json_string(submission.state_hash) << '}';
        if (publish("save_hash", content.str(), false, true)) {
            save_hash_sent_ = true;
        }
    }

    void handle_save_hash(
        const BrowserEvent& event,
        const JsonValue::Object& content
    ) {
        const Player sender = validated_sender(event, content);
        SaveBarrierSubmission submission{
            number_field(content, "target_tick"),
            string_field(content, "state_hash", 256),
            number_field(content, "last_bundle_sequence"),
        };
        if (!save_barrier_.submit(sender, std::move(submission))) {
            throw std::runtime_error("invalid save hash submission");
        }
        if (save_barrier_.status() == SaveBarrierStatus::matched &&
            !checkpoint_published_) {
            std::ostringstream checkpoint;
            checkpoint << base_content("checkpoint")
                       << ",\"target_tick\":"
                       << save_barrier_.target_tick()
                       << ",\"state_hash\":"
                       << json_string(save_barrier_.blue().state_hash) << '}';
            checkpoint_published_ = publish(
                "checkpoint", checkpoint.str(), false, true
            );
        }
    }

    void handle_drop(
        const BrowserEvent& event,
        const JsonValue::Object& content
    ) {
        if (!session_) return;
        const std::string stage = string_field(content, "stage", 16);
        const std::uint64_t id = number_field(content, "proposal_id");
        if (id == 0) throw std::runtime_error("invalid drop proposal ID");
        if (stage == "proposal") {
            if (hosting_ && event.pubkey == public_key_) return;
            if (event.pubkey != host_public_key_ || hosting_) {
                throw std::runtime_error("invalid drop proposal");
            }
            std::ostringstream response;
            response << base_content("drop")
                     << ",\"stage\":\"acknowledge\",\"proposal_id\":"
                     << id << '}';
            (void)publish("drop", response.str(), false, true);
        } else if (stage == "acknowledge") {
            if (!hosting_ && event.pubkey == public_key_) return;
            if (!hosting_ || event.pubkey != config_.red.peer_id ||
                !drop_proposal_pending_ || id != drop_proposal_id_) {
                throw std::runtime_error("invalid drop acknowledgement");
            }
            std::ostringstream response;
            response << base_content("drop")
                     << ",\"stage\":\"commit\",\"proposal_id\":"
                     << id << '}';
            (void)publish("drop", response.str(), false, true);
        } else if (stage == "commit") {
            if (event.pubkey != host_public_key_) {
                throw std::runtime_error("invalid drop commit");
            }
            session_->disconnect(hosting_ ? Player::red : Player::blue);
            reliability_status_ = MultiplayerReliabilityStatus::dropped;
            reliability_reason_ = MultiplayerReliabilityReason::host_dropped_peer;
            drop_proposal_pending_ = false;
            drop_proposal_id_ = 0;
        } else {
            throw std::runtime_error("invalid drop stage");
        }
    }

    void publish_terminal_result(const Simulation& simulation) {
        if (result_published_ || simulation.outcome() == MatchOutcome::ongoing ||
            !session_) return;
        std::ostringstream content;
        content << base_content("result")
                << ",\"sender_slot\":" << slot_number(local_slot_)
                << ",\"final_tick\":" << session_->current_tick()
                << ",\"outcome\":"
                << static_cast<int>(simulation.outcome())
                << ",\"state_hash\":"
                << json_string(deterministic_state_hash(simulation)) << '}';
        result_published_ = publish("result", content.str(), false, true);
    }

    void handle_result(
        const BrowserEvent& event,
        const JsonValue::Object& content
    ) {
        const Player sender = validated_sender(event, content);
        const std::uint64_t final_tick = number_field(content, "final_tick");
        const std::uint64_t outcome = number_field(content, "outcome");
        const std::string state_hash = string_field(content, "state_hash", 256);
        if (outcome == static_cast<std::uint64_t>(MatchOutcome::ongoing) ||
            outcome > static_cast<std::uint64_t>(MatchOutcome::draw) ||
            state_hash.empty()) {
            throw std::runtime_error("invalid terminal result");
        }
        std::ostringstream fingerprint;
        fingerprint << final_tick << ':' << outcome << ':' << state_hash;
        auto& accepted = result_fingerprints_[slot_number(sender)];
        if (accepted && *accepted != fingerprint.str()) {
            throw std::runtime_error("conflicting terminal result");
        }
        accepted = fingerprint.str();
    }

    void reset_lobby_prerequisites() {
        ack_event_ids_ = {};
        ready_event_ids_ = {};
        local_ack_sent_for_.clear();
        local_ready_sent_for_.clear();
        start_sent_ = false;
        pending_start_.reset();
    }

    std::size_t observed_count(const std::string& event_id) const {
        const auto found = observations_.find(event_id);
        if (found == observations_.end()) return 0;
        return static_cast<std::size_t>(std::ranges::count_if(
            found->second,
            [&](const std::string& relay) {
                const auto state = relay_states_.find(relay);
                return state != relay_states_.end() &&
                    !state->second.auth_required;
            }
        ));
    }

    std::size_t usable_relay_count() const {
        return static_cast<std::size_t>(std::ranges::count_if(
            relays_,
            [&](const std::string& relay) {
                const auto found = relay_states_.find(relay);
                return found != relay_states_.end() &&
                    found->second.connected && found->second.ready &&
                    !found->second.auth_required;
            }
        ));
    }

    std::size_t eose_relay_count() const {
        return static_cast<std::size_t>(std::ranges::count_if(
            relays_,
            [&](const std::string& relay) {
                const auto found = relay_states_.find(relay);
                return found != relay_states_.end() && found->second.eose &&
                    !found->second.auth_required;
            }
        ));
    }

    void update_reliability() {
        if (reliability_reason_ == MultiplayerReliabilityReason::protocol_conflict ||
            reliability_status_ == MultiplayerReliabilityStatus::dropped ||
            reliability_status_ == MultiplayerReliabilityStatus::disconnected) return;
        if (!initialized_ || usable_relay_count() < quorum_) {
            reliability_status_ = session_
                ? MultiplayerReliabilityStatus::suspended
                : MultiplayerReliabilityStatus::waiting;
            reliability_reason_ = MultiplayerReliabilityReason::relay_quorum_lost;
            return;
        }
        if (eose_relay_count() < quorum_) {
            reliability_status_ = MultiplayerReliabilityStatus::waiting;
            reliability_reason_ = MultiplayerReliabilityReason::backfill_incomplete;
            return;
        }
        if (!failed_turn_event_ids_.empty()) {
            reliability_status_ = MultiplayerReliabilityStatus::waiting;
            reliability_reason_ = MultiplayerReliabilityReason::backfill_incomplete;
            return;
        }
        if (session_ && session_->status() == LockstepStatus::running) {
            const auto now = std::chrono::steady_clock::now();
            const auto silence = now - last_peer_traffic_;
            if (silence >= std::chrono::seconds(5) &&
                now - last_peer_probe_at_ >= std::chrono::seconds(5) &&
                session_) {
                // Both peers can cross the silence threshold together. If
                // waiting then stops all turn publication, neither side can
                // generate traffic that recovers the session. Refreshing each
                // tagged subscription backfills unseen turns; a fresh receipt
                // is a bounded heartbeat and carries exact missing ranges.
                (void)nostr_bridge_refresh_subscriptions();
                publish_receipt(true);
                last_peer_probe_at_ = now;
            }
            if (silence >= std::chrono::seconds(30)) {
                reliability_status_ = MultiplayerReliabilityStatus::suspended;
                reliability_reason_ = MultiplayerReliabilityReason::peer_silent;
                return;
            }
            if (silence >= std::chrono::seconds(5)) {
                reliability_status_ = MultiplayerReliabilityStatus::waiting;
                reliability_reason_ = MultiplayerReliabilityReason::peer_silent;
                return;
            }
        }
        reliability_status_ = MultiplayerReliabilityStatus::active;
        reliability_reason_ = MultiplayerReliabilityReason::none;
    }

    void suspend(
        MultiplayerReliabilityReason reason,
        std::string message
    ) {
        reliability_status_ = MultiplayerReliabilityStatus::suspended;
        reliability_reason_ = reason;
        failure_ = std::move(message);
    }

    bool hosting_{};
    Player local_slot_{Player::blue};
    bool one_relay_development_{};
    bool initialized_{};
    bool initial_lobby_publish_pending_{};
    bool disconnected_{};
    bool ready_requested_{};
    bool start_requested_{};
    bool start_sent_{};
    bool result_published_{};
    std::array<std::optional<std::string>, 2> result_fingerprints_{};
    bool checkpoint_published_{};
    bool save_hash_sent_{};
    bool drop_proposal_pending_{};
    std::uint64_t drop_proposal_id_{};
    std::string match_id_;
    std::string host_public_key_;
    std::vector<std::string> relays_;
    std::size_t quorum_{2};
    std::map<std::string, RelayState> relay_states_;
    std::map<std::string, std::set<std::string>> observations_;
    std::uint64_t next_intent_id_{1};
    std::map<std::string, IntentState> pending_intents_;
    std::uint64_t lobby_revision_{};
    std::uint64_t lobby_expires_at_{};
    std::uint64_t accepted_lobby_revision_{};
    std::string accepted_lobby_event_id_;
    std::string current_lobby_event_id_;
    std::set<std::string> retired_lobby_event_ids_;
    std::string join_sent_for_;
    std::string local_ack_sent_for_;
    std::string local_ready_sent_for_;
    std::array<std::optional<std::string>, 2> ack_event_ids_{};
    std::array<std::optional<std::string>, 2> ready_event_ids_{};
    std::optional<BrowserEvent> pending_start_;
    std::vector<BrowserEvent> deferred_handshake_events_;
    std::vector<BrowserEvent> prestart_turn_events_;
    bool replaying_deferred_handshake_{};
    std::optional<LockstepSession> session_;
    std::vector<GameCommand> pending_commands_;
    std::vector<LockstepFrame> outbound_frames_;
    std::array<NostrSenderSequence, 2> sender_streams_;
    std::uint64_t next_submission_tick_{};
    std::uint64_t next_sender_sequence_{1};
    std::string last_outbound_turn_event_id_;
    std::map<std::uint64_t, std::string> outbound_turn_event_ids_;
    std::set<std::string> failed_turn_event_ids_;
    std::set<std::string> turn_republish_outstanding_ids_;
    bool turn_publish_outstanding_{};
    std::size_t received_since_receipt_{};
    std::uint64_t next_chat_sequence_{1};
    std::uint64_t next_signal_sequence_{1};
    std::set<std::uint64_t> chat_sequences_;
    std::set<std::uint64_t> signal_sequences_;
    std::uint64_t next_control_id_{1};
    std::optional<PendingControl> pending_control_;
    std::uint64_t last_completed_control_id_{};
    std::chrono::steady_clock::time_point last_peer_traffic_;
    std::chrono::steady_clock::time_point last_peer_probe_at_;
    std::chrono::steady_clock::time_point last_advanced_at_{};
    std::string failure_;
};

NostrMultiplayerRuntime::NostrMultiplayerRuntime(
    MultiplayerLaunchConfig launch
) : impl_(std::make_unique<Impl>(std::move(launch))) {}

NostrMultiplayerRuntime::~NostrMultiplayerRuntime() = default;
NostrMultiplayerRuntime::NostrMultiplayerRuntime(
    NostrMultiplayerRuntime&&
) noexcept = default;
NostrMultiplayerRuntime& NostrMultiplayerRuntime::operator=(
    NostrMultiplayerRuntime&&
) noexcept = default;

bool NostrMultiplayerRuntime::queue_command(GameCommand command) {
    return impl_->queue_command(std::move(command));
}
void NostrMultiplayerRuntime::poll_transport(Simulation& simulation) {
    impl_->poll_transport(simulation);
}
void NostrMultiplayerRuntime::pump(Simulation& simulation) {
    impl_->pump(simulation);
}
void NostrMultiplayerRuntime::disconnect() { impl_->disconnect(); }
bool NostrMultiplayerRuntime::send_chat(
    std::string text,
    ChatAudience audience
) { return impl_->send_chat(std::move(text), audience); }
bool NostrMultiplayerRuntime::send_signal(
    TilePosition tile,
    ChatAudience audience
) { return impl_->send_signal(tile, audience); }
bool NostrMultiplayerRuntime::request_save_barrier(std::uint64_t tick) {
    return impl_->request_save_barrier(tick);
}
bool NostrMultiplayerRuntime::propose_pause(
    bool paused,
    std::uint64_t tick
) { return impl_->propose_pause(paused, tick); }
bool NostrMultiplayerRuntime::propose_speed(
    GameSpeed speed,
    std::uint64_t tick
) { return impl_->propose_speed(speed, tick); }
bool NostrMultiplayerRuntime::drop_peer() { return impl_->drop_peer(); }
void NostrMultiplayerRuntime::set_ready(bool ready) { impl_->set_ready(ready); }
void NostrMultiplayerRuntime::request_start() { impl_->request_start(); }
LockstepStatus NostrMultiplayerRuntime::status() const { return impl_->status(); }
bool NostrMultiplayerRuntime::connected() const { return impl_->connected(); }
std::uint16_t NostrMultiplayerRuntime::port() const { return 0; }
std::uint64_t NostrMultiplayerRuntime::current_tick() const {
    return impl_->current_tick();
}
bool NostrMultiplayerRuntime::waiting_for_turn() const {
    return impl_->waiting_for_turn();
}
PlayerControllerState NostrMultiplayerRuntime::local_controller_state() const {
    return impl_->local_controller_state_;
}
bool NostrMultiplayerRuntime::peer_ready(Player player) const {
    return impl_->peer_ready(player);
}
const LockstepSessionConfig& NostrMultiplayerRuntime::session_config() const {
    return impl_->config_;
}
const std::vector<LockstepChatMessage>&
NostrMultiplayerRuntime::chat_log() const { return impl_->chat_log_; }
const std::vector<LockstepMapSignal>&
NostrMultiplayerRuntime::signal_log() const { return impl_->signal_log_; }
const LockstepSaveBarrier* NostrMultiplayerRuntime::save_barrier() const {
    return &impl_->save_barrier_;
}
NetworkTimingMetrics NostrMultiplayerRuntime::network_metrics() const {
    return impl_->network_metrics();
}
bool NostrMultiplayerRuntime::paused() const { return impl_->paused_; }
GameSpeed NostrMultiplayerRuntime::game_speed() const {
    return impl_->game_speed_;
}
int NostrMultiplayerRuntime::effective_tick_cadence_ms() const {
    return impl_->effective_tick_cadence_ms();
}
MultiplayerReliabilityStatus
NostrMultiplayerRuntime::reliability_status() const {
    return impl_->reliability_status_;
}
MultiplayerReliabilityReason
NostrMultiplayerRuntime::reliability_reason() const {
    return impl_->reliability_reason_;
}
std::string NostrMultiplayerRuntime::transport_name() const {
    return "PUBLIC NOSTR";
}
std::string NostrMultiplayerRuntime::transport_detail() const {
    return impl_->transport_detail();
}
std::string NostrMultiplayerRuntime::public_match_reference() const {
    return impl_->match_reference_;
}
std::string NostrMultiplayerRuntime::local_identity() const {
    return impl_->public_key_;
}

}  // namespace aoe
