#include "aoe/content_catalog.hpp"

#include <algorithm>
#include <stdexcept>

namespace aoe {

CommercialTaskAbility commercial_task_ability(std::uint16_t action_type) {
    switch (action_type) {
        case 3: return CommercialTaskAbility::garrison;
        case 5: return CommercialTaskAbility::gather;
        case 6: return CommercialTaskAbility::graze;
        case 7: return CommercialTaskAbility::combat;
        case 10: return CommercialTaskAbility::bird;
        case 11: return CommercialTaskAbility::predator;
        case 12: return CommercialTaskAbility::transport;
        case 13: return CommercialTaskAbility::guard;
        case 21: return CommercialTaskAbility::make;
        case 101: return CommercialTaskAbility::build;
        case 104: return CommercialTaskAbility::convert;
        case 105: return CommercialTaskAbility::heal;
        case 106: return CommercialTaskAbility::repair;
        case 107: return CommercialTaskAbility::auto_convert;
        case 109: return CommercialTaskAbility::retreat;
        case 110: return CommercialTaskAbility::hunt;
        case 111: return CommercialTaskAbility::trade;
        case 120: return CommercialTaskAbility::wonder_victory;
        case 121: return CommercialTaskAbility::deselect;
        case 122: return CommercialTaskAbility::loot;
        case 125: return CommercialTaskAbility::unpack_attack;
        case 131: return CommercialTaskAbility::off_map_trade;
        case 132: return CommercialTaskAbility::pickup;
        case 135: return CommercialTaskAbility::kidnap;
        case 136: return CommercialTaskAbility::deposit;
        default: throw std::invalid_argument("unknown commercial task type");
    }
}

std::span<const CommercialCivilizationId>
ContentCatalog::civilization_ids() const noexcept {
    return civilization_ids_;
}

const CommercialObjectRecord* ContentCatalog::object(
    CommercialCivilizationId civilization,
    CommercialObjectId id
) const noexcept {
    if (civilization >= civilization_variant_ids_.size()) return nullptr;
    const auto& variants = civilization_variant_ids_[civilization];
    if (id >= variants.size()) return nullptr;
    const auto variant = variants[id];
    if (variant == 65535) return nullptr;
    return variant < object_variants_.size() ? &object_variants_[variant]
                                             : nullptr;
}

const CommercialTechnologyRecord* ContentCatalog::technology(
    CommercialTechnologyId id
) const noexcept {
    if (id >= technologies_.size() || technologies_[id].id != id) {
        return nullptr;
    }
    return &technologies_[id];
}

const CommercialEffectRecord* ContentCatalog::effect(
    CommercialEffectId id
) const noexcept {
    if (id >= effects_.size() || effects_[id].id != id) return nullptr;
    return &effects_[id];
}

std::optional<CommercialEffectId> ContentCatalog::civilization_effect(
    CommercialCivilizationId id
) const noexcept {
    if (id >= civilization_effect_ids_.size()) return std::nullopt;
    return civilization_effect_ids_[id];
}

std::optional<CommercialEffectId>
ContentCatalog::civilization_bonus_effect(
    CommercialCivilizationId id
) const noexcept {
    if (id >= civilization_bonus_effect_ids_.size()) return std::nullopt;
    return civilization_bonus_effect_ids_[id];
}

std::size_t ContentCatalog::object_variant_count() const noexcept {
    return object_variants_.size();
}

std::size_t ContentCatalog::object_record_count() const noexcept {
    std::size_t count{};
    for (const auto& civilization : civilization_variant_ids_) {
        count += static_cast<std::size_t>(std::count_if(
            civilization.begin(), civilization.end(),
            [](std::uint16_t value) { return value != 65535; }
        ));
    }
    return count;
}

std::span<const CommercialTechnologyRecord>
ContentCatalog::technologies() const noexcept {
    return technologies_;
}

std::span<const CommercialObjectRecord>
ContentCatalog::object_variants() const noexcept {
    return object_variants_;
}

std::span<const CommercialEffectRecord> ContentCatalog::effects()
    const noexcept {
    return effects_;
}

const ContentCatalog& commercial_content_catalog() {
    static const ContentCatalog value = [] {
        ContentCatalog catalog;
#include "content_catalog.inc"
        return catalog;
    }();
    return value;
}

}  // namespace aoe
