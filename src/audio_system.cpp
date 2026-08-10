#include "aoe/audio_system.hpp"
#include "aoe/localization.hpp"
#include "aoe/asset_root.hpp"
#include "aoe/legacy_assets.hpp"
#include "aoe/legacy_dat.hpp"
#include "aoe/legacy_sound_resolver.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <map>
#include <string>
#include <utility>
#include <vector>

#if AOE_HAVE_NATIVE_MP3
#include <AudioToolbox/AudioToolbox.h>
#endif

#if AOE_HAVE_MPG123
#include <mpg123.h>
#include <dlfcn.h>
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

float requested_gain() {
    const char* value = SDL_getenv("AOE_AUDIO_VOLUME");
    if (value == nullptr) {
        return 1.0F;
    }
    char* end{};
    const float parsed = std::strtof(value, &end);
    if (end == value) {
        return 1.0F;
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

std::string civilization_stream_name(Civilization civilization) {
    switch (civilization) {
        case Civilization::britons: return "British.mp3";
        case Civilization::franks: return "French.mp3";
        case Civilization::teutons: return "Teuton.mp3";
        case Civilization::goths: return "Goth.mp3";
        case Civilization::celts: return "Celt.mp3";
        case Civilization::vikings: return "Viking.mp3";
        case Civilization::byzantines: return "Byzantin.mp3";
        case Civilization::japanese: return "Japanese.mp3";
        case Civilization::chinese: return "Chinese.mp3";
        case Civilization::persians: return "Persian.mp3";
        case Civilization::saracens: return "Saracen.mp3";
        case Civilization::turks: return "Turk.mp3";
        case Civilization::mongols: return "Mongol.mp3";
        case Civilization::spanish: return "Spanish.mp3";
        case Civilization::huns: return "Huns.mp3";
        case Civilization::koreans: return "Koreans.mp3";
        case Civilization::aztecs: return "Aztecs.mp3";
        case Civilization::mayans: return "Mayans.mp3";
        case Civilization::generic: return "Random.mp3";
    }
    return "Random.mp3";
}

std::filesystem::path music_path_for(
    const std::filesystem::path& root,
    AudioMusicContext context,
    Civilization civilization,
    bool expansion
) {
    const auto stream = root / "Sound" / "stream";
    switch (context) {
        case AudioMusicContext::opening:
            return stream / (expansion ? "xopen.mp3" : "open.mp3");
        case AudioMusicContext::menu:
            return stream / (expansion ? "xtown.mp3" : "town.mp3");
        case AudioMusicContext::gameplay:
            return root / "Sound" / "music" /
                (expansion ? "xmusic1.mp3" : "music1.mp3");
        case AudioMusicContext::civilization:
            return stream / civilization_stream_name(civilization);
        case AudioMusicContext::countdown:
            return stream / "Countdwn.mp3";
        case AudioMusicContext::victory:
            return stream / (expansion ? "won2.mp3" : "won1.mp3");
        case AudioMusicContext::defeat:
            return stream / "lost.mp3";
        case AudioMusicContext::credits:
            return stream /
                (expansion ? "xcredits.mp3" : "credits.mp3");
    }
    return {};
}

bool music_context_loops(AudioMusicContext context) {
    return context == AudioMusicContext::menu ||
        context == AudioMusicContext::gameplay;
}

struct AudioTrack {
    SDL_AudioStream* stream{};
    std::vector<Uint8> samples;
    std::size_t cursor{};
    bool looping{true};
    AudioCategory category{AudioCategory::combat};
    float spatial_gain{1.0F};

    AudioTrack() = default;
    AudioTrack(const AudioTrack&) = delete;
    AudioTrack& operator=(const AudioTrack&) = delete;

    AudioTrack(AudioTrack&& other) noexcept
        : stream{other.stream}, samples{std::move(other.samples)},
          cursor{other.cursor}, looping{other.looping},
          category{other.category},
          spatial_gain{other.spatial_gain} {
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
            spatial_gain = other.spatial_gain;
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

#if AOE_HAVE_NATIVE_MP3
bool load_native_mp3_track(
    AudioTrack& track,
    const std::filesystem::path& path,
    float gain,
    bool looping
) {
    const std::string native_path = path.string();
    CFURLRef url = CFURLCreateFromFileSystemRepresentation(
        kCFAllocatorDefault,
        reinterpret_cast<const UInt8*>(native_path.data()),
        native_path.size(),
        false
    );
    if (url == nullptr) return false;

    ExtAudioFileRef file{};
    const OSStatus open_status = ExtAudioFileOpenURL(url, &file);
    CFRelease(url);
    if (open_status != noErr || file == nullptr) return false;

    AudioStreamBasicDescription source{};
    UInt32 property_size = sizeof(source);
    if (ExtAudioFileGetProperty(
            file,
            kExtAudioFileProperty_FileDataFormat,
            &property_size,
            &source
        ) != noErr ||
        source.mSampleRate <= 0.0) {
        ExtAudioFileDispose(file);
        return false;
    }

    AudioStreamBasicDescription client{};
    client.mSampleRate = source.mSampleRate;
    client.mFormatID = kAudioFormatLinearPCM;
    client.mFormatFlags =
        kLinearPCMFormatFlagIsSignedInteger |
        kLinearPCMFormatFlagIsPacked |
        kAudioFormatFlagsNativeEndian;
    client.mBytesPerPacket = 4;
    client.mFramesPerPacket = 1;
    client.mBytesPerFrame = 4;
    client.mChannelsPerFrame = 2;
    client.mBitsPerChannel = 16;
    if (ExtAudioFileSetProperty(
            file,
            kExtAudioFileProperty_ClientDataFormat,
            sizeof(client),
            &client
        ) != noErr) {
        ExtAudioFileDispose(file);
        return false;
    }

    constexpr UInt32 frame_capacity = 4096;
    std::array<Uint8, frame_capacity * 4> block{};
    while (true) {
        UInt32 frames = frame_capacity;
        AudioBufferList buffers{};
        buffers.mNumberBuffers = 1;
        buffers.mBuffers[0].mNumberChannels = 2;
        buffers.mBuffers[0].mDataByteSize = block.size();
        buffers.mBuffers[0].mData = block.data();
        if (ExtAudioFileRead(file, &frames, &buffers) != noErr) {
            ExtAudioFileDispose(file);
            return false;
        }
        if (frames == 0) break;
        track.samples.insert(
            track.samples.end(),
            block.begin(),
            block.begin() + static_cast<std::ptrdiff_t>(frames * 4)
        );
    }
    ExtAudioFileDispose(file);

    const SDL_AudioSpec spec{
        SDL_AUDIO_S16,
        2,
        static_cast<int>(std::lround(source.mSampleRate))
    };
    track.looping = looping;
    return begin_playback(track, spec, gain);
}
#endif

#if AOE_HAVE_MPG123
struct Mpg123Api {
    void* library{};
    decltype(&mpg123_init) init{};
    decltype(&mpg123_exit) exit{};
    decltype(&mpg123_new) create{};
    decltype(&mpg123_delete) destroy{};
    decltype(&mpg123_open_fixed) open_fixed{};
    decltype(&mpg123_getformat) getformat{};
    decltype(&mpg123_outblock) outblock{};
    decltype(&mpg123_read) read{};
    decltype(&mpg123_plain_strerror) plain_strerror{};
    bool initialized{};

    ~Mpg123Api() {
        if (initialized && exit != nullptr) exit();
        if (library != nullptr) dlclose(library);
    }
};

template <typename Function>
Function load_mpg123_symbol(void* library, const char* name) {
    return reinterpret_cast<Function>(dlsym(library, name));
}

std::unique_ptr<Mpg123Api> load_mpg123_api() {
    constexpr std::array candidates{
        "libmpg123.dylib",
        "libmpg123.so.0",
        "libmpg123.so",
        "/opt/homebrew/opt/mpg123/lib/libmpg123.dylib",
        "/usr/local/opt/mpg123/lib/libmpg123.dylib",
    };
    void* library{};
    for (const char* candidate : candidates) {
        library = dlopen(candidate, RTLD_NOW | RTLD_LOCAL);
        if (library != nullptr) break;
    }
    if (library == nullptr) return nullptr;

    auto api = std::make_unique<Mpg123Api>();
    api->library = library;
    api->init = load_mpg123_symbol<decltype(api->init)>(
        library, "mpg123_init"
    );
    api->exit = load_mpg123_symbol<decltype(api->exit)>(
        library, "mpg123_exit"
    );
    api->create = load_mpg123_symbol<decltype(api->create)>(
        library, "mpg123_new"
    );
    api->destroy = load_mpg123_symbol<decltype(api->destroy)>(
        library, "mpg123_delete"
    );
    api->open_fixed = load_mpg123_symbol<decltype(api->open_fixed)>(
        library, "mpg123_open_fixed"
    );
    api->getformat = load_mpg123_symbol<decltype(api->getformat)>(
        library, "mpg123_getformat"
    );
    api->outblock = load_mpg123_symbol<decltype(api->outblock)>(
        library, "mpg123_outblock"
    );
    api->read = load_mpg123_symbol<decltype(api->read)>(
        library, "mpg123_read"
    );
    api->plain_strerror =
        load_mpg123_symbol<decltype(api->plain_strerror)>(
            library, "mpg123_plain_strerror"
        );
    if (api->init == nullptr || api->exit == nullptr ||
        api->create == nullptr || api->destroy == nullptr ||
        api->open_fixed == nullptr || api->getformat == nullptr ||
        api->outblock == nullptr || api->read == nullptr ||
        api->plain_strerror == nullptr) {
        return nullptr;
    }
    const int result = api->init();
    if (result != MPG123_OK) {
        return nullptr;
    }
    api->initialized = true;
    return api;
}

bool load_mp3_track(
    AudioTrack& track,
    const std::filesystem::path& path,
    float gain,
    Mpg123Api& api,
    bool looping
) {
    int error{};
    std::unique_ptr<mpg123_handle, decltype(api.destroy)> decoder{
        api.create(nullptr, &error),
        api.destroy
    };
    if (decoder == nullptr ||
        api.open_fixed(
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
    if (api.getformat(
            decoder.get(),
            &rate,
            &channels,
            &encoding
        ) != MPG123_OK ||
        rate <= 0 || channels != 2 || encoding != MPG123_ENC_SIGNED_16) {
        return false;
    }

    const std::size_t block_size = api.outblock(decoder.get());
    std::vector<Uint8> block(std::max<std::size_t>(block_size, 4096));
    while (true) {
        std::size_t decoded{};
        const int result = api.read(
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
    track.looping = looping;
    return begin_playback(track, spec, gain);
}
#endif

}  // namespace

struct AudioSystem::Impl {
    AudioTrack music;
    AudioTrack water_ambience;
    std::filesystem::path music_path;
    std::filesystem::path ambience_path;
    std::filesystem::path root;
    std::string audio_locale{"en"};
    std::unique_ptr<LegacySoundResolver> sound_resolver;
    std::vector<std::unique_ptr<AudioTrack>> effects;
    Civilization civilization{Civilization::generic};
    AudioMusicContext music_context{AudioMusicContext::opening};
    bool expansion_content{true};
    std::optional<std::pair<AudioMusicContext, bool>> queued_music;
    AudioMix mix;
    float environment_gain{1.0F};
    bool trace{};
    bool reported_music_failure{};
#if AOE_HAVE_MPG123
    std::unique_ptr<Mpg123Api> mpg123;
#endif

    bool start_music(
        const std::filesystem::path& path,
        bool looping
    );
    bool start_loose_effect(
        const std::filesystem::path& path,
        AudioCategory category,
        float spatial_gain = 1.0F
    );
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
            environment_gain * mix.ambience_gain()
        );
    }
    for (auto& effect : effects) {
        if (effect->stream != nullptr) {
            SDL_SetAudioStreamGain(
                effect->stream,
                environment_gain * mix.category_gain(effect->category) *
                    effect->spatial_gain
            );
        }
    }
}

bool AudioSystem::Impl::start_music(
    const std::filesystem::path& path,
    bool looping
) {
    if (!std::filesystem::is_regular_file(path)) return false;
    music = {};
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
            music, path, environment_gain * mix.music_gain(), looping
        );
    }
#if AOE_HAVE_NATIVE_MP3
    else if (extension == ".mp3") {
        loaded = load_native_mp3_track(
            music,
            path,
            environment_gain * mix.music_gain(),
            looping
        );
    }
#endif
#if AOE_HAVE_MPG123
    else if (extension == ".mp3" && mpg123 != nullptr) {
        loaded = load_mp3_track(
            music,
            path,
            environment_gain * mix.music_gain(),
            *mpg123,
            looping
        );
    }
#endif
    if (loaded) {
        music_path = path;
        reported_music_failure = false;
        if (trace) {
            std::cerr << "Audio music " << path.filename().string()
                      << '\n';
        }
        return true;
    }
    if (!reported_music_failure) {
#if AOE_HAVE_NATIVE_MP3 || AOE_HAVE_MPG123
        std::cerr
            << "Music unavailable: discovered tracks could not be decoded "
               "or opened; verify files and audio device ("
            << path.parent_path().string() << ")\n";
#else
        std::cerr
            << "Music unavailable: discovered MP3 tracks but this build has "
               "no mpg123 decoder; install mpg123 and reconfigure with "
               "-DAOE_ENABLE_MPG123=ON\n";
#endif
        reported_music_failure = true;
    }
    return false;
}

bool AudioSystem::Impl::start_loose_effect(
    const std::filesystem::path& path,
    AudioCategory category,
    float spatial_gain
) {
    if (!std::filesystem::is_regular_file(path)) return false;
    auto effect = std::make_unique<AudioTrack>();
    effect->category = category;
    effect->spatial_gain = std::clamp(spatial_gain, 0.0F, 1.0F);
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
            *effect,
            path,
            environment_gain * mix.category_gain(category) * effect->spatial_gain,
            false
        );
    }
#if AOE_HAVE_NATIVE_MP3
    else if (extension == ".mp3") {
        loaded = load_native_mp3_track(
            *effect,
            path,
            environment_gain * mix.category_gain(category) * effect->spatial_gain,
            false
        );
    }
#endif
#if AOE_HAVE_MPG123
    else if (extension == ".mp3" && mpg123 != nullptr) {
        loaded = load_mp3_track(
            *effect,
            path,
            environment_gain * mix.category_gain(category) * effect->spatial_gain,
            *mpg123,
            false
        );
    }
#endif
    if (!loaded) return false;
    if (trace) {
        std::cerr << "Audio loose effect " << path.filename().string()
                  << '\n';
    }
    effects.push_back(std::move(effect));
    return true;
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
    const std::filesystem::path initial_music =
        music_path_for(
            root,
            AudioMusicContext::opening,
            Civilization::generic,
            false
        );
    const std::filesystem::path dat_path =
        root / "Data" / "empires2_x1_p1.dat";
    const bool has_effects = std::filesystem::is_directory(
        root / "Sound" / "effects"
    );
    if (!std::filesystem::is_regular_file(ambience_path) &&
        !std::filesystem::is_regular_file(initial_music) &&
        !(std::filesystem::is_regular_file(dat_path) && has_effects)) {
        std::cerr
            << "Audio disabled: packaged resource root has no supported "
               "audio files\n";
        return nullptr;
    }
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        std::cerr << "Audio disabled: cannot initialize playback device: "
                  << SDL_GetError() << '\n';
        return nullptr;
    }

    auto impl = std::make_unique<Impl>();
    impl->root = root;
    impl->expansion_content = false;
    const float gain = requested_gain();
    impl->environment_gain = gain;
    impl->trace = enabled_value(SDL_getenv("AOE_AUDIO_TRACE"));
    try {
        if (std::filesystem::is_regular_file(dat_path) && has_effects) {
            impl->sound_resolver =
                std::make_unique<LegacySoundResolver>(root);
        }
    } catch (const std::exception& error) {
        std::cerr << "Optional effects disabled: " << error.what() << '\n';
        impl->sound_resolver.reset();
    }
    bool playing = false;

#if AOE_HAVE_MPG123
#if !AOE_HAVE_NATIVE_MP3
    if (std::ranges::any_of(
            std::array{
                initial_music,
                music_path_for(
                    root, AudioMusicContext::gameplay,
                    Civilization::generic, true
                )
            },
            [](const std::filesystem::path& path) {
                return std::filesystem::is_regular_file(path);
            })) {
        impl->mpg123 = load_mpg123_api();
        if (impl->mpg123 == nullptr) {
            std::cerr
                << "Music unavailable: mpg123 runtime library could not be "
                   "loaded; install mpg123 for this CPU architecture\n";
        }
    }
#endif
#endif
    playing = impl->start_music(initial_music, false) || playing;

    if (!playing && impl->sound_resolver == nullptr) {
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
        impl_->civilization = civilization;
        if (impl_->sound_resolver) {
            impl_->sound_resolver->set_listener_civilization(civilization);
        }
        if (impl_->music_context == AudioMusicContext::civilization) {
            set_music_context(
                AudioMusicContext::civilization,
                impl_->expansion_content
            );
        }
    }
}

void AudioSystem::set_locale(std::string_view locale) {
    if (impl_ == nullptr) return;
    try {
        impl_->audio_locale = language_profile(locale).audio_directory;
    } catch (const std::invalid_argument&) {
        impl_->audio_locale = "en";
    }
}

void AudioSystem::set_music_context(
    AudioMusicContext context,
    bool expansion_content
) {
    if (impl_ == nullptr) return;
    const auto path = music_path_for(
        impl_->root,
        context,
        impl_->civilization,
        expansion_content
    );
    const bool current_finished =
        impl_->music.stream == nullptr ||
        (impl_->music.cursor >= impl_->music.samples.size() &&
         SDL_GetAudioStreamQueued(impl_->music.stream) <= 0);
    if (((impl_->music_context == AudioMusicContext::civilization &&
          context == AudioMusicContext::gameplay) ||
         (impl_->music_context == AudioMusicContext::opening &&
          context == AudioMusicContext::menu)) &&
        !current_finished) {
        impl_->queued_music = std::pair{context, expansion_content};
        return;
    }
    if (context == impl_->music_context &&
        expansion_content == impl_->expansion_content &&
        path == impl_->music_path &&
        impl_->music.stream != nullptr) {
        return;
    }
    impl_->music_context = context;
    impl_->expansion_content = expansion_content;
    impl_->queued_music.reset();
    static_cast<void>(
        impl_->start_music(path, music_context_loops(context))
    );
}

void AudioSystem::play_taunt(unsigned number, std::string_view locale) {
    if (impl_ == nullptr || number == 0 || number > 999) return;
    update();
    std::string audio_locale = impl_->audio_locale;
    try { audio_locale = language_profile(locale).audio_directory; }
    catch (const std::invalid_argument&) {}
    const std::string prefix =
        (number < 10 ? "0" : "") + std::to_string(number) + " ";
    for (const auto& directory : localized_audio_directories(
             impl_->root, audio_locale, "taunt")) {
        std::error_code error;
        for (std::filesystem::directory_iterator it{directory, error}, end;
             !error && it != end;
             it.increment(error)) {
            if (it->is_regular_file(error) &&
                it->path().filename().string().starts_with(prefix)) {
                static_cast<void>(impl_->start_loose_effect(
                    it->path(), AudioCategory::interface
                ));
                return;
            }
        }
    }
}

void AudioSystem::play_narration(
    const std::filesystem::path& filename
) {
    if (impl_ == nullptr || filename.empty()) return;
    update();
    if (filename.is_absolute()) {
        static_cast<void>(impl_->start_loose_effect(
            filename, AudioCategory::interface
        ));
        return;
    }
    auto directories = localized_audio_directories(
        impl_->root, impl_->audio_locale, "scenario"
    );
    const auto campaign = localized_audio_directories(
        impl_->root, impl_->audio_locale, "campaign"
    );
    directories.insert(directories.end(), campaign.begin(), campaign.end());
    for (const auto& directory : directories) {
        const auto path = directory / filename.filename();
        if (impl_->start_loose_effect(
                path, AudioCategory::interface
            )) {
            return;
        }
    }
}

void AudioSystem::play_named_interface_effect(std::string_view filename) {
    if (impl_ == nullptr || filename.empty()) return;
    update();
    const std::filesystem::path leaf{
        std::filesystem::path{filename}.filename()
    };
    for (const auto& directory : {
             impl_->root / "Sound",
             impl_->root / "Sound" / "interface",
             impl_->root / "Data" / "Sound",
         }) {
        if (impl_->start_loose_effect(
                directory / leaf, AudioCategory::interface
            )) return;
    }
}

bool AudioSystem::play_graphic_frame_sounds(
    int slp_id,
    int frame,
    int angle,
    float spatial_gain,
    float pan,
    std::optional<Civilization> source_civilization
) {
    if (impl_ == nullptr || impl_->sound_resolver == nullptr || slp_id < 0) {
        return false;
    }
    const auto sounds = impl_->sound_resolver->graphic_frame_sound_ids(
        slp_id, frame, angle
    );
    for (const int sound : sounds) {
        play_effect(
            sound,
            AudioCategory::combat,
            spatial_gain,
            pan,
            source_civilization
        );
    }
    return !sounds.empty();
}

void AudioSystem::set_terrain_ambience(
    Terrain terrain,
    std::uint64_t variation
) {
    if (impl_ == nullptr) return;
    std::string filename;
    if (terrain == Terrain::water ||
        terrain == Terrain::deep_water ||
        terrain == Terrain::beach ||
        terrain == Terrain::shallows ||
        terrain == Terrain::fish || terrain == Terrain::fish_shore ||
        terrain == Terrain::fish_deep) {
        filename = "Wave" + std::to_string(variation % 5 + 1) + ".wav";
    } else if (terrain == Terrain::forest ||
        terrain == Terrain::pine_forest ||
        terrain == Terrain::oak_forest ||
        terrain == Terrain::bamboo_forest ||
        terrain == Terrain::palm_forest ||
        terrain == Terrain::jungle_forest) {
        filename = "jungle" + std::to_string(variation % 4 + 1) + ".wav";
    } else if (terrain == Terrain::grass || terrain == Terrain::grass2 ||
        terrain == Terrain::dirt || terrain == Terrain::dirt2 ||
        terrain == Terrain::dirt3 || terrain == Terrain::road ||
        terrain == Terrain::snow || terrain == Terrain::ice) {
        constexpr std::array names{
            "Cricket.wav", "Wind1.wav", "Wind2.wav", "Wind3.wav"
        };
        filename = names[variation % names.size()];
    } else {
        constexpr std::array names{
            "tf1.wav", "tf2.wav", "tf3.wav", "tf4.wav",
            "tf6.wav", "tf7.wav", "tf8.wav"
        };
        filename = names[variation % names.size()];
    }
    const auto path = impl_->root / "Sound" / "terrain" / filename;
    if (path == impl_->ambience_path) return;
    impl_->water_ambience = {};
    impl_->ambience_path.clear();
    if (std::filesystem::is_regular_file(path) &&
        load_wav_track(
            impl_->water_ambience,
            path,
            impl_->environment_gain * impl_->mix.ambience_gain(),
            true
        )) {
        impl_->ambience_path = path;
        if (impl_->trace) {
            std::cerr << "Audio ambience " << filename << '\n';
        }
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
    if (impl_->trace) {
        std::cerr << "Audio runtime focus " << focused << " music "
                  << impl_->mix.music_gain() << " sound "
                  << impl_->mix.ambience_gain() << '\n';
    }
}

void AudioSystem::set_paused(bool paused) {
    if (impl_ == nullptr) return;
    if (impl_->mix.paused == paused) return;
    impl_->mix.paused = paused;
    impl_->apply_stream_gains();
    if (impl_->trace) {
        std::cerr << "Audio runtime pause " << paused << " music "
                  << impl_->mix.music_gain() << " sound "
                  << impl_->mix.ambience_gain() << '\n';
    }
}

void AudioSystem::set_muted(bool muted) {
    if (impl_ == nullptr) return;
    if (impl_->mix.muted == muted) return;
    impl_->mix.muted = muted;
    impl_->apply_stream_gains();
    if (impl_->trace) {
        std::cerr << "Audio runtime mute " << muted << " music "
                  << impl_->mix.music_gain() << " sound "
                  << impl_->mix.ambience_gain() << '\n';
    }
}

void AudioSystem::play_effect(
    int sound_id,
    AudioCategory category,
    float spatial_gain,
    float pan,
    std::optional<Civilization> source_civilization
) {
    static_cast<void>(pan);
    if (!impl_ || !impl_->sound_resolver || sound_id < 0) return;
    const auto path = impl_->sound_resolver->resolve(
        sound_id, source_civilization
    );
    if (path) static_cast<void>(
        impl_->start_loose_effect(*path, category, spatial_gain)
    );
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
    if (impl_->queued_music &&
        (impl_->music.stream == nullptr ||
         (impl_->music.cursor >= impl_->music.samples.size() &&
          SDL_GetAudioStreamQueued(impl_->music.stream) <= 0))) {
        const auto [context, expansion] = *impl_->queued_music;
        impl_->queued_music.reset();
        impl_->music_context = context;
        impl_->expansion_content = expansion;
        static_cast<void>(impl_->start_music(
            music_path_for(
                impl_->root,
                context,
                impl_->civilization,
                expansion
            ),
            music_context_loops(context)
        ));
    }
}

}  // namespace aoe
