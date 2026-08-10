#include "aoe/audio_system.hpp"
#include "aoe/asset_root.hpp"
#include "aoe/legacy_sound_resolver.hpp"

#include <emscripten/emscripten.h>

#include <algorithm>

namespace aoe {

namespace {

EM_JS(bool, browser_audio_start, (), {
    if (Module.audioState) return true;
    if (!Module.browserAudioTelemetry) Module.browserAudioTelemetry = {
      starts: 0,
      stops: 0,
      musicPlayAttempts: 0,
      effectPlayAttempts: 0,
      liveMusicInstances: 0,
      liveEffectInstances: 0,
      errors: []
    };
    const telemetry = Module.browserAudioTelemetry;
    const state = {
      music: new Audio('game_data/Sound/music/xmusic1.mp3'),
      effects: new Set(),
      muted: false,
      paused: false,
      awaitingGesture: false,
      unlock: null
    };
    state.music.loop = true;
    state.music.preload = 'metadata';
    Module.audioState = state;
    telemetry.starts += 1;
    telemetry.liveMusicInstances += 1;
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
    };
    window.addEventListener('pointerdown', state.unlock, true);
    window.addEventListener('keydown', state.unlock, true);
    state.music.addEventListener('error', () => fail(
      'Music media error', state.music.error?.message || state.music.error?.code
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
    for (const effect of state.effects) effect.pause();
    window.removeEventListener('pointerdown', state.unlock, true);
    window.removeEventListener('keydown', state.unlock, true);
    state.music.removeAttribute('src');
    state.music.load();
    state.effects.clear();
    Module.browserAudioTelemetry.stops += 1;
    Module.browserAudioTelemetry.liveMusicInstances -= 1;
    Module.browserAudioTelemetry.liveEffectInstances = 0;
    Module.audioState = null;
});

EM_JS(void, browser_audio_set_paused, (bool paused), {
    const state = Module.audioState;
    if (!state) return;
    if (state.paused === paused) return;
    state.paused = paused;
    if (paused) {
      state.music.pause();
      for (const effect of state.effects) effect.pause();
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
      for (const effect of state.effects) {
        const effectPlay = effect.play();
        if (effectPlay) effectPlay.catch(() => {});
      }
    }
});

EM_JS(void, browser_audio_set_muted, (bool muted), {
    const state = Module.audioState;
    if (!state) return;
    state.muted = muted;
    state.music.muted = muted;
    for (const effect of state.effects) effect.muted = muted;
});

EM_JS(void, browser_audio_play_effect,
      (const char* source, float gain, float pan), {
    const state = Module.audioState;
    if (!state || state.muted || state.paused) return;
    const effect = new Audio(UTF8ToString(source));
    effect.preload = 'auto';
    effect.volume = Math.max(0, Math.min(1, gain));
    effect.muted = state.muted;
    effect.dataset.pan = String(pan);
    state.effects.add(effect);
    Module.browserAudioTelemetry.effectPlayAttempts += 1;
    Module.browserAudioTelemetry.liveEffectInstances = state.effects.size;
    const release = () => {
      state.effects.delete(effect);
      Module.browserAudioTelemetry.liveEffectInstances = state.effects.size;
      effect.removeAttribute('src');
      effect.load();
    };
    effect.addEventListener('ended', release, {once: true});
    effect.addEventListener('error', () => {
      const message = 'Effect media error: ' +
        (effect.error?.message || effect.error?.code);
      Module.browserAudioTelemetry.errors.push(message);
      release();
    }, {once: true});
    const play = effect.play();
    if (play) play.catch(reason => {
      if (reason?.name === 'NotAllowedError') {
        state.awaitingGesture = true;
      } else {
        Module.browserAudioTelemetry.errors.push(
          'Effect playback failed: ' + reason
        );
      }
      release();
    });
});

}  // namespace

struct AudioSystem::Impl {
    std::unique_ptr<LegacySoundResolver> sounds;
    AudioMix mix;
};

std::unique_ptr<AudioSystem> AudioSystem::start_from_environment() {
    if (!browser_audio_start()) return nullptr;
    auto impl = std::make_unique<Impl>();
    const auto root = configured_asset_root();
    if (root) {
        try {
            impl->sounds = std::make_unique<LegacySoundResolver>(*root, false);
        }
        catch (const std::exception&) {}
    }
    return std::unique_ptr<AudioSystem>(new AudioSystem(std::move(impl)));
}

AudioSystem::AudioSystem(std::unique_ptr<Impl> impl)
    : impl_(std::move(impl)) {}
AudioSystem::~AudioSystem() {
    browser_audio_stop();
}
AudioSystem::AudioSystem(AudioSystem&&) noexcept = default;
AudioSystem& AudioSystem::operator=(AudioSystem&&) noexcept = default;

void AudioSystem::set_listener_civilization(Civilization civilization) {
    if (impl_ && impl_->sounds) {
        impl_->sounds->set_listener_civilization(civilization);
    }
}
void AudioSystem::set_locale(std::string_view) {}
void AudioSystem::set_music_context(AudioMusicContext, bool) {}
void AudioSystem::play_taunt(unsigned, std::string_view) {}
void AudioSystem::play_narration(const std::filesystem::path&) {}
bool AudioSystem::play_graphic_frame_sounds(
    int slp_id, int frame, int angle, float gain, float pan,
    std::optional<Civilization> civilization
) {
    if (!impl_ || !impl_->sounds) return false;
    const auto sounds = impl_->sounds->graphic_frame_sound_ids(
        slp_id, frame, angle
    );
    for (int sound : sounds) {
        play_effect(sound, AudioCategory::combat, gain, pan, civilization);
    }
    return !sounds.empty();
}
void AudioSystem::set_terrain_ambience(Terrain, std::uint64_t) {}
void AudioSystem::apply_mix(const AudioMix& mix) { impl_->mix = mix; }
void AudioSystem::set_focused(bool) {}
void AudioSystem::set_paused(bool paused) {
    browser_audio_set_paused(paused);
}
void AudioSystem::set_muted(bool muted) {
    browser_audio_set_muted(muted);
}
void AudioSystem::play_effect(
    int sound_id, AudioCategory category, float spatial_gain, float pan,
    std::optional<Civilization> civilization
) {
    if (!impl_ || !impl_->sounds) return;
    const auto path = impl_->sounds->resolve(sound_id, civilization);
    if (!path) return;
    std::string source = path->generic_string();
    if (source.starts_with('/')) source.erase(0, 1);
    browser_audio_play_effect(
        source.c_str(),
        impl_->mix.category_gain(category) *
            std::clamp(spatial_gain, 0.0F, 1.0F),
        pan
    );
}
void AudioSystem::play_named_interface_effect(std::string_view) {
    // Named WAV interface effects have no legacy DAT ID and are not part of
    // generated MP3 catalog yet.
}
void AudioSystem::update() {}

}  // namespace aoe
