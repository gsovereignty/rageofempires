#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "aoe/types.hpp"

namespace aoe {

struct BuildingDamageRecord {
    std::int16_t graphic_id{};
    std::uint16_t serialized_threshold{};
    std::uint8_t flag{};
};

[[nodiscard]] std::array<BuildingDamageRecord, 3>
canonical_building_damage_records(
    BuildingKind kind,
    Civilization civilization
);

[[nodiscard]] constexpr std::optional<std::uint8_t>
building_damage_percent(int hit_points, int maximum_hit_points) noexcept {
    if (maximum_hit_points < 1 || hit_points <= 0) {
        return std::nullopt;
    }
    const auto remaining = static_cast<std::uint64_t>(hit_points) * 100U /
        static_cast<std::uint64_t>(maximum_hit_points);
    const auto damage = 100U - remaining;
    return damage <= 99U
        ? std::optional<std::uint8_t>{static_cast<std::uint8_t>(damage)}
        : std::nullopt;
}

[[nodiscard]] constexpr std::optional<std::size_t>
select_building_damage_record(
    int hit_points,
    int maximum_hit_points,
    std::span<const BuildingDamageRecord> records
) noexcept {
    const auto damage =
        building_damage_percent(hit_points, maximum_hit_points);
    if (!damage || records.empty()) {
        return std::nullopt;
    }
    std::optional<std::size_t> selected;
    for (std::size_t index = 0; index < records.size(); ++index) {
        const auto threshold = static_cast<std::uint8_t>(
            records[index].serialized_threshold & 0xffU
        );
        if (threshold < *damage) {
            selected = index;
        }
    }
    return selected;
}

}  // namespace aoe
