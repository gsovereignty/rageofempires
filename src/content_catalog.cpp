#include "aoe/content_catalog.hpp"

#include <algorithm>

namespace aoe {

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
