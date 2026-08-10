#include "aoe/audio_system.hpp"

#include <emscripten/emscripten.h>

namespace aoe {

namespace {

EM_JS(bool, browser_audio_start, (), {
    if (Module.audioState) return true;
    if (!Module.browserAudioTelemetry) Module.browserAudioTelemetry = {
      starts: 0,
      stops: 0,
      musicPlayAttempts: 0,
      effectPlayAttempts: 0,
      ignoredEffectRequests: 0,
      liveMusicInstances: 0,
      liveEffectInstances: 0,
      errors: []
    };
    const telemetry = Module.browserAudioTelemetry;
    const state = {
      music: new Audio('game_data/Sound/music/xmusic1.mp3'),
      effect: new Audio('game_data/Taunt/en/03%20Food,%20please.mp3'),
      muted: false,
      paused: false,
      awaitingGesture: false,
      unlock: null
    };
    state.music.loop = true;
    state.music.preload = 'metadata';
    state.effect.preload = 'auto';
    Module.audioState = state;
    telemetry.starts += 1;
    telemetry.liveMusicInstances += 1;
    telemetry.liveEffectInstances += 1;
    const fail = (kind, reason) => {
      const message = kind + ': ' + reason;
      telemetry.errors.push(message);
      Module.reportFailure(message);
    };
    const deferForGesture = reason => {
      if (reason?.name !== 'NotAllowedError') return false;
      state.awaitingGesture = true;
      return true;
    };
    state.unlock = () => {
      if (Module.audioState !== state || !state.awaitingGesture) return;
      state.awaitingGesture = false;
      if (!state.paused && !state.muted) {
        telemetry.musicPlayAttempts += 1;
        const musicPlay = state.music.play();
        if (musicPlay) musicPlay.catch(reason => {
          if (!deferForGesture(reason)) fail('Music unlock failed', reason);
        });
      }
      const priorMuted = state.effect.muted;
      state.effect.muted = true;
      const effectPlay = state.effect.play();
      if (effectPlay) effectPlay.then(() => {
        state.effect.pause();
        state.effect.currentTime = 0;
        state.effect.muted = priorMuted;
      }).catch(reason => {
        state.effect.muted = priorMuted;
        if (!deferForGesture(reason)) fail('Effect unlock failed', reason);
      });
    };
    window.addEventListener('pointerdown', state.unlock, true);
    window.addEventListener('keydown', state.unlock, true);
    state.music.addEventListener('error', () => fail(
      'Music media error', state.music.error?.message || state.music.error?.code
    ), {once: true});
    state.effect.addEventListener('error', () => fail(
      'Effect media error', state.effect.error?.message || state.effect.error?.code
    ), {once: true});
    telemetry.musicPlayAttempts += 1;
    const play = state.music.play();
    if (play) play.catch(reason => {
      if (reason?.name === 'AbortError' &&
          (state.paused || Module.audioState !== state)) return;
      if (!deferForGesture(reason)) fail('Music unlock failed', reason);
    });
    return true;
});

EM_JS(void, browser_audio_stop, (), {
    const state = Module.audioState;
    if (!state) return;
    state.music.pause();
    state.effect.pause();
    window.removeEventListener('pointerdown', state.unlock, true);
    window.removeEventListener('keydown', state.unlock, true);
    state.music.removeAttribute('src');
    state.effect.removeAttribute('src');
    state.music.load();
    state.effect.load();
    Module.browserAudioTelemetry.stops += 1;
    Module.browserAudioTelemetry.liveMusicInstances -= 1;
    Module.browserAudioTelemetry.liveEffectInstances -= 1;
    Module.audioState = null;
});

EM_JS(void, browser_audio_set_paused, (bool paused), {
    const state = Module.audioState;
    if (!state) return;
    if (state.paused === paused) return;
    state.paused = paused;
    if (paused) {
      state.music.pause();
    } else if (!state.muted) {
      Module.browserAudioTelemetry.musicPlayAttempts += 1;
      const play = state.music.play();
      if (play) play.catch(reason => {
        if (reason?.name === 'NotAllowedError') {
          state.awaitingGesture = true;
          return;
        }
        if (reason?.name === 'AbortError' &&
            (state.paused || Module.audioState !== state)) return;
        const message = 'Music resume failed: ' + reason;
        Module.browserAudioTelemetry.errors.push(message);
        Module.reportFailure(message);
      });
    }
});

EM_JS(void, browser_audio_set_muted, (bool muted), {
    const state = Module.audioState;
    if (!state) return;
    state.muted = muted;
    state.music.muted = muted;
    state.effect.muted = muted;
});

EM_JS(void, browser_audio_ignore_effect, (), {
    if (!Module.browserAudioTelemetry) return;
    Module.browserAudioTelemetry.ignoredEffectRequests += 1;
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
    // Browser package has no DAT/DRS effect mapping. Playing its one codec
    // probe for every legacy sound ID turns all gameplay sounds into taunt 3.
    browser_audio_ignore_effect();
}
void AudioSystem::play_named_interface_effect(std::string_view) {
    browser_audio_ignore_effect();
}
void AudioSystem::update() {}

}  // namespace aoe
