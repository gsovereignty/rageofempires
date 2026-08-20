#include <cstdlib>
#include <iostream>
#include <filesystem>

#include "aoe/save_game.hpp"
#include "aoe/simulation.hpp"

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::abort();
    }
}

}  // namespace

int main() {
    aoe::Simulation simulation(aoe::GameMap(12, 8));
    const aoe::EntityId archer = simulation.add_unit(
        aoe::UnitKind::archer, aoe::Player::blue, {2, 3});
    simulation.add_unit(
        aoe::UnitKind::villager, aoe::Player::red, {5, 3});
    require(simulation.command_unit(archer, {5, 3}), "attack order rejected");

    bool found_attack = false;
    std::uint64_t prior_sequence = 0;
    int prior_frame = -1;
    for (int tick = 0; tick < 20; ++tick) {
        simulation.update();
        for (const auto& event : simulation.reactive_sound_events()) {
            require(event.tick == simulation.tick_number(), "stale event retained");
            require(event.sequence > prior_sequence, "event order not stable");
            prior_sequence = event.sequence;
            if (event.source_entity_id != archer ||
                event.kind != aoe::ReactiveSoundKind::graphic_frame) continue;
            found_attack = true;
            require(event.angle == 0, "east-facing attack angle incorrect");
            if (event.frame == 0) prior_frame = -1;
            require(event.frame > prior_frame, "attack frames not monotonic");
            prior_frame = event.frame;
            if (event.frame == 0)
                require(event.fallback_sound_id == 314,
                        "archer fallback identity incorrect");
        }
    }
    require(found_attack, "authoritative attack emitted no sound schedule");
    const auto save = std::filesystem::temp_directory_path() /
        "aoe-reactive-audio-scheduler.save";
    aoe::save_game(simulation, save);
    aoe::Simulation restored = aoe::load_game(save);
    std::filesystem::remove(save);
    require(restored.pending_attack_sound_frames() ==
                simulation.pending_attack_sound_frames(),
            "pending graphic frames lost across save");
    require(restored.next_reactive_sound_sequence() ==
                simulation.next_reactive_sound_sequence(),
            "event sequence lost across save");

    aoe::Simulation movement(aoe::GameMap(8, 5));
    const auto scout = movement.add_unit(
        aoe::UnitKind::scout_cavalry, aoe::Player::blue, {1, 2});
    require(movement.command_unit(scout, {4, 2}), "move order rejected");
    movement.update();
    require(!movement.reactive_sound_events().empty(), "move start silent");
    require(movement.reactive_sound_events().front().sound_id == 467,
            "move sound identity incorrect");

    aoe::Simulation gathering = aoe::Simulation::create_demo();
    const auto villager = gathering.units().front().id;
    const int initial_wood = gathering.economy(aoe::Player::blue).wood;
    require(gathering.command_unit(villager, {3, 5}),
            "gather order rejected");
    // Base wood gathering is 0.39 resource per source second, while one
    // simulation source second spans five ticks. Allow enough time to fill
    // the villager's ten-resource capacity and complete the delivery route.
    for (int tick = 0; tick < 200; ++tick) {
        gathering.update();
        for (const auto& event : gathering.reactive_sound_events()) {
            require(event.source_entity_id != villager || event.sound_id != 301,
                    "automatic gather loop emitted villager voice");
        }
    }
    require(gathering.economy(aoe::Player::blue).wood > initial_wood,
            "gather sound test never completed a drop-off cycle");

    std::cout << "Reactive audio scheduler tests passed\n";
}
