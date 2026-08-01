#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

#include "aoe/simulation.hpp"

namespace aoe {

// Opt-in local automation boundary. Commands and responses use append-only
// files inside a caller-selected directory, keeping normal gameplay and
// packaged runtime behavior unchanged unless AOE_GAMEPLAY_TEST_API_DIR is set.
class GameplayTestApi {
public:
    explicit GameplayTestApi(std::filesystem::path directory);

    void poll(
        Simulation& simulation,
        Player player,
        bool match_active
    );

    [[nodiscard]] static std::string execute(
        Simulation& simulation,
        Player player,
        std::string_view command
    );
    [[nodiscard]] static std::string snapshot(
        const Simulation& simulation,
        Player player,
        bool include_units
    );

private:
    std::filesystem::path directory_;
    std::filesystem::path commands_path_;
    std::filesystem::path responses_path_;
    std::uintmax_t command_offset_{};
};

}  // namespace aoe
