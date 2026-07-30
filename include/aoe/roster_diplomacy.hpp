#pragma once

#include <array>
#include <cstdint>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>

#include "aoe/player_roster.hpp"

namespace aoe {

struct RosterDiplomacyRules {
    bool allied_victory{true};
    bool shared_vision{true};
};

class RosterDiplomacy {
public:
    [[nodiscard]] static std::optional<RosterDiplomacy> create(
        const MatchRoster& roster,
        RosterDiplomacyRules rules = {}
    ) {
        RosterDiplomacy result;
        result.rules_ = rules;
        for (std::size_t index = 0; index < 8; ++index) {
            const MatchRosterSlot& entry =
                roster.slot(*PlayerSlotId::from_index(index));
            result.occupied_[index] = entry.occupied;
            result.teams_[index] = entry.team;
            result.cooperative_[index] = entry.cooperative_control;
        }
        for (std::size_t from = 0; from < 8; ++from) {
            for (std::size_t to = 0; to < 8; ++to) {
                Diplomacy stance = Diplomacy::neutral;
                if (from == to) {
                    stance = Diplomacy::ally;
                } else if (
                    result.occupied_[from] &&
                    result.occupied_[to]
                ) {
                    const bool same_team =
                        result.teams_[from].has_team() &&
                        result.teams_[from] == result.teams_[to];
                    stance = same_team
                        ? Diplomacy::ally : Diplomacy::enemy;
                }
                result.stances_[from][to] = stance;
            }
        }
        return result.valid() ? std::optional<RosterDiplomacy>{result}
                              : std::nullopt;
    }

    [[nodiscard]] static RosterDiplomacy legacy_blue_red() {
        const auto blue = PlayerSlotId::from_index(0);
        const auto red = PlayerSlotId::from_index(1);
        const auto roster = MatchRoster::create({
            {
                *blue, true, TeamId::none(), false,
                {{"blue", RosterControllerKind::human}},
            },
            {
                *red, true, TeamId::none(), false,
                {{"red", RosterControllerKind::human}},
            },
        });
        return *create(*roster, {true, true});
    }

    [[nodiscard]] Diplomacy stance(
        PlayerSlotId from,
        PlayerSlotId to
    ) const {
        return stances_.at(*from.index()).at(*to.index());
    }

    bool set_stance(
        PlayerSlotId from,
        PlayerSlotId to,
        Diplomacy stance
    ) {
        const auto from_index = from.index();
        const auto to_index = to.index();
        if (!from_index || !to_index ||
            !occupied_[*from_index] || !occupied_[*to_index] ||
            from == to ||
            stance < Diplomacy::ally ||
            stance > Diplomacy::enemy) {
            return false;
        }
        stances_[*from_index][*to_index] = stance;
        return true;
    }

    bool set_symmetric_stance(
        PlayerSlotId first,
        PlayerSlotId second,
        Diplomacy stance
    ) {
        if (!set_stance(first, second, stance)) return false;
        if (!set_stance(second, first, stance)) return false;
        return true;
    }

    [[nodiscard]] bool shares_victory(
        PlayerSlotId first,
        PlayerSlotId second
    ) const {
        if (first == second) return true;
        return rules_.allied_victory &&
            stance(first, second) == Diplomacy::ally &&
            stance(second, first) == Diplomacy::ally;
    }

    [[nodiscard]] bool shares_vision(
        PlayerSlotId viewer,
        PlayerSlotId source
    ) const {
        if (viewer == source) return true;
        return rules_.shared_vision &&
            stance(viewer, source) == Diplomacy::ally;
    }

    [[nodiscard]] bool cooperative_control(
        PlayerSlotId slot
    ) const {
        return cooperative_.at(*slot.index());
    }

    [[nodiscard]] RosterDiplomacyRules rules() const {
        return rules_;
    }

    [[nodiscard]] bool valid() const {
        for (std::size_t from = 0; from < 8; ++from) {
            if (stances_[from][from] != Diplomacy::ally) return false;
            for (std::size_t to = 0; to < 8; ++to) {
                const Diplomacy value = stances_[from][to];
                if (value < Diplomacy::ally ||
                    value > Diplomacy::enemy) {
                    return false;
                }
                if (from != to &&
                    (!occupied_[from] || !occupied_[to]) &&
                    value != Diplomacy::neutral) {
                    return false;
                }
            }
        }
        return true;
    }

    [[nodiscard]] std::string canonical() const {
        std::ostringstream output;
        output << "roster-diplomacy-v1 "
               << rules_.allied_victory << ' '
               << rules_.shared_vision;
        for (std::size_t index = 0; index < 8; ++index) {
            output << ' ' << occupied_[index]
                   << ':' << teams_[index].number()
                   << ':' << cooperative_[index];
        }
        for (const auto& row : stances_) {
            for (Diplomacy value : row) {
                output << ' ' << (
                    value == Diplomacy::ally ? 'a' :
                    value == Diplomacy::neutral ? 'n' : 'e'
                );
            }
        }
        return output.str();
    }

    [[nodiscard]] std::string canonical_hash() const {
        std::uint64_t hash = 14695981039346656037ULL;
        for (unsigned char byte : canonical()) {
            hash ^= byte;
            hash *= 1099511628211ULL;
        }
        std::ostringstream output;
        output << "roster-diplomacy-fnv1a64:"
               << std::hex << std::setfill('0')
               << std::setw(16) << hash;
        return output.str();
    }

private:
    std::array<std::array<Diplomacy, 8>, 8> stances_{};
    std::array<bool, 8> occupied_{};
    std::array<TeamId, 8> teams_{};
    std::array<bool, 8> cooperative_{};
    RosterDiplomacyRules rules_{};
};

}  // namespace aoe
