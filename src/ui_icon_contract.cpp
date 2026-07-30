#include "aoe/ui_icon_contract.hpp"

namespace aoe::ui_icons {

std::optional<Binding> technology(std::int32_t dat_icon_id) {
    if (dat_icon_id < 0 || dat_icon_id >= 118) return std::nullopt;
    return Binding{
        technology_sheet, dat_icon_id,
        Evidence::exact_executable_dispatch,
    };
}

std::optional<Binding> technology_icon(Technology technology_value) {
    switch (technology_value) {
#include "aoe/generated/technology_icon_bindings.inc"
    }
    return std::nullopt;
}

std::optional<Binding> ordinary_unit(std::int32_t dat_icon_id) {
    if (dat_icon_id < 0 || dat_icon_id >= 134) return std::nullopt;
    return Binding{
        unit_sheet, dat_icon_id,
        Evidence::exact_executable_dispatch,
    };
}

std::optional<Binding> training_unit(UnitKind kind) {
    // Exact VER 5.7 record +0x54 values. FUN_005c7560 passes this field
    // unchanged to SLP 50730 for ordinary (non-building) records.
    switch (kind) {
    case UnitKind::villager: return ordinary_unit(15);
    case UnitKind::militia: return ordinary_unit(8);
    case UnitKind::spearman: return ordinary_unit(31);
    case UnitKind::archer: return ordinary_unit(17);
    case UnitKind::skirmisher: return ordinary_unit(20);
    case UnitKind::scout_cavalry: return ordinary_unit(64);
    case UnitKind::knight: return ordinary_unit(1);
    case UnitKind::camel_rider: return ordinary_unit(78);
    case UnitKind::battering_ram: return ordinary_unit(74);
    case UnitKind::mangonel: return ordinary_unit(27);
    case UnitKind::scorpion: return ordinary_unit(80);
    case UnitKind::monk: return ordinary_unit(33);
    case UnitKind::fishing_ship: return ordinary_unit(24);
    case UnitKind::galley: return ordinary_unit(87);
    case UnitKind::transport_ship: return ordinary_unit(95);
    case UnitKind::woad_raider:
    case UnitKind::elite_woad_raider:
        return ordinary_unit(47);
    default: return std::nullopt;
    }
}

}  // namespace aoe::ui_icons
