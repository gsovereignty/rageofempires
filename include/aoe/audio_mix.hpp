#pragma once

#include "aoe/settings.hpp"

#include <algorithm>

namespace aoe {

enum class AudioCategory { combat, interface, ambient };

struct AudioMix {
    float music{0.7F};
    float effects{0.7F};
    float combat{1.0F};
    float interface{1.0F};
    float ambient{1.0F};
    bool muted{};
    bool focused{true};
    bool paused{};

    static AudioMix from_settings(const ReconstructionSettings& settings) {
        constexpr float scale = 0.01F;
        return {
            settings.music_volume * scale,
            settings.effects_volume * scale,
            settings.combat_volume * scale,
            settings.interface_volume * scale,
            settings.ambient_volume * scale,
        };
    }

    [[nodiscard]] float music_gain() const noexcept {
        return enabled() && !paused ? clamp(music) : 0.0F;
    }

    [[nodiscard]] float category_gain(AudioCategory category) const noexcept {
        if (!enabled()) return 0.0F;
        float category_value{};
        switch (category) {
            case AudioCategory::combat: category_value = combat; break;
            case AudioCategory::interface: category_value = interface; break;
            case AudioCategory::ambient: category_value = ambient; break;
        }
        return clamp(effects) * clamp(category_value);
    }

    [[nodiscard]] float ambience_gain() const noexcept {
        return paused ? 0.0F : category_gain(AudioCategory::ambient);
    }

private:
    [[nodiscard]] bool enabled() const noexcept {
        return !muted && focused;
    }

    static float clamp(float value) noexcept {
        return std::clamp(value, 0.0F, 1.0F);
    }
};

}  // namespace aoe
