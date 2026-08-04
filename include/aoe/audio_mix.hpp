#pragma once

#include "aoe/settings.hpp"

#include <algorithm>
#include <cmath>

namespace aoe {

enum class AudioCategory { combat, interface, ambient };

struct AudioMix {
    int music_attenuation{50};
    int sound_attenuation{};
    bool muted{};
    bool focused{true};
    bool paused{};

    static AudioMix from_settings(const ReconstructionSettings& settings) {
        return {
            settings.music_volume,
            settings.effects_volume,
        };
    }

    [[nodiscard]] float music_gain() const noexcept {
        if (muted || music_attenuation >= 99) return 0.0F;
        // Original MP3 music path converts slider attenuation to DirectSound
        // hundredths of a decibel, then divides it by five.
        return decibel_gain(-clamp_setting(music_attenuation) * 20);
    }

    [[nodiscard]] float category_gain(AudioCategory) const noexcept {
        if (muted) return 0.0F;
        // Original has one sound control for combat, interface, and ambience.
        return decibel_gain(-clamp_setting(sound_attenuation) * 100);
    }

    [[nodiscard]] float ambience_gain() const noexcept {
        return category_gain(AudioCategory::ambient);
    }

    [[nodiscard]] bool focus_changes_gain() const noexcept { return false; }
    [[nodiscard]] bool pause_changes_gain() const noexcept { return false; }

private:
    static int clamp_setting(int value) noexcept {
        return std::clamp(value, 0, 99);
    }

    static float decibel_gain(int hundredths_db) noexcept {
        return std::pow(10.0F, static_cast<float>(hundredths_db) / 2000.0F);
    }
};

}  // namespace aoe
