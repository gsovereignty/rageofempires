#include "aoe/legacy_sound_resolver.hpp"

#include <cassert>
#include <filesystem>
#include <iostream>
#include <set>

int main() {
    const std::filesystem::path root{AOE_TEST_SOURCE_DIR "/game_data"};
    aoe::LegacySoundResolver resolver{root};
    resolver.set_listener_civilization(aoe::Civilization::britons);

    const std::set<std::string> briton_villager_move{
        "6229.mp3", "6230.mp3", "6231.mp3", "6232.mp3"
    };
    for (int attempt = 0; attempt < 20; ++attempt) {
        const auto path = resolver.resolve(301);
        assert(path.has_value());
        assert(briton_villager_move.contains(path->filename().string()));
        assert(std::filesystem::file_size(*path) > 0);
    }

    const auto trained = resolver.resolve(317);
    assert(trained.has_value());
    assert(trained->filename() == "5353.mp3");

    assert(!resolver.resolve(-1).has_value());
    assert(!resolver.resolve(99999).has_value());
    std::cout << "legacy sound resolver tests passed\n";
}
