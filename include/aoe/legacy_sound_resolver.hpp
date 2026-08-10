#pragma once

#include "aoe/legacy_dat.hpp"
#include "aoe/types.hpp"

#include <filesystem>
#include <optional>
#include <random>
#include <vector>

namespace aoe {

// Single sound-ID mapping used by every platform audio backend. Legacy DAT
// data selects civilization/variation; generated MP3 filenames use resource ID.
class LegacySoundResolver {
public:
    explicit LegacySoundResolver(
        const std::filesystem::path& installation_root,
        bool verify_effect_files = true
    );

    void set_listener_civilization(Civilization civilization) noexcept;
    [[nodiscard]] std::optional<std::filesystem::path> resolve(
        int sound_id,
        std::optional<Civilization> source_civilization = std::nullopt
    );
    [[nodiscard]] std::vector<int> graphic_frame_sound_ids(
        int slp_id, int frame, int angle
    ) const;

private:
    [[nodiscard]] static std::int16_t civilization_id(
        Civilization civilization
    ) noexcept;

    LegacyDatFile dat_;
    std::filesystem::path effect_directory_;
    std::int16_t listener_civilization_{-1};
    bool verify_effect_files_{true};
    std::mt19937 random_{std::random_device{}()};
};

}  // namespace aoe
