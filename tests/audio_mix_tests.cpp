#include "aoe/audio_mix.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

bool close(float left, float right) {
    return std::fabs(left - right) < 0.0001F;
}

}  // namespace

int main() {
    aoe::ReconstructionSettings settings;
    settings.music_volume = 60;
    settings.effects_volume = 50;
    settings.combat_volume = 80;
    settings.interface_volume = 30;
    settings.ambient_volume = 20;

    aoe::AudioMix mix = aoe::AudioMix::from_settings(settings);
    require(close(mix.music_gain(), 0.6F), "music bus");
    require(
        close(mix.category_gain(aoe::AudioCategory::combat), 0.4F),
        "combat bus composes master and category"
    );
    require(
        close(mix.category_gain(aoe::AudioCategory::interface), 0.15F),
        "interface bus"
    );
    require(close(mix.ambience_gain(), 0.1F), "ambient bus");

    mix.paused = true;
    require(close(mix.music_gain(), 0.0F), "pause stops music");
    require(close(mix.ambience_gain(), 0.0F), "pause stops ambience");
    require(
        close(mix.category_gain(aoe::AudioCategory::interface), 0.15F),
        "pause preserves interface feedback"
    );

    mix.focused = false;
    require(
        close(mix.category_gain(aoe::AudioCategory::interface), 0.0F),
        "focus loss silences all buses"
    );
    mix.focused = true;
    mix.muted = true;
    require(close(mix.music_gain(), 0.0F), "mute silences music");
    require(
        close(mix.category_gain(aoe::AudioCategory::combat), 0.0F),
        "mute silences effects"
    );

    std::cout << "audio mix tests passed\n";
}
