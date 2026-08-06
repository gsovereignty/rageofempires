#include "aoe/audio_system.hpp"

namespace aoe {

struct AudioSystem::Impl {};

std::unique_ptr<AudioSystem> AudioSystem::start_from_environment() {
    return nullptr;
}

AudioSystem::AudioSystem(std::unique_ptr<Impl> impl)
    : impl_(std::move(impl)) {}
AudioSystem::~AudioSystem() = default;
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
void AudioSystem::set_paused(bool) {}
void AudioSystem::set_muted(bool) {}
void AudioSystem::play_effect(
    int, AudioCategory, float, float, std::optional<Civilization>
) {}
void AudioSystem::play_named_interface_effect(std::string_view) {}
void AudioSystem::update() {}

}  // namespace aoe
