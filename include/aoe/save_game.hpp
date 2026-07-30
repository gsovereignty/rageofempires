#pragma once

#include <filesystem>

#include "aoe/simulation.hpp"

namespace aoe {

void save_game(const Simulation& simulation, const std::filesystem::path& path);
Simulation load_game(const std::filesystem::path& path);

}  // namespace aoe

