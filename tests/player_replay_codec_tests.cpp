#include "aoe/game_command.hpp"
#include "aoe/format_versions.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

int main() {
    const auto path =
        std::filesystem::temp_directory_path() /
        "aoe-player-replay-codec-golden.replay";
    aoe::Replay replay;
    replay.record(
        3,
        aoe::TributeResourceCommand{
            aoe::Player::blue,
            aoe::Player::red,
            aoe::ResourceKind::gold,
            50,
        }
    );
    replay.record(4, aoe::ResignCommand{aoe::Player::red});
    aoe::save_replay(replay, path);

    std::ifstream input(path);
    const std::string bytes{
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{},
    };
    const bool golden =
        bytes.starts_with(
            "AOE-ARCHAEOLOGY-REPLAY " +
            std::to_string(aoe::reconstruction_command_schema_version) +
            "\n"
        ) &&
        bytes.find("tribute 3 0 1 ") != std::string::npos &&
        bytes.find("resign 4 1\n") != std::string::npos;
    const aoe::Replay decoded = aoe::load_replay(path);
    std::filesystem::remove(path);
    const bool round_trip =
        decoded.commands().size() == 2 &&
        std::get<aoe::TributeResourceCommand>(
            decoded.commands()[0].command
        ).from == aoe::Player::blue &&
        std::get<aoe::TributeResourceCommand>(
            decoded.commands()[0].command
        ).to == aoe::Player::red &&
        std::get<aoe::ResignCommand>(
            decoded.commands()[1].command
        ).player == aoe::Player::red;
    if (golden && round_trip) {
        std::cout << "player replay codec tests passed\n";
        return 0;
    }
    std::cerr << "FAILED: player replay wire compatibility\n";
    return 1;
}
