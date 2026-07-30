#include "aoe/save_browser.hpp"
#include "aoe/format_versions.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace {
int failures{};
void expect(bool condition, const char* message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}
}

int main() {
    const auto root =
        std::filesystem::temp_directory_path() / "aoe-browser-tests";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    aoe::Simulation simulation = aoe::Simulation::create_demo();
    std::string error;
    expect(
        aoe::save_slot_atomic(simulation, root, "Campaign_1", false, error),
        "bounded atomic save failed"
    );
    expect(
        !aoe::save_slot_atomic(simulation, root, "Campaign_1", false, error),
        "overwrite accepted without confirmation"
    );
    expect(
        aoe::save_slot_atomic(simulation, root, "Campaign_1", true, error),
        "confirmed overwrite failed"
    );
    expect(
        !aoe::save_slot_atomic(simulation, root, "../escape", true, error),
        "path traversal slot accepted"
    );
    aoe::Replay replay;
    expect(
        aoe::replay_slot_atomic(replay, root, "match", false, error),
        "atomic replay save failed"
    );
    {
        std::ofstream incompatible{root / "future.save"};
        incompatible << "AOE-ARCHAEOLOGY-SAVE 999\n";
        std::ofstream corrupt{root / "broken.save"};
        corrupt << "AOE-ARCHAEOLOGY-SAVE "
                << aoe::reconstruction_save_version << "\nbroken\n";
        std::ofstream commercial{root / "classic.mgz"};
        commercial << "commercial bytes";
    }
    const auto entries = aoe::browse_user_data_files(root);
    const auto find = [&](std::string_view name) {
        return std::ranges::find(entries, name, &aoe::BrowserEntry::filename);
    };
    expect(
        find("Campaign_1.save") != entries.end() &&
        find("Campaign_1.save")->status ==
            aoe::BrowserFileStatus::compatible,
        "compatible save not inspected"
    );
    expect(
        find("future.save")->status ==
            aoe::BrowserFileStatus::incompatible,
        "future version not distinguished"
    );
    expect(
        find("broken.save")->status == aoe::BrowserFileStatus::corrupt,
        "corruption not diagnosed"
    );
    expect(
        find("classic.mgz")->status == aoe::BrowserFileStatus::inspect_only,
        "commercial file not inspect-only"
    );
    expect(
        !std::filesystem::exists(root / "Campaign_1.save.tmp"),
        "atomic temporary residue"
    );
    std::filesystem::remove_all(root);
    if (failures == 0) std::cout << "save browser tests passed\n";
    return failures == 0 ? 0 : 1;
}
