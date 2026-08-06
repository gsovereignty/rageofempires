#include "aoe/audio_system.hpp"

#include <emscripten/emscripten.h>

namespace aoe {

namespace {

EM_JS(bool, browser_audio_start, (), {
    if (Module.audioState) return true;
    const state = {
      music: new Audio('game_data/Sound/music/xmusic1.mp3'),
      effect: new Audio('game_data/Taunt/en/03%20Food,%20please.mp3'),
      muted: false,
      paused: false
    };
    state.music.loop = true;
    state.music.preload = 'metadata';
    state.effect.preload = 'auto';
    Module.audioState = state;
    const play = state.music.play();
    if (play) play.catch(reason => Module.reportFailure(
      'Music unlock failed: ' + reason
    ));
    return true;
});

EM_JS(void, browser_audio_stop, (), {
    const state = Module.audioState;
    if (!state) return;
    state.music.pause();
    state.effect.pause();
    state.music.removeAttribute('src');
    state.effect.removeAttribute('src');
    state.music.load();
    state.effect.load();
    Module.audioState = null;
});

EM_JS(void, browser_audio_set_paused, (bool paused), {
    const state = Module.audioState;
    if (!state) return;
    state.paused = paused;
    if (paused) {
      state.music.pause();
    } else if (!state.muted) {
      const play = state.music.play();
      if (play) play.catch(() => {});
    }
});

EM_JS(void, browser_audio_set_muted, (bool muted), {
    const state = Module.audioState;
    if (!state) return;
    state.muted = muted;
    state.music.muted = muted;
    state.effect.muted = muted;
});

EM_JS(void, browser_audio_play_effect, (), {
    const state = Module.audioState;
    if (!state || state.muted || state.paused) return;
    state.effect.currentTime = 0;
    const play = state.effect.play();
    if (play) play.catch(() => {});
});

}  // namespace

struct AudioSystem::Impl {};

std::unique_ptr<AudioSystem> AudioSystem::start_from_environment() {
    if (!browser_audio_start()) return nullptr;
    return std::unique_ptr<AudioSystem>(
        new AudioSystem(std::make_unique<Impl>())
    );
}

AudioSystem::AudioSystem(std::unique_ptr<Impl> impl)
    : impl_(std::move(impl)) {}
AudioSystem::~AudioSystem() {
    browser_audio_stop();
}
AudioSystem::AudioSystem(AudioSystem&&) noexcept = default;
AudioSystem& AudioSystem::operator=(AudioSystem&&) noexcept = default;

void AudioSystem::set_listener_civilization(Civilization) {}
void AudioSystem::set_locale(std::string_view) {}
void AudioSystem::set_music_context(AudioMusicContext, bool) {}
void AudioSystem::play_taunt(unsigned, std::string_view) {}
void AudioSystem::play_narration(const std::filesystem::path&) {}
bool AudioSystem::play_graphic_frame_sounds(
    int, int, int, float, float, std::optional<Civilization>
) {
    return false;
}
void AudioSystem::set_terrain_ambience(Terrain, std::uint64_t) {}
void AudioSystem::apply_mix(const AudioMix&) {}
void AudioSystem::set_focused(bool) {}
void AudioSystem::set_paused(bool paused) {
    browser_audio_set_paused(paused);
}
void AudioSystem::set_muted(bool muted) {
    browser_audio_set_muted(muted);
}
void AudioSystem::play_effect(
    int, AudioCategory, float, float, std::optional<Civilization>
) {
    browser_audio_play_effect();
}
void AudioSystem::play_named_interface_effect(std::string_view) {
    browser_audio_play_effect();
}
void AudioSystem::update() {}

}  // namespace aoe
