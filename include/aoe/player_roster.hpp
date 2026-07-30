#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "aoe/player_codec.hpp"

namespace aoe {

class PlayerSlotId {
public:
    static constexpr std::uint8_t neutral_value = 8;

    constexpr PlayerSlotId() = default;

    [[nodiscard]] static constexpr std::optional<PlayerSlotId> from_index(
        std::size_t index
    ) noexcept {
        return index < 8
            ? std::optional<PlayerSlotId>{
                  PlayerSlotId{static_cast<std::uint8_t>(index)}
              }
            : std::nullopt;
    }

    [[nodiscard]] static constexpr PlayerSlotId neutral() noexcept {
        return PlayerSlotId{neutral_value};
    }

    [[nodiscard]] constexpr bool is_neutral() const noexcept {
        return value_ == neutral_value;
    }

    [[nodiscard]] constexpr std::optional<std::size_t> index() const noexcept {
        return is_neutral()
            ? std::nullopt
            : std::optional<std::size_t>{value_};
    }

    [[nodiscard]] constexpr std::uint8_t stable_id() const noexcept {
        return value_;
    }

    friend constexpr bool operator==(
        PlayerSlotId, PlayerSlotId
    ) noexcept = default;

private:
    explicit constexpr PlayerSlotId(std::uint8_t value) : value_(value) {}

    std::uint8_t value_{};
};

inline constexpr std::array<std::string_view, 8> player_slot_colors{
    "blue", "red", "green", "yellow",
    "cyan", "purple", "gray", "orange",
};

inline constexpr std::array<std::string_view, 8> player_slot_names{
    "player1", "player2", "player3", "player4",
    "player5", "player6", "player7", "player8",
};

[[nodiscard]] constexpr std::string_view player_slot_color(
    PlayerSlotId slot
) noexcept {
    const auto index = slot.index();
    return index ? player_slot_colors[*index] : "neutral";
}

[[nodiscard]] constexpr std::string_view player_slot_name(
    PlayerSlotId slot
) noexcept {
    const auto index = slot.index();
    return index ? player_slot_names[*index] : "neutral";
}

[[nodiscard]] constexpr std::optional<PlayerSlotId> decode_player_slot_id(
    int stable_id
) noexcept {
    if (stable_id == PlayerSlotId::neutral_value) {
        return PlayerSlotId::neutral();
    }
    return stable_id >= 0
        ? PlayerSlotId::from_index(static_cast<std::size_t>(stable_id))
        : std::nullopt;
}

[[nodiscard]] constexpr int encode_player_slot_id(
    PlayerSlotId slot
) noexcept {
    return slot.stable_id();
}

[[nodiscard]] constexpr std::optional<PlayerSlotId> decode_player_slot_name(
    std::string_view name
) noexcept {
    if (name == "neutral") return PlayerSlotId::neutral();
    for (std::size_t index = 0; index < player_slot_names.size(); ++index) {
        if (name == player_slot_names[index] ||
            name == player_slot_colors[index]) {
            return PlayerSlotId::from_index(index);
        }
    }
    return std::nullopt;
}

[[nodiscard]] constexpr std::optional<PlayerSlotId> player_slot_from_legacy(
    Player player
) noexcept {
    if (player == Player::blue) return PlayerSlotId::from_index(0);
    if (player == Player::red) return PlayerSlotId::from_index(1);
    if (player == Player::neutral) return PlayerSlotId::neutral();
    return std::nullopt;
}

[[nodiscard]] constexpr std::optional<Player> player_slot_to_legacy(
    PlayerSlotId slot
) noexcept {
    if (slot.is_neutral()) return Player::neutral;
    if (slot.index() == 0U) return Player::blue;
    if (slot.index() == 1U) return Player::red;
    return std::nullopt;
}

[[nodiscard]] constexpr EntityOwner entity_owner_from_slot(
    PlayerSlotId slot
) noexcept {
    return *EntityOwner::from_stable_id(slot.stable_id());
}

[[nodiscard]] constexpr std::optional<PlayerSlotId> entity_owner_slot(
    EntityOwner owner
) noexcept {
    return decode_player_slot_id(owner.stable_id());
}

class TeamId {
public:
    constexpr TeamId() = default;

    [[nodiscard]] static constexpr TeamId none() noexcept {
        return TeamId{};
    }

    [[nodiscard]] static constexpr std::optional<TeamId> numbered(
        int number
    ) noexcept {
        return number >= 1 && number <= 4
            ? std::optional<TeamId>{
                  TeamId{static_cast<std::uint8_t>(number)}
              }
            : std::nullopt;
    }

    [[nodiscard]] constexpr bool has_team() const noexcept {
        return value_ != 0;
    }

    [[nodiscard]] constexpr int number() const noexcept {
        return value_;
    }

    friend constexpr bool operator==(TeamId, TeamId) noexcept = default;

private:
    explicit constexpr TeamId(std::uint8_t value) : value_(value) {}

    std::uint8_t value_{};
};

enum class RosterControllerKind {
    human,
    computer,
};

struct RosterController {
    std::string id;
    RosterControllerKind kind{RosterControllerKind::human};

    friend bool operator==(
        const RosterController&,
        const RosterController&
    ) = default;
};

struct MatchRosterSlot {
    PlayerSlotId slot;
    bool occupied{};
    TeamId team;
    bool cooperative_control{};
    std::vector<RosterController> controllers;
};

class MatchRoster {
public:
    [[nodiscard]] static MatchRoster legacy_blue_red() {
        return *create({
            {
                *PlayerSlotId::from_index(0), true, TeamId::none(), false,
                {{"blue", RosterControllerKind::human}},
            },
            {
                *PlayerSlotId::from_index(1), true, TeamId::none(), false,
                {{"red", RosterControllerKind::human}},
            },
        });
    }

    [[nodiscard]] static std::optional<MatchRoster> create(
        std::vector<MatchRosterSlot> source
    ) {
        MatchRoster result;
        std::array<bool, 8> seen_slots{};
        std::vector<std::string> seen_controller_ids;
        for (MatchRosterSlot& entry : source) {
            const auto index = entry.slot.index();
            if (!index || seen_slots[*index]) return std::nullopt;
            seen_slots[*index] = true;
            if (!entry.occupied) {
                if (entry.team.has_team() ||
                    entry.cooperative_control ||
                    !entry.controllers.empty()) {
                    return std::nullopt;
                }
            } else {
                if (entry.controllers.empty()) return std::nullopt;
                std::size_t human_count{};
                std::size_t computer_count{};
                for (const RosterController& controller :
                     entry.controllers) {
                    if (controller.id.empty()) return std::nullopt;
                    for (const std::string& seen : seen_controller_ids) {
                        if (seen == controller.id) return std::nullopt;
                    }
                    seen_controller_ids.push_back(controller.id);
                    if (controller.kind == RosterControllerKind::human) {
                        ++human_count;
                    } else {
                        ++computer_count;
                    }
                }
                if (computer_count > 1 ||
                    (computer_count != 0 && human_count != 0) ||
                    (computer_count != 0 && entry.cooperative_control) ||
                    (human_count > 1 && !entry.cooperative_control) ||
                    (human_count == 1 && entry.cooperative_control)) {
                    return std::nullopt;
                }
            }
            result.slots_[*index] = std::move(entry);
        }
        return result;
    }

    [[nodiscard]] const MatchRosterSlot& slot(
        PlayerSlotId id
    ) const {
        return slots_.at(*id.index());
    }

    [[nodiscard]] const std::array<MatchRosterSlot, 8>& slots() const {
        return slots_;
    }

private:
    MatchRoster() {
        for (std::size_t index = 0; index < slots_.size(); ++index) {
            slots_[index].slot = *PlayerSlotId::from_index(index);
        }
    }

    std::array<MatchRosterSlot, 8> slots_{};
};

}  // namespace aoe
