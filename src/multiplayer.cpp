#include "aoe/multiplayer.hpp"

#include <atomic>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#include "aoe/format_versions.hpp"
#include "aoe/player_codec.hpp"
#include "aoe/save_game.hpp"
#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif

namespace aoe {
namespace {

std::uint64_t process_id() {
#if defined(_WIN32)
    return static_cast<std::uint64_t>(::_getpid());
#else
    return static_cast<std::uint64_t>(::getpid());
#endif
}

std::string player_name(Player player) {
    if (is_playable_player(player)) {
        return std::string{player_wire_name(player)};
    }
    throw std::invalid_argument("lockstep requires blue or red player");
}

Player parse_player(const std::string& value) {
    const auto player = decode_player_wire_name(value);
    if (player && is_playable_player(*player)) return *player;
    throw std::runtime_error("invalid lockstep player");
}

PlayerSlotId frame_source(const LockstepFrame& frame) {
    if (frame.source) {
        if (frame.source->is_neutral()) {
            throw std::invalid_argument(
                "neutral cannot source lockstep frame"
            );
        }
        return *frame.source;
    }
    const auto source = player_slot_from_legacy(frame.player);
    if (!source || source->is_neutral()) {
        throw std::invalid_argument("invalid lockstep frame source");
    }
    return *source;
}

void validate_config(const LockstepSessionConfig& config) {
    const auto invalid = [](std::string_view field) {
        throw std::invalid_argument(
            "invalid lockstep session config: " + std::string{field}
        );
    };
    if (config.build_id.empty() || config.build_id.size() > 128) invalid("build ID");
    if (config.command_schema_version <= 0) invalid("command schema version");
    if (config.save_version != reconstruction_save_version) invalid("save version");
    if (config.scenario_version != reconstruction_scenario_version) invalid("scenario version");
    if (config.scenario_digest.empty() || config.scenario_digest.size() > 256) invalid("scenario digest");
    if (config.content_rules_digest.empty() || config.content_rules_digest.size() > 256) invalid("content rules digest");
    if (config.tick_cadence_ms <= 0 || config.tick_cadence_ms > 10000) invalid("tick cadence");
    if (config.input_delay_ticks < 0 || config.input_delay_ticks > 256) invalid("input delay");
    if (config.blue.peer_id.empty() || config.blue.peer_id.size() > 128) invalid("blue peer ID");
    if (config.red.peer_id.empty() || config.red.peer_id.size() > 128) invalid("red peer ID");
    if (config.blue.peer_id == config.red.peer_id) invalid("distinct peer IDs");
    if (config.blue.slot != Player::blue) invalid("blue slot");
    if (config.red.slot != Player::red) invalid("red slot");
    if (config.blue.team < 1 || config.blue.team > 8) invalid("blue team");
    if (config.red.team < 1 || config.red.team > 8) invalid("red team");
    if (config.blue.civilization < Civilization::generic ||
        config.blue.civilization > Civilization::mayans) invalid("blue civilization");
    if (config.red.civilization < Civilization::generic ||
        config.red.civilization > Civilization::mayans) invalid("red civilization");
    if (config.native_roster.has_value() != config.native_diplomacy.has_value()) {
        invalid("native roster/diplomacy pairing");
    }
    if (!config.native_roster) {
        if (config.host_slot &&
            config.host_slot != PlayerSlotId::from_index(0)) {
            throw std::invalid_argument("invalid lockstep session config");
        }
        return;
    }
    if (!config.native_diplomacy->valid()) {
        throw std::invalid_argument("invalid lockstep session config");
    }
    std::size_t occupied{};
    for (std::size_t index = 0; index < 8; ++index) {
        const PlayerSlotId slot = *PlayerSlotId::from_index(index);
        const MatchRosterSlot& entry = config.native_roster->slot(slot);
        if (entry.occupied) ++occupied;
        const Civilization civilization =
            config.native_civilizations[index];
        if (civilization < Civilization::generic ||
            civilization > Civilization::mayans) {
            throw std::invalid_argument("invalid lockstep session config");
        }
    }
    const PlayerSlotId host = config.host_slot.value_or(
        *PlayerSlotId::from_index(0)
    );
    if (occupied < 2 || !config.native_roster->slot(host).occupied) {
        throw std::invalid_argument("invalid lockstep session config");
    }
}

LockstepSessionConfig parse_config(const std::string& bytes) {
    std::istringstream input(bytes);
    std::string magic;
    LockstepSessionConfig config;
    int blue_slot{};
    int blue_civilization{};
    int red_slot{};
    int red_civilization{};
    input >> magic >> std::quoted(config.build_id) >>
        config.command_schema_version >> config.save_version >>
        config.scenario_version >> std::quoted(config.scenario_digest) >>
        std::quoted(config.content_rules_digest) >>
        config.tick_cadence_ms >> config.input_delay_ticks >>
        config.deterministic_seed;
    if (!input ||
        (magic != "lockstep-config-v1" &&
         magic != "lockstep-config-v2")) {
        throw std::runtime_error("invalid lockstep session config");
    }
    if (magic == "lockstep-config-v2") {
        bool allied_victory{};
        bool shared_vision{};
        int host{};
        input >> allied_victory >> shared_vision >> host;
        std::vector<MatchRosterSlot> slots;
        slots.reserve(8);
        for (std::size_t expected = 0; expected < 8; ++expected) {
            int stable_id{};
            bool occupied{};
            int team_number{};
            bool cooperative{};
            int civilization{};
            std::size_t controller_count{};
            input >> stable_id >> occupied >> team_number >> cooperative >>
                civilization >> controller_count;
            const auto slot = decode_player_slot_id(stable_id);
            if (!slot || slot->is_neutral() ||
                *slot != *PlayerSlotId::from_index(expected) ||
                controller_count > 8) {
                throw std::runtime_error(
                    "invalid lockstep session config"
                );
            }
            MatchRosterSlot entry;
            entry.slot = *slot;
            entry.occupied = occupied;
            entry.cooperative_control = cooperative;
            if (team_number != 0) {
                const auto team = TeamId::numbered(team_number);
                if (!team) {
                    throw std::runtime_error(
                        "invalid lockstep session config"
                    );
                }
                entry.team = *team;
            }
            for (std::size_t index = 0; index < controller_count; ++index) {
                int kind{};
                std::string id;
                input >> kind >> std::quoted(id);
                if (kind < static_cast<int>(
                        RosterControllerKind::human
                    ) ||
                    kind > static_cast<int>(
                        RosterControllerKind::computer
                    )) {
                    throw std::runtime_error(
                        "invalid lockstep session config"
                    );
                }
                entry.controllers.push_back({
                    std::move(id),
                    static_cast<RosterControllerKind>(kind),
                });
            }
            if (civilization < static_cast<int>(
                    Civilization::generic
                ) ||
                civilization > static_cast<int>(
                    Civilization::mayans
                )) {
                throw std::runtime_error(
                    "invalid lockstep session config"
                );
            }
            config.native_civilizations[expected] =
                static_cast<Civilization>(civilization);
            slots.push_back(std::move(entry));
        }
        config.native_roster = MatchRoster::create(std::move(slots));
        const auto decoded_host = decode_player_slot_id(host);
        if (!config.native_roster || !decoded_host ||
            decoded_host->is_neutral()) {
            throw std::runtime_error(
                "invalid lockstep session config"
            );
        }
        config.host_slot = *decoded_host;
        config.native_diplomacy = RosterDiplomacy::create(
            *config.native_roster,
            {allied_victory, shared_vision}
        );
        if (!config.native_diplomacy) {
            throw std::runtime_error(
                "invalid lockstep session config"
            );
        }
        for (std::size_t from = 0; from < 8; ++from) {
            for (std::size_t to = 0; to < 8; ++to) {
                int relation{};
                input >> relation;
                const PlayerSlotId source =
                    *PlayerSlotId::from_index(from);
                const PlayerSlotId target =
                    *PlayerSlotId::from_index(to);
                if (relation < static_cast<int>(Diplomacy::ally) ||
                    relation > static_cast<int>(Diplomacy::enemy) ||
                    (from != to &&
                     config.native_roster->slot(source).occupied &&
                     config.native_roster->slot(target).occupied &&
                     !config.native_diplomacy->set_stance(
                         source, target,
                         static_cast<Diplomacy>(relation)
                     )) ||
                    config.native_diplomacy->stance(source, target) !=
                        static_cast<Diplomacy>(relation)) {
                    throw std::runtime_error(
                        "invalid lockstep session config"
                    );
                }
            }
        }
        input >> std::ws;
        if (input.peek() != std::char_traits<char>::eof()) {
            throw std::runtime_error(
                "invalid lockstep session config"
            );
        }
        validate_config(config);
        return config;
    }
    input >> std::quoted(config.blue.peer_id) >> blue_slot >>
        blue_civilization >> config.blue.team >>
        std::quoted(config.red.peer_id) >> red_slot >>
        red_civilization >> config.red.team;
    input >> std::ws;
    if (input.peek() != std::char_traits<char>::eof()) {
        throw std::runtime_error("invalid lockstep session config");
    }
    const auto decoded_blue = decode_player_wire(blue_slot);
    const auto decoded_red = decode_player_wire(red_slot);
    if (!decoded_blue || !decoded_red ||
        !is_playable_player(*decoded_blue) ||
        !is_playable_player(*decoded_red)) {
        throw std::runtime_error("invalid lockstep session config");
    }
    config.blue.slot = *decoded_blue;
    config.red.slot = *decoded_red;
    config.blue.civilization =
        static_cast<Civilization>(blue_civilization);
    config.red.civilization =
        static_cast<Civilization>(red_civilization);
    validate_config(config);
    return config;
}

LockstepStatus config_mismatch(
    const LockstepSessionConfig& expected,
    const LockstepSessionConfig& received
) {
    if (expected.build_id != received.build_id) {
        return LockstepStatus::build_mismatch;
    }
    if (expected.command_schema_version !=
            received.command_schema_version ||
        expected.save_version != received.save_version ||
        expected.scenario_version != received.scenario_version) {
        return LockstepStatus::schema_mismatch;
    }
    if (expected.scenario_digest != received.scenario_digest) {
        return LockstepStatus::scenario_mismatch;
    }
    if (expected.content_rules_digest !=
        received.content_rules_digest) {
        return LockstepStatus::content_mismatch;
    }
    if (expected.tick_cadence_ms != received.tick_cadence_ms ||
        expected.input_delay_ticks != received.input_delay_ticks ||
        expected.deterministic_seed != received.deterministic_seed) {
        return LockstepStatus::settings_mismatch;
    }
    if (canonical_lockstep_config(expected) !=
        canonical_lockstep_config(received)) {
        return LockstepStatus::roster_mismatch;
    }
    return LockstepStatus::handshaking;
}

bool config_matches_simulation(
    const LockstepSessionConfig& config,
    const Simulation& simulation
) {
    if (!config.native_roster) return true;
    if (config.native_diplomacy->rules().allied_victory !=
            simulation.roster_diplomacy().rules().allied_victory ||
        config.native_diplomacy->rules().shared_vision !=
            simulation.roster_diplomacy().rules().shared_vision) {
        return false;
    }
    for (std::size_t index = 0; index < 8; ++index) {
        const PlayerSlotId slot = *PlayerSlotId::from_index(index);
        const MatchRosterSlot& expected =
            config.native_roster->slot(slot);
        const MatchRosterSlot& actual = simulation.roster().slot(slot);
        if (expected.occupied != actual.occupied ||
            expected.team != actual.team ||
            expected.cooperative_control !=
                actual.cooperative_control ||
            expected.controllers != actual.controllers ||
            config.native_civilizations[index] !=
                simulation.civilization(slot)) {
            return false;
        }
        for (std::size_t other = 0; other < 8; ++other) {
            const PlayerSlotId target =
                *PlayerSlotId::from_index(other);
            if (config.native_diplomacy->stance(slot, target) !=
                simulation.roster_diplomacy().stance(slot, target)) {
                return false;
            }
        }
    }
    return true;
}

std::string frame_kind_name(LockstepFrameKind kind) {
    switch (kind) {
        case LockstepFrameKind::hello: return "hello";
        case LockstepFrameKind::ready: return "ready";
        case LockstepFrameKind::start: return "start";
        case LockstepFrameKind::turn: return "turn";
        case LockstepFrameKind::disconnect: return "disconnect";
        case LockstepFrameKind::chat: return "chat";
        case LockstepFrameKind::signal: return "signal";
        case LockstepFrameKind::save_barrier: return "save-barrier";
        case LockstepFrameKind::save_hash: return "save-hash";
        case LockstepFrameKind::heartbeat_ping: return "ping";
        case LockstepFrameKind::heartbeat_pong: return "pong";
        case LockstepFrameKind::control_proposal:
            return "control-proposal";
        case LockstepFrameKind::control_ack: return "control-ack";
        case LockstepFrameKind::control_commit: return "control-commit";
        case LockstepFrameKind::peer_drop: return "peer-drop";
    }
    return "hello";
}

std::filesystem::path temporary_codec_path(const std::string& stem) {
    static std::atomic<std::uint64_t> sequence{};
    return std::filesystem::temp_directory_path() /
        (stem + "-" +
         std::to_string(process_id()) + "-" +
         std::to_string(
             std::chrono::steady_clock::now().time_since_epoch().count()
         ) + "-" + std::to_string(sequence.fetch_add(1)));
}

LockstepFrameKind parse_frame_kind(const std::string& value) {
    if (value == "hello") return LockstepFrameKind::hello;
    if (value == "ready") return LockstepFrameKind::ready;
    if (value == "start") return LockstepFrameKind::start;
    if (value == "turn") return LockstepFrameKind::turn;
    if (value == "disconnect") return LockstepFrameKind::disconnect;
    if (value == "chat") return LockstepFrameKind::chat;
    if (value == "signal") return LockstepFrameKind::signal;
    if (value == "save-barrier") return LockstepFrameKind::save_barrier;
    if (value == "save-hash") return LockstepFrameKind::save_hash;
    if (value == "ping") return LockstepFrameKind::heartbeat_ping;
    if (value == "pong") return LockstepFrameKind::heartbeat_pong;
    if (value == "control-proposal")
        return LockstepFrameKind::control_proposal;
    if (value == "control-ack") return LockstepFrameKind::control_ack;
    if (value == "control-commit")
        return LockstepFrameKind::control_commit;
    if (value == "peer-drop") return LockstepFrameKind::peer_drop;
    throw std::runtime_error("invalid lockstep frame kind");
}

void encode_command(std::ostream& output, const GameCommand& command) {
    std::visit([&](const auto& value) {
        using Type = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<Type, MoveUnitCommand>) {
            output << " move " << value.unit << ' ' << value.destination.x
                   << ' ' << value.destination.y;
        } else if constexpr (std::is_same_v<Type, StopUnitCommand>) {
            output << " stop " << value.unit;
        } else if constexpr (std::is_same_v<Type, ResignCommand>) {
            output << " resign " << player_name(value.player);
        } else {
            Replay replay;
            replay.record(0, command);
            const auto path = temporary_codec_path("aoe-lockstep-command");
            save_replay(replay, path);
            std::ifstream input(path);
            std::string header;
            std::string source;
            std::string line;
            std::getline(input, header);
            std::getline(input, source);
            std::getline(input, line);
            std::filesystem::remove(path);
            if (line.empty()) {
                throw std::runtime_error("cannot encode lockstep command");
            }
            output << " replay " << std::quoted(line);
        }
    }, command);
}

GameCommand decode_command(std::istream& input) {
    std::string type;
    input >> type;
    if (type == "move") {
        MoveUnitCommand command{};
        input >> command.unit >> command.destination.x >> command.destination.y;
        return command;
    }
    if (type == "stop") {
        StopUnitCommand command{};
        input >> command.unit;
        return command;
    }
    if (type == "resign") {
        std::string player;
        input >> player;
        return ResignCommand{parse_player(player)};
    }
    if (type == "replay") {
        std::string line;
        input >> std::quoted(line);
        const auto path = temporary_codec_path("aoe-lockstep-command");
        {
            std::ofstream output(path);
            output << "AOE-ARCHAEOLOGY-REPLAY "
                   << reconstruction_command_schema_version << '\n'
                   << "source -1\n"
                   << line << '\n';
        }
        Replay replay = load_replay(path);
        std::filesystem::remove(path);
        if (replay.commands().size() != 1 ||
            replay.commands().front().tick != 0) {
            throw std::runtime_error("invalid encoded lockstep command");
        }
        return replay.commands().front().command;
    }
    throw std::runtime_error("unknown lockstep command");
}

bool same_frame(const LockstepFrame& left, const LockstepFrame& right) {
    return encode_lockstep_frame(left) == encode_lockstep_frame(right);
}

}  // namespace

std::string canonical_lockstep_config(
    const LockstepSessionConfig& config
) {
    validate_config(config);
    std::ostringstream output;
    output << (config.native_roster
                   ? "lockstep-config-v2 " : "lockstep-config-v1 ")
           << std::quoted(config.build_id) << ' '
           << config.command_schema_version << ' ' << config.save_version
           << ' ' << config.scenario_version << ' '
           << std::quoted(config.scenario_digest) << ' '
           << std::quoted(config.content_rules_digest) << ' '
           << config.tick_cadence_ms << ' ' << config.input_delay_ticks
           << ' ' << config.deterministic_seed;
    if (config.native_roster) {
        const RosterDiplomacyRules rules =
            config.native_diplomacy->rules();
        output << ' ' << rules.allied_victory << ' '
               << rules.shared_vision << ' '
               << static_cast<int>(
                      config.host_slot.value_or(
                          *PlayerSlotId::from_index(0)
                      ).stable_id()
                  );
        for (std::size_t index = 0; index < 8; ++index) {
            const PlayerSlotId slot = *PlayerSlotId::from_index(index);
            const MatchRosterSlot& entry =
                config.native_roster->slot(slot);
            output << ' ' << index << ' ' << entry.occupied << ' '
                   << entry.team.number() << ' '
                   << entry.cooperative_control << ' '
                   << static_cast<int>(
                          config.native_civilizations[index]
                      ) << ' ' << entry.controllers.size();
            for (const RosterController& controller : entry.controllers) {
                output << ' ' << static_cast<int>(controller.kind)
                       << ' ' << std::quoted(controller.id);
            }
        }
        for (std::size_t from = 0; from < 8; ++from) {
            for (std::size_t to = 0; to < 8; ++to) {
                output << ' ' << static_cast<int>(
                    config.native_diplomacy->stance(
                        *PlayerSlotId::from_index(from),
                        *PlayerSlotId::from_index(to)
                    )
                );
            }
        }
        return output.str();
    }
    output << ' ' << std::quoted(config.blue.peer_id) << ' '
           << encode_player_wire(config.blue.slot) << ' '
           << static_cast<int>(config.blue.civilization) << ' '
           << config.blue.team << ' '
           << std::quoted(config.red.peer_id) << ' '
           << encode_player_wire(config.red.slot) << ' '
           << static_cast<int>(config.red.civilization) << ' '
           << config.red.team;
    return output.str();
}

std::string lockstep_config_digest(
    const LockstepSessionConfig& config
) {
    std::uint64_t hash = 14695981039346656037ULL;
    for (const unsigned char byte : canonical_lockstep_config(config)) {
        hash ^= byte;
        hash *= 1099511628211ULL;
    }
    std::ostringstream output;
    output << "lockstep-config-fnv1a64:" << std::hex
           << std::setfill('0') << std::setw(16) << hash;
    return output.str();
}

std::string lockstep_compatibility_digest(
    LockstepSessionConfig config
) {
    config.blue.peer_id = "compatibility-blue";
    config.red.peer_id = "compatibility-red";
    return lockstep_config_digest(config);
}

std::string encode_lockstep_frame(const LockstepFrame& frame) {
    std::ostringstream payload;
    payload << "aoe-lockstep 3 " << frame_kind_name(frame.kind) << ' '
            << frame.protocol_version << ' '
            << static_cast<int>(frame_source(frame).stable_id())
            << ' ' << std::quoted(frame.scenario_digest) << ' '
            << std::quoted(
                   frame.config
                   ? canonical_lockstep_config(*frame.config)
                   : std::string{}
               ) << ' ' << std::quoted(frame.config_digest) << ' '
            << (frame.chat ? frame.chat->sequence : 0) << ' '
            << (frame.chat &&
                        frame.chat->audience == ChatAudience::all
                    ? "all" : "allies") << ' '
            << (frame.chat ? player_name(frame.chat->sender) : "blue")
            << ' '
            << std::quoted(frame.chat ? frame.chat->text : std::string{})
            << ' '
            << (frame.signal ? frame.signal->sequence : 0) << ' '
            << (frame.signal &&
                        frame.signal->audience == ChatAudience::all
                    ? "all" : "allies") << ' '
            << (frame.signal
                    ? player_name(frame.signal->sender) : "blue") << ' '
            << (frame.signal ? frame.signal->tile.x : 0) << ' '
            << (frame.signal ? frame.signal->tile.y : 0) << ' '
            << (frame.control ? frame.control->proposal_id : 0) << ' '
            << (frame.control ? frame.control->barrier_tick : 0) << ' '
            << (frame.control
                    ? frame.control->kind == SessionControlKind::pause
                        ? "pause"
                        : frame.control->kind == SessionControlKind::resume
                            ? "resume" : "speed"
                    : "pause") << ' '
            << (frame.control
                    ? frame.control->speed == GameSpeed::slow
                        ? "slow"
                        : frame.control->speed == GameSpeed::fast
                            ? "fast" : "normal"
                    : "normal") << ' '
            << frame.tick << ' ' << frame.sequence << ' '
            << std::quoted(frame.state_hash) << ' '
            << frame.commands.size();
    for (const GameCommand& command : frame.commands) {
        encode_command(payload, command);
    }
    const std::string body = payload.str();
    return std::to_string(body.size()) + ":" + body;
}

LockstepFrame decode_lockstep_frame(const std::string& bytes) {
    const auto separator = bytes.find(':');
    if (separator == std::string::npos || separator == 0) {
        throw std::runtime_error("invalid lockstep frame length");
    }
    if ((separator > 1 && bytes.front() == '0') ||
        !std::ranges::all_of(
            bytes.begin(), bytes.begin() + separator,
            [](unsigned char character) {
                return character >= '0' && character <= '9';
            }
        )) {
        throw std::runtime_error("noncanonical lockstep frame length");
    }
    std::size_t used{};
    const std::size_t length = std::stoull(bytes.substr(0, separator), &used);
    if (used != separator || length != bytes.size() - separator - 1 ||
        length > 1024 * 1024) {
        throw std::runtime_error("invalid lockstep frame length");
    }
    std::istringstream input(bytes.substr(separator + 1));
    std::string magic;
    int envelope_version{};
    std::string kind;
    std::string player;
    std::string config;
    std::string config_digest;
    std::uint64_t chat_sequence{};
    std::string chat_audience;
    std::string chat_sender;
    std::string chat_text;
    std::uint64_t signal_sequence{};
    std::string signal_audience;
    std::string signal_sender;
    int signal_x{};
    int signal_y{};
    std::uint64_t control_id{};
    std::uint64_t control_tick{};
    std::string control_kind;
    std::string control_speed;
    std::size_t command_count{};
    LockstepFrame frame;
    input >> magic >> envelope_version >> kind >> frame.protocol_version >>
        player >> std::quoted(frame.scenario_digest) >>
        std::quoted(config) >> std::quoted(config_digest) >>
        chat_sequence >> chat_audience >> chat_sender >>
        std::quoted(chat_text) >> signal_sequence >> signal_audience >>
        signal_sender >> signal_x >> signal_y >>
        control_id >> control_tick >>
        control_kind >> control_speed >>
        frame.tick >>
        frame.sequence >> std::quoted(frame.state_hash) >> command_count;
    if (!input || magic != "aoe-lockstep" ||
        (envelope_version != 2 && envelope_version != 3) ||
        command_count > 256) {
        throw std::runtime_error("invalid lockstep frame");
    }
    frame.kind = parse_frame_kind(kind);
    if (envelope_version == 3) {
        std::size_t used{};
        int stable_id{};
        try {
            stable_id = std::stoi(player, &used);
        } catch (const std::exception&) {
            throw std::runtime_error("invalid lockstep frame source");
        }
        const auto source = decode_player_slot_id(stable_id);
        if (used != player.size() || !source ||
            source->is_neutral()) {
            throw std::runtime_error("invalid lockstep frame source");
        }
        frame.source = *source;
        frame.player = player_slot_to_legacy(*source)
            .value_or(Player::neutral);
    } else {
        frame.player = parse_player(player);
        frame.source = *player_slot_from_legacy(frame.player);
    }
    if (!config.empty()) frame.config = parse_config(config);
    frame.config_digest = std::move(config_digest);
    if (frame.kind == LockstepFrameKind::chat) {
        if (chat_text.empty() || chat_text.size() > 4096 ||
            (chat_audience != "all" && chat_audience != "allies")) {
            throw std::runtime_error("invalid lockstep chat frame");
        }
        frame.chat = LockstepChatMessage{
            chat_sequence, parse_player(chat_sender),
            chat_audience == "all"
                ? ChatAudience::all : ChatAudience::allies,
            std::move(chat_text),
        };
    } else if (chat_sequence != 0 || !chat_text.empty() ||
               chat_audience != "allies" ||
               chat_sender != "blue") {
        throw std::runtime_error("chat payload on non-chat frame");
    }
    if (frame.kind == LockstepFrameKind::signal) {
        if (signal_sequence > 0xffffffffffffULL ||
            signal_x < 0 || signal_y < 0 ||
            signal_x > 32767 || signal_y > 32767 ||
            (signal_audience != "all" &&
             signal_audience != "allies")) {
            throw std::runtime_error("invalid lockstep signal frame");
        }
        frame.signal = LockstepMapSignal{
            signal_sequence, parse_player(signal_sender),
            signal_audience == "all"
                ? ChatAudience::all : ChatAudience::allies,
            {signal_x, signal_y},
        };
    } else if (signal_sequence != 0 || signal_x != 0 || signal_y != 0 ||
               signal_audience != "allies" ||
               signal_sender != "blue") {
        throw std::runtime_error("signal payload on non-signal frame");
    }
    const bool control_frame =
        frame.kind == LockstepFrameKind::control_proposal ||
        frame.kind == LockstepFrameKind::control_ack ||
        frame.kind == LockstepFrameKind::control_commit;
    if (control_frame) {
        if (control_id == 0 ||
            (control_kind != "pause" &&
             control_kind != "resume" &&
             control_kind != "speed") ||
            (control_speed != "slow" &&
             control_speed != "normal" &&
             control_speed != "fast")) {
            throw std::runtime_error("invalid session control frame");
        }
        frame.control = SessionControlMessage{
            control_id, control_tick,
            control_kind == "pause"
                ? SessionControlKind::pause
                : control_kind == "resume"
                    ? SessionControlKind::resume
                    : SessionControlKind::speed,
            control_speed == "slow"
                ? GameSpeed::slow
                : control_speed == "fast"
                    ? GameSpeed::fast : GameSpeed::normal,
        };
    } else if (control_id != 0 || control_tick != 0) {
        throw std::runtime_error("control payload on non-control frame");
    }
    for (std::size_t index = 0; index < command_count; ++index) {
        frame.commands.push_back(decode_command(input));
        if (!input) throw std::runtime_error("malformed lockstep command");
    }
    input >> std::ws;
    if (input.peek() != std::char_traits<char>::eof()) {
        throw std::runtime_error("trailing lockstep frame data");
    }
    return frame;
}

std::string deterministic_state_hash(const Simulation& simulation) {
    static std::atomic<std::uint64_t> sequence{};
    const auto path = std::filesystem::temp_directory_path() /
        ("aoe-lockstep-hash-" +
         std::to_string(process_id()) + "-" +
         std::to_string(
             std::chrono::steady_clock::now().time_since_epoch().count()
         ) + "-" +
         std::to_string(sequence.fetch_add(1)) + ".save");
    save_game(simulation, path);
    std::ifstream input(path, std::ios::binary);
    std::string bytes(
        (std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>()
    );
    std::filesystem::remove(path);
    bytes += "\nlockstep-next-entity-id:";
    bytes += std::to_string(simulation.next_entity_id());
    bytes += "\nlockstep-next-formation-group-id:";
    bytes += std::to_string(simulation.next_formation_group_id());
    std::uint64_t hash = 14695981039346656037ULL;
    for (const unsigned char byte : bytes) {
        hash ^= byte;
        hash *= 1099511628211ULL;
    }
    std::ostringstream output;
    output << "save" << reconstruction_save_version
           << "+ids-fnv1a64:" << std::hex << std::setfill('0')
           << std::setw(16) << hash;
    return output.str();
}

LockstepSession::LockstepSession(
    std::string scenario_digest,
    std::uint64_t timeout_steps,
    std::uint64_t hash_interval
) : scenario_digest_(std::move(scenario_digest)),
    timeout_steps_(std::max<std::uint64_t>(1, timeout_steps)),
    hash_interval_(std::max<std::uint64_t>(1, hash_interval)) {
    if (scenario_digest_.empty()) {
        throw std::invalid_argument("empty lockstep scenario digest");
    }
    config_.scenario_digest = scenario_digest_;
    validate_config(config_);
    allow_legacy_config_ = true;
}

LockstepSession::LockstepSession(
    LockstepSessionConfig config,
    std::uint64_t timeout_steps,
    std::uint64_t hash_interval
) : scenario_digest_(config.scenario_digest),
    config_(std::move(config)),
    timeout_steps_(std::max<std::uint64_t>(1, timeout_steps)),
    hash_interval_(std::max<std::uint64_t>(1, hash_interval)) {
    validate_config(config_);
}

LockstepSession::Peer& LockstepSession::peer(PlayerSlotId player) {
    const auto index = player.index();
    if (!index) throw std::invalid_argument("invalid lockstep peer");
    return peers_.at(*index);
}

const LockstepSession::Peer& LockstepSession::peer(
    PlayerSlotId player
) const {
    return const_cast<LockstepSession*>(this)->peer(player);
}

bool LockstepSession::connected(Player player) const {
    const auto slot = player_slot_from_legacy(player);
    return slot && !slot->is_neutral() && connected(*slot);
}

bool LockstepSession::connected(PlayerSlotId player) const {
    return peer(player).connected;
}

std::vector<PlayerSlotId> LockstepSession::required_slots() const {
    std::vector<PlayerSlotId> result;
    if (config_.native_roster) {
        for (std::size_t index = 0; index < 8; ++index) {
            const PlayerSlotId slot = *PlayerSlotId::from_index(index);
            if (config_.native_roster->slot(slot).occupied) {
                result.push_back(slot);
            }
        }
    } else {
        result = {
            *PlayerSlotId::from_index(0),
            *PlayerSlotId::from_index(1),
        };
    }
    return result;
}

PlayerSlotId LockstepSession::source_of(
    const LockstepFrame& frame
) const {
    return frame_source(frame);
}

bool LockstepSession::receive(
    const LockstepFrame& frame,
    const Simulation& simulation
) {
    if (status_ != LockstepStatus::handshaking &&
        status_ != LockstepStatus::ready &&
        status_ != LockstepStatus::running) return false;
    PlayerSlotId frame_slot;
    try {
        frame_slot = source_of(frame);
    } catch (const std::invalid_argument&) {
        return false;
    }
    const auto slots = required_slots();
    if (std::ranges::find(slots, frame_slot) == slots.end()) return false;
    if (frame.protocol_version != lockstep_protocol_version) {
        status_ = LockstepStatus::protocol_mismatch;
        return false;
    }
    if (frame.scenario_digest != scenario_digest_) {
        status_ = LockstepStatus::scenario_mismatch;
        return false;
    }
    Peer& source = peer(frame_slot);
    if (frame.kind == LockstepFrameKind::hello) {
        if (!frame.commands.empty() ||
            frame.tick != 0 || frame.sequence != 0 ||
            !frame.state_hash.empty() ||
            (status_ != LockstepStatus::handshaking &&
             status_ != LockstepStatus::ready)) return false;
        if (!config_matches_simulation(config_, simulation)) {
            status_ = LockstepStatus::roster_mismatch;
            return false;
        }
        if (!frame.config) {
            if (!allow_legacy_config_ || !frame.config_digest.empty()) {
                status_ = LockstepStatus::settings_mismatch;
                return false;
            }
        } else {
            const LockstepStatus mismatch =
                config_mismatch(config_, *frame.config);
            if (mismatch != LockstepStatus::handshaking) {
                status_ = mismatch;
                return false;
            }
            const std::string received_digest =
                lockstep_config_digest(*frame.config);
            if (frame.config_digest != received_digest ||
                lockstep_config_digest(config_) != received_digest) {
                status_ = LockstepStatus::settings_mismatch;
                return false;
            }
        }
        source.connected = true;
        source.hello = true;
        idle_steps_ = 0;
        return true;
    }
    if (frame.config || !frame.config_digest.empty()) return false;
    if (!source.connected) return false;
    if (frame.kind == LockstepFrameKind::ready) {
        if (!source.hello || !frame.commands.empty() ||
            frame.tick != 0 || frame.sequence != 0 ||
            !frame.state_hash.empty() ||
            (status_ != LockstepStatus::handshaking &&
             status_ != LockstepStatus::ready)) return false;
        source.ready = true;
        if (std::ranges::all_of(
                slots,
                [&](PlayerSlotId slot) { return peer(slot).ready; }
            )) {
            status_ = LockstepStatus::ready;
        }
        idle_steps_ = 0;
        return true;
    }
    if (frame.kind == LockstepFrameKind::start) {
        const PlayerSlotId host = config_.host_slot.value_or(
            *PlayerSlotId::from_index(0)
        );
        if (status_ != LockstepStatus::ready ||
            frame_slot != host ||
            !frame.commands.empty() || frame.tick != 0 ||
            frame.sequence != 0 || !frame.state_hash.empty()) return false;
        status_ = LockstepStatus::running;
        idle_steps_ = 0;
        return true;
    }
    if (frame.kind == LockstepFrameKind::disconnect) {
        if (!frame.commands.empty() || frame.tick != 0 ||
            frame.sequence != 0 || !frame.state_hash.empty()) return false;
        disconnect(frame_slot);
        return true;
    }
    if (frame.kind != LockstepFrameKind::turn ||
        status_ != LockstepStatus::running ||
        frame.tick < current_tick_ ||
        frame.sequence != frame.tick) return false;
    if (frame.tick > current_tick_ + 4096 ||
        frame.commands.size() > 256) return false;
    if ((frame.tick % hash_interval_ == 0) !=
        !frame.state_hash.empty()) return false;
    auto& destination =
        turns_[frame.tick].slots[*frame_slot.index()];
    if (destination) {
        if (!same_frame(*destination, frame)) {
            status_ = LockstepStatus::desync;
            return false;
        }
        idle_steps_ = 0;
        return true;
    }
    destination = frame;
    idle_steps_ = 0;
    return true;
}

bool LockstepSession::advance(Simulation& simulation) {
    if (status_ != LockstepStatus::running) return false;
    const auto found = turns_.find(current_tick_);
    const auto slots = required_slots();
    if (found == turns_.end() ||
        std::ranges::any_of(
            slots,
            [&](PlayerSlotId slot) {
                return !found->second.slots[*slot.index()];
            }
        )) {
        return false;
    }
    if (current_tick_ % hash_interval_ == 0) {
        const std::string local_hash = deterministic_state_hash(simulation);
        for (PlayerSlotId slot : slots) {
            const std::string& hash =
                found->second.slots[*slot.index()]->state_hash;
            if (hash.empty() || hash != local_hash) {
                status_ = LockstepStatus::desync;
                return false;
            }
        }
    }
    for (PlayerSlotId player : slots) {
        const auto& frame =
            *found->second.slots[*player.index()];
        if (!simulation.roster().slot(player).occupied ||
            simulation.controller_state(player) !=
                PlayerControllerState::active) {
            if (!frame.commands.empty()) {
                status_ = LockstepStatus::invalid_command;
                return false;
            }
            continue;
        }
        for (const GameCommand& command : frame.commands) {
            if (!validate_command_owner(simulation, player, command)) {
                status_ = LockstepStatus::invalid_command;
                return false;
            }
        }
    }
    for (PlayerSlotId player : slots) {
        const auto& frame =
            *found->second.slots[*player.index()];
        for (const GameCommand& command : frame.commands) {
            replay_.record(current_tick_, player, command);
            (void)execute(simulation, command, player);
        }
    }
    simulation.update();
    turns_.erase(found);
    ++current_tick_;
    idle_steps_ = 0;
    return true;
}

void LockstepSession::elapse() {
    if (status_ != LockstepStatus::running) return;
    if (++idle_steps_ >= timeout_steps_) {
        status_ = LockstepStatus::timed_out;
    }
}

void LockstepSession::disconnect(Player player) {
    const auto slot = player_slot_from_legacy(player);
    if (!slot || slot->is_neutral()) {
        throw std::invalid_argument("invalid lockstep peer");
    }
    disconnect(*slot);
}

void LockstepSession::disconnect(PlayerSlotId player) {
    peer(player).connected = false;
    status_ = LockstepStatus::disconnected;
}

bool LockstepSession::validate_command_owner(
    const Simulation& simulation,
    PlayerSlotId player,
    const GameCommand& command
) const {
    const auto unit_owned = [&](EntityId id) {
        const auto found = std::ranges::find(
            simulation.units(), id, &Unit::id
        );
        return found != simulation.units().end() &&
            found->owner == entity_owner_from_slot(player);
    };
    const auto building_owned = [&](EntityId id) {
        const auto found = std::ranges::find(
            simulation.buildings(), id, &Building::id
        );
        return found != simulation.buildings().end() &&
            found->owner == entity_owner_from_slot(player);
    };
    return std::visit([&](const auto& value) {
        using Type = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<Type, MoveUnitCommand> ||
                      std::is_same_v<Type, StopUnitCommand> ||
                      std::is_same_v<Type, AttackMoveCommand> ||
                      std::is_same_v<Type, AttackGroundCommand> ||
                      std::is_same_v<Type, PatrolCommand> ||
                      std::is_same_v<Type, QueueWaypointCommand> ||
                      std::is_same_v<Type, SetStanceCommand> ||
                      std::is_same_v<Type, PackTrebuchetCommand>) {
            return unit_owned(value.unit);
        } else if constexpr (
            std::is_same_v<Type, GatherUnitCommand>) {
            return unit_owned(value.villager);
        } else if constexpr (
            std::is_same_v<Type, ConvertUnitCommand> ||
            std::is_same_v<Type, HealUnitCommand> ||
            std::is_same_v<Type, CollectRelicCommand> ||
            std::is_same_v<Type, DepositRelicCommand>) {
            return unit_owned(value.monk);
        } else if constexpr (
            std::is_same_v<Type, TradeRouteCommand>) {
            return unit_owned(value.cart);
        } else if constexpr (std::is_same_v<Type, EmbarkCommand>) {
            return unit_owned(value.unit);
        } else if constexpr (std::is_same_v<Type, DisembarkCommand>) {
            return unit_owned(value.transport);
        } else if constexpr (std::is_same_v<Type, GuardCommand>) {
            return unit_owned(value.unit);
        } else if constexpr (
            std::is_same_v<Type, ConstructBuildingCommand>) {
            return unit_owned(value.builder);
        } else if constexpr (
            std::is_same_v<Type, QueueUnitCommand> ||
            std::is_same_v<Type, SetRallyPointCommand> ||
            std::is_same_v<Type, CancelProductionCommand> ||
            std::is_same_v<Type, ReseedFarmCommand> ||
            std::is_same_v<Type, UngarrisonCommand> ||
            std::is_same_v<Type, TownBellCommand> ||
            std::is_same_v<Type, SetGateLockedCommand> ||
            std::is_same_v<Type, AdvanceAgeCommand> ||
            std::is_same_v<Type, ResearchTechnologyCommand>) {
            return building_owned(value.building);
        } else if constexpr (std::is_same_v<Type, DeleteEntityCommand>) {
            return value.is_building
                ? building_owned(value.entity) : unit_owned(value.entity);
        } else if constexpr (std::is_same_v<Type, MoveFormationCommand>) {
            return !value.units.empty() &&
                std::ranges::all_of(value.units, unit_owned);
        } else if constexpr (
            std::is_same_v<Type, BuyResourceCommand> ||
            std::is_same_v<Type, SellResourceCommand> ||
            std::is_same_v<Type, SetCivilizationCommand> ||
            std::is_same_v<Type, ResignCommand> ||
            std::is_same_v<Type, SetFormationKindCommand>) {
            return player_slot_from_legacy(value.player) == player;
        } else if constexpr (
            std::is_same_v<Type, TributeResourceCommand>) {
            return player_slot_from_legacy(value.from) == player;
        } else if constexpr (
            std::is_same_v<Type, SetDiplomacyCommand>) {
            return player_slot_from_legacy(value.player) == player;
        }
        return false;
    }, command);
}

}  // namespace aoe
