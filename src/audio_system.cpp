#include "aoe/audio_system.hpp"
#include "aoe/asset_root.hpp"
#include "aoe/legacy_assets.hpp"
#include "aoe/legacy_dat.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <map>
#include <string>
#include <vector>

#if AOE_HAVE_MPG123
#include <mpg123.h>
#endif

namespace aoe {
namespace {

bool enabled_value(const char* value) {
    if (value == nullptr || *value == '\0') {
        return false;
    }
    std::string normalized{value};
    std::transform(
        normalized.begin(),
        normalized.end(),
        normalized.begin(),
        [](unsigned char character) {
            return static_cast<char>(std::tolower(character));
        }
    );
    return normalized != "0" && normalized != "false" &&
           normalized != "off" && normalized != "no";
}

std::int16_t legacy_civilization_id(Civilization civilization) {
    switch (civilization) {
        case Civilization::britons: return 1;
        case Civilization::franks: return 2;
        case Civilization::goths: return 3;
        case Civilization::teutons: return 4;
        case Civilization::japanese: return 5;
        case Civilization::chinese: return 6;
        case Civilization::byzantines: return 7;
        case Civilization::persians: return 8;
        case Civilization::saracens: return 9;
        case Civilization::turks: return 10;
        case Civilization::vikings: return 11;
        case Civilization::mongols: return 12;
        case Civilization::celts: return 13;
        case Civilization::spanish: return 14;
        case Civilization::aztecs: return 15;
        case Civilization::mayans: return 16;
        case Civilization::huns: return 17;
        case Civilization::koreans: return 18;
        case Civilization::generic: return -1;
    }
    return -1;
}

float requested_gain() {
    const char* value = SDL_getenv("AOE_AUDIO_VOLUME");
    if (value == nullptr) {
        return 0.35F;
    }
    char* end{};
    const float parsed = std::strtof(value, &end);
    if (end == value) {
        return 0.35F;
    }
    return std::clamp(parsed, 0.0F, 1.0F);
}

std::filesystem::path installation_root(
    const std::filesystem::path& requested
) {
    if (std::filesystem::is_directory(requested / "Sound")) {
        return requested;
    }
    if (std::filesystem::is_directory(requested / "app" / "Sound")) {
        return requested / "app";
    }
    return requested;
}

struct AudioTrack {
    SDL_AudioStream* stream{};
    std::vector<Uint8> samples;
    std::size_t cursor{};
    bool looping{true};
    AudioCategory category{AudioCategory::combat};

    AudioTrack() = default;
    AudioTrack(const AudioTrack&) = delete;
    AudioTrack& operator=(const AudioTrack&) = delete;

    AudioTrack(AudioTrack&& other) noexcept
        : stream{other.stream}, samples{std::move(other.samples)},
          cursor{other.cursor}, looping{other.looping},
          category{other.category} {
        other.stream = nullptr;
    }

    AudioTrack& operator=(AudioTrack&& other) noexcept {
        if (this != &other) {
            if (stream != nullptr) {
                SDL_DestroyAudioStream(stream);
            }
            stream = other.stream;
            samples = std::move(other.samples);
            cursor = other.cursor;
            looping = other.looping;
            category = other.category;
            other.stream = nullptr;
        }
        return *this;
    }

    ~AudioTrack() {
        if (stream != nullptr) {
            SDL_DestroyAudioStream(stream);
        }
    }
};

void SDLCALL refill_track(
    void* userdata,
    SDL_AudioStream* stream,
    int additional_amount,
    int
) {
    auto* track = static_cast<AudioTrack*>(userdata);
    if (additional_amount <= 0 || track->samples.empty()) {
        return;
    }
    while (additional_amount > 0 &&
           (track->looping || track->cursor < track->samples.size())) {
        const std::size_t available =
            track->samples.size() - track->cursor;
        const int amount = std::min(
            additional_amount,
            static_cast<int>(available)
        );
        const Uint8* source =
            track->samples.data() + track->cursor;
        if (!SDL_PutAudioStreamData(stream, source, amount)) {
            return;
        }
        track->cursor += static_cast<std::size_t>(amount);
        if (track->looping &&
            track->cursor == track->samples.size()) {
            track->cursor = 0;
        }
        additional_amount -= amount;
    }
}

bool begin_playback(
    AudioTrack& track,
    const SDL_AudioSpec& spec,
    float gain
) {
    if (track.samples.empty()) {
        return false;
    }
    track.stream = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
        &spec,
        refill_track,
        &track
    );
    if (track.stream == nullptr) {
        return false;
    }
    SDL_SetAudioStreamGain(track.stream, gain);
    if (!SDL_ResumeAudioStreamDevice(track.stream)) {
        SDL_DestroyAudioStream(track.stream);
        track.stream = nullptr;
        return false;
    }
    return true;
}

bool load_wav_track(
    AudioTrack& track,
    const std::filesystem::path& path,
    float gain,
    bool looping = true
) {
    SDL_AudioSpec spec{};
    Uint8* buffer{};
    Uint32 length{};
    if (!SDL_LoadWAV(path.string().c_str(), &spec, &buffer, &length)) {
        return false;
    }
    track.samples.assign(buffer, buffer + length);
    SDL_free(buffer);
    track.looping = looping;
    return begin_playback(track, spec, gain);
}

bool load_wav_bytes(
    AudioTrack& track,
    std::span<const std::byte> bytes,
    float gain
) {
    SDL_IOStream* source = SDL_IOFromConstMem(bytes.data(), bytes.size());
    if (source == nullptr) {
        return false;
    }
    SDL_AudioSpec spec{};
    Uint8* buffer{};
    Uint32 length{};
    if (!SDL_LoadWAV_IO(source, true, &spec, &buffer, &length)) {
        return false;
    }
    track.samples.assign(buffer, buffer + length);
    SDL_free(buffer);
    track.looping = false;
    return begin_playback(track, spec, gain);
}

#if AOE_HAVE_MPG123
bool load_mp3_track(
    AudioTrack& track,
    const std::filesystem::path& path,
    float gain
) {
    int error{};
    std::unique_ptr<mpg123_handle, decltype(&mpg123_delete)> decoder{
        mpg123_new(nullptr, &error),
        mpg123_delete
    };
    if (decoder == nullptr ||
        mpg123_open_fixed(
            decoder.get(),
            path.string().c_str(),
            MPG123_STEREO,
            MPG123_ENC_SIGNED_16
        ) != MPG123_OK) {
        return false;
    }

    long rate{};
    int channels{};
    int encoding{};
    if (mpg123_getformat(
            decoder.get(),
            &rate,
            &channels,
            &encoding
        ) != MPG123_OK ||
        rate <= 0 || channels != 2 || encoding != MPG123_ENC_SIGNED_16) {
        return false;
    }

    const std::size_t block_size = mpg123_outblock(decoder.get());
    std::vector<Uint8> block(std::max<std::size_t>(block_size, 4096));
    while (true) {
        std::size_t decoded{};
        const int result = mpg123_read(
            decoder.get(),
            block.data(),
            block.size(),
            &decoded
        );
        track.samples.insert(
            track.samples.end(),
            block.begin(),
            block.begin() + static_cast<std::ptrdiff_t>(decoded)
        );
        if (result == MPG123_DONE) {
            break;
        }
        if (result != MPG123_OK) {
            return false;
        }
    }

    const SDL_AudioSpec spec{
        SDL_AUDIO_S16,
        channels,
        static_cast<int>(rate)
    };
    track.looping = false;
    return begin_playback(track, spec, gain);
}
#endif

}  // namespace

struct AudioSystem::Impl {
    AudioTrack music;
    AudioTrack water_ambience;
    std::vector<std::filesystem::path> music_paths;
    std::size_t next_music_index{};
    std::filesystem::path root;
    std::unique_ptr<LegacyDatFile> dat;
    std::unique_ptr<LegacyWavResources> sounds;
    std::vector<std::unique_ptr<AudioTrack>> effects;
    std::int16_t listener_civilization{-1};
    AudioMix mix;
    float environment_gain{0.35F};
    bool trace{};
#if AOE_HAVE_MPG123
    bool owns_mpg123{};
#endif

    bool start_next_music();
    void apply_stream_gains();

    ~Impl() {
        // SdlApp currently owns SDL_Quit. SDL already closes attached streams
        // if it shuts down first; do not destroy their stale handles again.
        if (SDL_WasInit(SDL_INIT_AUDIO) != 0) {
            music = {};
            water_ambience = {};
            effects.clear();
        } else {
            music.stream = nullptr;
            water_ambience.stream = nullptr;
            for (auto& effect : effects) {
                effect->stream = nullptr;
            }
            effects.clear();
        }
#if AOE_HAVE_MPG123
        if (owns_mpg123) {
            mpg123_exit();
        }
#endif
    }
};

void AudioSystem::Impl::apply_stream_gains() {
    if (music.stream != nullptr) {
        SDL_SetAudioStreamGain(
            music.stream, environment_gain * mix.music_gain()
        );
    }
    if (water_ambience.stream != nullptr) {
        SDL_SetAudioStreamGain(
            water_ambience.stream,
            environment_gain * 0.45F * mix.ambience_gain()
        );
    }
    for (auto& effect : effects) {
        if (effect->stream != nullptr) {
            SDL_SetAudioStreamGain(
                effect->stream,
                environment_gain * mix.category_gain(effect->category)
            );
        }
    }
}

bool AudioSystem::Impl::start_next_music() {
    if (music_paths.empty()) {
        return false;
    }
    music = {};
    const std::size_t attempts = music_paths.size();
    for (std::size_t attempt = 0; attempt < attempts; ++attempt) {
        const std::filesystem::path& path =
            music_paths[next_music_index];
        next_music_index =
            (next_music_index + 1) % music_paths.size();
        std::string extension = path.extension().string();
        std::ranges::transform(
            extension, extension.begin(),
            [](unsigned char character) {
                return static_cast<char>(std::tolower(character));
            }
        );
        bool loaded = false;
        if (extension == ".wav") {
            loaded = load_wav_track(
                music, path, environment_gain * mix.music_gain(), false
            );
        }
#if AOE_HAVE_MPG123
        else if (extension == ".mp3" && owns_mpg123) {
            loaded = load_mp3_track(
                music, path, environment_gain * mix.music_gain()
            );
        }
#endif
        if (loaded) {
            if (trace) {
                std::cerr << "Audio music " << path.filename().string()
                          << '\n';
            }
            return true;
        }
        music = {};
    }
    return false;
}

std::unique_ptr<AudioSystem> AudioSystem::start_from_environment() {
    if (enabled_value(SDL_getenv("AOE_MUTE"))) {
        return nullptr;
    }
    const auto configured_root = configured_asset_root();
    if (!configured_root) {
        return nullptr;
    }

    const std::filesystem::path root =
        installation_root(*configured_root);
    const std::filesystem::path ambience_path =
        root / "Sound" / "terrain" / "Wave1.wav";
    const std::vector<std::filesystem::path> music_paths =
        discover_legacy_music_tracks(root);
    const std::filesystem::path dat_path =
        root / "Data" / "empires2_x1_p1.dat";
    const std::array sound_archives{
        root / "Data" / "sounds.drs",
        root / "Data" / "sounds_x1.drs",
        root / "Data" / "sounds_x2.drs"
    };
    const bool has_sound_archive = std::ranges::any_of(
        sound_archives,
        [](const std::filesystem::path& path) {
            return std::filesystem::is_regular_file(path);
        }
    );
    if (!std::filesystem::is_regular_file(ambience_path) &&
        music_paths.empty() &&
        !(std::filesystem::is_regular_file(dat_path) &&
          has_sound_archive)) {
        std::cerr
            << "Audio disabled: AOE_ASSET_ROOT has no supported original "
               "audio files\n";
        return nullptr;
    }
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        std::cerr << "Audio disabled: " << SDL_GetError() << '\n';
        return nullptr;
    }

    auto impl = std::make_unique<Impl>();
    impl->root = root;
    impl->music_paths = music_paths;
    const float gain = requested_gain();
    impl->environment_gain = gain;
    impl->trace = enabled_value(SDL_getenv("AOE_AUDIO_TRACE"));
    try {
        if (std::filesystem::is_regular_file(dat_path)) {
            impl->dat = std::make_unique<LegacyDatFile>(
                LegacyDatFile::load(dat_path)
            );
        }
        if (has_sound_archive) {
            impl->sounds = std::make_unique<LegacyWavResources>(root);
        }
    } catch (const std::exception& error) {
        std::cerr << "Optional effects disabled: " << error.what() << '\n';
        impl->dat.reset();
        impl->sounds.reset();
    }
    bool playing = false;
    if (std::filesystem::is_regular_file(ambience_path)) {
        playing = load_wav_track(
            impl->water_ambience,
            ambience_path,
            gain * 0.45F * impl->mix.ambience_gain()
        );
    }

#if AOE_HAVE_MPG123
    if (std::ranges::any_of(
            music_paths,
            [](const std::filesystem::path& path) {
                std::string extension = path.extension().string();
                std::ranges::transform(
                    extension, extension.begin(),
                    [](unsigned char character) {
                        return static_cast<char>(
                            std::tolower(character)
                        );
                    }
                );
                return extension == ".mp3";
            }
        ) && mpg123_init() == MPG123_OK) {
        impl->owns_mpg123 = true;
    }
#endif
    playing = impl->start_next_music() || playing;

    if (!playing && impl->sounds == nullptr) {
        std::cerr << "Audio disabled: original files could not be played\n";
        return nullptr;
    }
    return std::unique_ptr<AudioSystem>{
        new AudioSystem{std::move(impl)}
    };
}

AudioSystem::AudioSystem(std::unique_ptr<Impl> impl)
    : impl_{std::move(impl)} {}

AudioSystem::~AudioSystem() = default;
AudioSystem::AudioSystem(AudioSystem&&) noexcept = default;
AudioSystem& AudioSystem::operator=(AudioSystem&&) noexcept = default;

void AudioSystem::set_listener_civilization(Civilization civilization) {
    if (impl_ != nullptr) {
        impl_->listener_civilization =
            legacy_civilization_id(civilization);
    }
}

void AudioSystem::apply_mix(const AudioMix& mix) {
    if (impl_ == nullptr) return;
    impl_->mix = mix;
    impl_->apply_stream_gains();
}

void AudioSystem::set_focused(bool focused) {
    if (impl_ == nullptr) return;
    if (impl_->mix.focused == focused) return;
    impl_->mix.focused = focused;
    impl_->apply_stream_gains();
}

void AudioSystem::set_paused(bool paused) {
    if (impl_ == nullptr) return;
    if (impl_->mix.paused == paused) return;
    impl_->mix.paused = paused;
    impl_->apply_stream_gains();
}

void AudioSystem::set_muted(bool muted) {
    if (impl_ == nullptr) return;
    if (impl_->mix.muted == muted) return;
    impl_->mix.muted = muted;
    impl_->apply_stream_gains();
}

void AudioSystem::play_effect(int sound_id, AudioCategory category) {
    if (impl_ == nullptr || sound_id < 0) {
        return;
    }
    update();
    if (impl_->effects.size() >= 16) {
        return;
    }

    std::vector<std::byte> bytes;
    int resource_id = sound_id;
    if (impl_->dat != nullptr) {
        const LegacySound* sound =
            impl_->dat->sound(static_cast<std::size_t>(sound_id));
        if (sound == nullptr || sound->items.empty()) {
            return;
        }
        const LegacySoundItem* item = select_legacy_sound_item(
            *sound,
            impl_->listener_civilization
        );
        if (item == nullptr) {
            return;
        }
        resource_id = item->resource_id;
        const auto resource_exists = [this](int id) {
            return (impl_->sounds != nullptr &&
                    impl_->sounds->contains(id)) ||
                   std::filesystem::is_regular_file(
                       impl_->root / "Sound" /
                       (std::to_string(id) + ".wav")
                   );
        };
        if (!resource_exists(resource_id) &&
            impl_->listener_civilization >= 0) {
            item = select_legacy_sound_item(*sound, -1);
            if (item == nullptr) {
                return;
            }
            resource_id = item->resource_id;
        }
    }
    try {
        if (impl_->sounds != nullptr &&
            impl_->sounds->contains(resource_id)) {
            bytes = impl_->sounds->read(resource_id);
        } else {
            const std::filesystem::path loose =
                impl_->root / "Sound" /
                (std::to_string(resource_id) + ".wav");
            if (!std::filesystem::is_regular_file(loose)) {
                return;
            }
            SDL_AudioSpec spec{};
            Uint8* buffer{};
            Uint32 length{};
            if (!SDL_LoadWAV(
                    loose.string().c_str(), &spec, &buffer, &length
                )) {
                return;
            }
            auto effect = std::make_unique<AudioTrack>();
            effect->category = category;
            effect->samples.assign(buffer, buffer + length);
            SDL_free(buffer);
            effect->looping = false;
            if (begin_playback(
                    *effect,
                    spec,
                    impl_->environment_gain *
                        impl_->mix.category_gain(category)
                )) {
                if (impl_->trace) {
                    std::cerr << "Audio effect " << sound_id
                              << " loose resource " << resource_id << '\n';
                }
                impl_->effects.push_back(std::move(effect));
            }
            return;
        }
    } catch (const std::exception&) {
        return;
    }

    auto effect = std::make_unique<AudioTrack>();
    effect->category = category;
    if (load_wav_bytes(
            *effect,
            bytes,
            impl_->environment_gain * impl_->mix.category_gain(category)
        )) {
        if (impl_->trace) {
            std::cerr << "Audio effect " << sound_id
                      << " resource " << resource_id << '\n';
        }
        impl_->effects.push_back(std::move(effect));
    }
}

void AudioSystem::update() {
    if (impl_ == nullptr) {
        return;
    }
    std::erase_if(
        impl_->effects,
        [](const std::unique_ptr<AudioTrack>& effect) {
            return effect->cursor >= effect->samples.size() &&
                SDL_GetAudioStreamQueued(effect->stream) <= 0;
        }
    );
    if (!impl_->music_paths.empty() &&
        impl_->music.stream != nullptr &&
        impl_->music.cursor >= impl_->music.samples.size() &&
        SDL_GetAudioStreamQueued(impl_->music.stream) <= 0) {
        static_cast<void>(impl_->start_next_music());
    }
}

}  // namespace aoe
