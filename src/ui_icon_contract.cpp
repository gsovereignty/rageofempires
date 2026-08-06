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
    case UnitKind::trade_cart: return ordinary_unit(34);
    case UnitKind::king: return ordinary_unit(48);
    case UnitKind::fishing_ship: return ordinary_unit(24);
    // Galley upgrades are distinct runtime kinds and retain distinct DAT
    // +0x54 interface frames rather than inheriting base Galley artwork.
    case UnitKind::galley: return ordinary_unit(87);
    case UnitKind::war_galley: return ordinary_unit(25);
    case UnitKind::galleon: return ordinary_unit(60);
    case UnitKind::transport_ship: return ordinary_unit(95);
    case UnitKind::woad_raider:
    case UnitKind::elite_woad_raider:
        return ordinary_unit(47);
    default: return std::nullopt;
    }
}

std::optional<Binding> building(BuildingKind kind) {
    // Exact VER 5.7 record +0x54 values. FUN_005c7560 dispatches building
    // subtypes to civilization-selected ico_bld%d sheets without changing
    // this frame. Installed DRS resources 50705..50708 are byte-identical.
    const auto icon = [](std::int32_t frame) {
        return Binding{
            building_sheet,
            frame,
            Evidence::exact_executable_dispatch,
        };
    };
    switch (kind) {
    case BuildingKind::town_center: return icon(28);
    case BuildingKind::barracks: return icon(2);
    case BuildingKind::archery_range: return icon(0);
    case BuildingKind::house: return icon(34);
    case BuildingKind::mill: return icon(19);
    case BuildingKind::lumber_camp: return icon(40);
    case BuildingKind::mining_camp: return icon(39);
    case BuildingKind::farm: return icon(35);
    case BuildingKind::stable: return icon(23);
    case BuildingKind::blacksmith: return icon(4);
    case BuildingKind::castle: return icon(7);
    case BuildingKind::university: return icon(32);
    case BuildingKind::siege_workshop: return icon(22);
    case BuildingKind::palisade_wall: return icon(30);
    case BuildingKind::watch_tower: return icon(25);
    case BuildingKind::guard_tower: return icon(25);
    case BuildingKind::keep: return icon(26);
    case BuildingKind::stone_wall: return icon(31);
    case BuildingKind::fortified_wall: return icon(31);
    case BuildingKind::palisade_gate_x:
    case BuildingKind::palisade_gate_y:
    case BuildingKind::stone_gate_x:
    case BuildingKind::stone_gate_y:
        return icon(36);
    case BuildingKind::fortified_gate_x:
    case BuildingKind::fortified_gate_y:
        return icon(36);
    case BuildingKind::monastery: return icon(10);
    case BuildingKind::market: return icon(16);
    case BuildingKind::dock: return icon(13);
    case BuildingKind::bombard_tower: return icon(42);
    case BuildingKind::fish_trap: return icon(41);
    case BuildingKind::outpost: return icon(38);
    case BuildingKind::wonder: return icon(37);
    }
    return std::nullopt;
}

}  // namespace aoe::ui_icons
