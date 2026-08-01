#include "aoe/gameplay_test_api.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

}  // namespace

int main() {
    aoe::Simulation simulation = aoe::Simulation::create_demo();
    const auto villager = simulation.units().front().id;
    const auto initial_tick = simulation.tick_number();

    const std::string listed = aoe::GameplayTestApi::execute(
        simulation, aoe::Player::blue, "list_units"
    );
    require(listed.find("\"units\":[") != std::string::npos,
            "list_units must include semantic unit state");
    require(listed.find("\"resources\":") != std::string::npos,
            "state must include economy resources");

    const std::string selected = aoe::GameplayTestApi::execute(
        simulation,
        aoe::Player::blue,
        "select " + std::to_string(villager)
    );
    require(selected.find("\"ok\":true") != std::string::npos,
            "owned unit should be selectable by id");
    require(simulation.selected_units() ==
                std::vector<aoe::EntityId>{villager},
            "selection should update live simulation");

    const std::string advanced = aoe::GameplayTestApi::execute(
        simulation, aoe::Player::blue, "advance 5"
    );
    require(advanced.find("\"ok\":true") != std::string::npos,
            "advance should return state");
    require(simulation.tick_number() == initial_tick + 5,
            "advance should run requested deterministic ticks");
    require(aoe::GameplayTestApi::execute(
                simulation, aoe::Player::blue, "advance 10001"
            ).find("\"ok\":false") != std::string::npos,
            "advance must enforce bounded work");

    const std::filesystem::path directory =
        std::filesystem::temp_directory_path() /
        "aoe-gameplay-test-api-tests";
    std::filesystem::remove_all(directory);
    aoe::GameplayTestApi api(directory);
    {
        std::ofstream commands(
            directory / "commands.jsonl", std::ios::app
        );
        commands << "request-1\tstate\n";
    }
    api.poll(simulation, aoe::Player::blue);
    std::ifstream responses(directory / "responses.jsonl");
    const std::string response{
        std::istreambuf_iterator<char>{responses},
        std::istreambuf_iterator<char>{}
    };
    require(response.find("\"id\":\"request-1\"") !=
                std::string::npos,
            "file boundary must correlate response id");
    std::filesystem::remove_all(directory);
    return 0;
}
