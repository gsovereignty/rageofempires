#pragma once

#include <string_view>

#include "aoe/random_map.hpp"
#include "aoe/scenario.hpp"

namespace aoe {

enum class FrontendGameMode {
    standard,
    regicide,
    death_match,
    learn_to_play,
};

struct ZoneServiceContract {
    std::string_view service_name;
    std::string_view original_url;
    bool available;
    std::string_view status;
    std::string_view supported_alternative;
};

[[nodiscard]] Scenario configure_frontend_game_mode(
    Scenario scenario,
    FrontendGameMode mode
);

[[nodiscard]] Scenario make_learn_to_play_scenario(
    std::uint64_t seed = 1
);

[[nodiscard]] const ZoneServiceContract& zone_service_contract();

}  // namespace aoe
