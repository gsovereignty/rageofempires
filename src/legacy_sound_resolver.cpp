#include "aoe/legacy_sound_resolver.hpp"

#include <algorithm>

namespace aoe {

LegacySoundResolver::LegacySoundResolver(
    const std::filesystem::path& root, bool verify_effect_files
) : dat_(LegacyDatFile::load(root / "Data" / "empires2_x1_p1.dat")),
    effect_directory_(root / "Sound" / "effects"),
    verify_effect_files_(verify_effect_files) {}

std::int16_t LegacySoundResolver::civilization_id(
    Civilization civilization
) noexcept {
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

void LegacySoundResolver::set_listener_civilization(
    Civilization civilization
) noexcept {
    listener_civilization_ = civilization_id(civilization);
}

std::optional<std::filesystem::path> LegacySoundResolver::resolve(
    int sound_id, std::optional<Civilization> source_civilization
) {
    if (sound_id < 0) return std::nullopt;
    const LegacySound* sound = dat_.sound(static_cast<std::size_t>(sound_id));
    if (!sound || sound->items.empty()) return std::nullopt;
    const std::int16_t civilization = source_civilization
        ? civilization_id(*source_civilization) : listener_civilization_;
    const LegacySoundItem* item = select_legacy_sound_item(
        *sound, civilization, random_()
    );
    if (!item) return std::nullopt;
    auto path = effect_directory_ / (std::to_string(item->resource_id) + ".mp3");
    if (verify_effect_files_ && !std::filesystem::is_regular_file(path) &&
        civilization >= 0) {
        item = select_legacy_sound_item(*sound, -1, random_());
        if (!item) return std::nullopt;
        path = effect_directory_ / (std::to_string(item->resource_id) + ".mp3");
    }
    return !verify_effect_files_ || std::filesystem::is_regular_file(path)
        ? std::optional<std::filesystem::path>{std::move(path)}
        : std::nullopt;
}

std::vector<int> LegacySoundResolver::graphic_frame_sound_ids(
    int slp_id, int frame, int angle
) const {
    if (slp_id < 0) return {};
    const LegacyGraphic* graphic{};
    for (const auto& candidate : dat_.graphics()) {
        if (candidate.graphic_id >= 0 && candidate.slp_id == slp_id) {
            graphic = &candidate;
            break;
        }
    }
    if (!graphic || graphic->angle_sounds.empty()) return {};
    std::vector<int> result;
    const auto& sounds = graphic->angle_sounds[
        static_cast<std::size_t>(std::max(angle, 0)) % graphic->angle_sounds.size()
    ];
    for (const auto& sound : sounds) {
        if (sound.sound_id >= 0 && sound.frame == frame &&
            std::ranges::find(result, sound.sound_id) == result.end()) {
            result.push_back(sound.sound_id);
        }
    }
    return result;
}

}  // namespace aoe
