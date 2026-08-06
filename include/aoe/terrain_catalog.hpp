#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "aoe/types.hpp"

namespace aoe {

enum class ClassicTerrainRestriction : std::uint8_t {
    ship = 3,
    generic_building = 4,
    dock = 6,
    land_unit = 7,
    fish_trap = 13,
};

struct ClassicTerrainRecord {
    std::uint8_t id{};
    Terrain terrain{Terrain::grass};
    std::string_view name;
    std::string_view token;
    std::int32_t slp_id{-1};
};

[[nodiscard]] std::span<const ClassicTerrainRecord>
classic_terrain_catalog() noexcept;
[[nodiscard]] std::optional<Terrain>
classic_terrain_from_id(std::uint8_t id) noexcept;
[[nodiscard]] std::optional<std::uint8_t>
classic_terrain_id(Terrain terrain) noexcept;
[[nodiscard]] const ClassicTerrainRecord*
classic_terrain_record(Terrain terrain) noexcept;
[[nodiscard]] std::optional<Terrain>
classic_terrain_from_token(std::string_view token) noexcept;
[[nodiscard]] bool classic_terrain_passable(
    Terrain terrain,
    ClassicTerrainRestriction restriction
) noexcept;

}  // namespace aoe
