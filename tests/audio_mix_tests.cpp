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
    settings.music_volume = 50;
    settings.effects_volume = 40;

    aoe::AudioMix mix = aoe::AudioMix::from_settings(settings);
    require(
        close(mix.music_gain(), std::pow(10.0F, -0.5F)),
        "music dB scale"
    );
    require(
        close(mix.category_gain(aoe::AudioCategory::combat), 0.01F),
        "combat uses sound bus"
    );
    require(
        close(mix.category_gain(aoe::AudioCategory::interface), 0.01F),
        "interface uses sound bus"
    );
    require(close(mix.ambience_gain(), 0.01F), "ambience uses sound bus");

    mix.paused = true;
    require(
        close(mix.music_gain(), std::pow(10.0F, -0.5F)),
        "pause preserves music"
    );
    require(close(mix.ambience_gain(), 0.01F), "pause preserves ambience");
    require(
        close(mix.category_gain(aoe::AudioCategory::interface), 0.01F),
        "pause preserves interface feedback"
    );

    mix.focused = false;
    require(
        close(mix.category_gain(aoe::AudioCategory::interface), 0.01F),
        "focus loss preserves sound"
    );
    require(
        close(mix.music_gain(), std::pow(10.0F, -0.5F)),
        "focus loss preserves music"
    );
    mix.focused = true;
    mix.music_attenuation = 0;
    mix.sound_attenuation = 0;
    require(close(mix.music_gain(), 1.0F), "music loud endpoint");
    require(close(mix.ambience_gain(), 1.0F), "sound loud endpoint");
    mix.music_attenuation = 99;
    mix.sound_attenuation = 99;
    require(close(mix.music_gain(), 0.0F), "music quiet endpoint stops");
    require(
        close(mix.ambience_gain(), std::pow(10.0F, -4.95F)),
        "sound quiet endpoint uses -9900 hundredths dB"
    );
    mix.muted = true;
    require(close(mix.music_gain(), 0.0F), "mute silences music");
    require(
        close(mix.category_gain(aoe::AudioCategory::combat), 0.0F),
        "mute silences effects"
    );

    std::cout << "audio mix tests passed\n";
}
