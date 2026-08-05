#include "aoe/building_damage.hpp"

namespace aoe {
namespace {

std::size_t damage_architecture_family(Civilization civilization) {
    switch (civilization) {
        case Civilization::goths:
        case Civilization::teutons:
        case Civilization::vikings:
        case Civilization::huns:
            return 1;
        case Civilization::japanese:
        case Civilization::chinese:
        case Civilization::mongols:
        case Civilization::koreans:
            return 2;
        case Civilization::byzantines:
        case Civilization::persians:
        case Civilization::saracens:
        case Civilization::turks:
            return 3;
        case Civilization::aztecs:
        case Civilization::mayans:
            return 4;
        default:
            return 0;
    }
}

}  // namespace

std::array<BuildingDamageRecord, 3> canonical_building_damage_records(
    BuildingKind kind,
    Civilization civilization
) {
    using Roots = std::array<std::int16_t, 3>;
    Roots roots{};
    std::uint8_t flag{};
    const std::size_t family = damage_architecture_family(civilization);
    const auto family_roots = [family](
        const std::array<Roots, 5>& values
    ) { return values[family]; };
    switch (kind) {
        case BuildingKind::town_center: roots = {4563,4564,4565}; break;
        case BuildingKind::barracks: roots = {4429,4430,4431}; break;
        case BuildingKind::archery_range: roots = family_roots({{
            {4670,4674,4678},{4667,4671,4675},{4668,4672,4676},
            {4669,4673,4677},{6652,6653,6654}}}); break;
        case BuildingKind::house: roots = {4503,4504,4505}; break;
        case BuildingKind::mill: roots = {4524,4525,4526}; break;
        case BuildingKind::lumber_camp: roots = family_roots({{
            {5118,5122,5126},{5115,5119,5123},{5116,5120,5124},
            {5117,5121,5125},{7045,7046,7047}}}); break;
        case BuildingKind::mining_camp: roots = family_roots({{
            {4982,4986,4990},{4979,4983,4987},{4980,4984,4988},
            {4981,4985,4989},{6935,6936,6937}}}); break;
        case BuildingKind::farm: roots = {5354,5355,5356}; break;
        case BuildingKind::stable: roots = family_roots({{
            {5130,5134,5138},{5127,5131,5135},{5128,5132,5136},
            {5129,5133,5137},{7055,7056,7057}}}); break;
        case BuildingKind::blacksmith: roots = family_roots({{
            {4702,4706,4710},{4699,4703,4707},{4700,4704,4708},
            {4701,4705,4709},{6669,6670,6671}}}); break;
        case BuildingKind::castle: roots = family_roots({{
            {4790,4794,4798},{4787,4791,4795},{4788,4792,4796},
            {4789,4793,4797},{6741,6742,6743}}}); break;
        case BuildingKind::university: roots = family_roots({{
            {5174,5178,5182},{5171,5175,5179},{5172,5176,5180},
            {5173,5177,5181},{7080,7081,7082}}}); break;
        case BuildingKind::siege_workshop: roots = family_roots({{
            {5102,5106,5110},{5099,5103,5107},{5100,5104,5108},
            {5101,5105,5109},{7036,7037,7038}}}); break;
        case BuildingKind::palisade_wall: roots = {4610,4611,4612}; break;
        case BuildingKind::watch_tower: roots = family_roots({{
            {5198,5202,5206},{5195,5199,5203},{5196,5200,5204},
            {5197,5201,5205},{7110,7111,7112}}}); break;
        case BuildingKind::guard_tower: roots = family_roots({{
            {5214,5218,5222},{5211,5215,5219},{5212,5216,5220},
            {5213,5217,5221},{7117,7118,7119}}}); break;
        case BuildingKind::keep: roots = family_roots({{
            {5230,5234,5238},{5227,5231,5235},{5228,5232,5236},
            {5229,5233,5237},{7125,7126,7127}}}); break;
        case BuildingKind::stone_wall:
            roots = family_roots({{
                {3794,3798,3802},{3791,3795,3799},{3792,3796,3800},
                {3793,3797,3801},{7150,7152,7154}}});
            flag = 2;
            break;
        case BuildingKind::fortified_wall:
            roots = family_roots({{
                {3806,3810,3814},{3803,3807,3811},{3804,3808,3812},
                {3805,3809,3813},{7156,7158,7160}}});
            flag = 2;
            break;
        case BuildingKind::palisade_gate_x: roots = {6509,6510,6511}; break;
        case BuildingKind::palisade_gate_y: roots = {6530,6531,6532}; break;
        case BuildingKind::stone_gate_x: roots = {6492,6493,6494}; break;
        case BuildingKind::stone_gate_y: roots = {6513,6514,6515}; break;
        case BuildingKind::fortified_gate_x: roots = {6492,6493,6494}; break;
        case BuildingKind::fortified_gate_y: roots = {6513,6514,6515}; break;
        case BuildingKind::monastery: roots = family_roots({{
            {4774,4778,4782},{4771,4775,4779},{4772,4776,4780},
            {4773,4777,4781},{6728,6729,6730}}}); break;
        case BuildingKind::market: roots = family_roots({{
            {4994,4998,5002},{4991,4995,4999},{4992,4996,5000},
            {4993,4997,5001},{6945,6946,6947}}}); break;
        case BuildingKind::dock: roots = {4456,4457,4458}; break;
        case BuildingKind::bombard_tower: roots = family_roots({{
            {5246,5250,5254},{5243,5247,5251},{5244,5248,5252},
            {5245,5249,5253},{7133,7134,7135}}}); break;
        case BuildingKind::fish_trap: roots = {5357,5358,5359}; break;
        case BuildingKind::outpost: roots = {4633,4634,4635}; break;
        case BuildingKind::wonder:
            switch (civilization) {
                case Civilization::britons: roots={5275,5288,5301}; break;
                case Civilization::franks: roots={5282,5295,5308}; break;
                case Civilization::goths: roots={5276,5289,5302}; break;
                case Civilization::teutons: roots={5285,5298,5311}; break;
                case Civilization::japanese: roots={5278,5291,5304}; break;
                case Civilization::chinese: roots={5287,5300,5313}; break;
                case Civilization::byzantines: roots={5286,5299,5312}; break;
                case Civilization::persians: roots={5281,5294,5307}; break;
                case Civilization::saracens: roots={5283,5296,5309}; break;
                case Civilization::turks: roots={5284,5297,5310}; break;
                case Civilization::vikings: roots={5277,5290,5303}; break;
                case Civilization::mongols: roots={5280,5293,5306}; break;
                case Civilization::celts: roots={5279,5292,5305}; break;
                case Civilization::spanish: roots={6307,6310,6313}; break;
                case Civilization::aztecs: roots={6626,6627,6628}; break;
                case Civilization::mayans: roots={6309,6312,6315}; break;
                case Civilization::huns: roots={6308,6311,6314}; break;
                case Civilization::koreans: roots={7244,7245,7246}; break;
                default: roots={5275,5288,5301}; break;
            }
            break;
    }
    return {{
        {roots[0], static_cast<std::uint16_t>(25 + flag * 256), flag},
        {roots[1], static_cast<std::uint16_t>(50 + flag * 256), flag},
        {roots[2], static_cast<std::uint16_t>(75 + flag * 256), flag},
    }};
}

}  // namespace aoe
