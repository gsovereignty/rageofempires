#include "aoe/legacy_assets.hpp"
#include "aoe/legacy_dat.hpp"
#include "aoe/game_rules.hpp"
#include "aoe/render_asset_coverage.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>

namespace {

using aoe::AssetCoverageStatus;
using aoe::RenderAction;

struct Validation {
    AssetCoverageStatus status{AssetCoverageStatus::renderable};
    std::string reason{"SLP present, layout valid, all frames decoded"};
    std::size_t actual_frames{};
    int stored_directions{};
};

std::string building_state_name(aoe::RenderBuildingState state) {
    switch (state) {
        case aoe::RenderBuildingState::foundation: return "foundation";
        case aoe::RenderBuildingState::construction: return "construction";
        case aoe::RenderBuildingState::completed: return "completed";
        case aoe::RenderBuildingState::damaged: return "damaged";
        case aoe::RenderBuildingState::dying: return "dying";
        case aoe::RenderBuildingState::destroyed: return "destroyed";
    }
    throw std::logic_error{"unknown building render state"};
}

std::string civilization_name(aoe::Civilization civilization) {
    static constexpr std::array names{
        "generic", "britons", "franks", "teutons", "goths", "celts",
        "vikings", "byzantines", "japanese", "chinese", "persians",
        "saracens", "turks", "mongols", "spanish", "huns", "koreans",
        "aztecs", "mayans",
    };
    const auto index = static_cast<std::size_t>(civilization);
    if (index >= names.size()) {
        throw std::logic_error{"unknown civilization"};
    }
    return names[index];
}

std::string age_name(aoe::Age age) {
    static constexpr std::array names{
        "dark", "feudal", "castle", "imperial",
    };
    const auto index = static_cast<std::size_t>(age);
    if (index >= names.size()) throw std::logic_error{"unknown age"};
    return names[index];
}

std::string json_escape(const std::string& value) {
    std::string result;
    for (const char character : value) {
        if (character == '"' || character == '\\') result += '\\';
        result += character;
    }
    return result;
}

Validation validate(
    const aoe::DrsArchive& graphics,
    const aoe::LegacyPalette& palette,
    std::int32_t slp,
    int frames_per_direction,
    unsigned player
) {
    if (!graphics.contains("slp", slp)) {
        return {
            AssetCoverageStatus::missing_archive_resource,
            "SLP " + std::to_string(slp) + " absent from graphics.drs",
        };
    }
    std::vector<std::byte> bytes;
    std::size_t count{};
    try {
        bytes = graphics.read("slp", slp);
        count = aoe::slp_frame_count(bytes);
    } catch (const std::exception& error) {
        return {
            AssetCoverageStatus::decode_failure,
            std::string{"cannot read SLP header: "} + error.what(),
        };
    }
    if (frames_per_direction <= 0 ||
        count < static_cast<std::size_t>(frames_per_direction)) {
        return {
            AssetCoverageStatus::missing_frame,
            "SLP frame count " + std::to_string(count) +
                " is smaller than frames-per-direction " +
                std::to_string(frames_per_direction),
            count,
        };
    }
    const int directions = static_cast<int>(
        count / static_cast<std::size_t>(frames_per_direction)
    );
    // Runtime accepts any positive stored-angle count. Missing display angles
    // deterministically mirror when possible, then modulo stored angles.
    if (directions <= 0) {
        return {
            AssetCoverageStatus::missing_frame,
            "SLP has no complete stored direction",
            count,
            directions,
        };
    }
    try {
        for (std::size_t frame = 0; frame < count; ++frame) {
            static_cast<void>(
                aoe::decode_slp_frame(bytes, palette, frame, player)
            );
        }
    } catch (const std::exception& error) {
        return {
            AssetCoverageStatus::decode_failure,
            "player " + std::to_string(player) +
                " frame decode failed: " + error.what(),
            count,
            directions,
        };
    }
    return {
        AssetCoverageStatus::renderable,
            "SLP present, runtime directional layout valid, all frames decoded",
        count,
        directions,
    };
}

Validation validate_composite(
    const aoe::LegacyDatFile& dat,
    const aoe::DrsArchive& graphics,
    const aoe::LegacyPalette& palette,
    std::int16_t root,
    unsigned player,
    bool expand_deltas,
    std::vector<std::int32_t>& slps
) {
    std::set<std::int16_t> active;
    Validation result;
    result.reason =
        "DAT composite graph valid; all SLP frames decoded";
    const auto visit = [&](const auto& self,
                           std::int16_t graphic_id,
                           bool is_root) -> bool {
        if (graphic_id < 0) return true;
        if (!active.insert(graphic_id).second) {
            result.status = AssetCoverageStatus::invalid_dat_reference;
            result.reason = "cycle in DAT graphic composition at " +
                std::to_string(graphic_id);
            return false;
        }
        const aoe::LegacyGraphic* graphic = dat.graphic(
            static_cast<std::size_t>(graphic_id)
        );
        if (graphic == nullptr) {
            result.status = AssetCoverageStatus::invalid_dat_reference;
            result.reason = "graphic " + std::to_string(graphic_id) +
                " absent from DAT";
            return false;
        }
        if (graphic->slp_id >= 0) {
            if (!graphics.contains("slp", graphic->slp_id)) {
                if (!(is_root && !graphic->deltas.empty())) {
                    result.status = is_root
                        ? AssetCoverageStatus::missing_archive_resource
                        : AssetCoverageStatus::missing_composite_part;
                    result.reason = "composite graphic " +
                        std::to_string(graphic_id) + " references absent SLP " +
                        std::to_string(graphic->slp_id);
                    return false;
                }
            } else {
                slps.push_back(graphic->slp_id);
                const Validation part = validate(
                    graphics,
                    palette,
                    graphic->slp_id,
                    std::max<int>(graphic->frame_count, 1),
                    player
                );
                if (part.status != AssetCoverageStatus::renderable) {
                    result = part;
                    result.reason = "composite graphic " +
                        std::to_string(graphic_id) + ": " + part.reason;
                    return false;
                }
                result.actual_frames += part.actual_frames;
                result.stored_directions = std::max(
                    result.stored_directions,
                    std::max<int>(graphic->angle_count, 1)
                );
            }
        } else if (graphic->deltas.empty()) {
            result.status = AssetCoverageStatus::invalid_dat_reference;
            result.reason = "graphic " + std::to_string(graphic_id) +
                " has neither SLP nor delta parts";
            return false;
        }
        if (expand_deltas) {
            for (const aoe::LegacyGraphicDelta& delta : graphic->deltas) {
                if (!self(self, delta.graphic_id, false)) return false;
            }
        }
        active.erase(graphic_id);
        return true;
    };
    if (!visit(visit, root, true)) return result;
    std::ranges::sort(slps);
    slps.erase(std::unique(slps.begin(), slps.end()), slps.end());
    if (slps.empty()) {
        result.status = AssetCoverageStatus::missing_composite_part;
        result.reason = "DAT composite resolves no drawable SLP part";
    }
    return result;
}

struct Arguments {
    std::filesystem::path data_root;
    std::optional<std::filesystem::path> output;
    bool fail_on_unresolved{};
};

Arguments parse_arguments(int argc, char** argv) {
    Arguments result;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--data-root" && index + 1 < argc) {
            result.data_root = argv[++index];
        } else if (argument == "--output" && index + 1 < argc) {
            result.output = std::filesystem::path{argv[++index]};
        } else if (argument == "--fail-on-unresolved") {
            result.fail_on_unresolved = true;
        } else {
            throw std::invalid_argument{"unknown or incomplete argument: " +
                                        argument};
        }
    }
    if (result.data_root.empty()) {
        throw std::invalid_argument{"--data-root is required"};
    }
    return result;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const Arguments arguments = parse_arguments(argc, argv);
        const aoe::DrsArchive graphics{
            arguments.data_root / "graphics.drs"
        };
        const aoe::DrsArchive interface{
            arguments.data_root / "interfac.drs"
        };
        const aoe::LegacyPalette palette = aoe::LegacyPalette::from_jasc(
            interface.read("bina", 50500)
        );
        const aoe::LegacyDatFile dat = aoe::LegacyDatFile::load(
            arguments.data_root / "empires2_x1_p1.dat"
        );

        std::ostringstream report;
        std::map<
            std::tuple<std::int32_t, int, unsigned>,
            Validation
        > validations;
        std::map<
            std::tuple<std::int16_t, unsigned, bool>,
            std::pair<Validation, std::vector<std::int32_t>>
        > composite_validations;
        std::map<std::string, std::size_t> status_counts;
        std::map<std::string, std::map<std::string, std::size_t>> summary;
        std::size_t rows{};
        bool first = true;
        report << "{\n  \"schema\":\"aoe-renderer-asset-coverage-v2\",\n"
               << "  \"evidence\":{"
               << "\"asset_source\":\"classic Data archives\","
               << "\"mapping_source\":\"canonical_unit_animation_sets\","
               << "\"absolute_paths_omitted\":true},\n"
               << "  \"rows\":[";

        for (int raw_kind = 0;
             raw_kind <= static_cast<int>(aoe::UnitKind::elite_woad_raider);
             ++raw_kind) {
            const auto kind = static_cast<aoe::UnitKind>(raw_kind);
            const std::string kind_name = aoe::render_unit_kind_name(kind);
            for (const aoe::UnitRenderStateVariant& variant :
                 aoe::canonical_unit_render_states(kind)) {
                const RenderAction action = variant.action;
                const int family_count =
                    aoe::naval_composite_set(kind, action) != nullptr
                    ? 4
                    : 1;
                for (int family = 0; family < family_count; ++family) {
                for (unsigned owner = 0; owner < 8; ++owner) {
                    for (int direction = 0; direction < 8; ++direction) {
                        aoe::RenderStateKey state;
                        state.category = action == RenderAction::dying
                            ? aoe::RenderObjectCategory::unit_death
                            : aoe::RenderObjectCategory::unit;
                        state.object_kind = kind_name;
                        state.action = action;
                        state.action_detail = variant.action_detail;
                        state.owner = static_cast<std::uint8_t>(owner);
                        state.architecture_family = family;
                        state.direction = direction;
                        state.moving = variant.moving;
                        aoe::AssetResolution resolution =
                            aoe::resolve_unit_asset(state, kind);

                        Validation validation;
                        if (resolution.status ==
                            AssetCoverageStatus::renderable) {
                            if (resolution.request.slp_id) {
                                const auto cache_key = std::tuple{
                                    *resolution.request.slp_id,
                                    resolution.request.required_frame_count,
                                    owner + 1,
                                };
                                auto [found, inserted] =
                                    validations.try_emplace(cache_key);
                                if (inserted) {
                                    found->second = validate(
                                        graphics,
                                        palette,
                                        *resolution.request.slp_id,
                                        resolution.request.required_frame_count,
                                        owner + 1
                                    );
                                }
                                validation = found->second;
                            } else if (resolution.request.graphic_id) {
                                const aoe::NavalCompositeSet* mapping =
                                    aoe::naval_composite_set(kind, action);
                                const bool expand = mapping == nullptr ||
                                    mapping->expand_deltas;
                                const auto key = std::tuple{
                                    static_cast<std::int16_t>(
                                        *resolution.request.graphic_id
                                    ),
                                    owner + 1,
                                    expand,
                                };
                                auto [found, inserted] =
                                    composite_validations.try_emplace(key);
                                if (inserted) {
                                    found->second.first = validate_composite(
                                        dat,
                                        graphics,
                                        palette,
                                        std::get<0>(key),
                                        owner + 1,
                                        expand,
                                        found->second.second
                                    );
                                }
                                validation = found->second.first;
                                resolution.request.composite_slp_ids =
                                    found->second.second;
                            } else {
                                validation.status =
                                    AssetCoverageStatus::invalid_runtime_selection;
                                validation.reason =
                                    "resolution returned no graphic or SLP";
                            }
                            resolution.status = validation.status;
                            resolution.reason = validation.reason;
                        } else {
                            validation.status = resolution.status;
                            validation.reason = resolution.reason;
                        }

                        const int action_frames = std::max(
                            resolution.request.required_frame_count, 1
                        );
                        for (int frame = 0; frame < action_frames; ++frame) {
                            if (!first) report << ',';
                            first = false;
                            ++rows;
                            const std::string status =
                                aoe::asset_coverage_status_name(
                                    resolution.status
                                );
                            ++status_counts[status];
                            ++summary[kind_name][status];
                            report << "\n    {\"object_kind\":\""
                                   << kind_name << "\","
                                   << "\"state_dimensions\":{"
                                   << "\"action\":\""
                                   << aoe::render_action_name(action)
                                   << "\",\"action_detail\":\""
                                   << aoe::render_action_detail_name(
                                          variant.action_detail
                                      )
                                   << "\",\"moving\":"
                                   << (variant.moving ? "true" : "false")
                                   << ",\"owner\":" << owner
                                   << ",\"architecture_family\":" << family
                                   << ",\"direction\":" << direction
                                   << ",\"animation_frame\":" << frame
                                   << "},\"expected_asset_ids\":{"
                                   << "\"graphic_id\":";
                            if (resolution.request.graphic_id) {
                                report << *resolution.request.graphic_id;
                            } else {
                                report << "null";
                            }
                            report << ",\"slp_id\":";
                            if (resolution.request.slp_id) {
                                report << *resolution.request.slp_id;
                            } else {
                                report << "null";
                            }
                            report << ",\"composite_slp_ids\":[";
                            for (std::size_t slp_index = 0;
                                 slp_index <
                                     resolution.request.composite_slp_ids.size();
                                 ++slp_index) {
                                if (slp_index != 0) report << ',';
                                report << resolution.request
                                    .composite_slp_ids[slp_index];
                            }
                            report << "],\"overlay_graphic_ids\":[]},"
                                   << "\"frame_requirements\":{"
                                   << "\"frames_per_direction\":"
                                   << resolution.request.required_frame_count
                                   << ",\"stored_directions\":"
                                   << validation.stored_directions
                                   << ",\"actual_frame_count\":"
                                   << validation.actual_frames
                                   << "},\"status\":\"" << status << "\","
                                   << "\"failure_reason\":\""
                                   << json_escape(
                                          resolution.status ==
                                                  AssetCoverageStatus::renderable
                                              ? ""
                                              : resolution.reason
                                      )
                                   << "\",\"intentional_procedural\":"
                                   << (resolution.intentional_procedural
                                           ? "true"
                                           : "false")
                                   << ','
                                   << "\"evidence_source\":["
                                   << "\"canonical_unit_animation_sets\","
                                   << "\"graphics.drs\","
                                   << "\"interfac.drs:50500\"]}";
                        }
                    }
                }
                }
            }
        }
        for (const aoe::ProjectileAssetBinding& expected :
             aoe::canonical_projectile_asset_bindings()) {
            const std::string kind_name{
                aoe::projectile_asset_kind_name(expected.kind)
            };
            const auto live = aoe::find_projectile_asset_binding(
                dat, expected.kind
            );
            const auto audit_component = [&](
                aoe::RenderObjectCategory category,
                bool shadow,
                int direction_count,
                int frame_count
            ) {
                aoe::RenderStateKey state;
                state.category = category;
                state.object_kind = kind_name;
                state.shadow = shadow;
                aoe::AssetResolution resolution =
                    aoe::resolve_projectile_asset(state, expected.kind);
                Validation validation;
                if (!live) {
                    resolution.status =
                        AssetCoverageStatus::invalid_dat_reference;
                    resolution.reason =
                        "live DAT projectile record differs from canonical "
                        "binding";
                    resolution.intentional_procedural = false;
                    validation.status = resolution.status;
                    validation.reason = resolution.reason;
                } else if (resolution.request.slp_id) {
                    int frames_per_direction = frame_count;
                    if (frames_per_direction <= 0 &&
                        resolution.request.graphic_id) {
                        const aoe::LegacyGraphic* graphic = dat.graphic(
                            static_cast<std::size_t>(
                                *resolution.request.graphic_id
                            )
                        );
                        frames_per_direction = graphic == nullptr
                            ? 0
                            : graphic->frame_count;
                        direction_count = graphic == nullptr
                            ? 0
                            : graphic->angle_count;
                    }
                    const auto cache_key = std::tuple{
                        *resolution.request.slp_id,
                        frames_per_direction,
                        1U,
                    };
                    auto [found, inserted] =
                        validations.try_emplace(cache_key);
                    if (inserted) {
                        found->second = validate(
                            graphics,
                            palette,
                            *resolution.request.slp_id,
                            frames_per_direction,
                            1
                        );
                    }
                    validation = found->second;
                    if (validation.status !=
                        AssetCoverageStatus::renderable) {
                        resolution.status = validation.status;
                        resolution.reason = validation.reason;
                        resolution.intentional_procedural = false;
                    }
                    frame_count = frames_per_direction;
                } else {
                    validation.status = resolution.status;
                    validation.reason = resolution.reason;
                }
                direction_count = std::max(direction_count, 1);
                frame_count = std::max(frame_count, 1);
                for (int direction = 0;
                     direction < direction_count;
                     ++direction) {
                    for (int frame = 0; frame < frame_count; ++frame) {
                        if (!first) report << ',';
                        first = false;
                        ++rows;
                        const std::string status =
                            aoe::asset_coverage_status_name(
                                resolution.status
                            );
                        ++status_counts[status];
                        ++summary[kind_name][status];
                        report << "\n    {\"object_kind\":\""
                               << kind_name << "\","
                               << "\"state_dimensions\":{"
                               << "\"object_category\":\""
                               << (category ==
                                       aoe::RenderObjectCategory::impact
                                       ? "impact"
                                       : "projectile")
                               << "\",\"shadow\":"
                               << (shadow ? "true" : "false")
                               << ",\"direction\":" << direction
                               << ",\"animation_frame\":" << frame
                               << "},\"expected_asset_ids\":{"
                               << "\"graphic_id\":";
                        if (resolution.request.graphic_id) {
                            report << *resolution.request.graphic_id;
                        } else {
                            report << "null";
                        }
                        report << ",\"slp_id\":";
                        if (resolution.request.slp_id) {
                            report << *resolution.request.slp_id;
                        } else {
                            report << "null";
                        }
                        report << ",\"shadow_slp_id\":";
                        if (resolution.request.shadow_slp_id) {
                            report << *resolution.request.shadow_slp_id;
                        } else {
                            report << "null";
                        }
                        report << ",\"composite_slp_ids\":[],"
                               << "\"overlay_graphic_ids\":[]},"
                               << "\"frame_requirements\":{"
                               << "\"frames_per_direction\":"
                               << frame_count
                               << ",\"logical_directions\":"
                               << direction_count
                               << ",\"actual_frame_count\":"
                               << validation.actual_frames
                               << "},\"status\":\"" << status << "\","
                               << "\"failure_reason\":\""
                               << json_escape(
                                      resolution.status ==
                                              AssetCoverageStatus::renderable
                                          ? ""
                                          : resolution.reason
                                  )
                               << "\",\"intentional_procedural\":"
                               << (resolution.intentional_procedural
                                       ? "true"
                                       : "false")
                               << ",\"evidence_source\":["
                               << "\"canonical_projectile_asset_bindings\","
                               << "\"empires2_x1_p1.dat\","
                               << "\"graphics.drs\"]}";
                    }
                }
            };
            audit_component(
                aoe::RenderObjectCategory::projectile,
                false,
                expected.angle_count,
                expected.frame_count
            );
            if (expected.shadow_graphic && expected.shadow_slp_id) {
                audit_component(
                    aoe::RenderObjectCategory::projectile,
                    true,
                    0,
                    0
                );
            }
            if (expected.impact_graphic &&
                expected.impact_slp_id &&
                expected.impact_frame_count) {
                audit_component(
                    aoe::RenderObjectCategory::impact,
                    false,
                    1,
                    *expected.impact_frame_count
                );
            }
        }
        for (const auto& [kind_name, category, reason] :
             std::array{
                 std::tuple{
                     std::string_view{"generic_splash_stone"},
                     std::string_view{"projectile"},
                     std::string_view{
                         "generic splash projectile has explicit "
                         "procedural renderer"
                     },
                 },
                 std::tuple{
                     std::string_view{"generic_impact"},
                     std::string_view{"impact"},
                     std::string_view{
                         "generic impact has explicit procedural renderer"
                     },
                 },
             }) {
            if (!first) report << ',';
            first = false;
            ++rows;
            ++status_counts["intentional_procedural"];
            ++summary[std::string{kind_name}]["intentional_procedural"];
            report << "\n    {\"object_kind\":\"" << kind_name
                   << "\",\"state_dimensions\":{"
                   << "\"object_category\":\"" << category
                   << "\",\"procedural_phase\":\"all\"},"
                   << "\"expected_asset_ids\":{"
                   << "\"graphic_id\":null,\"slp_id\":null,"
                   << "\"shadow_slp_id\":null,"
                   << "\"composite_slp_ids\":[],"
                   << "\"overlay_graphic_ids\":[]},"
                   << "\"status\":\"intentional_procedural\","
                   << "\"failure_reason\":\"" << reason << "\","
                   << "\"intentional_procedural\":true,"
                   << "\"evidence_source\":["
                   << "\"runtime explicit procedural projectile path\"]}";
        }
        for (const aoe::ResourceAssetSet& mapping :
             aoe::canonical_resource_asset_sets()) {
            const std::string kind_name{
                aoe::resource_render_kind_name(mapping.kind)
            };
            const auto cache_key = std::tuple{
                mapping.slp_id, mapping.frame_count, 1U
            };
            auto [found, inserted] =
                validations.try_emplace(cache_key);
            if (inserted) {
                found->second = validate(
                    graphics,
                    palette,
                    mapping.slp_id,
                    mapping.frame_count,
                    1
                );
            }
            for (int frame = 0; frame < mapping.frame_count; ++frame) {
                aoe::RenderStateKey state;
                state.category = aoe::RenderObjectCategory::resource;
                state.object_kind = kind_name;
                state.animation_frame = frame;
                aoe::AssetResolution resolution =
                    aoe::resolve_resource_asset(state, mapping.kind);
                if (found->second.status !=
                    AssetCoverageStatus::renderable) {
                    resolution.status = found->second.status;
                    resolution.reason = found->second.reason;
                }
                if (!first) report << ',';
                first = false;
                ++rows;
                const std::string status =
                    aoe::asset_coverage_status_name(resolution.status);
                ++status_counts[status];
                ++summary[kind_name][status];
                report << "\n    {\"object_kind\":\"" << kind_name
                       << "\",\"state_dimensions\":{"
                       << "\"object_category\":\"resource\","
                       << "\"animation_frame\":" << frame
                       << "},\"expected_asset_ids\":{"
                       << "\"graphic_id\":null,\"slp_id\":"
                       << mapping.slp_id
                       << ",\"shadow_slp_id\":null,"
                       << "\"composite_slp_ids\":[],"
                       << "\"overlay_graphic_ids\":[]},"
                       << "\"frame_requirements\":{"
                       << "\"frame_count\":" << mapping.frame_count
                       << ",\"actual_frame_count\":"
                       << found->second.actual_frames
                       << "},\"status\":\"" << status << "\","
                       << "\"failure_reason\":\""
                       << json_escape(
                              resolution.status ==
                                      AssetCoverageStatus::renderable
                                  ? ""
                                  : resolution.reason
                          )
                       << "\",\"intentional_procedural\":false,"
                       << "\"evidence_source\":["
                       << "\"canonical_resource_asset_sets\","
                       << "\"graphics.drs\"]}";
            }
        }
        constexpr std::array building_states{
            aoe::RenderBuildingState::foundation,
            aoe::RenderBuildingState::construction,
            aoe::RenderBuildingState::completed,
            aoe::RenderBuildingState::damaged,
            aoe::RenderBuildingState::dying,
            aoe::RenderBuildingState::destroyed,
        };
        for (int raw_kind = 0;
             raw_kind <= static_cast<int>(aoe::BuildingKind::wonder);
             ++raw_kind) {
            const auto kind = static_cast<aoe::BuildingKind>(raw_kind);
            const std::string kind_name =
                aoe::render_building_kind_name(kind);
            for (const auto building_state : building_states) {
                for (int age = 0; age < 4; ++age) {
                    for (int raw_civilization =
                             static_cast<int>(aoe::Civilization::generic);
                         raw_civilization <=
                             static_cast<int>(aoe::Civilization::mayans);
                         ++raw_civilization) {
                        const auto civilization =
                            static_cast<aoe::Civilization>(
                                raw_civilization
                            );
                        if (civilization !=
                                aoe::Civilization::generic &&
                            !aoe::civilization_has_building(
                                civilization, kind
                            )) {
                            continue;
                        }
                        const int family =
                            aoe::render_building_architecture_family(
                                kind, civilization
                            );
                        for (unsigned owner = 0; owner < 8; ++owner) {
                        const int damage_stage_count =
                            building_state ==
                                aoe::RenderBuildingState::damaged
                            ? 4
                            : 1;
                        for (int damage_stage = 0;
                             damage_stage < damage_stage_count;
                             ++damage_stage) {
                        const int construction_stage_count =
                            building_state ==
                                aoe::RenderBuildingState::construction
                            ? 4
                            : 1;
                        for (int construction_stage = 0;
                             construction_stage <
                                 construction_stage_count;
                             ++construction_stage) {
                        const int topology_frame_count =
                            (building_state ==
                                 aoe::RenderBuildingState::completed ||
                             building_state ==
                                 aoe::RenderBuildingState::damaged) &&
                                aoe::building_topology_slp_set(kind) !=
                                    nullptr
                            ? aoe::building_topology_slp_set(kind)
                                  ->reachable_frame_count
                            : 1;
                        for (int topology_frame = 0;
                             topology_frame < topology_frame_count;
                             ++topology_frame) {
                        const int upgrade_variant_count =
                            kind == aoe::BuildingKind::watch_tower &&
                                (building_state ==
                                     aoe::RenderBuildingState::completed ||
                                 building_state ==
                                     aoe::RenderBuildingState::damaged)
                            ? 3
                            : 1;
                        for (int upgrade_variant = 0;
                             upgrade_variant < upgrade_variant_count;
                             ++upgrade_variant) {
                            if ((upgrade_variant == 1 &&
                                 age < static_cast<int>(aoe::Age::castle)) ||
                                (upgrade_variant == 2 &&
                                 age < static_cast<int>(
                                     aoe::Age::imperial
                                 ))) {
                                continue;
                            }
                            aoe::RenderStateKey state;
                            state.category =
                                building_state ==
                                    aoe::RenderBuildingState::destroyed ||
                                building_state ==
                                    aoe::RenderBuildingState::dying
                                ? aoe::RenderObjectCategory::building_rubble
                                : aoe::RenderObjectCategory::building;
                            state.object_kind = kind_name;
                            state.action =
                                building_state ==
                                    aoe::RenderBuildingState::destroyed
                                ? RenderAction::destroyed
                                : building_state ==
                                    aoe::RenderBuildingState::dying
                                    ? RenderAction::dying
                                : building_state ==
                                    aoe::RenderBuildingState::foundation ||
                                  building_state ==
                                    aoe::RenderBuildingState::construction
                                    ? RenderAction::working
                                    : RenderAction::idle;
                            state.building_state = building_state;
                            state.owner =
                                static_cast<std::uint8_t>(owner);
                            state.civilization = civilization;
                            state.age = static_cast<aoe::Age>(age);
                            state.architecture_family = family;
                            state.animation_frame = topology_frame;
                            state.upgrade_variant = upgrade_variant;
                            state.damage_stage = damage_stage;
                            state.construction_stage =
                                building_state ==
                                    aoe::RenderBuildingState::foundation
                                ? 0
                                : building_state ==
                                    aoe::RenderBuildingState::construction
                                    ? construction_stage
                                    : 4;
                            aoe::AssetResolution resolution =
                                aoe::resolve_building_asset(state, kind);
                            Validation validation;
                            if (resolution.status ==
                                    AssetCoverageStatus::renderable &&
                                resolution.request.slp_id) {
                                const auto cache_key = std::tuple{
                                    *resolution.request.slp_id,
                                    resolution.request.required_frame_count,
                                    owner + 1,
                                };
                                auto [found, inserted] =
                                    validations.try_emplace(cache_key);
                                if (inserted) {
                                    found->second = validate(
                                        graphics,
                                        palette,
                                        *resolution.request.slp_id,
                                        resolution.request.required_frame_count,
                                        owner + 1
                                    );
                                }
                                validation = found->second;
                                resolution.status = validation.status;
                                resolution.reason = validation.reason;
                            } else if (resolution.status ==
                                           AssetCoverageStatus::renderable &&
                                       resolution.request.graphic_id) {
                                bool expand = true;
                                if (building_state ==
                                    aoe::RenderBuildingState::completed) {
                                    const auto* mapping =
                                        aoe::building_composite_set(kind);
                                    expand = mapping == nullptr ||
                                        mapping->composition_policy ==
                                            aoe::CompositePolicy::delta_graph;
                                }
                                const auto key = std::tuple{
                                    static_cast<std::int16_t>(
                                        *resolution.request.graphic_id
                                    ),
                                    owner + 1,
                                    expand,
                                };
                                auto [found, inserted] =
                                    composite_validations.try_emplace(key);
                                if (inserted) {
                                    found->second.first = validate_composite(
                                        dat,
                                        graphics,
                                        palette,
                                        std::get<0>(key),
                                        owner + 1,
                                        expand,
                                        found->second.second
                                    );
                                }
                                validation = found->second.first;
                                resolution.request.composite_slp_ids =
                                    found->second.second;
                                resolution.status = validation.status;
                                resolution.reason = validation.reason;
                            } else {
                                validation.status = resolution.status;
                                validation.reason = resolution.reason;
                            }
                            if (resolution.status ==
                                    AssetCoverageStatus::renderable) {
                                const auto* topology =
                                    aoe::building_topology_slp_set(kind);
                                if (resolution.request.shadow_slp_id) {
                                    const auto key = std::tuple{
                                        *resolution.request.shadow_slp_id,
                                        topology == nullptr
                                            ? 1
                                            : topology->asset_frame_count,
                                        1U,
                                    };
                                    auto [found, inserted] =
                                        validations.try_emplace(key);
                                    if (inserted) {
                                        found->second = validate(
                                            graphics,
                                            palette,
                                            std::get<0>(key),
                                            std::get<1>(key),
                                            1
                                        );
                                    }
                                    if (found->second.status !=
                                        AssetCoverageStatus::renderable) {
                                        resolution.status =
                                            AssetCoverageStatus::
                                                missing_shadow;
                                        resolution.reason =
                                            "linked shadow SLP " +
                                            std::to_string(std::get<0>(key)) +
                                            ": " + found->second.reason;
                                    }
                                }
                                if (resolution.status ==
                                        AssetCoverageStatus::renderable &&
                                    topology != nullptr &&
                                    topology->junction_overlay_slp_id &&
                                    std::ranges::find(
                                        resolution.request
                                            .composite_slp_ids,
                                        *topology
                                             ->junction_overlay_slp_id
                                    ) != resolution.request
                                             .composite_slp_ids.end()) {
                                    const auto key = std::tuple{
                                        *topology
                                             ->junction_overlay_slp_id,
                                        topology
                                            ->junction_overlay_frame_count,
                                        owner + 1,
                                    };
                                    auto [found, inserted] =
                                        validations.try_emplace(key);
                                    if (inserted) {
                                        found->second = validate(
                                            graphics,
                                            palette,
                                            std::get<0>(key),
                                            std::get<1>(key),
                                            std::get<2>(key)
                                        );
                                    }
                                    if (found->second.status !=
                                        AssetCoverageStatus::renderable) {
                                        resolution.status =
                                            AssetCoverageStatus::
                                                missing_composite_part;
                                        resolution.reason =
                                            "junction overlay SLP " +
                                            std::to_string(std::get<0>(key)) +
                                            ": " + found->second.reason;
                                    }
                                }
                            }
                            if (resolution.status ==
                                    AssetCoverageStatus::renderable ||
                                resolution.status ==
                                    AssetCoverageStatus::
                                        intentional_procedural) {
                                for (const std::int16_t overlay_root :
                                     resolution.request
                                         .overlay_graphic_ids) {
                                    const auto key = std::tuple{
                                        overlay_root,
                                        owner + 1,
                                        true,
                                    };
                                    auto [found, inserted] =
                                        composite_validations.try_emplace(key);
                                    if (inserted) {
                                        found->second.first =
                                            validate_composite(
                                                dat,
                                                graphics,
                                                palette,
                                                overlay_root,
                                                owner + 1,
                                                true,
                                                found->second.second
                                            );
                                    }
                                    resolution.request.composite_slp_ids.insert(
                                        resolution.request
                                            .composite_slp_ids.end(),
                                        found->second.second.begin(),
                                        found->second.second.end()
                                    );
                                    if (found->second.first.status !=
                                        AssetCoverageStatus::renderable) {
                                        resolution.status =
                                            found->second.first.status;
                                        resolution.reason =
                                            "damage overlay root " +
                                            std::to_string(overlay_root) +
                                            ": " +
                                            found->second.first.reason;
                                        resolution.intentional_procedural =
                                            false;
                                        break;
                                    }
                                }
                            }

                            if (!first) report << ',';
                            first = false;
                            ++rows;
                            const std::string status =
                                aoe::asset_coverage_status_name(
                                    resolution.status
                                );
                            ++status_counts[status];
                            ++summary[kind_name][status];
                            report << "\n    {\"object_kind\":\""
                                   << kind_name << "\","
                                   << "\"state_dimensions\":{"
                                   << "\"building_state\":\""
                                   << building_state_name(building_state)
                                   << "\",\"owner\":" << owner
                                   << ",\"age\":\""
                                   << age_name(static_cast<aoe::Age>(age))
                                   << "\",\"age_id\":" << age
                                   << ",\"civilization\":\""
                                   << civilization_name(civilization)
                                   << "\",\"civilization_id\":"
                                   << raw_civilization
                                   << ",\"architecture_family\":" << family
                                   << ",\"damage_stage\":"
                                   << state.damage_stage
                                   << ",\"construction_stage\":"
                                   << state.construction_stage
                                   << ",\"topology_frame\":"
                                   << state.animation_frame
                                   << ",\"upgrade_variant\":"
                                   << state.upgrade_variant
                                   << "},\"expected_asset_ids\":{"
                                   << "\"graphic_id\":";
                            if (resolution.request.graphic_id) {
                                report << *resolution.request.graphic_id;
                            } else {
                                report << "null";
                            }
                            report << ",\"slp_id\":";
                            if (resolution.request.slp_id) {
                                report << *resolution.request.slp_id;
                            } else {
                                report << "null";
                            }
                            report << ",\"composite_slp_ids\":[";
                            for (std::size_t index = 0;
                                 index <
                                     resolution.request.composite_slp_ids.size();
                                 ++index) {
                                if (index != 0) report << ',';
                                report << resolution.request
                                    .composite_slp_ids[index];
                            }
                            report << "],\"overlay_graphic_ids\":[";
                            for (std::size_t index = 0;
                                 index <
                                     resolution.request
                                         .overlay_graphic_ids.size();
                                 ++index) {
                                if (index != 0) report << ',';
                                report << resolution.request
                                    .overlay_graphic_ids[index];
                            }
                            report << "],\"shadow_slp_id\":";
                            if (resolution.request.shadow_slp_id) {
                                report << *resolution.request.shadow_slp_id;
                            } else {
                                report << "null";
                            }
                            report << "},\"status\":\"" << status << "\","
                                   << "\"failure_reason\":\""
                                   << json_escape(
                                          resolution.status ==
                                                  AssetCoverageStatus::renderable
                                              ? ""
                                              : resolution.reason
                                      )
                                   << "\",\"intentional_procedural\":"
                                   << (resolution.intentional_procedural
                                           ? "true"
                                           : "false")
                                   << ",\"evidence_source\":["
                                   << "\"canonical building catalog\","
                                   << "\"empires2_x1_p1.dat\","
                                   << "\"graphics.drs\"]}";
                        }
                        }
                        }
                    }
                }
            }
            }
            }
        }
        report << "\n  ],\n  \"summary\":{"
               << "\"row_count\":" << rows << ",\"by_status\":{";
        bool first_status = true;
        for (const auto& [status, count] : status_counts) {
            if (!first_status) report << ',';
            first_status = false;
            report << '"' << status << "\":" << count;
        }
        report << "},\"by_object_kind\":{";
        bool first_kind = true;
        for (const auto& [kind, statuses] : summary) {
            if (!first_kind) report << ',';
            first_kind = false;
            report << '"' << kind << "\":{";
            bool first_kind_status = true;
            for (const auto& [status, count] : statuses) {
                if (!first_kind_status) report << ',';
                first_kind_status = false;
                report << '"' << status << "\":" << count;
            }
            report << '}';
        }
        report << "}}\n}\n";

        if (arguments.output) {
            std::ofstream output{
                *arguments.output,
                std::ios::binary | std::ios::trunc
            };
            output << report.str();
            if (!output) throw std::runtime_error{"cannot write audit output"};
        } else {
            std::cout << report.str();
        }
        std::cerr << "renderer asset audit: " << rows << " states";
        for (const auto& [status, count] : status_counts) {
            std::cerr << ", " << status << '=' << count;
        }
        std::cerr << '\n';

        const std::size_t unresolved =
            rows - status_counts["renderable"] -
            status_counts["intentional_procedural"];
        return arguments.fail_on_unresolved && unresolved != 0 ? 2 : 0;
    } catch (const std::exception& error) {
        std::cerr << "renderer_asset_coverage_audit: "
                  << error.what() << '\n';
        return 1;
    }
}
