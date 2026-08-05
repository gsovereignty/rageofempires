#pragma once

#include "aoe/audio_mix.hpp"
#include "aoe/types.hpp"

#include <filesystem>
#include <memory>
#include <optional>
#include <string_view>

namespace aoe {

enum class AudioMusicContext {
    opening,
    menu,
    gameplay,
    civilization,
    countdown,
    victory,
    defeat,
    credits,
};

// Optional playback of audio from a user's own Age of Empires II installation.
// No audio is required by, copied into, or distributed with this project.
class AudioSystem {
public:
    static std::unique_ptr<AudioSystem> start_from_environment();

    // Plays an optional original WAV resource once. Missing or malformed
    // user-supplied assets are ignored without affecting gameplay.
    void set_listener_civilization(Civilization civilization);
    void set_locale(std::string_view locale);
    void set_music_context(
        AudioMusicContext context,
        bool expansion_content = true
    );
    void play_taunt(unsigned number, std::string_view locale = "en");
    void play_narration(const std::filesystem::path& filename);
    bool play_graphic_frame_sounds(
        int slp_id,
        int frame,
        int angle,
        float spatial_gain,
        float pan,
        std::optional<Civilization> source_civilization = std::nullopt
    );
    void set_terrain_ambience(Terrain terrain, std::uint64_t variation);
    void apply_mix(const AudioMix& mix);
    void set_focused(bool focused);
    void set_paused(bool paused);
    void set_muted(bool muted);
    void play_effect(
        int sound_id,
        AudioCategory category = AudioCategory::combat,
        float spatial_gain = 1.0F,
        float pan = 0.0F,
        std::optional<Civilization> source_civilization = std::nullopt
    );
    void update();

    ~AudioSystem();
    AudioSystem(AudioSystem&&) noexcept;
    AudioSystem& operator=(AudioSystem&&) noexcept;

    AudioSystem(const AudioSystem&) = delete;
    AudioSystem& operator=(const AudioSystem&) = delete;

private:
    struct Impl;

    explicit AudioSystem(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

}  // namespace aoe
