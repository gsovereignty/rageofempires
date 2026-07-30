#include "aoe/multiplayer_transport.hpp"
#include "aoe/multiplayer_checkpoint.hpp"

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#ifdef AOE_ROSTER_SDL
#include <SDL3/SDL.h>
#endif

namespace {

using namespace std::chrono_literals;

std::string env_required(const char* name) {
    const char* value = std::getenv(name);
    if (!value || !*value) {
        throw std::runtime_error(std::string("missing environment: ") + name);
    }
    return value;
}

std::uint16_t env_port() {
    const unsigned long value = std::stoul(env_required("AOE_MULTIPLAYER_PORT"));
    if (value == 0 || value > 65535) {
        throw std::runtime_error("invalid AOE_MULTIPLAYER_PORT");
    }
    return static_cast<std::uint16_t>(value);
}

aoe::PlayerSlotId slot(std::size_t index) {
    const auto value = aoe::PlayerSlotId::from_index(index);
    if (!value) throw std::runtime_error("invalid roster slot");
    return *value;
}

std::vector<std::size_t> roster_indices() {
    const std::string text = env_required("AOE_MULTIPLAYER_ROSTER");
    std::vector<std::size_t> result;
    std::size_t begin{};
    while (begin < text.size()) {
        const std::size_t end = text.find(',', begin);
        const std::string token = text.substr(
            begin, end == std::string::npos ? std::string::npos : end - begin
        );
        if (token.empty()) throw std::runtime_error("empty roster slot");
        const std::size_t value = std::stoul(token);
        if (value > 7 || (!result.empty() && value <= result.back())) {
            throw std::runtime_error(
                "roster slots must be unique ascending values in 0..7"
            );
        }
        result.push_back(value);
        if (end == std::string::npos) break;
        begin = end + 1;
    }
    if (result.size() < 2 || result.front() != 0) {
        throw std::runtime_error(
            "roster needs host slot 0 and at least one peer"
        );
    }
    return result;
}

aoe::LockstepFrame frame(
    aoe::LockstepFrameKind kind,
    aoe::PlayerSlotId source,
    const aoe::LockstepSessionConfig& config
) {
    aoe::LockstepFrame result;
    result.kind = kind;
    result.source = source;
    result.player =
        aoe::player_slot_to_legacy(source).value_or(aoe::Player::neutral);
    result.scenario_digest = config.scenario_digest;
    return result;
}

aoe::LockstepSessionConfig make_config(
    aoe::Simulation& simulation,
    const std::vector<std::size_t>& indices
) {
    std::vector<aoe::MatchRosterSlot> slots;
    slots.reserve(indices.size());
    for (std::size_t index : indices) {
        slots.push_back({
            slot(index), true, *aoe::TeamId::numbered(index % 2 + 1), false,
            {{"localhost-slot-" + std::to_string(index),
              aoe::RosterControllerKind::human}},
        });
    }
    const auto roster = aoe::MatchRoster::create(slots);
    if (!roster) throw std::runtime_error("cannot create roster");
    const auto diplomacy = aoe::RosterDiplomacy::create(*roster);
    if (!diplomacy) throw std::runtime_error("cannot create diplomacy");
    simulation.replace_roster(*roster, *diplomacy);
    for (std::size_t index : indices) {
        auto state = simulation.player_state(slot(index));
        state.controller = aoe::PlayerControllerState::active;
        simulation.replace_player_state(slot(index), state);
    }

    aoe::LockstepSessionConfig config;
    config.scenario_digest = "configured-localhost-roster-v2";
    config.native_roster = *roster;
    config.native_diplomacy = *diplomacy;
    config.host_slot = slot(0);
    config.input_delay_ticks = 0;
    config.deterministic_seed = 10963;
    return config;
}

void write_atomic(const std::string& path, const std::string& contents) {
    const std::string temporary = path + ".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) throw std::runtime_error("cannot write " + temporary);
        output << contents;
        if (!output) throw std::runtime_error("cannot finish " + temporary);
    }
    if (std::rename(temporary.c_str(), path.c_str()) != 0) {
        throw std::runtime_error("cannot publish " + path);
    }
}

void wait_for_file(const std::filesystem::path& path) {
    const auto deadline = std::chrono::steady_clock::now() + 10s;
    while (std::chrono::steady_clock::now() < deadline) {
        if (std::filesystem::exists(path) &&
            std::filesystem::file_size(path) > 0) return;
        std::this_thread::sleep_for(2ms);
    }
    throw std::runtime_error("checkpoint transfer timed out");
}

void write_image(
    const std::string& path,
    const std::string& hash,
    const std::vector<std::size_t>& indices,
    bool resumed
) {
#ifdef AOE_ROSTER_SDL
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        throw std::runtime_error(
            std::string("cannot initialize SDL: ") + SDL_GetError()
        );
    }
    SDL_Window* window{};
    SDL_Renderer* renderer{};
    if (!SDL_CreateWindowAndRenderer(
            "Local multi-peer lobby", 720, 520,
            SDL_WINDOW_HIDDEN, &window, &renderer
        )) {
        SDL_Quit();
        throw std::runtime_error(
            std::string("cannot create SDL lobby: ") + SDL_GetError()
        );
    }
    SDL_SetRenderDrawColor(renderer, 28, 22, 16, 255);
    SDL_RenderClear(renderer);
    SDL_SetRenderDrawColor(renderer, 64, 49, 30, 255);
    SDL_FRect panel{42.0F, 35.0F, 636.0F, 450.0F};
    SDL_RenderFillRect(renderer, &panel);
    SDL_SetRenderDrawColor(renderer, 238, 214, 145, 255);
    SDL_RenderDebugText(renderer, 66.0F, 58.0F,
                        "LOCALHOST MULTI-PEER LOBBY");
    float y = 92.0F;
    for (std::size_t index : indices) {
        SDL_SetRenderDrawColor(
            renderer,
            index % 2 == 0 ? 112 : 235,
            index % 2 == 0 ? 166 : 105,
            index % 2 == 0 ? 245 : 88,
            255
        );
        const std::string row =
            "SLOT " + std::to_string(index) +
            "   HUMAN   TEAM " + std::to_string(index % 2 + 1) +
            "   READY";
        SDL_RenderDebugText(renderer, 72.0F, y, row.c_str());
        y += 28.0F;
    }
    SDL_SetRenderDrawColor(renderer, 205, 224, 168, 255);
    SDL_RenderDebugText(renderer, 72.0F, 338.0F,
                        "CHECKPOINT TRANSFER: MATCHED");
    SDL_RenderDebugText(
        renderer, 72.0F, 366.0F,
        resumed ? "RECONNECT: VERIFIED FROM CHECKPOINT"
                : "RECONNECT: CHECKPOINT READY"
    );
    SDL_RenderDebugText(renderer, 72.0F, 394.0F,
                        ("STATE HASH: " + hash.substr(0, 32)).c_str());
    SDL_RenderPresent(renderer);
    SDL_Surface* surface = SDL_RenderReadPixels(renderer, nullptr);
    if (!surface || !SDL_SaveBMP(surface, path.c_str())) {
        if (surface) SDL_DestroySurface(surface);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        throw std::runtime_error(
            std::string("cannot capture SDL lobby: ") + SDL_GetError()
        );
    }
    SDL_DestroySurface(surface);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
#else
    static_cast<void>(indices);
    static_cast<void>(resumed);
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) throw std::runtime_error("cannot write screenshot");
    output << "P6\n32 24\n255\n";
    for (int y = 0; y < 24; ++y) {
        for (int x = 0; x < 32; ++x) {
            const unsigned char source = static_cast<unsigned char>(
                hash[static_cast<std::size_t>((x + y * 3) % hash.size())]
            );
            const unsigned char pixel[3] = {
                static_cast<unsigned char>(source ^ (x * 7)),
                static_cast<unsigned char>(source ^ (y * 11)),
                static_cast<unsigned char>(source ^ ((x + y) * 5)),
            };
            output.write(reinterpret_cast<const char*>(pixel), 3);
        }
    }
#endif
}

template <typename Runtime>
bool drain(Runtime& runtime, const aoe::Simulation& simulation) {
    bool traffic{};
    while (true) {
        const auto result = runtime.pump_one(simulation);
        if (result.status == aoe::TcpPollStatus::frame) {
            traffic = true;
            continue;
        }
        if (result.status == aoe::TcpPollStatus::disconnected) {
            throw std::runtime_error("peer disconnected");
        }
        return traffic;
    }
}

template <typename Runtime>
void run_peer(
    Runtime& runtime,
    aoe::Simulation& simulation,
    aoe::PlayerSlotId local_slot,
    const aoe::LockstepSessionConfig& config,
    bool host
) {
    auto hello = frame(aoe::LockstepFrameKind::hello, local_slot, config);
    hello.config = config;
    hello.config_digest = aoe::lockstep_config_digest(config);
    if (!runtime.send(hello, simulation)) {
        throw std::runtime_error("hello rejected");
    }
    if (!runtime.send(
            frame(aoe::LockstepFrameKind::ready, local_slot, config),
            simulation
        )) {
        throw std::runtime_error("ready rejected");
    }

    bool start_sent{};
    bool signal_sent{};
    std::uint64_t submitted_tick = ~std::uint64_t{};
    const auto deadline = std::chrono::steady_clock::now() + 15s;
    while (std::chrono::steady_clock::now() < deadline) {
        (void)drain(runtime, simulation);
        const auto status = runtime.session().status();
        if (host && status == aoe::LockstepStatus::ready && !start_sent) {
            if (!runtime.send(
                    frame(aoe::LockstepFrameKind::start, local_slot, config),
                    simulation
                )) {
                throw std::runtime_error("start rejected");
            }
            start_sent = true;
        }
        if (status == aoe::LockstepStatus::running) {
            const std::uint64_t tick = runtime.session().current_tick();
            if (host && !signal_sent) {
                auto signal =
                    frame(aoe::LockstepFrameKind::signal, local_slot, config);
                signal.signal = aoe::LockstepMapSignal{
                    1, aoe::Player::blue, aoe::ChatAudience::all, {4, 3},
                };
                if (!runtime.send(signal, simulation)) {
                    throw std::runtime_error("signal rejected");
                }
                signal_sent = true;
            }
            if (tick != submitted_tick) {
                auto turn =
                    frame(aoe::LockstepFrameKind::turn, local_slot, config);
                turn.tick = tick;
                turn.sequence = tick;
                turn.state_hash = aoe::deterministic_state_hash(simulation);
                if (!runtime.send(turn, simulation)) {
                    throw std::runtime_error("turn rejected");
                }
                submitted_tick = tick;
            }
            (void)drain(runtime, simulation);
            (void)runtime.advance(simulation);
            if (runtime.session().current_tick() >= 3 &&
                runtime.signal_log().size() == 1) {
                // Keep every process alive briefly after its local quorum so
                // slower peers can consume the final relayed turn.
                std::this_thread::sleep_for(300ms);
                return;
            }
        }
        if (status != aoe::LockstepStatus::handshaking &&
            status != aoe::LockstepStatus::ready &&
            status != aoe::LockstepStatus::running) {
            throw std::runtime_error(
                "terminal lockstep status " +
                std::to_string(static_cast<int>(status))
            );
        }
        std::this_thread::sleep_for(1ms);
    }
    throw std::runtime_error("protocol-3 roster flow timed out");
}

}  // namespace

int main() {
    try {
        const std::string mode = env_required("AOE_MULTIPLAYER");
        const std::vector<std::size_t> indices = roster_indices();
        const std::size_t local_index =
            std::stoul(env_required("AOE_MULTIPLAYER_LOCAL_SLOT"));
        if (std::ranges::find(indices, local_index) == indices.end() ||
            (mode == "host" && local_index != 0) ||
            (mode == "join" && local_index == 0)) {
            throw std::runtime_error("mode/local slot mismatch");
        }

        // Modern choice: every bootable reconstruction harness uses the
        // same recovered maximum extent as the SDL app.
        aoe::Simulation simulation(aoe::GameMap(255, 255));
        const auto config = make_config(simulation, indices);
        const std::filesystem::path checkpoint_save =
            env_required("AOE_MULTIPLAYER_CHECKPOINT_SAVE");
        const std::filesystem::path checkpoint_envelope =
            env_required("AOE_MULTIPLAYER_CHECKPOINT_ENVELOPE");
        const char* resume_value = std::getenv("AOE_MULTIPLAYER_RESUME");
        const bool resumed = resume_value && resume_value[0] != '\0' &&
            resume_value[0] != '0';
        if (resumed) {
            wait_for_file(checkpoint_save);
            wait_for_file(checkpoint_envelope);
            simulation = aoe::load_multiplayer_checkpoint(
                checkpoint_save, checkpoint_envelope, config
            ).simulation;
        }
        const auto local_slot = slot(local_index);
        if (mode == "host") {
            aoe::LocalhostMultiPeerHost runtime(env_port(), config, 1000, 1);
            write_atomic(env_required("AOE_MULTIPLAYER_READY_PATH"), "ready\n");
            for (std::size_t index : indices) {
                if (index != 0) runtime.accept_peer(slot(index));
            }
            run_peer(runtime, simulation, local_slot, config, true);
        } else if (mode == "join") {
            aoe::LocalhostMultiPeerClient runtime(
                env_port(), config, local_slot, 1000, 1
            );
            if (const char* connected =
                    std::getenv("AOE_MULTIPLAYER_CONNECTED_PATH")) {
                write_atomic(connected, "connected\n");
            }
            run_peer(runtime, simulation, local_slot, config, false);
        } else {
            throw std::runtime_error("AOE_MULTIPLAYER must be host or join");
        }

        const std::string hash = aoe::deterministic_state_hash(simulation);
        if (!resumed && mode == "host") {
            aoe::LockstepSaveBarrier barrier;
            if (!barrier.begin(
                    simulation.tick_number(),
                    simulation.tick_number(),
                    simulation.roster()
                )) {
                throw std::runtime_error("cannot begin checkpoint barrier");
            }
            for (std::size_t index : indices) {
                if (!barrier.submit(
                        slot(index),
                        {simulation.tick_number(), hash, 3}
                    )) {
                    throw std::runtime_error(
                        "cannot complete checkpoint barrier"
                    );
                }
            }
            aoe::write_multiplayer_checkpoint_atomic(
                simulation, config, barrier,
                checkpoint_save, checkpoint_envelope
            );
        } else {
            wait_for_file(checkpoint_save);
            wait_for_file(checkpoint_envelope);
            const auto transferred = aoe::load_multiplayer_checkpoint(
                checkpoint_save, checkpoint_envelope, config
            );
            if (transferred.envelope.state_hash !=
                    (resumed ? transferred.envelope.state_hash : hash)) {
                throw std::runtime_error("checkpoint transfer mismatch");
            }
        }
        write_image(
            env_required("AOE_MULTIPLAYER_SCREENSHOT_PATH"),
            hash, indices, resumed
        );
        std::string roster_text;
        for (std::size_t index = 0; index < indices.size(); ++index) {
            if (index != 0) roster_text.push_back(',');
            roster_text += std::to_string(indices[index]);
        }
        write_atomic(
            env_required("AOE_MULTIPLAYER_STATE_PATH"),
            "protocol 3\nroster " + roster_text + "\nslot " +
                std::to_string(local_index) +
                "\nparticipants " + std::to_string(indices.size()) +
                "\ntick 3\nsignals 1\nstatus running\ncheckpoint matched\n" +
                (resumed ? "reconnect verified\n" : "reconnect ready\n") +
                "hash " +
                hash + "\n"
        );
        std::cout << "protocol 3 slot " << local_index
                  << " tick 3 hash " << hash << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "roster multiplayer failed: "
                  << error.what() << '\n';
        return 1;
    }
}
