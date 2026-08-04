#include <array>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

#include "aoe/rms_import.hpp"

int main(int argc, char** argv) {
    constexpr std::array families{
        std::pair{std::string_view{"arabia"}, aoe::RandomMapKind::arabia},
        std::pair{std::string_view{"black_forest"}, aoe::RandomMapKind::black_forest},
        std::pair{std::string_view{"islands"}, aoe::RandomMapKind::islands},
        std::pair{std::string_view{"rivers"}, aoe::RandomMapKind::rivers},
    };
    constexpr std::array sizes{
        std::pair{std::string_view{"tiny"}, aoe::RandomMapSize::tiny},
        std::pair{std::string_view{"normal"}, aoe::RandomMapSize::normal},
        std::pair{std::string_view{"giant"}, aoe::RandomMapSize::giant},
    };
    constexpr std::array<std::uint64_t, 2> seeds{1, 0x12345678U};
    std::vector<std::string> actual;
    for (const auto& family : families) {
        for (const auto& size : sizes) {
            for (const std::uint64_t seed : seeds) {
                aoe::RandomMapSettings settings;
                settings.kind = family.second;
                settings.size = size.second;
                settings.seed = seed;
                const aoe::RmsMapResult result = aoe::generate_rms_map(settings);
                if (!result.scenario) {
                    std::cerr << result.error << '\n';
                    return 1;
                }
                std::size_t cliffs{};
                for (int y = 0; y < result.scenario->map.height(); ++y) {
                    for (int x = 0; x < result.scenario->map.width(); ++x) {
                        cliffs += result.scenario->map.cliff_at({x, y});
                    }
                }
                std::ostringstream line;
                line << family.first << ' ' << size.first << ' '
                     << seed << ' ' << aoe::random_map_hash(*result.scenario)
                     << ' ' << cliffs;
                actual.push_back(line.str());
                std::cout << actual.back() << '\n';
            }
        }
    }
    if (argc == 2) {
        std::ifstream input(argv[1]);
        std::vector<std::string> expected;
        std::string line;
        while (std::getline(input, line)) {
            if (!line.empty() && line.front() != '#') expected.push_back(line);
        }
        if (!input.eof() || expected != actual) {
            std::cerr << "built-in RMS parity baseline mismatch\n";
            return 1;
        }
    }
}
