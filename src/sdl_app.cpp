#include "aoe/sdl_app.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iterator>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <vector>

#include "aoe/audio_system.hpp"
#include "aoe/asset_root.hpp"
#include "aoe/campaign.hpp"
#include "aoe/building_placement.hpp"
#include "aoe/building_damage.hpp"
#include "aoe/command_panel.hpp"
#include "aoe/cursor_contract.hpp"
#include "aoe/frontend_audio.hpp"
#include "aoe/frame_timing.hpp"
#include "aoe/computer_player.hpp"
#include "aoe/game_command.hpp"
#include "aoe/game_rules.hpp"
#include "aoe/hud_layout_contract.hpp"
#include "aoe/initial_camera.hpp"
#include "aoe/legacy_assets.hpp"
#include "aoe/legacy_dat.hpp"
#include "aoe/localization.hpp"
#include "aoe/multiplayer.hpp"
#include "aoe/multiplayer_transport.hpp"
#include "aoe/minimap_contract.hpp"
#include "aoe/projectile_catalog.hpp"
#include "aoe/random_map.hpp"
#include "aoe/render_asset_coverage.hpp"
#include "aoe/rms_import.hpp"
#include "aoe/save_game.hpp"
#include "aoe/save_browser.hpp"
#include "aoe/scenario.hpp"
#include "aoe/scenario_editor.hpp"
#include "aoe/selection_controls.hpp"
#include "aoe/settings.hpp"
#include "aoe/shadow_catalog.hpp"
#include "aoe/statistics_view.hpp"
#include "aoe/technology_tree.hpp"
#include "aoe/terrain_transition.hpp"
#include "aoe/ui_assets.hpp"
#include "aoe/ui_icon_contract.hpp"
#include "aoe/ui_perspective.hpp"
#include "aoe/world_tile_picker.hpp"

namespace aoe {
namespace {

constexpr int tile_width = 64;
constexpr int tile_height = 32;
constexpr int half_tile_width = tile_width / 2;
constexpr int half_tile_height = tile_height / 2;
// World viewport size in pixels. This is the drawing surface, not the map:
// it used to be spelled (24 + 16) * half_tile, which silently tied the
// window and the camera clamp to a 24x16 map.
constexpr int hud_height = 175;
int view_pixel_width = 40 * half_tile_width;
int logical_screen_height = 720;
int view_pixel_height = logical_screen_height - hud_height;
constexpr int map_origin_y = 16;
constexpr int elevation_pixel_step = half_tile_height;
constexpr float default_camera_zoom = 1.25F;
constexpr float minimum_camera_zoom = 1.0F;
constexpr float maximum_camera_zoom = 2.0F;

std::optional<unsigned> taunt_number(std::string_view text) {
    while (!text.empty() && text.front() == ' ') text.remove_prefix(1);
    while (!text.empty() && text.back() == ' ') text.remove_suffix(1);
    unsigned number{};
    const auto [end, error] = std::from_chars(
        text.data(), text.data() + text.size(), number
    );
    if (error != std::errc{} ||
        end != text.data() + text.size() ||
        number < 1 || number > 42) {
        return std::nullopt;
    }
    return number;
}

bool panel_hotkey_matches(std::string_view hotkey, SDL_Keycode key) {
    if (hotkey == "ESC") return key == SDLK_ESCAPE;
    if (hotkey == "<") {
        return key == SDLK_COMMA || key == SDLK_LEFT;
    }
    if (hotkey == ">") {
        return key == SDLK_PERIOD || key == SDLK_RIGHT;
    }
    if (hotkey.size() != 1) return false;
    const unsigned char expected =
        static_cast<unsigned char>(hotkey.front());
    const char* key_name = SDL_GetKeyName(key);
    return key_name != nullptr && key_name[0] != '\0' &&
        key_name[1] == '\0' &&
        std::toupper(static_cast<unsigned char>(key_name[0])) ==
            std::toupper(expected);
}

struct CameraView {
    float x{};
    float y{};
    float zoom{default_camera_zoom};
};

struct ScenarioPresentation {
    bool objectives_visible{};
};

struct CampaignPresentation {
    enum class Screen { briefing, status, debrief };
    Campaign campaign;
    CampaignProgress progress;
    std::filesystem::path progress_path;
    CampaignScenarioEntry scenario;
    bool visible{};
    bool outcome_processed{};
    bool narration_started{};
    bool debrief_narration_started{};
    Screen screen{Screen::briefing};
    std::string optional_narration_path;
    std::string optional_cinematic_path;
};

struct ScenarioStartup {
    Scenario scenario;
    std::optional<CampaignPresentation> campaign;
};

struct MultiplayerPresentation {
    std::string mode;
    std::string scenario_digest;
    Player local_slot{Player::blue};
    LockstepSession session;
    LockstepSessionConfig config;
    bool hosting{};
    bool local_ready{};
    bool blue_ready{};
    bool red_ready{};
    bool live_transport{};
    bool transport_connected{};
    std::uint16_t port{};
    LockstepStatus live_status{LockstepStatus::handshaking};
    std::uint64_t live_tick{};
    bool visible{true};
    bool waiting_for_turn{};
    bool chat_input_active{};
    ChatAudience chat_audience{ChatAudience::all};
    std::string chat_input;
    std::string chat_feedback;
    std::vector<LockstepChatMessage> chat_log;
    std::vector<LockstepMapSignal> signal_log;
    NetworkTimingMetrics network_metrics;
    SaveBarrierStatus checkpoint_status{SaveBarrierStatus::idle};
    std::uint64_t checkpoint_tick{};
    std::string checkpoint_path;
    std::string checkpoint_feedback{"F6: COORDINATED SAVE"};
    bool network_paused{};
    GameSpeed game_speed{GameSpeed::normal};
    int effective_cadence_ms{200};
    std::string control_feedback{"F7: PAUSE   F8: SPEED"};

    MultiplayerPresentation(
        std::string mode_value,
        std::string digest,
        Player slot
    ) : mode(std::move(mode_value)),
        scenario_digest(std::move(digest)),
        local_slot(slot),
        session(scenario_digest, 3, 10) {}
};

CameraView active_camera;
Player active_view_player{Player::blue};
const GameMap* active_render_map{};
const StringTable* active_string_table{};

// Tile extent of the map currently being presented. The isometric origin
// and the camera clamp both depend on it, so a map larger than the old
// hardcoded 24x16 would otherwise be pinned to the top corner of a 24x16
// diamond with everything else unreachable.
[[nodiscard]] int active_map_tiles_x() {
    return active_render_map != nullptr
        ? std::max(active_render_map->width(), 1)
        : 1;
}

[[nodiscard]] int active_map_tiles_y() {
    return active_render_map != nullptr
        ? std::max(active_render_map->height(), 1)
        : 1;
}

// Left-hand tile (0, height - 1) sits at x = 0, so the whole diamond is
// inside the positive world quadrant.
[[nodiscard]] int map_origin_x() {
    return active_map_tiles_y() * half_tile_width;
}

[[nodiscard]] int world_pixel_width() {
    return (active_map_tiles_x() + active_map_tiles_y()) * half_tile_width;
}

[[nodiscard]] int world_pixel_height() {
    return (active_map_tiles_x() + active_map_tiles_y()) * half_tile_height;
}

enum class EditorTool {
    grass, water, forest, elevation, villager, house, erase
};
bool active_editor_overlay{};
EditorTool active_editor_tool{EditorTool::grass};
std::string active_editor_status{"EDITOR READY"};
std::size_t active_editor_focus{};
TilePosition active_editor_cursor{1, 1};
Player active_editor_player{Player::blue};
enum class FrontendScreen { hidden, main_menu, single_player_setup };
FrontendScreen active_frontend_screen{FrontendScreen::main_menu};
std::string active_frontend_status{"SELECT A MODE"};
RandomMapSettings active_random_settings{
    RandomMapKind::arabia, RandomMapSize::maximum, 1
};
const Scenario* active_random_preview{};
std::string active_random_map_source{"CLASSIC RMS"};

// Modern choice: the reconstruction presents only the recovered maximum.
// Smaller presets remain available to import/generation APIs for fidelity
// tests, but cannot become a playable frontend selection.
constexpr std::array<RandomMapSize, 1> random_map_size_order{{
    RandomMapSize::maximum,
}};

const char* random_map_size_label(RandomMapSize size) {
    switch (size) {
        case RandomMapSize::tiny: return "TINY";
        case RandomMapSize::small: return "SMALL";
        case RandomMapSize::medium: return "MEDIUM";
        case RandomMapSize::normal: return "NORMAL";
        case RandomMapSize::large: return "LARGE";
        case RandomMapSize::giant: return "GIANT";
        case RandomMapSize::maximum: return "MAXIMUM";
    }
    return "SMALL";
}

RandomMapSize next_random_map_size(RandomMapSize size) {
    const auto current = std::ranges::find(random_map_size_order, size);
    if (current == random_map_size_order.end()) {
        return random_map_size_order.front();
    }
    const auto next = std::next(current);
    return next == random_map_size_order.end()
        ? random_map_size_order.front()
        : *next;
}

Civilization active_setup_civilization{Civilization::britons};
ComputerDifficulty active_setup_difficulty{ComputerDifficulty::moderate};
int active_setup_victory{};  // 0 conquest, 1 wonder, 2 relic.
bool active_technology_tree_visible{};
TechnologyTreeLayout active_technology_tree =
    build_technology_tree(Civilization::britons);
float active_tree_pan_x{};
float active_tree_pan_y{};
float active_tree_zoom{0.72F};
std::string active_tree_hover;
std::size_t active_tree_focus{};
bool active_tree_dragging{};
SDL_FPoint active_tree_drag_origin{};
bool active_diplomacy_panel_visible{};
ResourceKind active_tribute_resource{ResourceKind::food};
int active_tribute_amount{100};
std::string active_diplomacy_status{"SELECT ACTION"};
bool active_options_visible{};
bool active_options_hotkeys{};
ReconstructionSettings active_settings;
ReconstructionSettings draft_settings;
std::filesystem::path active_settings_path;
std::string active_options_status{"LETTER KEYS CHANGE  A APPLY  S SAVE"};
bool active_statistics_visible{};
bool active_statistics_postgame{};
StatisticsTab active_statistics_tab{StatisticsTab::economy};
bool active_save_browser_visible{};
bool active_save_slot_input{};
bool active_save_overwrite_armed{};
std::string active_save_slot{"quicksave"};
std::string active_save_browser_status{"F4 CLOSE  N NAME/SAVE"};
std::vector<BrowserEntry> active_browser_entries;
std::size_t active_browser_selection{};
std::filesystem::path active_browser_root;
int active_command_hover{-1};
PanelPage active_command_page{PanelPage::root};
std::size_t active_command_subpage{};
std::optional<TilePosition> active_build_preview_tile;
std::optional<TilePosition> active_wall_drag_start;
struct VisibleMapSignal {
    LockstepMapSignal signal;
    Uint64 received_ms{};
};
std::vector<VisibleMapSignal> active_map_signals;
std::uint64_t active_last_signal_sequence{};
std::uint64_t active_last_taunt_sequence{};

std::string_view ui_text(std::string_view key) {
    return active_string_table != nullptr
        ? active_string_table->text(key)
        : key;
}

std::string ui_debug_text(std::string_view key) {
    return debug_font_fallback(ui_text(key));
}

struct TerrainTextures {
    SDL_Texture* grass{};
    SDL_Texture* water{};
    SDL_Texture* beach{};
    SDL_Texture* shallows{};
    SDL_Texture* farm_growing{};
    SDL_Texture* farm_harvested{};
    std::vector<SDL_Texture*> grass_archive_frames;
    std::vector<SDL_Texture*> water_archive_frames;
    std::vector<SDL_Texture*> beach_archive_frames;
    std::vector<SDL_Texture*> shallows_archive_frames;
    std::vector<RgbaFrame> grass_archive_rgba;
    std::vector<RgbaFrame> water_archive_rgba;
    std::vector<RgbaFrame> beach_archive_rgba;
    std::vector<RgbaFrame> shallows_archive_rgba;
    std::optional<BlendomaticData> blendomatic;
    std::map<std::string, SDL_Texture*> transition_cache;

    void destroy() {
        SDL_DestroyTexture(grass);
        SDL_DestroyTexture(water);
        SDL_DestroyTexture(beach);
        SDL_DestroyTexture(shallows);
        SDL_DestroyTexture(farm_growing);
        SDL_DestroyTexture(farm_harvested);
        for (SDL_Texture* texture : grass_archive_frames) {
            SDL_DestroyTexture(texture);
        }
        for (SDL_Texture* texture : water_archive_frames) {
            SDL_DestroyTexture(texture);
        }
        for (SDL_Texture* texture : beach_archive_frames) {
            SDL_DestroyTexture(texture);
        }
        for (SDL_Texture* texture : shallows_archive_frames) {
            SDL_DestroyTexture(texture);
        }
        grass = nullptr;
        water = nullptr;
        beach = nullptr;
        shallows = nullptr;
        farm_growing = nullptr;
        farm_harvested = nullptr;
        grass_archive_frames.clear();
        water_archive_frames.clear();
        beach_archive_frames.clear();
        shallows_archive_frames.clear();
        grass_archive_rgba.clear();
        water_archive_rgba.clear();
        beach_archive_rgba.clear();
        shallows_archive_rgba.clear();
        for (auto& [key, texture] : transition_cache) {
            (void)key;
            SDL_DestroyTexture(texture);
        }
        transition_cache.clear();
        blendomatic.reset();
    }
};

TerrainTextures active_terrain_textures;

struct LegacySprite {
    SDL_Texture* texture{};
    int width{};
    int height{};
    int hotspot_x{};
    int hotspot_y{};

    void destroy() {
        SDL_DestroyTexture(texture);
        texture = nullptr;
    }
};

struct LegacyHudBackground {
    std::array<LegacySprite, hud_layout::game_background_frame_count> frames;

    void destroy() {
        for (LegacySprite& frame : frames) frame.destroy();
    }

    [[nodiscard]] bool complete() const {
        return std::ranges::all_of(
            frames,
            [](const LegacySprite& frame) {
                return frame.texture != nullptr;
            }
        );
    }
};

struct LegacyStaticShadow {
    LegacySprite sprite;
    int offset_x{};
    int offset_y{};
};

struct LegacyAnimation {
    std::vector<LegacySprite> frames;
    std::size_t frames_per_angle{1};
    std::vector<std::size_t> angle_offsets;
    std::vector<std::size_t> angle_frame_counts;
    std::vector<LegacySprite> shadow_frames;
    std::size_t shadow_frames_per_angle{1};
    int shadow_angle_count{};
    int shadow_display_angle{-1};
    int shadow_offset_x{};
    int shadow_offset_y{};

    void destroy() {
        for (LegacySprite& frame : frames) {
            frame.destroy();
        }
        frames.clear();
        angle_offsets.clear();
        angle_frame_counts.clear();
        for (LegacySprite& frame : shadow_frames) {
            frame.destroy();
        }
        shadow_frames.clear();
    }
};

struct PlayerLegacySprites {
    LegacyAnimation blue;
    LegacyAnimation red;
    std::array<LegacyAnimation, 6> additional;

    LegacyAnimation& slot(std::size_t index) {
        if (index == 0) return blue;
        if (index == 1) return red;
        if (index < 8) return additional[index - 2];
        throw LegacyAssetError{"player sprite slot is invalid"};
    }
    const LegacyAnimation& slot(std::size_t index) const {
        return const_cast<PlayerLegacySprites*>(this)->slot(index);
    }
    const LegacyAnimation* owner(EntityOwner value) const {
        const auto index = value.slot_index();
        return index ? &slot(*index) : nullptr;
    }

    void destroy() {
        blue.destroy();
        red.destroy();
        for (LegacyAnimation& animation : additional) {
            animation.destroy();
        }
    }
};

struct LegacyCompositePart {
    LegacySprite sprite;
    int layer{};
    int offset_x{};
    int offset_y{};
};

struct LegacyComposite {
    std::vector<LegacyCompositePart> parts;

    void destroy() {
        for (LegacyCompositePart& part : parts) {
            part.sprite.destroy();
        }
        parts.clear();
    }
};

struct PlayerLegacyComposite {
    LegacyComposite blue;
    LegacyComposite red;
    std::array<LegacyComposite, 6> additional;

    LegacyComposite& slot(std::size_t index) {
        if (index == 0) return blue;
        if (index == 1) return red;
        if (index < 8) return additional[index - 2];
        throw LegacyAssetError{"player composite slot is invalid"};
    }
    const LegacyComposite& slot(std::size_t index) const {
        return const_cast<PlayerLegacyComposite*>(this)->slot(index);
    }
    const LegacyComposite* owner(EntityOwner value) const {
        const auto index = value.slot_index();
        return index ? &slot(*index) : nullptr;
    }

    void destroy() {
        blue.destroy();
        red.destroy();
        for (LegacyComposite& composite : additional) {
            composite.destroy();
        }
    }
};

struct LegacyAnimatedCompositePart {
    LegacyAnimation animation;
    int layer{};
    int offset_x{};
    int offset_y{};
    int angle_count{1};
    int display_angle{-1};
};

struct LegacyAnimatedComposite {
    std::vector<LegacyAnimatedCompositePart> parts;

    void destroy() {
        for (LegacyAnimatedCompositePart& part : parts) {
            part.animation.destroy();
        }
        parts.clear();
    }
};

struct PlayerLegacyAnimatedComposite {
    LegacyAnimatedComposite blue;
    LegacyAnimatedComposite red;
    std::array<LegacyAnimatedComposite, 6> additional;

    LegacyAnimatedComposite& slot(std::size_t index) {
        if (index == 0) return blue;
        if (index == 1) return red;
        if (index < 8) return additional[index - 2];
        throw LegacyAssetError{"player animated composite slot is invalid"};
    }
    const LegacyAnimatedComposite& slot(std::size_t index) const {
        return const_cast<PlayerLegacyAnimatedComposite*>(this)->slot(index);
    }
    const LegacyAnimatedComposite* owner(EntityOwner value) const {
        const auto index = value.slot_index();
        return index ? &slot(*index) : nullptr;
    }

    void destroy() {
        blue.destroy();
        red.destroy();
        for (LegacyAnimatedComposite& composite : additional) {
            composite.destroy();
        }
    }
};

struct LegacySprites {
    LegacySprite sheep_blue;
    LegacySprite sheep_red;
    LegacySprite villager_blue;
    LegacySprite villager_red;
    std::array<LegacySprite, 4> town_center_blue;
    std::array<LegacySprite, 4> town_center_red;
    std::array<LegacySprite, 2> archery_range_blue;
    std::array<LegacySprite, 2> archery_range_red;
    LegacySprite tree;
    std::array<LegacySprite, 4> berry_states;
    std::array<LegacySprite, 7> gold_states;
    std::array<LegacySprite, 7> stone_states;
    LegacySprite fish;
    std::map<SDL_Texture*, LegacyStaticShadow>
        building_static_shadows;
    LegacyAnimation fire_projectile;
    LegacyAnimation cannonball_projectile;
    LegacyAnimation gunshot_projectile;
    LegacyAnimation scorpion_projectile;
    LegacyAnimation onager_primary_projectile;
    LegacyAnimation onager_volley_projectile;
    LegacyAnimation trebuchet_projectile;
    LegacyAnimation axe_projectile;
    LegacyAnimation arrow_projectile;
    LegacyAnimation siege_impact;
    LegacyAnimation trebuchet_impact;
    LegacySprite bombard_tower_standing_blue;
    LegacySprite bombard_tower_standing_red;
    LegacySprite bombard_tower_construction_blue;
    LegacySprite bombard_tower_construction_red;
    LegacySprite bombard_tower_dying_blue;
    LegacySprite bombard_tower_dying_red;
    LegacySprite relic;
    PlayerLegacySprites carried_relic_standing;
    PlayerLegacySprites carried_relic_moving;
    LegacySprite hud_background;
    std::array<LegacyHudBackground, 19> civilization_hud_backgrounds;
    LegacySprite hud_actions;
    std::map<std::int32_t, LegacySprite> action_command_icons;
    std::map<std::int32_t, LegacySprite> unit_command_icons;
    std::map<std::int32_t, LegacySprite> technology_command_icons;
    std::map<std::int32_t, LegacySprite> building_command_icons;
    std::array<LegacySprite, 4> resource_icons;
    LegacySprite portrait_frame;
    SDL_Texture* frontend_background{};
    SDL_Texture* scenario_background{};
    LegacySprite campaign_background;
    LegacySprite market_western_blue;
    LegacySprite market_western_red;
    LegacySprite market_eastern_blue;
    LegacySprite market_eastern_red;
    LegacySprite market_mediterranean_blue;
    LegacySprite market_mediterranean_red;
    LegacySprite market_far_eastern_blue;
    LegacySprite market_far_eastern_red;
    PlayerLegacySprites trade_cart_standing;
    PlayerLegacySprites trade_cart_moving;
    PlayerLegacySprites fishing_ship_standing;
    PlayerLegacySprites fishing_ship_moving;
    PlayerLegacySprites fish_trap_standing;
    PlayerLegacySprites fish_trap_construction;
    PlayerLegacySprites outpost_construction;
    PlayerLegacySprites outpost_death;
    PlayerLegacySprites wonder_construction;
    PlayerLegacySprites wonder_death;
    std::map<Civilization, PlayerLegacyComposite> wonder_standing;
    PlayerLegacySprites longboat_attack;
    PlayerLegacySprites longboat_idle;
    PlayerLegacySprites longboat_move;
    std::array<std::array<LegacySprite, 4>, 4> house_blue;
    std::array<std::array<LegacySprite, 4>, 4> house_red;
    std::array<std::array<LegacySprite, 4>, 4> town_center_age_blue;
    std::array<std::array<LegacySprite, 4>, 4> town_center_age_red;
    std::array<std::array<LegacySprite, 4>, 2> blacksmith_blue;
    std::array<std::array<LegacySprite, 4>, 2> blacksmith_red;
    std::array<LegacySprite, 4> lumber_camp_blue;
    std::array<LegacySprite, 4> lumber_camp_red;
    std::array<LegacySprite, 4> mining_camp_blue;
    std::array<LegacySprite, 4> mining_camp_red;
    std::array<std::array<LegacySprite, 4>, 2> university_blue;
    std::array<std::array<LegacySprite, 4>, 2> university_red;
    std::array<std::array<LegacySprite, 4>, 3> market_age_blue;
    std::array<std::array<LegacySprite, 4>, 3> market_age_red;
    std::array<
        std::array<std::array<LegacySprite, 5>, 5>,
        8
    > stone_wall_by_owner;
    std::array<PlayerLegacyComposite, 5>
        stone_wall_construction;
    std::map<BuildingKind, std::array<PlayerLegacyComposite, 5>>
        gate_construction;
    std::array<LegacySprite, 5> palisade_wall_shadow;
    std::array<std::array<LegacySprite, 5>, 8>
        palisade_wall_by_owner;
    PlayerLegacySprites palisade_wall_flags;
    std::array<std::array<std::array<LegacySprite, 4>, 4>, 4>
        town_center_layers_blue;
    std::array<std::array<std::array<LegacySprite, 4>, 4>, 4>
        town_center_layers_red;
    std::map<
        BuildingKind,
        std::array<std::array<PlayerLegacyComposite, 4>, 4>
    > building_composites;
    std::map<BuildingKind, PlayerLegacyAnimatedComposite>
        building_construction_composites;
    std::map<BuildingKind, PlayerLegacyAnimatedComposite>
        building_death_composites;
    std::map<std::int16_t, PlayerLegacyAnimatedComposite>
        building_damage_graphics;
    std::map<
        std::tuple<BuildingKind, std::size_t, std::size_t, std::size_t>,
        LegacySprite
    > direct_building_player_variants;
    PlayerLegacySprites villager_animation;
    PlayerLegacySprites sheep_animation;
    PlayerLegacySprites deer_animation;
    PlayerLegacySprites boar_animation;
    std::map<UnitKind, PlayerLegacySprites> military;
    std::map<UnitKind, PlayerLegacySprites> movement;
    std::map<UnitKind, PlayerLegacySprites> attack;
    std::map<UnitKind, PlayerLegacySprites> death;
    PlayerLegacySprites packed_trebuchet_transform;
    PlayerLegacySprites unpacked_trebuchet_transform;
    std::map<UnitKind, std::array<PlayerLegacyAnimatedComposite, 4>>
        naval_idle;
    std::map<UnitKind, std::array<PlayerLegacyAnimatedComposite, 4>>
        naval_move;
    std::map<UnitKind, std::array<PlayerLegacyAnimatedComposite, 4>>
        naval_attack;
    PlayerLegacySprites villager_gather;
    PlayerLegacySprites villager_build;
    PlayerLegacySprites monk_convert;
    PlayerLegacySprites missionary_heal;

    void destroy() {
        SDL_DestroyTexture(frontend_background);
        frontend_background = nullptr;
        SDL_DestroyTexture(scenario_background);
        scenario_background = nullptr;
        campaign_background.destroy();
        sheep_blue.destroy();
        sheep_red.destroy();
        villager_blue.destroy();
        villager_red.destroy();
        for (LegacySprite& sprite : town_center_blue) {
            sprite.destroy();
        }
        for (LegacySprite& sprite : town_center_red) {
            sprite.destroy();
        }
        for (LegacySprite& sprite : archery_range_blue) {
            sprite.destroy();
        }
        for (LegacySprite& sprite : archery_range_red) {
            sprite.destroy();
        }
        tree.destroy();
        for (LegacySprite& sprite : berry_states) sprite.destroy();
        for (LegacySprite& sprite : gold_states) sprite.destroy();
        for (LegacySprite& sprite : stone_states) sprite.destroy();
        fish.destroy();
        for (auto& [texture, shadow] : building_static_shadows) {
            static_cast<void>(texture);
            shadow.sprite.destroy();
        }
        building_static_shadows.clear();
        fire_projectile.destroy();
        cannonball_projectile.destroy();
        gunshot_projectile.destroy();
        scorpion_projectile.destroy();
        onager_primary_projectile.destroy();
        onager_volley_projectile.destroy();
        trebuchet_projectile.destroy();
        axe_projectile.destroy();
        arrow_projectile.destroy();
        siege_impact.destroy();
        trebuchet_impact.destroy();
        bombard_tower_standing_blue.destroy();
        bombard_tower_standing_red.destroy();
        bombard_tower_construction_blue.destroy();
        bombard_tower_construction_red.destroy();
        bombard_tower_dying_blue.destroy();
        bombard_tower_dying_red.destroy();
        relic.destroy();
        carried_relic_standing.destroy();
        carried_relic_moving.destroy();
        hud_background.destroy();
        for (LegacyHudBackground& background :
             civilization_hud_backgrounds) {
            background.destroy();
        }
        hud_actions.destroy();
        for (auto& [frame, icon] : action_command_icons) {
            static_cast<void>(frame);
            icon.destroy();
        }
        for (auto& [frame, icon] : unit_command_icons) {
            static_cast<void>(frame);
            icon.destroy();
        }
        unit_command_icons.clear();
        for (auto& [frame, icon] : technology_command_icons) {
            static_cast<void>(frame);
            icon.destroy();
        }
        technology_command_icons.clear();
        for (auto& [frame, icon] : building_command_icons) {
            static_cast<void>(frame);
            icon.destroy();
        }
        building_command_icons.clear();
        for (LegacySprite& icon : resource_icons) {
            icon.destroy();
        }
        portrait_frame.destroy();
        market_western_blue.destroy();
        market_western_red.destroy();
        market_eastern_blue.destroy();
        market_eastern_red.destroy();
        market_mediterranean_blue.destroy();
        market_mediterranean_red.destroy();
        market_far_eastern_blue.destroy();
        market_far_eastern_red.destroy();
        trade_cart_standing.destroy();
        trade_cart_moving.destroy();
        fishing_ship_standing.destroy();
        fishing_ship_moving.destroy();
        fish_trap_standing.destroy();
        fish_trap_construction.destroy();
        outpost_construction.destroy();
        outpost_death.destroy();
        wonder_construction.destroy();
        wonder_death.destroy();
        for (auto& [civilization, composite] : wonder_standing) {
            static_cast<void>(civilization);
            composite.destroy();
        }
        wonder_standing.clear();
        longboat_attack.destroy();
        longboat_idle.destroy();
        longboat_move.destroy();
        for (auto& age : house_blue) {
            for (LegacySprite& sprite : age) sprite.destroy();
        }
        for (auto& age : house_red) {
            for (LegacySprite& sprite : age) sprite.destroy();
        }
        for (auto& age : town_center_age_blue) {
            for (LegacySprite& sprite : age) sprite.destroy();
        }
        for (auto& age : town_center_age_red) {
            for (LegacySprite& sprite : age) sprite.destroy();
        }
        for (auto& age : blacksmith_blue) {
            for (LegacySprite& sprite : age) sprite.destroy();
        }
        for (auto& age : blacksmith_red) {
            for (LegacySprite& sprite : age) sprite.destroy();
        }
        for (LegacySprite& sprite : lumber_camp_blue) sprite.destroy();
        for (LegacySprite& sprite : lumber_camp_red) sprite.destroy();
        for (LegacySprite& sprite : mining_camp_blue) sprite.destroy();
        for (LegacySprite& sprite : mining_camp_red) sprite.destroy();
        for (auto& age : university_blue) {
            for (LegacySprite& sprite : age) sprite.destroy();
        }
        for (auto& age : university_red) {
            for (LegacySprite& sprite : age) sprite.destroy();
        }
        for (auto& age : market_age_blue) {
            for (LegacySprite& sprite : age) sprite.destroy();
        }
        for (auto& age : market_age_red) {
            for (LegacySprite& sprite : age) sprite.destroy();
        }
        for (auto& owners : stone_wall_by_owner) {
            for (auto& family : owners) {
                for (LegacySprite& sprite : family) sprite.destroy();
            }
        }
        for (PlayerLegacyComposite& composite :
             stone_wall_construction) {
            composite.destroy();
        }
        for (auto& [kind, families] : gate_construction) {
            static_cast<void>(kind);
            for (PlayerLegacyComposite& composite : families) {
                composite.destroy();
            }
        }
        gate_construction.clear();
        for (LegacySprite& sprite : palisade_wall_shadow) {
            sprite.destroy();
        }
        for (auto& owner : palisade_wall_by_owner) {
            for (LegacySprite& sprite : owner) sprite.destroy();
        }
        palisade_wall_flags.destroy();
        for (auto& age : town_center_layers_blue) {
            for (auto& family : age) {
                for (LegacySprite& sprite : family) sprite.destroy();
            }
        }
        for (auto& age : town_center_layers_red) {
            for (auto& family : age) {
                for (LegacySprite& sprite : family) sprite.destroy();
            }
        }
        for (auto& [kind, ages] : building_composites) {
            static_cast<void>(kind);
            for (auto& families : ages) {
                for (PlayerLegacyComposite& composite : families) {
                    composite.destroy();
                }
            }
        }
        building_composites.clear();
        for (auto& [kind, composite] :
             building_construction_composites) {
            static_cast<void>(kind);
            composite.destroy();
        }
        building_construction_composites.clear();
        for (auto& [kind, composite] : building_death_composites) {
            static_cast<void>(kind);
            composite.destroy();
        }
        building_death_composites.clear();
        for (auto& [root, composite] : building_damage_graphics) {
            static_cast<void>(root);
            composite.destroy();
        }
        building_damage_graphics.clear();
        for (auto& [key, sprite] : direct_building_player_variants) {
            static_cast<void>(key);
            sprite.destroy();
        }
        direct_building_player_variants.clear();
        villager_animation.destroy();
        sheep_animation.destroy();
        deer_animation.destroy();
        boar_animation.destroy();
        for (auto& [kind, sprites] : military) {
            static_cast<void>(kind);
            sprites.destroy();
        }
        military.clear();
        for (auto& [kind, sprites] : movement) {
            static_cast<void>(kind);
            sprites.destroy();
        }
        movement.clear();
        for (auto& [kind, sprites] : attack) {
            static_cast<void>(kind);
            sprites.destroy();
        }
        attack.clear();
        for (auto& [kind, sprites] : death) {
            static_cast<void>(kind);
            sprites.destroy();
        }
        death.clear();
        packed_trebuchet_transform.destroy();
        unpacked_trebuchet_transform.destroy();
        for (auto* table : {&naval_idle, &naval_move, &naval_attack}) {
            for (auto& [kind, families] : *table) {
                static_cast<void>(kind);
                for (PlayerLegacyAnimatedComposite& composite : families) {
                    composite.destroy();
                }
            }
            table->clear();
        }
        villager_gather.destroy();
        villager_build.destroy();
        monk_convert.destroy();
        missionary_heal.destroy();
    }
};

LegacySprites active_legacy_sprites;
struct RuntimeAssetLoadFailure {
    AssetCoverageStatus status{AssetCoverageStatus::decode_failure};
    std::string reason;
};
std::map<std::tuple<std::int16_t, unsigned>, RuntimeAssetLoadFailure>
    active_composite_load_failures;
std::map<std::tuple<std::int32_t, unsigned>, RuntimeAssetLoadFailure>
    active_animation_load_failures;
std::map<EntityId, std::pair<std::int16_t, std::uint64_t>>
    active_building_damage_animation;

int render_direction(
    TilePosition previous,
    TilePosition current
) {
    const int x = std::clamp(current.x - previous.x, -1, 1);
    const int y = std::clamp(current.y - previous.y, -1, 1);
    if (x == 0 && y < 0) return 0;
    if (x > 0 && y < 0) return 1;
    if (x > 0 && y == 0) return 2;
    if (x > 0 && y > 0) return 3;
    if (x == 0 && y > 0) return 4;
    if (x < 0 && y > 0) return 5;
    if (x < 0 && y == 0) return 6;
    if (x < 0 && y < 0) return 7;
    return 0;
}

void record_unit_procedural_fallback(
    const Simulation& simulation,
    const Unit& unit,
    bool legacy_selection_existed
) {
    RuntimeFallbackEvent event;
    event.entity_id = unit.id;
    event.state.category = RenderObjectCategory::unit;
    event.state.object_kind = render_unit_kind_name(unit.kind);
    event.state.action = render_action_for(simulation, unit);
    event.state.action_detail =
        render_action_detail_for(simulation, unit);
    event.state.owner = unit.owner.stable_id();
    event.state.civilization = simulation.civilization(unit.owner);
    event.state.age = simulation.age(unit.owner);
    event.state.architecture_family = render_architecture_family(
        event.state.civilization
    );
    event.state.direction = render_direction(
        unit.previous_position, unit.position
    );
    event.state.moving = unit.moving ||
        render_unit_is_interpolating(simulation, unit);
    const AssetResolution resolution = resolve_unit_asset(
        event.state, unit.kind
    );
    event.request = resolution.request;
    const auto load_failure = resolution.request.slp_id
        ? active_animation_load_failures.find({
              *resolution.request.slp_id,
              static_cast<unsigned>(unit.owner.stable_id() + 1),
          })
        : active_animation_load_failures.end();
    if (load_failure != active_animation_load_failures.end()) {
        event.status = load_failure->second.status;
        event.reason = load_failure->second.reason;
    } else {
        event.status = legacy_selection_existed ||
                resolution.status == AssetCoverageStatus::renderable
            ? AssetCoverageStatus::renderer_failure
            : resolution.status;
        event.reason = legacy_selection_existed
            ? "selected legacy animation did not render: " +
                resolution.reason
            : resolution.reason;
    }
    event.simulation_tick = simulation.tick_number();
    event.renderer_call_site = "render_unit:procedural_body";
    static_cast<void>(
        runtime_fallback_telemetry().record(std::move(event))
    );
}

void record_building_procedural_fallback(
    const Simulation& simulation,
    const Building& building,
    int maximum_hit_points,
    std::string_view renderer_call_site =
        "render_building:procedural_body"
) {
    RuntimeFallbackEvent event;
    event.entity_id = building.id;
    event.state.category = RenderObjectCategory::building;
    event.state.object_kind = render_building_kind_name(building.kind);
    event.state.action = building.completed()
        ? RenderAction::idle
        : RenderAction::working;
    event.state.building_state = render_state_for(
        building, maximum_hit_points
    );
    event.state.owner = building.owner.stable_id();
    event.state.civilization = simulation.civilization(building.owner);
    event.state.age = simulation.age(building.owner);
    event.state.architecture_family =
        render_building_architecture_family(
            building.kind, event.state.civilization
        );
    event.state.animation_frame =
        render_building_topology_frame(simulation, building);
    event.state.upgrade_variant =
        render_building_upgrade_variant(simulation, building);
    event.state.damage_stage =
        event.state.building_state == RenderBuildingState::damaged
        ? render_damage_stage(
              building.hit_points, maximum_hit_points
          )
        : 0;
    event.state.construction_stage = render_construction_stage(
        building, rules_for(building.kind).construction_ticks
    );
    const AssetResolution resolution = resolve_building_asset(
        event.state, building.kind
    );
    event.request = resolution.request;
    const unsigned player =
        static_cast<unsigned>(building.owner.stable_id() + 1);
    const auto direct_failure = resolution.request.slp_id
        ? active_animation_load_failures.find({
              *resolution.request.slp_id, player
          })
        : active_animation_load_failures.end();
    const auto shadow_failure = resolution.request.shadow_slp_id
        ? active_animation_load_failures.find({
              *resolution.request.shadow_slp_id, 1U
          })
        : active_animation_load_failures.end();
    auto overlay_failure = active_animation_load_failures.end();
    for (const std::int32_t slp :
         resolution.request.composite_slp_ids) {
        overlay_failure = active_animation_load_failures.find({
            slp, player
        });
        if (overlay_failure !=
            active_animation_load_failures.end()) {
            break;
        }
    }
    const auto load_failure = resolution.request.graphic_id
        ? active_composite_load_failures.find({
              static_cast<std::int16_t>(
                  *resolution.request.graphic_id
              ),
              player,
          })
        : active_composite_load_failures.end();
    auto overlay_composite_failure =
        active_composite_load_failures.end();
    for (const std::int16_t root :
         resolution.request.overlay_graphic_ids) {
        overlay_composite_failure =
            active_composite_load_failures.find({root, player});
        if (overlay_composite_failure !=
            active_composite_load_failures.end()) {
            break;
        }
    }
    if (direct_failure != active_animation_load_failures.end()) {
        event.status = direct_failure->second.status;
        event.reason = direct_failure->second.reason;
    } else if (shadow_failure !=
               active_animation_load_failures.end()) {
        event.status = AssetCoverageStatus::missing_shadow;
        event.reason =
            "linked building shadow failed: " +
            shadow_failure->second.reason;
    } else if (overlay_failure !=
               active_animation_load_failures.end()) {
        event.status = AssetCoverageStatus::missing_composite_part;
        event.reason =
            "building overlay failed: " +
            overlay_failure->second.reason;
    } else if (overlay_composite_failure !=
               active_composite_load_failures.end()) {
        event.status = overlay_composite_failure->second.status;
        event.reason =
            "building damage overlay failed: " +
            overlay_composite_failure->second.reason;
    } else if (load_failure != active_composite_load_failures.end()) {
        event.status = load_failure->second.status;
        event.reason = load_failure->second.reason;
    } else {
        event.status = resolution.status ==
                AssetCoverageStatus::renderable
            ? AssetCoverageStatus::renderer_failure
            : resolution.status;
        event.reason =
            resolution.status == AssetCoverageStatus::renderable
            ? "selected building asset did not render: " +
                resolution.reason
            : resolution.reason;
    }
    event.simulation_tick = simulation.tick_number();
    event.renderer_call_site = renderer_call_site;
    static_cast<void>(
        runtime_fallback_telemetry().record(std::move(event))
    );
}

void record_projectile_procedural_fallback(
    const Simulation& simulation,
    const Projectile& projectile,
    std::optional<ProjectileAssetKind> kind
) {
    RuntimeFallbackEvent event;
    event.entity_id = projectile.source_entity_id != 0
        ? projectile.source_entity_id
        : projectile.target;
    event.state.category = RenderObjectCategory::projectile;
    event.state.owner = projectile.owner.stable_id();
    event.state.civilization =
        simulation.civilization(projectile.owner);
    event.state.age = simulation.age(projectile.owner);
    event.state.architecture_family = render_architecture_family(
        event.state.civilization
    );
    event.state.direction = render_direction(
        projectile.origin, projectile.destination
    );
    event.state.animation_frame = std::max(
        projectile.total_ticks - projectile.ticks_remaining,
        0
    );
    event.simulation_tick = simulation.tick_number();
    event.renderer_call_site = "render_projectile:procedural_body";
    if (!kind) {
        event.state.object_kind = "generic_splash_stone";
        event.status = AssetCoverageStatus::intentional_procedural;
        event.reason =
            "generic splash projectile has explicit procedural renderer";
        static_cast<void>(
            runtime_fallback_telemetry().record(std::move(event))
        );
        return;
    }
    event.state.object_kind = std::string{
        projectile_asset_kind_name(*kind)
    };
    const auto bindings = canonical_projectile_asset_bindings();
    const auto binding = std::ranges::find(
        bindings, *kind, &ProjectileAssetBinding::kind
    );
    if (binding != bindings.end()) {
        const std::size_t stored_angles =
            static_cast<std::size_t>(binding->angle_count / 2 + 1);
        const auto selection = select_projectile_frame(
            projectile.origin,
            projectile.destination,
            binding->frame_count,
            binding->angle_count,
            stored_angles *
                static_cast<std::size_t>(binding->frame_count),
            static_cast<std::uint64_t>(
                projectile.total_ticks - projectile.ticks_remaining
            )
        );
        if (selection) {
            const int stored_direction = static_cast<int>(
                selection->frame_index /
                static_cast<std::size_t>(binding->frame_count)
            );
            event.state.direction = selection->flip_horizontal
                ? binding->angle_count - stored_direction
                : stored_direction;
            event.state.animation_frame = static_cast<int>(
                selection->frame_index %
                static_cast<std::size_t>(binding->frame_count)
            );
        }
    }
    const AssetResolution resolution = resolve_projectile_asset(
        event.state, *kind
    );
    event.request = resolution.request;
    const auto load_failure = resolution.request.slp_id
        ? active_animation_load_failures.find({
              *resolution.request.slp_id, 1U
          })
        : active_animation_load_failures.end();
    if (load_failure != active_animation_load_failures.end()) {
        event.status = load_failure->second.status;
        event.reason = load_failure->second.reason;
    } else if (resolution.status ==
               AssetCoverageStatus::intentional_procedural) {
        event.status = resolution.status;
        event.reason = resolution.reason;
    } else {
        event.status = AssetCoverageStatus::renderer_failure;
        event.reason =
            "selected projectile asset did not render: " +
            resolution.reason;
    }
    static_cast<void>(
        runtime_fallback_telemetry().record(std::move(event))
    );
}

void record_impact_procedural_fallback(
    const Simulation& simulation,
    const ImpactEffect& impact,
    std::optional<ProjectileAssetKind> kind
) {
    RuntimeFallbackEvent event;
    event.entity_id = impact.source_entity_id;
    event.state.category = RenderObjectCategory::impact;
    event.state.object_kind = kind
        ? std::string{projectile_asset_kind_name(*kind)}
        : "generic_impact";
    event.state.animation_frame = std::max(
        impact.total_ticks - impact.ticks_remaining,
        0
    );
    event.simulation_tick = simulation.tick_number();
    event.renderer_call_site = "render_impact:procedural_body";
    if (!kind) {
        event.status = AssetCoverageStatus::intentional_procedural;
        event.reason =
            "generic impact has explicit procedural renderer";
    } else {
        const AssetResolution resolution = resolve_projectile_asset(
            event.state, *kind
        );
        event.request = resolution.request;
        const auto load_failure = resolution.request.slp_id
            ? active_animation_load_failures.find({
                  *resolution.request.slp_id, 1U
              })
            : active_animation_load_failures.end();
        if (load_failure != active_animation_load_failures.end()) {
            event.status = load_failure->second.status;
            event.reason = load_failure->second.reason;
        } else {
            event.status = AssetCoverageStatus::renderer_failure;
            event.reason =
                "selected impact asset did not render: " +
                resolution.reason;
        }
    }
    static_cast<void>(
        runtime_fallback_telemetry().record(std::move(event))
    );
}

void record_resource_procedural_fallback(
    const Simulation& simulation,
    ResourceRenderKind kind,
    int frame
) {
    RuntimeFallbackEvent event;
    event.state.category = RenderObjectCategory::resource;
    event.state.object_kind = std::string{
        resource_render_kind_name(kind)
    };
    event.state.animation_frame = frame;
    const AssetResolution resolution = resolve_resource_asset(
        event.state, kind
    );
    event.request = resolution.request;
    const auto load_failure = resolution.request.slp_id
        ? active_animation_load_failures.find({
              *resolution.request.slp_id, 1U
          })
        : active_animation_load_failures.end();
    if (load_failure != active_animation_load_failures.end()) {
        event.status = load_failure->second.status;
        event.reason = load_failure->second.reason;
    } else {
        event.status = resolution.status ==
                AssetCoverageStatus::renderable
            ? AssetCoverageStatus::renderer_failure
            : resolution.status;
        event.reason = resolution.status ==
                AssetCoverageStatus::renderable
            ? "selected resource asset did not render: " +
                resolution.reason
            : resolution.reason;
    }
    event.simulation_tick = simulation.tick_number();
    event.renderer_call_site = "render_resource:procedural_body";
    static_cast<void>(
        runtime_fallback_telemetry().record(std::move(event))
    );
}

void record_unit_death_procedural_fallback(
    const Simulation& simulation,
    const UnitDeathEffect& effect,
    bool legacy_selection_existed
) {
    RuntimeFallbackEvent event;
    event.entity_id = effect.entity_id;
    event.state.category = RenderObjectCategory::unit_death;
    event.state.object_kind = render_unit_kind_name(effect.kind);
    event.state.action = RenderAction::dying;
    event.state.owner = effect.owner.stable_id();
    event.state.civilization = simulation.civilization(effect.owner);
    event.state.age = simulation.age(effect.owner);
    event.state.architecture_family = render_architecture_family(
        event.state.civilization
    );
    event.state.direction = render_direction(
        effect.previous_position, effect.position
    );
    event.state.animation_frame = std::max(
        effect.total_ticks - effect.ticks_remaining, 0
    ) * 2;
    const AssetResolution resolution = resolve_unit_asset(
        event.state, effect.kind
    );
    event.request = resolution.request;
    const auto load_failure = resolution.request.slp_id
        ? active_animation_load_failures.find({
              *resolution.request.slp_id,
              static_cast<unsigned>(effect.owner.stable_id() + 1),
          })
        : active_animation_load_failures.end();
    if (load_failure != active_animation_load_failures.end()) {
        event.status = load_failure->second.status;
        event.reason = load_failure->second.reason;
    } else {
        event.status = legacy_selection_existed ||
                resolution.status == AssetCoverageStatus::renderable
            ? AssetCoverageStatus::renderer_failure
            : resolution.status;
        event.reason = legacy_selection_existed
            ? "selected unit death animation did not render: " +
                resolution.reason
            : resolution.reason;
    }
    event.simulation_tick = simulation.tick_number();
    event.renderer_call_site = "render_unit_death:procedural_body";
    static_cast<void>(
        runtime_fallback_telemetry().record(std::move(event))
    );
}

void record_building_rubble_procedural_fallback(
    const Simulation& simulation,
    const BuildingRubbleEffect& effect,
    bool legacy_selection_existed
) {
    RuntimeFallbackEvent event;
    event.entity_id = effect.entity_id;
    event.state.category = RenderObjectCategory::building_rubble;
    event.state.object_kind = render_building_kind_name(effect.kind);
    event.state.action = RenderAction::dying;
    event.state.building_state = RenderBuildingState::dying;
    event.state.owner = effect.owner.stable_id();
    event.state.civilization = simulation.civilization(effect.owner);
    event.state.age = simulation.age(effect.owner);
    event.state.architecture_family =
        render_building_architecture_family(
            effect.kind, event.state.civilization
        );
    event.state.animation_frame = std::max(
        effect.total_ticks - effect.ticks_remaining, 0
    );
    const AssetResolution resolution = resolve_building_asset(
        event.state, effect.kind
    );
    event.request = resolution.request;
    const unsigned player =
        static_cast<unsigned>(effect.owner.stable_id() + 1);
    const auto direct_failure = resolution.request.slp_id
        ? active_animation_load_failures.find({
              *resolution.request.slp_id, player
          })
        : active_animation_load_failures.end();
    const auto composite_failure = resolution.request.graphic_id
        ? active_composite_load_failures.find({
              static_cast<std::int16_t>(
                  *resolution.request.graphic_id
              ),
              player,
          })
        : active_composite_load_failures.end();
    if (direct_failure != active_animation_load_failures.end()) {
        event.status = direct_failure->second.status;
        event.reason = direct_failure->second.reason;
    } else if (composite_failure !=
               active_composite_load_failures.end()) {
        event.status = composite_failure->second.status;
        event.reason = composite_failure->second.reason;
    } else {
        event.status = legacy_selection_existed ||
                resolution.status == AssetCoverageStatus::renderable
            ? AssetCoverageStatus::renderer_failure
            : resolution.status;
        event.reason = legacy_selection_existed
            ? "selected building death asset did not render: " +
                resolution.reason
            : resolution.reason;
    }
    event.simulation_tick = simulation.tick_number();
    event.renderer_call_site = "render_building_rubble:procedural_body";
    static_cast<void>(
        runtime_fallback_telemetry().record(std::move(event))
    );
}

struct SelectionDrag {
    SDL_FPoint start;
    SDL_FPoint current;
};

using ControlGroup = SelectionControlGroup;

bool siege_sound(UnitKind kind) {
    switch (kind) {
        case UnitKind::battering_ram:
        case UnitKind::capped_ram:
        case UnitKind::siege_ram:
        case UnitKind::mangonel:
        case UnitKind::scorpion:
        case UnitKind::heavy_scorpion:
        case UnitKind::onager:
        case UnitKind::siege_onager:
        case UnitKind::packed_trebuchet:
        case UnitKind::trebuchet:
            return true;
        default:
            return false;
    }
}

bool siege_engineers_unit(UnitKind kind) {
    return kind == UnitKind::battering_ram ||
        kind == UnitKind::capped_ram ||
        kind == UnitKind::siege_ram ||
        kind == UnitKind::mangonel ||
        kind == UnitKind::onager ||
        kind == UnitKind::siege_onager ||
        kind == UnitKind::scorpion ||
        kind == UnitKind::heavy_scorpion ||
        kind == UnitKind::packed_trebuchet ||
        kind == UnitKind::trebuchet ||
        kind == UnitKind::bombard_cannon;
}

bool arrow_sound(UnitKind kind) {
    switch (kind) {
        case UnitKind::archer:
        case UnitKind::crossbowman:
        case UnitKind::arbalester:
        case UnitKind::skirmisher:
        case UnitKind::elite_skirmisher:
        case UnitKind::longbowman:
        case UnitKind::elite_longbowman:
        case UnitKind::plumed_archer:
        case UnitKind::elite_plumed_archer:
        case UnitKind::chu_ko_nu:
        case UnitKind::elite_chu_ko_nu:
        case UnitKind::mangudai:
        case UnitKind::elite_mangudai:
            return true;
        default:
            return false;
    }
}

int unit_attack_sound(UnitKind kind) {
    switch (kind) {
        case UnitKind::hand_cannoneer:
        case UnitKind::janissary:
        case UnitKind::elite_janissary:
        case UnitKind::conquistador:
        case UnitKind::elite_conquistador:
            return 385;
        case UnitKind::bombard_cannon:
            return 411;
        case UnitKind::mameluke:
        case UnitKind::elite_mameluke:
            return 486;
        case UnitKind::tarkan:
        case UnitKind::elite_tarkan:
            return 497;
        case UnitKind::war_elephant:
        case UnitKind::elite_war_elephant:
            return 26;
        case UnitKind::longbowman:
        case UnitKind::elite_longbowman:
            return 312;
        default:
            return arrow_sound(kind) ? 314 : 329;
    }
}

int unit_death_sound(UnitKind kind) {
    if (kind == UnitKind::petard) {
        return 323;
    }
    if (kind == UnitKind::fishing_ship) {
        return 505;
    }
    if (is_ship(kind)) {
        return 379;
    }
    if (siege_sound(kind)) {
        return 293;
    }
    switch (kind) {
        case UnitKind::cataphract:
        case UnitKind::elite_cataphract:
        case UnitKind::war_elephant:
        case UnitKind::elite_war_elephant:
        case UnitKind::tarkan:
        case UnitKind::elite_tarkan:
            return -1;
        default:
            return 294;
    }
}

bool building_has_death_sound(BuildingKind kind) {
    return kind != BuildingKind::farm &&
        kind != BuildingKind::fish_trap;
}

TilePosition active_audio_listener_tile{};

std::pair<float, float> world_effect_mix(TilePosition source) {
    const float dx = static_cast<float>(
        source.x - active_audio_listener_tile.x
    );
    const float dy = static_cast<float>(
        source.y - active_audio_listener_tile.y
    );
    const float distance = std::hypot(dx, dy);
    return {
        std::clamp(1.0F - distance / 32.0F, 0.0F, 1.0F),
        std::clamp((dx - dy) / 16.0F, -1.0F, 1.0F)
    };
}

void play_world_effect(
    AudioSystem& audio,
    int sound_id,
    TilePosition source,
    AudioCategory category = AudioCategory::combat,
    std::optional<Civilization> source_civilization = std::nullopt
) {
    if (sound_id < 0) return;
    const auto [gain, pan] = world_effect_mix(source);
    if (gain <= 0.0F) return;
    audio.play_effect(
        sound_id, category, gain, pan, source_civilization
    );
}

int death_animation_slp(UnitKind kind) {
    if (const auto animation = unit_animation_set(kind);
        animation && animation->death_slp >= 0) {
        return animation->death_slp;
    }
    if (const UnitDeathAnimationSet* animation =
            unit_death_animation_set(kind)) {
        return animation->slp;
    }
    return -1;
}

std::pair<int, int> attack_animation(UnitKind kind) {
    if (const auto animation = unit_animation_set(kind);
        animation && animation->attack_slp >= 0) {
        return {animation->attack_slp, animation->attack_frames};
    }
    return {-1, 0};
}

class FrontendAudioEvents {
public:
    void prime(const Simulation& simulation) {
        cooldowns_.clear();
        attack_animation_frames_.clear();
        moving_.clear();
        known_units_.clear();
        conversion_targets_.clear();
        healing_targets_.clear();
        building_cooldowns_.clear();
        known_buildings_.clear();
        for (const Unit& unit : simulation.units()) {
            cooldowns_[unit.id] = unit.attack_cooldown;
            moving_[unit.id] = unit.moving;
            known_units_.insert(unit.id);
            conversion_targets_[unit.id] = unit.conversion_target_id;
            healing_targets_[unit.id] = unit.healing_target_id;
        }
        for (const Building& building : simulation.buildings()) {
            building_cooldowns_[building.id] = building.attack_cooldown;
            known_buildings_.insert(building.id);
        }
        selection_ = simulation.selected_units();
        selected_building_ = simulation.selected_building();
        known_scenario_audio_.clear();
        for (const ScenarioMessage& message :
             simulation.scenario_messages()) {
            if (!message.audio_file.empty()) {
                known_scenario_audio_.insert({
                    message.text,
                    message.audio_file,
                    message.expires_tick
                });
            }
        }
        world_tick_ = simulation.tick_number();
    }

    void update(
        const Simulation& simulation,
        AudioSystem* audio
    ) {
        if (audio == nullptr) {
            return;
        }
        audio->set_listener_civilization(
            simulation.civilization(active_view_player)
        );
        audio->update();

        const std::vector<EntityId>& selected =
            simulation.selected_units();
        if (selected != selection_ && !selected.empty()) {
            const auto unit = std::ranges::find_if(
                simulation.units(),
                [id = selected.front()](const Unit& candidate) {
                    return candidate.id == id;
                }
            );
            if (unit != simulation.units().end()) {
                audio->play_effect(
                    selected_sound(unit->kind),
                    AudioCategory::interface
                );
            }
        }
        selection_ = selected;
        if (simulation.selected_building() != selected_building_ &&
            simulation.selected_building()) {
            const auto building = std::ranges::find_if(
                simulation.buildings(),
                [id = *simulation.selected_building()](
                    const Building& candidate
                ) {
                    return candidate.id == id;
                }
            );
            if (building != simulation.buildings().end()) {
                audio->play_effect(
                    selected_sound(building->kind),
                    AudioCategory::interface
                );
            }
        }
        selected_building_ = simulation.selected_building();
        if (simulation.tick_number() == world_tick_) {
            return;
        }
        world_tick_ = simulation.tick_number();

        std::set<std::tuple<std::string, std::string, std::uint64_t>>
            present_scenario_audio;
        for (const ScenarioMessage& message :
             simulation.scenario_messages()) {
            if (message.audio_file.empty()) continue;
            const auto key = std::tuple{
                message.text,
                message.audio_file,
                message.expires_tick
            };
            present_scenario_audio.insert(key);
            if (!known_scenario_audio_.contains(key) &&
                message.player == active_view_player) {
                audio->play_narration(message.audio_file);
            }
        }
        known_scenario_audio_ = std::move(present_scenario_audio);

        std::set<EntityId> present;
        for (const Unit& unit : simulation.units()) {
            present.insert(unit.id);
            if (!known_units_.contains(unit.id) &&
                belongs_to_local_view(
                    unit.owner, active_view_player
                )) {
                play_world_effect(
                    *audio, trained_sound(unit.kind), unit.position,
                    AudioCategory::combat,
                    simulation.civilization(unit.owner)
                );
            }
            const int previous = cooldowns_.contains(unit.id)
                ? cooldowns_.at(unit.id)
                : unit.attack_cooldown;
            if (unit.attack_cooldown > previous &&
                simulation.is_visible_to_controller(active_view_player, unit.position)) {
                attack_animation_frames_[unit.id] = 0;
            }
            if (const auto pending =
                    attack_animation_frames_.find(unit.id);
                pending != attack_animation_frames_.end()) {
                const auto [slp, frame_count] =
                    attack_animation(unit.kind);
                const auto [gain, pan] =
                    world_effect_mix(unit.position);
                const bool graphic_sounds =
                    gain > 0.0F &&
                    audio->play_graphic_frame_sounds(
                        slp,
                        pending->second,
                        0,
                        gain,
                        pan,
                        simulation.civilization(unit.owner)
                    );
                if (!graphic_sounds) {
                    if (pending->second == 0) {
                        play_world_effect(
                            *audio,
                            unit_attack_sound(unit.kind),
                            unit.position,
                            AudioCategory::combat,
                            simulation.civilization(unit.owner)
                        );
                    }
                    attack_animation_frames_.erase(pending);
                } else if (++pending->second >= frame_count) {
                    attack_animation_frames_.erase(pending);
                }
            }
            const bool was_moving = moving_.contains(unit.id) &&
                moving_.at(unit.id);
            if (unit.moving && !was_moving &&
                simulation.is_visible_to_controller(
                    active_view_player, unit.position
                )) {
                play_world_effect(
                    *audio, movement_sound(unit.kind), unit.position,
                    AudioCategory::combat,
                    simulation.civilization(unit.owner)
                );
            }
            const EntityId previous_conversion =
                conversion_targets_.contains(unit.id)
                ? conversion_targets_.at(unit.id)
                : 0;
            const EntityId previous_healing =
                healing_targets_.contains(unit.id)
                ? healing_targets_.at(unit.id)
                : 0;
            if (unit.kind == UnitKind::missionary &&
                unit.conversion_target_id != 0 &&
                previous_conversion == 0) {
                play_world_effect(
                    *audio, 417, unit.position, AudioCategory::combat,
                    simulation.civilization(unit.owner)
                );
            }
            if (unit.kind == UnitKind::missionary &&
                unit.healing_target_id != 0 &&
                previous_healing == 0) {
                play_world_effect(
                    *audio, 418, unit.position, AudioCategory::combat,
                    simulation.civilization(unit.owner)
                );
            }
            conversion_targets_[unit.id] = unit.conversion_target_id;
            healing_targets_[unit.id] = unit.healing_target_id;
            moving_[unit.id] = unit.moving;
            cooldowns_[unit.id] = unit.attack_cooldown;
        }
        known_units_ = std::move(present);
        std::erase_if(
            cooldowns_,
            [&simulation](const auto& entry) {
                return std::ranges::none_of(
                    simulation.units(),
                    [id = entry.first](const Unit& unit) {
                        return unit.id == id;
                    }
                );
            }
        );
        std::erase_if(
            moving_,
            [&present = known_units_](const auto& entry) {
                return !present.contains(entry.first);
            }
        );
        std::erase_if(
            attack_animation_frames_,
            [&present = known_units_](const auto& entry) {
                return !present.contains(entry.first);
            }
        );

        std::set<EntityId> present_buildings;
        for (const Building& building : simulation.buildings()) {
            present_buildings.insert(building.id);
            if (!known_buildings_.contains(building.id) &&
                belongs_to_local_view(
                    building.owner, active_view_player
                ) &&
                (building.kind == BuildingKind::bombard_tower ||
                 building.kind == BuildingKind::outpost ||
                 building.kind == BuildingKind::wonder)) {
                play_world_effect(
                    *audio,
                    building.kind == BuildingKind::wonder ? 383 : 23,
                    building.position
                );
            }
            const int previous =
                building_cooldowns_.contains(building.id)
                ? building_cooldowns_.at(building.id)
                : building.attack_cooldown;
            if (building.kind == BuildingKind::bombard_tower &&
                building.attack_cooldown > previous &&
                simulation.is_building_visible(active_view_player, building)) {
                play_world_effect(*audio, 411, building.position);
            }
            building_cooldowns_[building.id] = building.attack_cooldown;
        }
        known_buildings_ = std::move(present_buildings);

        for (const UnitDeathEffect& effect :
             simulation.death_effects()) {
            if (!simulation.is_visible_to_controller(
                    active_view_player, effect.position
                )) {
                continue;
            }
            const int elapsed = std::max(
                effect.total_ticks - effect.ticks_remaining, 0
            );
            const auto [gain, pan] = world_effect_mix(effect.position);
            const bool graphic_sounds =
                gain > 0.0F &&
                audio->play_graphic_frame_sounds(
                    death_animation_slp(effect.kind),
                    elapsed,
                    0,
                    gain,
                    pan,
                    simulation.civilization(effect.owner)
                );
            if (elapsed == 0 && !graphic_sounds) {
                play_world_effect(
                    *audio, unit_death_sound(effect.kind), effect.position,
                    AudioCategory::combat,
                    simulation.civilization(effect.owner)
                );
            }
        }
        for (const ImpactEffect& effect : simulation.impact_effects()) {
            if (effect.ticks_remaining == effect.total_ticks &&
                effect.source_kind != UnitKind::petard &&
                simulation.is_visible_to_controller(active_view_player, effect.position)) {
                play_world_effect(*audio, 323, effect.position);
            }
        }
        for (const BuildingRubbleEffect& effect :
             simulation.rubble_effects()) {
            if (building_has_death_sound(effect.kind) &&
                effect.ticks_remaining == effect.total_ticks &&
                simulation.is_visible_to_controller(active_view_player, effect.position)) {
                play_world_effect(*audio, 323, effect.position);
            }
        }
    }

private:
    std::map<EntityId, int> cooldowns_;
    std::map<EntityId, int> attack_animation_frames_;
    std::map<EntityId, bool> moving_;
    std::map<EntityId, EntityId> conversion_targets_;
    std::map<EntityId, EntityId> healing_targets_;
    std::set<EntityId> known_units_;
    std::map<EntityId, int> building_cooldowns_;
    std::set<EntityId> known_buildings_;
    std::vector<EntityId> selection_;
    std::optional<EntityId> selected_building_;
    std::set<std::tuple<std::string, std::string, std::uint64_t>>
        known_scenario_audio_;
    std::uint64_t world_tick_{};
};

std::optional<std::size_t> control_group_index(SDL_Keycode key) {
    if (key >= SDLK_1 && key <= SDLK_9) {
        return static_cast<std::size_t>(key - SDLK_1 + 1);
    }
    return std::nullopt;
}

void set_color(SDL_Renderer* renderer, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}

SDL_Color terrain_color(Terrain terrain) {
    switch (terrain) {
        case Terrain::grass:
            return {92, 138, 74, 255};
        case Terrain::water:
            return {65, 110, 170, 255};
        case Terrain::beach:
            return {208, 177, 135, 255};
        case Terrain::shallows:
            return {26, 124, 124, 255};
        case Terrain::forest:
            return {92, 138, 74, 255};
        case Terrain::berry_bush:
            return {82, 126, 66, 255};
        case Terrain::gold_mine:
            return {148, 128, 68, 255};
        case Terrain::stone_mine:
            return {112, 116, 112, 255};
        case Terrain::fish:
            return {58, 103, 163, 255};
    }
    return {255, 0, 255, 255};
}

SDL_Color shade_color(SDL_Color color, int amount) {
    const auto shade = [amount](Uint8 channel) {
        return static_cast<Uint8>(std::clamp(
            static_cast<int>(channel) + amount,
            0,
            255
        ));
    };
    return {shade(color.r), shade(color.g), shade(color.b), color.a};
}

bool is_resource_terrain(Terrain terrain) {
    return terrain == Terrain::forest ||
           terrain == Terrain::berry_bush ||
           terrain == Terrain::gold_mine ||
           terrain == Terrain::stone_mine ||
           terrain == Terrain::fish;
}

bool is_water_surface(Terrain terrain) {
    return terrain == Terrain::water || terrain == Terrain::fish ||
           terrain == Terrain::shallows;
}

SDL_Color building_color(const Building& building) {
    if (!building.completed()) {
        return {126, 105, 75, 255};
    }
    const bool blue = building.owner == Player::blue;
    switch (building.kind) {
        case BuildingKind::town_center:
            return blue
                ? SDL_Color{35, 75, 150, 255}
                : SDL_Color{145, 45, 38, 255};
        case BuildingKind::barracks:
            return blue
                ? SDL_Color{55, 100, 175, 255}
                : SDL_Color{175, 65, 55, 255};
        case BuildingKind::archery_range:
            return blue
                ? SDL_Color{70, 115, 155, 255}
                : SDL_Color{155, 75, 65, 255};
        case BuildingKind::house:
            return blue
                ? SDL_Color{125, 105, 75, 255}
                : SDL_Color{135, 90, 70, 255};
        case BuildingKind::mill:
            return blue
                ? SDL_Color{125, 100, 55, 255}
                : SDL_Color{145, 95, 55, 255};
        case BuildingKind::lumber_camp:
            return blue
                ? SDL_Color{75, 115, 70, 255}
                : SDL_Color{115, 90, 60, 255};
        case BuildingKind::mining_camp:
            return blue
                ? SDL_Color{105, 110, 125, 255}
                : SDL_Color{125, 100, 105, 255};
        case BuildingKind::farm:
            return blue
                ? SDL_Color{180, 145, 55, 255}
                : SDL_Color{160, 105, 50, 255};
        case BuildingKind::stable:
            return blue
                ? SDL_Color{95, 85, 150, 255}
                : SDL_Color{150, 75, 65, 255};
        case BuildingKind::blacksmith:
            return blue
                ? SDL_Color{85, 92, 110, 255}
                : SDL_Color{125, 80, 75, 255};
        case BuildingKind::castle:
            return blue
                ? SDL_Color{82, 95, 125, 255}
                : SDL_Color{125, 82, 78, 255};
        case BuildingKind::university:
            return blue
                ? SDL_Color{105, 98, 145, 255}
                : SDL_Color{145, 88, 82, 255};
        case BuildingKind::siege_workshop:
            return blue
                ? SDL_Color{112, 94, 72, 255}
                : SDL_Color{142, 78, 68, 255};
        case BuildingKind::palisade_wall:
            return blue
                ? SDL_Color{115, 91, 55, 255}
                : SDL_Color{128, 75, 53, 255};
        case BuildingKind::watch_tower:
        case BuildingKind::bombard_tower:
            return blue
                ? SDL_Color{105, 102, 92, 255}
                : SDL_Color{125, 88, 78, 255};
        case BuildingKind::stone_wall:
            return blue
                ? SDL_Color{122, 123, 116, 255}
                : SDL_Color{137, 108, 101, 255};
        case BuildingKind::palisade_gate_x:
        case BuildingKind::palisade_gate_y:
            return blue
                ? SDL_Color{115, 91, 55, 255}
                : SDL_Color{128, 75, 53, 255};
        case BuildingKind::stone_gate_x:
        case BuildingKind::stone_gate_y:
            return blue
                ? SDL_Color{122, 123, 116, 255}
                : SDL_Color{137, 108, 101, 255};
        case BuildingKind::monastery:
            return blue
                ? SDL_Color{165, 158, 132, 255}
                : SDL_Color{174, 142, 126, 255};
        case BuildingKind::market:
            return blue
                ? SDL_Color{128, 102, 72, 255}
                : SDL_Color{145, 88, 68, 255};
        case BuildingKind::dock:
            return blue
                ? SDL_Color{90, 104, 112, 255}
                : SDL_Color{126, 88, 72, 255};
        case BuildingKind::fish_trap:
            return blue
                ? SDL_Color{116, 146, 124, 255}
                : SDL_Color{151, 112, 92, 255};
        case BuildingKind::outpost:
            return blue
                ? SDL_Color{128, 116, 92, 255}
                : SDL_Color{145, 94, 76, 255};
        case BuildingKind::wonder:
            return blue
                ? SDL_Color{178, 158, 112, 255}
                : SDL_Color{183, 118, 91, 255};
    }
    return {255, 0, 255, 255};
}

SDL_FPoint tile_top(TilePosition position) {
    const int elevation =
        active_render_map != nullptr &&
            active_render_map->contains(position)
        ? active_render_map->elevation_at(position)
        : 0;
    return {
        static_cast<float>(
            map_origin_x() + (position.x - position.y) * half_tile_width
        ) - active_camera.x,
        static_cast<float>(
            map_origin_y +
            (position.x + position.y) * half_tile_height
        ) - active_camera.y -
            static_cast<float>(elevation * elevation_pixel_step),
    };
}

SDL_FPoint subtile_top(
    TilePosition subtile,
    TilePosition elevation_position
) {
    constexpr float subtile_scale = 320.0F;
    const float x = static_cast<float>(subtile.x) / subtile_scale;
    const float y = static_cast<float>(subtile.y) / subtile_scale;
    const int elevation =
        active_render_map != nullptr &&
            active_render_map->contains(elevation_position)
        ? active_render_map->elevation_at(elevation_position)
        : 0;
    return {
        static_cast<float>(map_origin_x()) +
            (x - y) * static_cast<float>(half_tile_width) -
            active_camera.x,
        static_cast<float>(map_origin_y) +
            (x + y) * static_cast<float>(half_tile_height) -
            active_camera.y -
            static_cast<float>(elevation * elevation_pixel_step),
    };
}

SDL_FPoint building_top(const Building& building) {
    const BuildingRules& rules = rules_for(building.kind);
    const float center_x =
        static_cast<float>(building.position.x) +
        static_cast<float>(rules.footprint_width - 1) / 2.0F;
    const float center_y =
        static_cast<float>(building.position.y) +
        static_cast<float>(rules.footprint_height - 1) / 2.0F;
    return {
        static_cast<float>(map_origin_x()) +
            (center_x - center_y) * half_tile_width - active_camera.x,
        static_cast<float>(map_origin_y) +
            (center_x + center_y) * half_tile_height - active_camera.y,
    };
}

void clamp_camera(CameraView& camera) {
    camera.zoom = std::clamp(
        camera.zoom,
        minimum_camera_zoom,
        maximum_camera_zoom
    );
    const float view_width =
        static_cast<float>(view_pixel_width) / camera.zoom;
    const float view_height =
        static_cast<float>(view_pixel_height) / camera.zoom;
    // Pan limits come from the loaded map, not the viewport: the two only
    // coincide for a 24x16 map, which is what the old constants encoded.
    const float world_width = static_cast<float>(world_pixel_width());
    const float world_height =
        static_cast<float>(world_pixel_height() + map_origin_y);
    camera.x = std::clamp(
        camera.x,
        0.0F,
        std::max(0.0F, world_width - view_width)
    );
    camera.y = std::clamp(
        camera.y,
        0.0F,
        std::max(0.0F, world_height - view_height)
    );
}

void center_camera_on(CameraView& camera, TilePosition tile) {
    const float world_x = static_cast<float>(
        map_origin_x() + (tile.x - tile.y) * half_tile_width
    );
    const float world_y = static_cast<float>(
        map_origin_y + (tile.x + tile.y) * half_tile_height
    );
    camera.x =
        world_x - static_cast<float>(view_pixel_width) / camera.zoom * 0.5F;
    camera.y =
        world_y - static_cast<float>(view_pixel_height) / camera.zoom * 0.5F;
    clamp_camera(camera);
}

SDL_FPoint tile_screen_top(TilePosition position) {
    const SDL_FPoint world = tile_top(position);
    return {world.x * active_camera.zoom, world.y * active_camera.zoom};
}

[[nodiscard]] bool tile_near_world_view(
    TilePosition position,
    float margin = 256.0F
) {
    const SDL_FPoint top = tile_top(position);
    const float view_width =
        static_cast<float>(view_pixel_width) / active_camera.zoom;
    const float view_height =
        static_cast<float>(view_pixel_height) / active_camera.zoom;
    return top.x >= -margin &&
           top.x <= view_width + margin &&
           top.y >= -margin &&
           top.y <= view_height + margin;
}

void fill_diamond(
    SDL_Renderer* renderer,
    SDL_FPoint top,
    SDL_Color color,
    SDL_Texture* texture = nullptr,
    TilePosition position = {},
    bool full_texture_diamond = false
) {
    const SDL_FColor vertex_color{
        color.r / 255.0F,
        color.g / 255.0F,
        color.b / 255.0F,
        color.a / 255.0F,
    };
    const float sample_size = full_texture_diamond ? 1.0F : 0.25F;
    const unsigned hash =
        static_cast<unsigned>(position.x) * 17U +
        static_cast<unsigned>(position.y) * 31U;
    const float sample_left = full_texture_diamond
        ? 0.0F
        : static_cast<float>(hash % 4U) * sample_size;
    const float sample_top = full_texture_diamond
        ? 0.0F
        : static_cast<float>((hash / 4U) % 4U) * sample_size;
    const float sample_right = sample_left + sample_size;
    const float sample_bottom = sample_top + sample_size;
    const float sample_center_x = (sample_left + sample_right) * 0.5F;
    const float sample_center_y = (sample_top + sample_bottom) * 0.5F;
    const std::array<SDL_Vertex, 4> vertices{{
        {{top.x, top.y}, vertex_color, {sample_center_x, sample_top}},
        {{top.x + half_tile_width, top.y + half_tile_height},
         vertex_color, {sample_right, sample_center_y}},
        {{top.x, top.y + tile_height},
         vertex_color, {sample_center_x, sample_bottom}},
        {{top.x - half_tile_width, top.y + half_tile_height},
         vertex_color, {sample_left, sample_center_y}},
    }};
    constexpr std::array<int, 6> indices{{0, 1, 2, 0, 2, 3}};
    SDL_RenderGeometry(
        renderer,
        texture,
        vertices.data(),
        static_cast<int>(vertices.size()),
        indices.data(),
        static_cast<int>(indices.size())
    );
}

void render_elevation_faces(
    SDL_Renderer* renderer,
    const Simulation& simulation,
    TilePosition position,
    SDL_FPoint top
) {
    const int elevation = simulation.map().elevation_at(position);
    if (elevation == 0) return;
    const auto face = [&](TilePosition neighbor,
                          SDL_FPoint first,
                          SDL_FPoint second,
                          SDL_Color color) {
        const int neighbor_elevation =
            simulation.map().contains(neighbor)
            ? simulation.map().elevation_at(neighbor)
            : 0;
        const int difference = elevation - neighbor_elevation;
        if (difference <= 0) return;
        const float drop = static_cast<float>(
            difference * elevation_pixel_step
        );
        const SDL_FColor vertex_color{
            color.r / 255.0F,
            color.g / 255.0F,
            color.b / 255.0F,
            1.0F,
        };
        const std::array<SDL_Vertex, 4> vertices{{
            {first, vertex_color, {}},
            {second, vertex_color, {}},
            {{second.x, second.y + drop}, vertex_color, {}},
            {{first.x, first.y + drop}, vertex_color, {}},
        }};
        constexpr std::array<int, 6> indices{{0, 1, 2, 0, 2, 3}};
        SDL_RenderGeometry(
            renderer, nullptr,
            vertices.data(), static_cast<int>(vertices.size()),
            indices.data(), static_cast<int>(indices.size())
        );
        SDL_SetRenderDrawColor(
            renderer,
            static_cast<Uint8>(color.r * 0.62F),
            static_cast<Uint8>(color.g * 0.62F),
            static_cast<Uint8>(color.b * 0.62F),
            210
        );
        SDL_RenderLine(
            renderer,
            first.x,
            first.y + drop,
            second.x,
            second.y + drop
        );
    };
    face(
        {position.x + 1, position.y},
        {top.x + half_tile_width, top.y + half_tile_height},
        {top.x, top.y + tile_height},
        {83, 67, 43, 255}
    );
    face(
        {position.x, position.y + 1},
        {top.x, top.y + tile_height},
        {top.x - half_tile_width, top.y + half_tile_height},
        {67, 54, 37, 255}
    );
}

SDL_Texture* load_local_terrain_texture(
    SDL_Renderer* renderer,
    const std::filesystem::path& path
) {
    std::error_code path_error;
    if (!std::filesystem::is_regular_file(path, path_error)) {
        SDL_LogWarn(
            SDL_LOG_CATEGORY_APPLICATION,
            "optional original terrain texture missing: %s",
            path.string().c_str()
        );
        return nullptr;
    }
    SDL_Surface* surface = SDL_LoadPNG(path.string().c_str());
    if (surface == nullptr) {
        SDL_LogWarn(
            SDL_LOG_CATEGORY_APPLICATION,
            "cannot decode optional original terrain texture %s: %s",
            path.string().c_str(),
            SDL_GetError()
        );
        return nullptr;
    }
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);
    if (texture == nullptr) {
        SDL_LogWarn(
            SDL_LOG_CATEGORY_APPLICATION,
            "cannot upload optional original terrain texture %s: %s",
            path.string().c_str(),
            SDL_GetError()
        );
        return nullptr;
    }
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);
    return texture;
}

std::vector<SDL_Texture*> load_terrain_archive_frames(
    SDL_Renderer* renderer,
    const DrsArchive& terrain,
    const LegacyPalette& palette,
    std::int32_t resource_id,
    std::vector<RgbaFrame>* rgba_frames
) {
    const std::vector<std::byte> bytes =
        terrain.read("slp", resource_id);
    const std::size_t count = slp_frame_count(bytes);
    std::vector<SDL_Texture*> frames;
    frames.reserve(count);
    if (rgba_frames != nullptr) {
        rgba_frames->clear();
        rgba_frames->reserve(count);
    }
    try {
        for (std::size_t index = 0; index < count; ++index) {
            RgbaFrame decoded = decode_slp_frame(
                bytes, palette, index
            );
            SDL_Surface* surface = SDL_CreateSurfaceFrom(
                decoded.width,
                decoded.height,
                SDL_PIXELFORMAT_RGBA32,
                decoded.rgba.data(),
                decoded.width * 4
            );
            if (surface == nullptr) {
                throw LegacyAssetError{SDL_GetError()};
            }
            SDL_Texture* texture =
                SDL_CreateTextureFromSurface(renderer, surface);
            SDL_DestroySurface(surface);
            if (texture == nullptr) {
                throw LegacyAssetError{SDL_GetError()};
            }
            // Terrain is continuously scaled by camera zoom. Linear sampling
            // preserves Blendomatic edge gradients instead of magnifying
            // individual transition pixels into blocky map boundaries.
            SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);
            frames.push_back(texture);
            if (rgba_frames != nullptr) {
                rgba_frames->push_back(std::move(decoded));
            }
        }
    } catch (...) {
        for (SDL_Texture* texture : frames) SDL_DestroyTexture(texture);
        if (rgba_frames != nullptr) rgba_frames->clear();
        throw;
    }
    return frames;
}

TerrainTextures load_local_terrain_textures(SDL_Renderer* renderer) {
    TerrainTextures textures;
    const auto requested_root = configured_asset_root();
    if (!requested_root) {
        return textures;
    }
    const std::filesystem::path texture_root =
        *requested_root / "Terrain" / "Textures";
    textures.grass = load_local_terrain_texture(
        renderer,
        texture_root / "g_grs_00_COLOR.png"
    );
    textures.water = load_local_terrain_texture(
        renderer,
        texture_root / "g_wtr_00_COLOR.png"
    );
    textures.beach = load_local_terrain_texture(
        renderer,
        texture_root / "g_bch_00_COLOR.png"
    );
    textures.shallows = load_local_terrain_texture(
        renderer,
        texture_root / "g_sha_00_COLOR.png"
    );
    textures.farm_growing = load_local_terrain_texture(
        renderer,
        texture_root / "g_fm1_00_COLOR.png"
    );
    textures.farm_harvested = load_local_terrain_texture(
        renderer,
        texture_root / "g_fm2_00_COLOR.png"
    );
    if (textures.grass == nullptr || textures.water == nullptr ||
        textures.beach == nullptr || textures.shallows == nullptr) {
        const std::filesystem::path data_root =
            *requested_root / "Data";
        try {
            const DrsArchive terrain{data_root / "terrain.drs"};
            const DrsArchive interface{data_root / "interfac.drs"};
            const LegacyPalette palette = LegacyPalette::from_jasc(
                interface.read("bina", 50500)
            );
            if (textures.grass == nullptr) {
                textures.grass_archive_frames =
                    load_terrain_archive_frames(
                        renderer, terrain, palette, 15001,
                        &textures.grass_archive_rgba
                );
                SDL_Log(
                    "using original terrain.drs SLP 15001 grass frames: %zu",
                    textures.grass_archive_frames.size()
                );
            }
            if (textures.water == nullptr) {
                textures.water_archive_frames =
                    load_terrain_archive_frames(
                        renderer, terrain, palette, 15002,
                        &textures.water_archive_rgba
                );
                SDL_Log(
                    "using original terrain.drs SLP 15002 water frames: %zu",
                    textures.water_archive_frames.size()
                );
            }
            if (textures.beach == nullptr) {
                textures.beach_archive_frames =
                    load_terrain_archive_frames(
                        renderer, terrain, palette, 15017,
                        &textures.beach_archive_rgba
                );
                SDL_Log(
                    "using original terrain.drs SLP 15017 beach frames: %zu",
                    textures.beach_archive_frames.size()
                );
            }
            if (textures.shallows == nullptr) {
                textures.shallows_archive_frames =
                    load_terrain_archive_frames(
                        renderer, terrain, palette, 15014,
                        &textures.shallows_archive_rgba
                );
                SDL_Log(
                    "using original terrain.drs SLP 15014 shallows frames: %zu",
                    textures.shallows_archive_frames.size()
                );
            }
            const std::array blend_paths{
                data_root / "blendomatic.dat",
                data_root / "Blendomatic.dat",
            };
            for (const auto& path : blend_paths) {
                if (std::filesystem::is_regular_file(path)) {
                    textures.blendomatic = load_blendomatic(path);
                    SDL_Log(
                        "using original blendomatic.dat: %zu modes",
                        textures.blendomatic->modes.size()
                    );
                    break;
                }
            }
        } catch (const std::exception& error) {
            SDL_LogWarn(
                SDL_LOG_CATEGORY_APPLICATION,
                "optional terrain.drs textures unavailable: %s",
                error.what()
            );
        }
    }
    if (textures.grass != nullptr || textures.water != nullptr ||
        textures.beach != nullptr || textures.shallows != nullptr ||
        !textures.grass_archive_frames.empty() ||
        !textures.water_archive_frames.empty() ||
        !textures.beach_archive_frames.empty() ||
        !textures.shallows_archive_frames.empty()) {
        SDL_Log(
            "using optional original terrain textures from %s",
            requested_root->string().c_str()
        );
    }
    return textures;
}

const std::vector<RgbaFrame>* terrain_archive_rgba(Terrain terrain) {
    if (terrain == Terrain::water || terrain == Terrain::fish) {
        return &active_terrain_textures.water_archive_rgba;
    }
    if (terrain == Terrain::beach) {
        return &active_terrain_textures.beach_archive_rgba;
    }
    if (terrain == Terrain::shallows) {
        return &active_terrain_textures.shallows_archive_rgba;
    }
    return &active_terrain_textures.grass_archive_rgba;
}

SDL_Texture* terrain_transition_texture(
    SDL_Renderer* renderer,
    const Simulation& simulation,
    TilePosition position,
    std::size_t frame_index
) {
    TerrainTextures& textures = active_terrain_textures;
    if (!textures.blendomatic) return nullptr;
    constexpr std::array<TilePosition, 8> offsets{{
        {0, -1}, {1, -1}, {1, 0}, {1, 1},
        {0, 1}, {-1, 1}, {-1, 0}, {-1, -1},
    }};
    std::array<std::optional<Terrain>, 8> neighbors;
    for (std::size_t index = 0; index < offsets.size(); ++index) {
        const TilePosition neighbor{
            position.x + offsets[index].x,
            position.y + offsets[index].y,
        };
        if (simulation.map().contains(neighbor) &&
            (!active_settings.fog ||
             simulation.is_explored_to_controller(
                 active_view_player, neighbor))) {
            neighbors[index] = simulation.map().terrain_at(neighbor);
        }
    }
    const Terrain base_terrain =
        simulation.map().terrain_at(position);
    const auto selections = select_terrain_transition_masks(
        base_terrain, neighbors, position
    );
    if (selections.empty() ||
        std::ranges::any_of(
            selections,
            [](const TerrainMaskSelection& selected) {
                return selected.unresolved_cardinal_family.has_value() ||
                    selected.fixed_mask_ids.empty();
            })) {
        return nullptr;
    }
    const std::vector<RgbaFrame>* base_frames =
        terrain_archive_rgba(base_terrain);
    if (frame_index >= base_frames->size()) return nullptr;
    std::ostringstream key;
    key << static_cast<int>(base_terrain) << ':' << frame_index;
    for (const TerrainMaskSelection& selected : selections) {
        key << '/' << static_cast<int>(selected.overlay)
            << ':' << selected.blend_mode;
        for (int mask : selected.fixed_mask_ids) key << ':' << mask;
    }
    const auto cached = textures.transition_cache.find(key.str());
    if (cached != textures.transition_cache.end()) {
        return cached->second;
    }
    RgbaFrame composed = (*base_frames)[frame_index];
    for (const TerrainMaskSelection& selected : selections) {
        const std::vector<RgbaFrame>* overlay_frames =
            terrain_archive_rgba(selected.overlay);
        if (frame_index >= overlay_frames->size() ||
            selected.blend_mode < 0 ||
            static_cast<std::size_t>(selected.blend_mode) >=
                textures.blendomatic->modes.size()) {
            return nullptr;
        }
        for (int mask_id : selected.fixed_mask_ids) {
            const auto& masks = textures.blendomatic->modes[
                static_cast<std::size_t>(selected.blend_mode)];
            if (mask_id < 0 ||
                static_cast<std::size_t>(mask_id) >= masks.size()) {
                return nullptr;
            }
            composed = compose_terrain_transition(
                composed,
                (*overlay_frames)[frame_index],
                masks[static_cast<std::size_t>(mask_id)]
            );
        }
    }
    SDL_Surface* surface = SDL_CreateSurfaceFrom(
        composed.width,
        composed.height,
        SDL_PIXELFORMAT_RGBA32,
        composed.rgba.data(),
        composed.width * 4
    );
    if (surface == nullptr) return nullptr;
    SDL_Texture* texture =
        SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);
    if (texture == nullptr) return nullptr;
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);
    textures.transition_cache.emplace(key.str(), texture);
    return texture;
}

LegacySprite create_legacy_sprite(
    SDL_Renderer* renderer,
    const DrsArchive& graphics,
    const LegacyPalette& palette,
    std::int32_t resource_id,
    unsigned player,
    std::size_t frame_index = 0
) {
    if (!graphics.contains("slp", resource_id)) {
        throw LegacyAssetError{"mapped SLP is absent from graphics.drs"};
    }
    const std::vector<std::byte> bytes = graphics.read("slp", resource_id);
    RgbaFrame frame = decode_slp_frame(
        bytes, palette, frame_index, player
    );
    SDL_Surface* surface = SDL_CreateSurfaceFrom(
        frame.width,
        frame.height,
        SDL_PIXELFORMAT_RGBA32,
        frame.rgba.data(),
        frame.width * 4
    );
    if (surface == nullptr) {
        throw LegacyAssetError{SDL_GetError()};
    }
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);
    if (texture == nullptr) {
        throw LegacyAssetError{SDL_GetError()};
    }
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    return {
        texture,
        frame.width,
        frame.height,
        frame.hotspot_x,
        frame.hotspot_y,
    };
}

std::vector<std::byte> read_binary_file(
    const std::filesystem::path& path
) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
        throw LegacyAssetError{"cannot open loose SLP"};
    }
    const std::streamsize size = input.tellg();
    if (size < 0) {
        throw LegacyAssetError{"cannot determine loose SLP size"};
    }
    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    input.seekg(0);
    if (size != 0 &&
        !input.read(
            reinterpret_cast<char*>(bytes.data()),
            size
        )) {
        throw LegacyAssetError{"cannot read loose SLP"};
    }
    return bytes;
}

LegacySprite create_loose_legacy_sprite(
    SDL_Renderer* renderer,
    const std::filesystem::path& path,
    const LegacyPalette& palette,
    std::size_t frame_index = 0
) {
    const std::vector<std::byte> bytes = read_binary_file(path);
    RgbaFrame frame = decode_slp_frame(
        bytes, palette, frame_index, 1
    );
    SDL_Surface* surface = SDL_CreateSurfaceFrom(
        frame.width,
        frame.height,
        SDL_PIXELFORMAT_RGBA32,
        frame.rgba.data(),
        frame.width * 4
    );
    if (surface == nullptr) {
        throw LegacyAssetError{SDL_GetError()};
    }
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);
    if (texture == nullptr) {
        throw LegacyAssetError{SDL_GetError()};
    }
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    return {
        texture,
        frame.width,
        frame.height,
        frame.hotspot_x,
        frame.hotspot_y,
    };
}

LegacyHudBackground create_hud_background(
    SDL_Renderer* renderer,
    const std::filesystem::path& path,
    const LegacyPalette& palette
) {
    const std::vector<std::byte> bytes = read_binary_file(path);
    if (slp_frame_count(bytes) <
        hud_layout::game_background_frame_count) {
        throw LegacyAssetError{
            "loose game background has fewer than eight frames"
        };
    }
    LegacyHudBackground background;
    try {
        for (std::size_t index = 0;
             index < background.frames.size();
             ++index) {
            RgbaFrame frame = decode_slp_frame(
                bytes, palette, index, 1
            );
            SDL_Surface* surface = SDL_CreateSurfaceFrom(
                frame.width,
                frame.height,
                SDL_PIXELFORMAT_RGBA32,
                frame.rgba.data(),
                frame.width * 4
            );
            if (surface == nullptr) {
                throw LegacyAssetError{SDL_GetError()};
            }
            SDL_Texture* texture =
                SDL_CreateTextureFromSurface(renderer, surface);
            SDL_DestroySurface(surface);
            if (texture == nullptr) {
                throw LegacyAssetError{SDL_GetError()};
            }
            SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
            SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
            background.frames[index] = {
                texture,
                frame.width,
                frame.height,
                frame.hotspot_x,
                frame.hotspot_y,
            };
        }
    } catch (...) {
        background.destroy();
        throw;
    }
    return background;
}

SDL_Cursor* load_archive_cursor(aoe::cursor::State state) {
    const auto requested_root = configured_asset_root();
    if (!requested_root) {
        return nullptr;
    }
    try {
        const std::filesystem::path data_root =
            *requested_root / "Data";
        const DrsArchive interface{data_root / "interfac.drs"};
        const LegacyPalette palette = LegacyPalette::from_jasc(
            interface.read("bina", 50500)
        );
        const UiAssetMapping& mapping =
            ui_asset_mapping(UiAssetRole::default_cursor);
        const aoe::cursor::Selection selection =
            aoe::cursor::select(state);
        RgbaFrame frame = decode_slp_frame(
            interface.read("slp", mapping.resource_id),
            palette,
            selection.frame
        );
        SDL_Surface* surface = SDL_CreateSurfaceFrom(
            frame.width,
            frame.height,
            SDL_PIXELFORMAT_RGBA32,
            frame.rgba.data(),
            frame.width * 4
        );
        if (surface == nullptr) return nullptr;
        SDL_Cursor* cursor = SDL_CreateColorCursor(
            surface,
            std::clamp(frame.hotspot_x, 0, frame.width - 1),
            std::clamp(frame.hotspot_y, 0, frame.height - 1)
        );
        SDL_DestroySurface(surface);
        return cursor;
    } catch (const std::exception& error) {
        SDL_LogWarn(
            SDL_LOG_CATEGORY_APPLICATION,
            "cannot load optional archive cursor: %s",
            error.what()
        );
        return nullptr;
    }
}

LegacyAnimation create_legacy_animation(
    SDL_Renderer* renderer,
    const DrsArchive& graphics,
    const LegacyPalette& palette,
    std::int32_t resource_id,
    std::size_t frames_per_angle,
    unsigned player,
    const std::optional<ExactShadowBinding>& shadow = std::nullopt
) {
    const std::vector<std::byte> bytes = graphics.read("slp", resource_id);
    const std::size_t count = slp_frame_count(bytes);
    LegacyAnimation animation;
    animation.frames_per_angle = frames_per_angle;
    animation.frames.reserve(count);
    try {
        for (std::size_t index = 0; index < count; ++index) {
            RgbaFrame frame = decode_slp_frame(bytes, palette, index, player);
            SDL_Surface* surface = SDL_CreateSurfaceFrom(
                frame.width,
                frame.height,
                SDL_PIXELFORMAT_RGBA32,
                frame.rgba.data(),
                frame.width * 4
            );
            if (surface == nullptr) {
                throw LegacyAssetError{SDL_GetError()};
            }
            SDL_Texture* texture =
                SDL_CreateTextureFromSurface(renderer, surface);
            SDL_DestroySurface(surface);
            if (texture == nullptr) {
                throw LegacyAssetError{SDL_GetError()};
            }
            SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
            SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
            animation.frames.push_back({
                texture,
                frame.width,
                frame.height,
                frame.hotspot_x,
                frame.hotspot_y,
            });
        }
    } catch (...) {
        animation.destroy();
        throw;
    }
    if (shadow) {
        try {
            const std::vector<std::byte> shadow_bytes =
                graphics.read("slp", shadow->shadow_slp);
            const std::size_t shadow_count =
                slp_frame_count(shadow_bytes);
            animation.shadow_frames.reserve(shadow_count);
            for (std::size_t index = 0;
                 index < shadow_count;
                 ++index) {
                RgbaFrame frame = decode_slp_frame(
                    shadow_bytes, palette, index, 1
                );
                SDL_Surface* surface = SDL_CreateSurfaceFrom(
                    frame.width,
                    frame.height,
                    SDL_PIXELFORMAT_RGBA32,
                    frame.rgba.data(),
                    frame.width * 4
                );
                if (surface == nullptr) {
                    throw LegacyAssetError{SDL_GetError()};
                }
                SDL_Texture* texture =
                    SDL_CreateTextureFromSurface(renderer, surface);
                SDL_DestroySurface(surface);
                if (texture == nullptr) {
                    throw LegacyAssetError{SDL_GetError()};
                }
                SDL_SetTextureScaleMode(
                    texture, SDL_SCALEMODE_NEAREST
                );
                SDL_SetTextureBlendMode(
                    texture, SDL_BLENDMODE_BLEND
                );
                animation.shadow_frames.push_back({
                    texture, frame.width, frame.height,
                    frame.hotspot_x, frame.hotspot_y,
                });
            }
            animation.shadow_frames_per_angle =
                static_cast<std::size_t>(
                    std::max<std::int16_t>(
                        shadow->shadow_frame_count, 1
                    )
                );
            animation.shadow_angle_count =
                std::max<int>(shadow->shadow_angle_count, 1);
            animation.shadow_display_angle =
                shadow->display_angle;
            animation.shadow_offset_x = shadow->offset_x;
            animation.shadow_offset_y = shadow->offset_y;
        } catch (const std::exception&) {
            for (LegacySprite& frame : animation.shadow_frames) {
                frame.destroy();
            }
            animation.shadow_frames.clear();
        }
    }
    return animation;
}

LegacyComposite create_legacy_composite(
    SDL_Renderer* renderer,
    const DrsArchive& graphics,
    const LegacyPalette& palette,
    const LegacyDatFile& dat,
    std::int16_t root_graphic,
    unsigned player,
    bool expand_deltas = true,
    bool allow_missing_root_sprite = false
) {
    LegacyComposite composite;
    std::set<std::int16_t> active_path;
    std::size_t sequence{};
    struct PendingPart {
        LegacyCompositePart part;
        std::size_t sequence{};
    };
    std::vector<PendingPart> pending;
    const std::function<void(std::int16_t, int, int)> visit =
        [&](std::int16_t graphic_id, int offset_x, int offset_y) {
            if (graphic_id < 0) {
                return;
            }
            if (!active_path.insert(graphic_id).second) {
                throw LegacyAssetError{
                    "cycle in mapped legacy graphic composition"
                };
            }
            const LegacyGraphic* graphic = dat.graphic(
                static_cast<std::size_t>(graphic_id)
            );
            if (graphic == nullptr) {
                throw LegacyAssetError{
                    "mapped legacy graphic is absent"
                };
            }
            if (graphic->slp_id >= 0) {
                if (!graphics.contains("slp", graphic->slp_id)) {
                    if (!allow_missing_root_sprite ||
                        graphic_id != root_graphic ||
                        graphic->deltas.empty()) {
                        throw LegacyAssetError{
                            "composite graphic " +
                            std::to_string(graphic_id) +
                            " references absent SLP " +
                            std::to_string(graphic->slp_id)
                        };
                    }
                } else {
                    PendingPart value;
                    value.part.sprite = create_legacy_sprite(
                        renderer,
                        graphics,
                        palette,
                        graphic->slp_id,
                        player
                    );
                    value.part.layer = graphic->layer;
                    value.part.offset_x = offset_x;
                    value.part.offset_y = offset_y;
                    value.sequence = sequence++;
                    pending.push_back(std::move(value));
                }
            } else if (graphic->deltas.empty()) {
                throw LegacyAssetError{
                    "mapped legacy graphic has no drawable data"
                };
            }
            if (expand_deltas) {
                for (const LegacyGraphicDelta& delta : graphic->deltas) {
                    visit(
                        delta.graphic_id,
                        offset_x + delta.offset_x,
                        offset_y + delta.offset_y
                    );
                }
            }
            active_path.erase(graphic_id);
        };
    try {
        visit(root_graphic, 0, 0);
        std::stable_sort(
            pending.begin(),
            pending.end(),
            [](const PendingPart& left, const PendingPart& right) {
                return left.part.layer < right.part.layer;
            }
        );
        composite.parts.reserve(pending.size());
        for (PendingPart& value : pending) {
            composite.parts.push_back(std::move(value.part));
        }
    } catch (...) {
        for (PendingPart& value : pending) {
            value.part.sprite.destroy();
        }
        throw;
    }
    return composite;
}

LegacyAnimatedComposite create_legacy_animated_composite(
    SDL_Renderer* renderer,
    const DrsArchive& graphics,
    const LegacyPalette& palette,
    const LegacyDatFile& dat,
    std::int16_t root_graphic,
    unsigned player,
    bool allow_missing_root_sprite,
    bool expand_deltas = true
) {
    LegacyAnimatedComposite composite;
    std::set<std::int16_t> active_path;
    const std::function<void(std::int16_t, int, int, int)> visit =
        [&](std::int16_t graphic_id,
            int offset_x,
            int offset_y,
            int display_angle) {
            if (graphic_id < 0) {
                return;
            }
            if (!active_path.insert(graphic_id).second) {
                throw LegacyAssetError{
                    "cycle in mapped animated legacy composition"
                };
            }
            const LegacyGraphic* graphic = dat.graphic(
                static_cast<std::size_t>(graphic_id)
            );
            if (graphic == nullptr) {
                throw LegacyAssetError{
                    "mapped animated legacy graphic is absent"
                };
            }
            if (graphic->slp_id >= 0) {
                if (!graphics.contains("slp", graphic->slp_id)) {
                    if (!allow_missing_root_sprite ||
                        graphic_id != root_graphic ||
                        graphic->deltas.empty()) {
                        throw LegacyAssetError{
                            "composite graphic " +
                            std::to_string(graphic_id) +
                            " references absent SLP " +
                            std::to_string(graphic->slp_id)
                        };
                    }
                } else {
                    LegacyAnimatedCompositePart part;
                    part.animation = create_legacy_animation(
                        renderer,
                        graphics,
                        palette,
                        graphic->slp_id,
                        static_cast<std::size_t>(
                            std::max<std::int16_t>(graphic->frame_count, 1)
                        ),
                        player
                    );
                    part.layer = graphic->layer;
                    part.offset_x = offset_x;
                    part.offset_y = offset_y;
                    part.angle_count =
                        std::max<int>(graphic->angle_count, 1);
                    part.display_angle = display_angle;
                    composite.parts.push_back(std::move(part));
                }
            } else if (graphic->deltas.empty()) {
                throw LegacyAssetError{
                    "mapped animated graphic has no drawable data"
                };
            }
            if (expand_deltas) {
                for (const LegacyGraphicDelta& delta : graphic->deltas) {
                    visit(
                        delta.graphic_id,
                        offset_x + delta.offset_x,
                        offset_y + delta.offset_y,
                        delta.display_angle
                    );
                }
            }
            active_path.erase(graphic_id);
        };
    try {
        visit(root_graphic, 0, 0, -1);
        std::stable_sort(
            composite.parts.begin(),
            composite.parts.end(),
            [](const LegacyAnimatedCompositePart& left,
               const LegacyAnimatedCompositePart& right) {
                return left.layer < right.layer;
            }
        );
        if (composite.parts.empty()) {
            throw LegacyAssetError{
                "animated legacy composition is empty"
            };
        }
    } catch (...) {
        composite.destroy();
        throw;
    }
    return composite;
}

LegacySprites load_local_legacy_sprites(
    SDL_Renderer* renderer,
    const std::array<bool, 8>& required_owner_slots,
    const std::array<bool, 19>& required_civilizations
) {
    LegacySprites sprites;
    active_composite_load_failures.clear();
    active_animation_load_failures.clear();
    const auto requested_root = configured_asset_root();
    if (!requested_root) {
        return sprites;
    }
    const std::filesystem::path data_root =
        *requested_root / "Data";
    const auto load_packaged_texture =
        [renderer](const std::filesystem::path& path, bool png) {
            SDL_Surface* surface = png
                ? SDL_LoadPNG(path.string().c_str())
                : SDL_LoadBMP(path.string().c_str());
            if (surface == nullptr) return static_cast<SDL_Texture*>(nullptr);
            SDL_Texture* texture =
                SDL_CreateTextureFromSurface(renderer, surface);
            SDL_DestroySurface(surface);
            if (texture != nullptr) {
                SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);
            }
            return texture;
        };
    sprites.frontend_background = load_packaged_texture(
        *requested_root / "launcher_res" / "background.png", true
    );
    sprites.scenario_background = load_packaged_texture(
        *requested_root / "scenariobkg.bmp", false
    );
    if (sprites.frontend_background != nullptr) {
        SDL_Log(
            "using packaged frontend background from %s",
            requested_root->string().c_str()
        );
    }
    if (sprites.scenario_background != nullptr) {
        SDL_Log(
            "using packaged scenario background from %s",
            requested_root->string().c_str()
        );
    }
    try {
        const std::filesystem::path media =
            *requested_root / "Campaign" / "Media";
        const LegacyPalette campaign_palette = LegacyPalette::from_jasc(
            read_binary_file(media / "backgrd1.pal")
        );
        sprites.campaign_background = create_loose_legacy_sprite(
            renderer, media / "backgrd1.SLP", campaign_palette
        );
        SDL_Log(
            "using packaged campaign SLP background from %s",
            media.string().c_str()
        );
    } catch (const std::exception& error) {
        SDL_LogWarn(
            SDL_LOG_CATEGORY_APPLICATION,
            "cannot load packaged campaign SLP background: %s",
            error.what()
        );
    }
    try {
        const DrsArchive graphics{data_root / "graphics.drs"};
        const DrsArchive interface{data_root / "interfac.drs"};
        const LegacyPalette palette = LegacyPalette::from_jasc(
            interface.read("bina", 50500)
        );
        try {
            const LegacyPalette game_palette =
                LegacyPalette::from_jasc(
                    read_binary_file(data_root / "pal_2.pal")
                );
            for (int civilization = 1;
                 civilization <= 18;
                 ++civilization) {
                const Civilization value =
                    static_cast<Civilization>(civilization);
                const std::filesystem::path path =
                    data_root / "Slp" /
                    hud_layout::civilization_file_name(value);
                try {
                    sprites.civilization_hud_backgrounds[
                        static_cast<std::size_t>(civilization)
                    ] = create_hud_background(
                        renderer, path, game_palette
                    );
                } catch (const std::exception& error) {
                    SDL_LogWarn(
                        SDL_LOG_CATEGORY_APPLICATION,
                        "cannot load original HUD %s: %s",
                        path.string().c_str(),
                        error.what()
                    );
                }
            }
        } catch (const std::exception& error) {
            SDL_LogWarn(
                SDL_LOG_CATEGORY_APPLICATION,
                "cannot load original civilization HUD files: %s",
                error.what()
            );
        }
        const LegacyDatFile dat =
            LegacyDatFile::load(data_root / "empires2_x1_p1.dat");
        std::array<bool, 4> required_architecture_families{};
        for (std::size_t civilization = 0;
             civilization < required_civilizations.size();
             ++civilization) {
            if (!required_civilizations[civilization]) continue;
            required_architecture_families[
                static_cast<std::size_t>(render_architecture_family(
                    static_cast<Civilization>(civilization)
                ))
            ] = true;
        }
        const auto attempt = [&](LegacySprite& target,
                                 std::int32_t resource_id,
                                 unsigned player,
                                 std::size_t frame_index = 0) {
            if (!graphics.contains("slp", resource_id)) {
                const std::string reason =
                    "SLP " + std::to_string(resource_id) +
                    " absent from graphics.drs";
                active_animation_load_failures[{
                    resource_id, player
                }] = {
                    AssetCoverageStatus::missing_archive_resource,
                    reason,
                };
                return;
            }
            try {
                target = create_legacy_sprite(
                    renderer, graphics, palette, resource_id, player,
                    frame_index
                );
            } catch (const std::exception& error) {
                active_animation_load_failures[{
                    resource_id, player
                }] = {
                    AssetCoverageStatus::decode_failure,
                    "SLP " + std::to_string(resource_id) +
                        " frame load/decode failed: " + error.what(),
                };
                SDL_LogWarn(
                    SDL_LOG_CATEGORY_APPLICATION,
                    "cannot load optional legacy SLP %d: %s",
                    resource_id,
                    error.what()
                );
            }
        };
        const auto attempt_building_shadowed = [&](
            LegacySprite& target,
            std::int32_t resource_id,
            unsigned player,
            std::size_t frame_index = 0
        ) {
            attempt(target, resource_id, player, frame_index);
            if (target.texture == nullptr) return;
            const auto binding =
                find_exact_shadow_binding(dat, resource_id);
            if (!binding) return;
            const std::size_t shadow_frame =
                binding->shadow_frame_count == 1
                ? 0U : frame_index;
            try {
                LegacyStaticShadow shadow;
                shadow.sprite = create_legacy_sprite(
                    renderer,
                    graphics,
                    palette,
                    binding->shadow_slp,
                    1,
                    shadow_frame
                );
                shadow.offset_x = binding->offset_x;
                shadow.offset_y = binding->offset_y;
                sprites.building_static_shadows.emplace(
                    target.texture, std::move(shadow)
                );
            } catch (const std::exception& error) {
                SDL_LogWarn(
                    SDL_LOG_CATEGORY_APPLICATION,
                    "cannot load proved building shadow for SLP %d: %s",
                    resource_id,
                    error.what()
                );
            }
        };
        const auto attempt_interface = [&](
            LegacySprite& target,
            std::int32_t resource_id,
            std::size_t frame_index = 0
        ) {
            try {
                target = create_legacy_sprite(
                    renderer,
                    interface,
                    palette,
                    resource_id,
                    1,
                    frame_index
                );
            } catch (const std::exception& error) {
                SDL_LogWarn(
                    SDL_LOG_CATEGORY_APPLICATION,
                    "cannot load optional interface SLP %d: %s",
                    resource_id,
                    error.what()
                );
            }
        };
        const auto attempt_animation = [&](
            PlayerLegacySprites& target,
            std::int32_t resource_id,
            std::size_t frames_per_angle
        ) {
            if (!graphics.contains("slp", resource_id)) {
                const std::string reason =
                    "SLP " + std::to_string(resource_id) +
                    " absent from graphics.drs";
                for (std::size_t index = 0; index < 8; ++index) {
                    if (!required_owner_slots[index]) continue;
                    active_animation_load_failures[{
                        resource_id,
                        static_cast<unsigned>(index + 1),
                    }] = {
                        AssetCoverageStatus::missing_archive_resource,
                        reason,
                    };
                }
                SDL_LogWarn(
                    SDL_LOG_CATEGORY_APPLICATION,
                    "cannot load optional legacy animation %d: %s",
                    resource_id,
                    reason.c_str()
                );
                return;
            }
            try {
                for (std::size_t index = 0; index < 8; ++index) {
                    if (!required_owner_slots[index]) continue;
                    target.slot(index) = create_legacy_animation(
                        renderer,
                        graphics,
                        palette,
                        resource_id,
                        frames_per_angle,
                        static_cast<unsigned>(index + 1),
                        find_exact_shadow_binding(dat, resource_id)
                    );
                }
            } catch (const std::exception& error) {
                target.destroy();
                const std::string reason =
                    "SLP " + std::to_string(resource_id) +
                    " load/decode failed: " + error.what();
                for (std::size_t index = 0; index < 8; ++index) {
                    if (!required_owner_slots[index]) continue;
                    active_animation_load_failures[{
                        resource_id,
                        static_cast<unsigned>(index + 1),
                    }] = {
                        AssetCoverageStatus::decode_failure,
                        reason,
                    };
                }
                SDL_LogWarn(
                    SDL_LOG_CATEGORY_APPLICATION,
                    "cannot load optional legacy animation %d: %s",
                    resource_id,
                    reason.c_str()
                );
            }
        };
        const auto attempt_composite = [&](
            PlayerLegacyComposite& target,
            std::int16_t root_graphic,
            bool expand_deltas = true,
            bool allow_missing_root_sprite = false
        ) {
            try {
                const auto load = [&](unsigned player) {
                    if (!expand_deltas) {
                        return create_legacy_composite(
                            renderer,
                            graphics,
                            palette,
                            dat,
                            root_graphic,
                            player,
                            false,
                            allow_missing_root_sprite
                        );
                    }
                    return create_legacy_composite(
                        renderer,
                        graphics,
                        palette,
                        dat,
                        root_graphic,
                        player,
                        true,
                        allow_missing_root_sprite
                    );
                };
                for (std::size_t index = 0; index < 8; ++index) {
                    if (!required_owner_slots[index]) continue;
                    target.slot(index) = load(
                        static_cast<unsigned>(index + 1)
                    );
                }
            } catch (const std::exception& error) {
                target.destroy();
                const std::string reason = error.what();
                const AssetCoverageStatus status =
                    reason.find("references absent SLP") !=
                            std::string::npos
                    ? AssetCoverageStatus::missing_composite_part
                    : AssetCoverageStatus::decode_failure;
                for (std::size_t index = 0; index < 8; ++index) {
                    if (!required_owner_slots[index]) continue;
                    active_composite_load_failures[{
                        root_graphic,
                        static_cast<unsigned>(index + 1),
                    }] = {status, reason};
                }
                SDL_LogWarn(
                    SDL_LOG_CATEGORY_APPLICATION,
                    "cannot load optional legacy composite %d: %s",
                    root_graphic,
                    error.what()
                );
            }
        };
        const auto attempt_animated_composite = [&](
            PlayerLegacyAnimatedComposite& target,
            std::int16_t root_graphic,
            bool allow_missing_root_sprite = false
        ) {
            try {
                for (std::size_t index = 0; index < 8; ++index) {
                    if (!required_owner_slots[index]) continue;
                    target.slot(index) =
                        create_legacy_animated_composite(
                            renderer, graphics, palette, dat,
                            root_graphic,
                            static_cast<unsigned>(index + 1),
                            allow_missing_root_sprite
                        );
                }
            } catch (const std::exception& error) {
                target.destroy();
                const std::string reason = error.what();
                const AssetCoverageStatus status =
                    reason.find("references absent SLP") !=
                            std::string::npos
                    ? AssetCoverageStatus::missing_composite_part
                    : AssetCoverageStatus::decode_failure;
                for (std::size_t index = 0; index < 8; ++index) {
                    if (!required_owner_slots[index]) continue;
                    active_composite_load_failures[{
                        root_graphic,
                        static_cast<unsigned>(index + 1),
                    }] = {status, reason};
                }
                SDL_LogWarn(
                    SDL_LOG_CATEGORY_APPLICATION,
                    "cannot load exact building-state composite %d: %s",
                    root_graphic,
                    error.what()
                );
            }
        };
        // Exact civilization records share these construction roots. Farm
        // has no root; Stone Wall and Palisade Gate roots vary by civilization
        // and remain fallback until that complete selection map is bound.
        constexpr std::array construction_roots{
            std::pair{BuildingKind::palisade_wall, std::int16_t{118}},
            std::pair{BuildingKind::watch_tower, std::int16_t{118}},
            std::pair{BuildingKind::stone_gate_x, std::int16_t{118}},
            std::pair{BuildingKind::stone_gate_y, std::int16_t{118}},
            std::pair{BuildingKind::outpost, std::int16_t{118}},
            std::pair{BuildingKind::bombard_tower, std::int16_t{118}},
            std::pair{BuildingKind::house, std::int16_t{119}},
            std::pair{BuildingKind::mill, std::int16_t{119}},
            std::pair{BuildingKind::lumber_camp, std::int16_t{119}},
            std::pair{BuildingKind::mining_camp, std::int16_t{119}},
            std::pair{BuildingKind::barracks, std::int16_t{120}},
            std::pair{BuildingKind::archery_range, std::int16_t{120}},
            std::pair{BuildingKind::stable, std::int16_t{120}},
            std::pair{BuildingKind::blacksmith, std::int16_t{120}},
            std::pair{BuildingKind::monastery, std::int16_t{120}},
            std::pair{BuildingKind::town_center, std::int16_t{121}},
            std::pair{BuildingKind::castle, std::int16_t{121}},
            std::pair{BuildingKind::university, std::int16_t{121}},
            std::pair{BuildingKind::siege_workshop, std::int16_t{121}},
            std::pair{BuildingKind::market, std::int16_t{121}},
            std::pair{BuildingKind::wonder, std::int16_t{123}},
            std::pair{BuildingKind::dock, std::int16_t{4248}},
        };
        for (const auto& [kind, root] : construction_roots) {
            attempt_animated_composite(
                sprites.building_construction_composites[kind],
                root
            );
        }
        constexpr std::array death_roots{
            std::pair{BuildingKind::palisade_wall, std::int16_t{37}},
            std::pair{BuildingKind::stone_wall, std::int16_t{37}},
            std::pair{BuildingKind::outpost, std::int16_t{37}},
            std::pair{BuildingKind::house, std::int16_t{38}},
            std::pair{BuildingKind::mill, std::int16_t{38}},
            std::pair{BuildingKind::lumber_camp, std::int16_t{38}},
            std::pair{BuildingKind::mining_camp, std::int16_t{38}},
            std::pair{BuildingKind::watch_tower, std::int16_t{38}},
            std::pair{BuildingKind::palisade_gate_x, std::int16_t{38}},
            std::pair{BuildingKind::palisade_gate_y, std::int16_t{38}},
            std::pair{BuildingKind::stone_gate_x, std::int16_t{38}},
            std::pair{BuildingKind::stone_gate_y, std::int16_t{38}},
            std::pair{BuildingKind::bombard_tower, std::int16_t{38}},
            std::pair{BuildingKind::barracks, std::int16_t{39}},
            std::pair{BuildingKind::archery_range, std::int16_t{39}},
            std::pair{BuildingKind::stable, std::int16_t{39}},
            std::pair{BuildingKind::blacksmith, std::int16_t{39}},
            std::pair{BuildingKind::monastery, std::int16_t{39}},
            std::pair{BuildingKind::town_center, std::int16_t{40}},
            std::pair{BuildingKind::castle, std::int16_t{40}},
            std::pair{BuildingKind::university, std::int16_t{40}},
            std::pair{BuildingKind::siege_workshop, std::int16_t{40}},
            std::pair{BuildingKind::market, std::int16_t{40}},
            std::pair{BuildingKind::wonder, std::int16_t{42}},
            std::pair{BuildingKind::dock, std::int16_t{5452}},
        };
        for (const auto& [kind, root] : death_roots) {
            const BuildingStateRoot* canonical = building_state_root(
                kind, RenderBuildingState::destroyed
            );
            if (canonical == nullptr) {
                throw LegacyAssetError{
                    "building death loader has no canonical mapping"
                };
            }
            attempt_animated_composite(
                sprites.building_death_composites[kind],
                canonical->graphic_root
            );
            static_cast<void>(root);
        }
        std::set<std::int16_t> damage_roots;
        for (int kind = 0; kind <= static_cast<int>(BuildingKind::wonder);
             ++kind) {
            if (static_cast<BuildingKind>(kind) ==
                BuildingKind::fish_trap) {
                continue;
            }
            for (int civilization = static_cast<int>(Civilization::generic);
                 civilization <= static_cast<int>(Civilization::mayans);
                 ++civilization) {
                if (!required_civilizations[
                        static_cast<std::size_t>(civilization)
                    ]) {
                    continue;
                }
                for (const BuildingDamageRecord& record :
                     canonical_building_damage_records(
                         static_cast<BuildingKind>(kind),
                         static_cast<Civilization>(civilization)
                     )) {
                    damage_roots.insert(record.graphic_id);
                }
            }
        }
        for (const std::int16_t root : damage_roots) {
            attempt_animated_composite(
                sprites.building_damage_graphics[root],
                root,
                true
            );
        }
        // Exact HD DAT mappings: unit ID -> standing graphic -> SLP.
        attempt(sprites.sheep_blue, 3629, 1);       // unit 594
        attempt(sprites.sheep_red, 3629, 2);
        attempt(sprites.villager_blue, 1479, 1);    // male unit 83
        attempt(sprites.villager_red, 1479, 2);
        // Building 109 standing graphic is four DAT delta-composed layers.
        constexpr std::array<std::int32_t, 4> town_center_slps{
            889, 890, 3596, 4612
        };
        for (std::size_t index = 0;
             index < town_center_slps.size();
             ++index) {
            attempt(
                sprites.town_center_blue[index],
                town_center_slps[index],
                1
            );
            attempt(
                sprites.town_center_red[index],
                town_center_slps[index],
                2
            );
        }
        // Proven VER 5.7 north-European composites. Index 0 is
        // Feudal; index 1 is the Castle/Imperial appearance.
        constexpr std::array<std::int32_t, 2> archery_range_slps{
            21, 33
        };
        for (std::size_t index = 0;
             index < archery_range_slps.size();
             ++index) {
            attempt(
                sprites.archery_range_blue[index],
                archery_range_slps[index],
                1
            );
            attempt(
                sprites.archery_range_red[index],
                archery_range_slps[index],
                2
            );
        }
        const auto resource_slp = [](ResourceRenderKind kind) {
            return resource_asset_set(kind)->slp_id;
        };
        attempt(
            sprites.tree,
            resource_slp(ResourceRenderKind::forest),
            1
        );
        for (std::size_t frame = 0;
             frame < sprites.berry_states.size(); ++frame) {
            attempt(
                sprites.berry_states[frame],
                resource_slp(ResourceRenderKind::berry_bush),
                1,
                frame
            );
        }
        for (std::size_t frame = 0;
             frame < sprites.gold_states.size(); ++frame) {
            attempt(
                sprites.gold_states[frame],
                resource_slp(ResourceRenderKind::gold_mine),
                1,
                frame
            );
            attempt(
                sprites.stone_states[frame],
                resource_slp(ResourceRenderKind::stone_mine),
                1,
                frame
            );
        }
        attempt(
            sprites.fish,
            resource_slp(ResourceRenderKind::fish),
            1
        );
        const auto attempt_projectile = [&](
            LegacyAnimation& target,
            ProjectileAssetKind kind
        ) {
            const auto binding =
                find_projectile_asset_binding(dat, kind);
            if (!binding || !binding->direction_mapping_proved) {
                return;
            }
            if (!graphics.contains("slp", binding->slp_id)) {
                active_animation_load_failures[{
                    binding->slp_id, 1U
                }] = {
                    AssetCoverageStatus::missing_archive_resource,
                    "SLP " + std::to_string(binding->slp_id) +
                        " absent from graphics.drs",
                };
                return;
            }
            if (binding->shadow_slp_id &&
                !graphics.contains("slp", *binding->shadow_slp_id)) {
                active_animation_load_failures[{
                    binding->slp_id, 1U
                }] = {
                    AssetCoverageStatus::missing_shadow,
                    "projectile SLP " +
                        std::to_string(binding->slp_id) +
                        " references absent shadow SLP " +
                        std::to_string(*binding->shadow_slp_id),
                };
                return;
            }
            try {
                target = create_legacy_animation(
                    renderer,
                    graphics,
                    palette,
                    binding->slp_id,
                    static_cast<std::size_t>(
                        binding->frame_count
                    ),
                    1,
                    find_exact_shadow_binding(
                        dat, binding->slp_id
                    )
                );
            } catch (const std::exception& error) {
                target.destroy();
                active_animation_load_failures[{
                    binding->slp_id, 1U
                }] = {
                    AssetCoverageStatus::decode_failure,
                    "SLP " + std::to_string(binding->slp_id) +
                        " load/decode failed: " + error.what(),
                };
                SDL_LogWarn(
                    SDL_LOG_CATEGORY_APPLICATION,
                    "cannot load proved projectile SLP %d: %s",
                    binding->slp_id,
                    error.what()
                );
            }
        };
        attempt_projectile(
            sprites.fire_projectile,
            ProjectileAssetKind::fire_stream
        );
        attempt_projectile(
            sprites.cannonball_projectile,
            ProjectileAssetKind::cannonball
        );
        attempt_projectile(
            sprites.gunshot_projectile,
            ProjectileAssetKind::gunpowder_shot
        );
        attempt_projectile(
            sprites.scorpion_projectile,
            ProjectileAssetKind::scorpion_bolt
        );
        attempt_projectile(
            sprites.onager_primary_projectile,
            ProjectileAssetKind::onager_primary
        );
        attempt_projectile(
            sprites.onager_volley_projectile,
            ProjectileAssetKind::onager_volley
        );
        attempt_projectile(
            sprites.trebuchet_projectile,
            ProjectileAssetKind::trebuchet_stone
        );
        attempt_projectile(
            sprites.axe_projectile,
            ProjectileAssetKind::throwing_axe
        );
        attempt_projectile(
            sprites.arrow_projectile,
            ProjectileAssetKind::arrow
        );
        // Bombard Tower has no snow SLP in shipped data. Always use normal
        // player-colored graphics, including on snow terrain.
        const BuildingDirectSlpSet* bombard_tower =
            building_direct_slp_set(BuildingKind::bombard_tower);
        attempt_building_shadowed(
            sprites.bombard_tower_standing_blue,
            bombard_tower->slps[0][0],
            1
        );
        attempt_building_shadowed(
            sprites.bombard_tower_standing_red,
            bombard_tower->slps[0][0],
            2
        );
        attempt_building_shadowed(
            sprites.bombard_tower_construction_blue, 236, 1
        );
        attempt_building_shadowed(
            sprites.bombard_tower_construction_red, 236, 2
        );
        attempt_building_shadowed(
            sprites.bombard_tower_dying_blue, 73, 1
        );
        attempt_building_shadowed(
            sprites.bombard_tower_dying_red, 73, 2
        );
        const auto attempt_impact = [&](
            LegacyAnimation& target,
            ProjectileAssetKind kind
        ) {
            const auto binding =
                find_projectile_asset_binding(dat, kind);
            if (!binding || !binding->impact_slp_id ||
                !binding->impact_frame_count) {
                return;
            }
            if (!graphics.contains("slp", *binding->impact_slp_id)) {
                active_animation_load_failures[{
                    *binding->impact_slp_id, 1U
                }] = {
                    AssetCoverageStatus::missing_archive_resource,
                    "SLP " + std::to_string(*binding->impact_slp_id) +
                        " absent from graphics.drs",
                };
                return;
            }
            try {
                target = create_legacy_animation(
                    renderer,
                    graphics,
                    palette,
                    *binding->impact_slp_id,
                    static_cast<std::size_t>(
                        *binding->impact_frame_count
                    ),
                    1
                );
            } catch (const std::exception& error) {
                target.destroy();
                active_animation_load_failures[{
                    *binding->impact_slp_id, 1U
                }] = {
                    AssetCoverageStatus::decode_failure,
                    "SLP " +
                        std::to_string(*binding->impact_slp_id) +
                        " load/decode failed: " + error.what(),
                };
                SDL_LogWarn(
                    SDL_LOG_CATEGORY_APPLICATION,
                    "cannot load proved impact SLP %d: %s",
                    *binding->impact_slp_id,
                    error.what()
                );
            }
        };
        attempt_impact(
            sprites.siege_impact,
            ProjectileAssetKind::cannonball
        );
        attempt_impact(
            sprites.trebuchet_impact,
            ProjectileAssetKind::gunpowder_shot
        );
        // Unit 285 -> graphic 647 ARTCT_FN -> composite SLP 53.
        attempt(sprites.relic, 53, 1);
        attempt_animation(sprites.carried_relic_standing, 3827, 1);
        attempt_animation(sprites.carried_relic_moving, 3831, 1);
        // openage's hardcoded AoC inventory identifies 51141 as an
        // in-game HUD background and 50721 as the HUD action sheet.
        attempt_interface(
            sprites.hud_background,
            ui_asset_mapping(
                UiAssetRole::hud_background
            ).resource_id
        );
        attempt_interface(
            sprites.hud_actions,
            ui_asset_mapping(UiAssetRole::action_sheet).resource_id
        );
        for (std::int32_t frame = 0; frame < 69; ++frame) {
            attempt_interface(
                sprites.action_command_icons[frame],
                ui_icons::command_sheet,
                static_cast<std::size_t>(frame)
            );
        }
        for (std::int32_t frame = 0; frame < 134; ++frame) {
            attempt_interface(
                sprites.unit_command_icons[frame],
                ui_icons::unit_sheet,
                static_cast<std::size_t>(frame)
            );
        }
        for (std::int32_t frame = 0; frame < 118; ++frame) {
            attempt_interface(
                sprites.technology_command_icons[frame],
                ui_icons::technology_sheet,
                static_cast<std::size_t>(frame)
            );
        }
        for (std::int32_t frame = 0; frame < 52; ++frame) {
            attempt_interface(
                sprites.building_command_icons[frame],
                ui_icons::building_sheet,
                static_cast<std::size_t>(frame)
            );
        }
        constexpr std::array<std::size_t, 4> resource_frames{
            18, 19, 20, 21
        };
        for (std::size_t index = 0;
             index < resource_frames.size();
             ++index) {
            attempt_interface(
                sprites.resource_icons[index],
                ui_asset_mapping(
                    UiAssetRole::action_sheet
                ).resource_id,
                resource_frames[index]
            );
        }
        attempt_interface(
            sprites.portrait_frame,
            ui_asset_mapping(UiAssetRole::portrait_frame).resource_id
        );
        // Strict genie-rs unit links for civilization index 1 (Britons).
        // Unit 84 MRKT standing graphic 2268 -> composite SLP 2278.
        attempt(sprites.market_western_blue, 2278, 1);
        attempt(sprites.market_western_red, 2278, 2);
        attempt(sprites.market_eastern_blue, 2275, 1);
        attempt(sprites.market_eastern_red, 2275, 2);
        attempt(sprites.market_mediterranean_blue, 2277, 1);
        attempt(sprites.market_mediterranean_red, 2277, 2);
        attempt(sprites.market_far_eastern_blue, 2276, 1);
        attempt(sprites.market_far_eastern_red, 2276, 2);
        // Unit 128 TCART: standing 1141 -> 1122, walking 1681 -> 4486.
        attempt_animation(
            sprites.trade_cart_standing, 1122, 10
        );
        attempt_animation(
            sprites.trade_cart_moving, 4486, 10
        );
        // Direct composite SLPs proved through strict unit upgrade links.
        // Family order: western, eastern, Mediterranean, far-eastern.
        constexpr std::array<std::array<std::int32_t, 4>, 4>
            house_slps{{
                {{2223, 2223, 2223, 2223}},
                {{2235, 2232, 2234, 2233}},
                {{2247, 2244, 2246, 2245}},
                {{2247, 2244, 2246, 2245}},
            }};
        constexpr std::array<std::array<std::int32_t, 4>, 4>
            town_center_age_slps{{
                {{891, 891, 891, 891}},
                {{903, 900, 902, 901}},
                {{915, 912, 914, 913}},
                {{927, 924, 926, 925}},
            }};
        constexpr std::array<std::array<std::int32_t, 4>, 2>
            blacksmith_slps{{
                {{93, 90, 92, 91}},
                {{105, 102, 104, 103}},
            }};
        constexpr std::array<std::int32_t, 4> lumber_camp_slps{
            3507, 3504, 3506, 3505
        };
        constexpr std::array<std::int32_t, 4> mining_camp_slps{
            3495, 3492, 3494, 3493
        };
        constexpr std::array<std::array<std::int32_t, 4>, 2>
            university_slps{{
                {{3835, 3832, 3834, 3833}},
                {{3839, 3836, 3838, 3837}},
            }};
        constexpr std::array<std::array<std::int32_t, 4>, 3>
            market_age_slps{{
                {{2278, 2275, 2277, 2276}},
                {{820, 817, 819, 818}},
                {{3797, 3794, 3796, 3795}},
            }};
        constexpr std::array<
            std::array<std::array<std::int32_t, 4>, 4>,
            4
        > town_center_layer_slps{{
            {{
                {{889, 890, 3596, 4612}},
                {{889, 890, 3596, 4612}},
                {{889, 890, 3596, 4612}},
                {{889, 890, 3596, 4612}},
            }},
            {{
                {{895, 899, 3608, 4624}},
                {{892, 896, 3605, 4621}},
                {{894, 898, 3607, 4623}},
                {{893, 897, 3606, 4622}},
            }},
            {{
                {{907, 911, 3620, 4636}},
                {{904, 908, 3617, 4633}},
                {{906, 910, 3619, 4635}},
                {{905, 909, 3618, 4634}},
            }},
            {{
                {{919, 923, 3476, 4648}},
                {{916, 920, 3473, 4645}},
                {{918, 922, 3475, 4647}},
                {{917, 921, 3474, 4646}},
            }},
        }};
        for (std::size_t age = 0; age < house_slps.size(); ++age) {
            for (std::size_t family = 0;
                 family < house_slps[age].size();
                 ++family) {
                attempt_building_shadowed(
                    sprites.house_blue[age][family],
                    house_slps[age][family],
                    1
                );
                attempt_building_shadowed(
                    sprites.house_red[age][family],
                    house_slps[age][family],
                    2
                );
                attempt(
                    sprites.town_center_age_blue[age][family],
                    town_center_age_slps[age][family],
                    1
                );
                attempt(
                    sprites.town_center_age_red[age][family],
                    town_center_age_slps[age][family],
                    2
                );
                for (std::size_t layer = 0; layer < 4; ++layer) {
                    attempt(
                        sprites
                            .town_center_layers_blue[age][family][layer],
                        town_center_layer_slps[age][family][layer],
                        1
                    );
                    attempt(
                        sprites
                            .town_center_layers_red[age][family][layer],
                        town_center_layer_slps[age][family][layer],
                        2
                    );
                }
            }
        }
        for (std::size_t family = 0; family < 4; ++family) {
            for (std::size_t age = 0; age < 2; ++age) {
                attempt_building_shadowed(
                    sprites.blacksmith_blue[age][family],
                    blacksmith_slps[age][family],
                    1
                );
                attempt_building_shadowed(
                    sprites.blacksmith_red[age][family],
                    blacksmith_slps[age][family],
                    2
                );
                attempt_building_shadowed(
                    sprites.university_blue[age][family],
                    university_slps[age][family],
                    1
                );
                attempt_building_shadowed(
                    sprites.university_red[age][family],
                    university_slps[age][family],
                    2
                );
            }
            for (std::size_t age = 0; age < 3; ++age) {
                attempt_building_shadowed(
                    sprites.market_age_blue[age][family],
                    market_age_slps[age][family],
                    1
                );
                attempt_building_shadowed(
                    sprites.market_age_red[age][family],
                    market_age_slps[age][family],
                    2
                );
            }
            attempt_building_shadowed(
                sprites.lumber_camp_blue[family],
                lumber_camp_slps[family],
                1
            );
            attempt_building_shadowed(
                sprites.lumber_camp_red[family],
                lumber_camp_slps[family],
                2
            );
            attempt_building_shadowed(
                sprites.mining_camp_blue[family],
                mining_camp_slps[family],
                1
            );
            attempt_building_shadowed(
                sprites.mining_camp_red[family],
                mining_camp_slps[family],
                2
            );
        }
        for (const BuildingDirectSlpSet& mapping :
             canonical_building_direct_slp_sets()) {
            for (std::size_t age = 0; age < mapping.slps.size(); ++age) {
                for (std::size_t family = 0;
                     family < mapping.slps[age].size();
                     ++family) {
                    if (!required_architecture_families[family]) continue;
                    for (std::size_t player = 2; player < 8; ++player) {
                        if (!required_owner_slots[player]) continue;
                        LegacySprite& target =
                            sprites.direct_building_player_variants[
                                {mapping.kind, age, family, player}
                            ];
                        attempt_building_shadowed(
                            target,
                            mapping.slps[age][family],
                            static_cast<unsigned>(player + 1)
                        );
                    }
                }
            }
        }
        const BuildingTopologySlpSet* stone_wall =
            building_topology_slp_set(BuildingKind::stone_wall);
        for (std::size_t owner = 0;
             owner < required_owner_slots.size();
             ++owner) {
            if (!required_owner_slots[owner]) continue;
            for (std::size_t family = 0;
                 family < stone_wall->family_slps.size();
                 ++family) {
                for (int frame = 0;
                     frame < stone_wall->reachable_frame_count;
                     ++frame) {
                    attempt_building_shadowed(
                        sprites.stone_wall_by_owner[owner][family][
                            static_cast<std::size_t>(frame)
                        ],
                        stone_wall->family_slps[family],
                        static_cast<unsigned>(owner + 1),
                        static_cast<std::size_t>(frame)
                    );
                }
            }
        }
        for (std::size_t family = 0;
             family < stone_wall->construction_graphic_roots.size();
             ++family) {
            if (family < required_architecture_families.size() &&
                !required_architecture_families[family]) {
                continue;
            }
            attempt_composite(
                sprites.stone_wall_construction[family],
                stone_wall->construction_graphic_roots[family],
                true
            );
        }
        for (const GateConstructionSet& gate :
             canonical_gate_construction_sets()) {
            for (std::size_t family = 0;
                 family < gate.family_graphic_roots.size();
                 ++family) {
                attempt_composite(
                    sprites.gate_construction[gate.kind][family],
                    gate.family_graphic_roots[family],
                    true
                );
            }
        }
        const BuildingTopologySlpSet* palisade_wall =
            building_topology_slp_set(BuildingKind::palisade_wall);
        for (int frame = 0;
             frame < palisade_wall->asset_frame_count;
             ++frame) {
            attempt(
                sprites.palisade_wall_shadow[
                    static_cast<std::size_t>(frame)
                ],
                *palisade_wall->explicit_shadow_slp_id,
                1,
                static_cast<std::size_t>(frame)
            );
            for (std::size_t owner = 0;
                 owner < required_owner_slots.size();
                 ++owner) {
                if (!required_owner_slots[owner]) continue;
                attempt(
                    sprites.palisade_wall_by_owner[owner][
                        static_cast<std::size_t>(frame)
                    ],
                    palisade_wall->family_slps[0],
                    static_cast<unsigned>(owner + 1),
                    static_cast<std::size_t>(frame)
                );
            }
        }
        attempt_animation(
            sprites.palisade_wall_flags,
            *palisade_wall->junction_overlay_slp_id,
            9
        );
        // Strict unit/upgrade standing roots. Internal family order:
        // western, eastern, Mediterranean, far-eastern.
        constexpr std::array<std::array<std::int16_t, 4>, 4>
            town_center_roots{{
                {{3241, 3241, 3241, 3241}},
                {{3253, 3250, 3252, 3251}},
                {{3265, 3262, 3264, 3263}},
                {{3041, 3038, 3040, 3039}},
            }};
        constexpr std::array<std::array<std::int16_t, 4>, 4>
            barracks_roots{{
                {{2575, 2575, 2575, 2575}},
                {{93, 90, 92, 91}},
                {{105, 102, 104, 103}},
                {{105, 102, 104, 103}},
            }};
        constexpr std::array<std::array<std::int16_t, 4>, 4>
            mill_roots{{
                {{3124, 3124, 3124, 3124}},
                {{368, 365, 367, 366}},
                {{380, 377, 379, 378}},
                {{380, 377, 379, 378}},
            }};
        constexpr std::array<std::array<std::int16_t, 4>, 4>
            archery_range_roots{{
                {{-1, -1, -1, -1}},
                {{12, 9, 11, 10}},
                {{24, 21, 23, 22}},
                {{24, 21, 23, 22}},
            }};
        // Variant slots: base Watch Tower, Guard Tower, Keep.
        constexpr std::array<std::array<std::int16_t, 4>, 4>
            watch_tower_roots{{
                {{4202, 4199, 4201, 4200}},
                {{2532, 2529, 2531, 2530}},
                {{2407, 2404, 2406, 2405}},
                {{-1, -1, -1, -1}},
            }};
        constexpr std::array<std::array<std::int16_t, 4>, 4>
            stable_roots{{
                {{-1, -1, -1, -1}},
                {{513, 510, 512, 511}},
                {{525, 522, 524, 523}},
                {{525, 522, 524, 523}},
            }};
        constexpr std::array<std::array<std::int16_t, 4>, 4>
            castle_roots{{
                {{-1, -1, -1, -1}},
                {{-1, -1, -1, -1}},
                {{174, 171, 173, 172}},
                {{174, 171, 173, 172}},
            }};
        constexpr std::array<std::array<std::int16_t, 4>, 4>
            siege_workshop_roots{{
                {{-1, -1, -1, -1}},
                {{-1, -1, -1, -1}},
                {{489, 486, 488, 487}},
                {{489, 486, 488, 487}},
            }};
        constexpr std::array<std::array<std::int16_t, 4>, 4>
            dock_roots{{
                {{215, 215, 215, 215}},
                {{215, 215, 215, 215}},
                {{215, 215, 215, 215}},
                {{215, 215, 215, 215}},
            }};
        constexpr std::array<std::array<std::int16_t, 4>, 4>
            outpost_roots{{
                {{3223, 3223, 3223, 3223}},
                {{3223, 3223, 3223, 3223}},
                {{3223, 3223, 3223, 3223}},
                {{3223, 3223, 3223, 3223}},
            }};
        constexpr std::array<std::array<std::int16_t, 4>, 4>
            monastery_roots{{
                {{-1, -1, -1, -1}},
                {{-1, -1, -1, -1}},
                {{150, 147, 149, 148}},
                {{150, 147, 149, 148}},
            }};
        constexpr std::array<std::array<std::int16_t, 4>, 4>
            stone_gate_x_roots{{
                {{-1, -1, -1, -1}},
                {{-1, -1, -1, -1}},
                {{6497, 6497, 6497, 6497}},
                {{6497, 6497, 6497, 6497}},
            }};
        constexpr std::array<std::array<std::int16_t, 4>, 4>
            stone_gate_y_roots{{
                {{-1, -1, -1, -1}},
                {{-1, -1, -1, -1}},
                {{6518, 6518, 6518, 6518}},
                {{6518, 6518, 6518, 6518}},
            }};
        const auto load_building_roots = [&](
            BuildingKind kind,
            const auto&
        ) {
            const BuildingCompositeSet* mapping =
                building_composite_set(kind);
            if (mapping == nullptr) {
                throw LegacyAssetError{
                    "building loader has no canonical mapping"
                };
            }
            const auto& roots = mapping->graphic_roots;
            auto& composites = sprites.building_composites[kind];
            for (std::size_t age = 0; age < roots.size(); ++age) {
                for (std::size_t family = 0;
                     family < roots[age].size();
                     ++family) {
                    if (!required_architecture_families[family] ||
                        roots[age][family] < 0) {
                        continue;
                    }
                    attempt_composite(
                        composites[age][family],
                        roots[age][family],
                        mapping->expand_deltas
                    );
                }
            }
        };
        load_building_roots(
            BuildingKind::town_center, town_center_roots
        );
        load_building_roots(BuildingKind::barracks, barracks_roots);
        load_building_roots(BuildingKind::mill, mill_roots);
        load_building_roots(
            BuildingKind::archery_range, archery_range_roots
        );
        load_building_roots(
            BuildingKind::watch_tower, watch_tower_roots
        );
        load_building_roots(BuildingKind::stable, stable_roots);
        load_building_roots(BuildingKind::castle, castle_roots);
        load_building_roots(
            BuildingKind::siege_workshop, siege_workshop_roots
        );
        load_building_roots(BuildingKind::dock, dock_roots);
        load_building_roots(BuildingKind::outpost, outpost_roots);
        load_building_roots(
            BuildingKind::monastery, monastery_roots
        );
        load_building_roots(
            BuildingKind::stone_gate_x, stone_gate_x_roots
        );
        load_building_roots(
            BuildingKind::stone_gate_y, stone_gate_y_roots
        );
        load_building_roots(
            BuildingKind::palisade_gate_x,
            building_composite_set(
                BuildingKind::palisade_gate_x
            )->graphic_roots
        );
        load_building_roots(
            BuildingKind::palisade_gate_y,
            building_composite_set(
                BuildingKind::palisade_gate_y
            )->graphic_roots
        );
        for (const WonderCompositeSet& mapping :
             canonical_wonder_composite_sets()) {
            const Civilization civilization = mapping.civilization;
            if (!required_civilizations[
                    static_cast<std::size_t>(civilization)
                ]) {
                continue;
            }
            attempt_composite(
                sprites.wonder_standing[civilization],
                mapping.graphic_root,
                true
            );
        }
        const auto load_naval_roots = [&](
            auto& table,
            UnitKind kind,
            const std::array<std::int16_t, 4>&,
            bool = true
        ) {
            const RenderAction action =
                &table == &sprites.naval_attack
                ? RenderAction::attacking
                : &table == &sprites.naval_move
                    ? RenderAction::moving
                    : RenderAction::idle;
            const NavalCompositeSet* mapping =
                naval_composite_set(kind, action);
            if (mapping == nullptr) {
                throw LegacyAssetError{
                    "naval loader call has no canonical mapping"
                };
            }
            const auto& roots = mapping->graphic_roots;
            const bool expand_deltas = mapping->expand_deltas;
            auto& families = table[kind];
            for (std::size_t family = 0; family < roots.size(); ++family) {
                if (!required_architecture_families[family]) continue;
                try {
                    for (std::size_t player = 0; player < 8; ++player) {
                        if (!required_owner_slots[player]) continue;
                        families[family].slot(player) =
                            create_legacy_animated_composite(
                            renderer,
                            graphics,
                            palette,
                            dat,
                            roots[family],
                            static_cast<unsigned>(player + 1),
                            true,
                            expand_deltas
                        );
                    }
                } catch (const std::exception& error) {
                    families[family].destroy();
                    const std::string reason = error.what();
                    const AssetCoverageStatus status =
                        reason.find("references absent SLP") !=
                                std::string::npos
                        ? AssetCoverageStatus::missing_composite_part
                        : AssetCoverageStatus::decode_failure;
                    for (std::size_t player = 0;
                         player < 8;
                         ++player) {
                        if (!required_owner_slots[player]) continue;
                        active_composite_load_failures[{
                            roots[family],
                            static_cast<unsigned>(player + 1),
                        }] = {status, reason};
                    }
                    SDL_LogWarn(
                        SDL_LOG_CATEGORY_APPLICATION,
                        "cannot load optional animated composite %d: %s",
                        roots[family],
                        error.what()
                    );
                }
            }
        };
        load_naval_roots(
            sprites.naval_attack, UnitKind::galley,
            std::array<std::int16_t, 4>{4048, 4045, 4047, 4046}
        );
        load_naval_roots(
            sprites.naval_idle, UnitKind::galley,
            std::array<std::int16_t, 4>{4052, 4049, 4051, 4050}
        );
        load_naval_roots(
            sprites.naval_move, UnitKind::galley,
            std::array<std::int16_t, 4>{4056, 4053, 4055, 4054}
        );
        load_naval_roots(
            sprites.naval_attack, UnitKind::war_galley,
            std::array<std::int16_t, 4>{4009, 4006, 4008, 4007}
        );
        load_naval_roots(
            sprites.naval_idle, UnitKind::war_galley,
            std::array<std::int16_t, 4>{4013, 4010, 4012, 4011}
        );
        load_naval_roots(
            sprites.naval_move, UnitKind::war_galley,
            std::array<std::int16_t, 4>{4017, 4014, 4016, 4015}
        );
        load_naval_roots(
            sprites.naval_attack, UnitKind::galleon,
            std::array<std::int16_t, 4>{3885, 3882, 3884, 3883}
        );
        load_naval_roots(
            sprites.naval_idle, UnitKind::galleon,
            std::array<std::int16_t, 4>{3889, 3886, 3888, 3887}
        );
        load_naval_roots(
            sprites.naval_move, UnitKind::galleon,
            std::array<std::int16_t, 4>{3893, 3890, 3892, 3891}
        );
        load_naval_roots(
            sprites.naval_attack, UnitKind::transport_ship,
            std::array<std::int16_t, 4>{4089, 4086, 4088, 4087}
        );
        load_naval_roots(
            sprites.naval_idle, UnitKind::transport_ship,
            std::array<std::int16_t, 4>{4093, 4090, 4092, 4091}
        );
        load_naval_roots(
            sprites.naval_move, UnitKind::transport_ship,
            std::array<std::int16_t, 4>{4097, 4094, 4096, 4095}
        );
        load_naval_roots(
            sprites.naval_attack, UnitKind::fire_ship,
            std::array<std::int16_t, 4>{4178, 4175, 4177, 4176}
        );
        load_naval_roots(
            sprites.naval_idle, UnitKind::fire_ship,
            std::array<std::int16_t, 4>{4182, 4179, 4181, 4180}
        );
        load_naval_roots(
            sprites.naval_move, UnitKind::fire_ship,
            std::array<std::int16_t, 4>{4004, 4001, 4003, 4002}
        );
        load_naval_roots(
            sprites.naval_attack, UnitKind::fast_fire_ship,
            std::array<std::int16_t, 4>{4022, 4019, 4021, 4020}
        );
        load_naval_roots(
            sprites.naval_idle, UnitKind::fast_fire_ship,
            std::array<std::int16_t, 4>{4026, 4023, 4025, 4024}
        );
        load_naval_roots(
            sprites.naval_move, UnitKind::fast_fire_ship,
            std::array<std::int16_t, 4>{4030, 4027, 4029, 4028}
        );
        load_naval_roots(
            sprites.naval_attack, UnitKind::demolition_ship,
            std::array<std::int16_t, 4>{4173, 4173, 4173, 4173}
        );
        load_naval_roots(
            sprites.naval_idle, UnitKind::demolition_ship,
            std::array<std::int16_t, 4>{4039, 4036, 4038, 4037}
        );
        load_naval_roots(
            sprites.naval_move, UnitKind::demolition_ship,
            std::array<std::int16_t, 4>{4043, 4040, 4042, 4041}
        );
        load_naval_roots(
            sprites.naval_attack, UnitKind::heavy_demolition_ship,
            std::array<std::int16_t, 4>{4155, 4152, 4154, 4153}
        );
        load_naval_roots(
            sprites.naval_idle, UnitKind::heavy_demolition_ship,
            std::array<std::int16_t, 4>{3988, 3985, 3987, 3986}
        );
        load_naval_roots(
            sprites.naval_move, UnitKind::heavy_demolition_ship,
            std::array<std::int16_t, 4>{3992, 3989, 3991, 3990}
        );
        for (UnitKind kind : {
                 UnitKind::cannon_galleon,
                 UnitKind::elite_cannon_galleon,
             }) {
            load_naval_roots(
                sprites.naval_attack, kind,
                std::array<std::int16_t, 4>{3957, 3954, 3956, 3955}
            );
            load_naval_roots(
                sprites.naval_idle, kind,
                std::array<std::int16_t, 4>{3961, 3958, 3960, 3959}
            );
            load_naval_roots(
                sprites.naval_move, kind,
                std::array<std::int16_t, 4>{3965, 3962, 3964, 3963}
            );
        }
        for (UnitKind kind : {
                 UnitKind::longboat,
                 UnitKind::elite_longboat,
             }) {
            load_naval_roots(
                sprites.naval_attack, kind,
                std::array<std::int16_t, 4>{953, 953, 953, 953},
                false
            );
            load_naval_roots(
                sprites.naval_idle, kind,
                std::array<std::int16_t, 4>{959, 959, 959, 959},
                false
            );
            load_naval_roots(
                sprites.naval_move, kind,
                std::array<std::int16_t, 4>{963, 963, 963, 963},
                false
            );
        }
        for (UnitKind kind : {
                 UnitKind::turtle_ship,
                 UnitKind::elite_turtle_ship,
             }) {
            load_naval_roots(
                sprites.naval_attack, kind,
                std::array<std::int16_t, 4>{7257, 7257, 7257, 7257}
            );
            load_naval_roots(
                sprites.naval_idle, kind,
                std::array<std::int16_t, 4>{7258, 7258, 7258, 7258}
            );
            load_naval_roots(
                sprites.naval_move, kind,
                std::array<std::int16_t, 4>{7259, 7259, 7259, 7259}
            );
        }
        load_naval_roots(
            sprites.naval_attack, UnitKind::trade_cog,
            std::array<std::int16_t, 4>{3971, 3971, 3971, 3971}
        );
        load_naval_roots(
            sprites.naval_idle, UnitKind::trade_cog,
            std::array<std::int16_t, 4>{3975, 3975, 3975, 3975}
        );
        load_naval_roots(
            sprites.naval_move, UnitKind::trade_cog,
            std::array<std::int16_t, 4>{3979, 3979, 3979, 3979}
        );
        attempt_animation(sprites.villager_animation, 1479, 15);
        attempt_animation(sprites.sheep_animation, 3629, 9);
        attempt_animation(sprites.fishing_ship_standing, 444, 1);
        attempt_animation(sprites.fishing_ship_moving, 449, 1);
        const auto* fish_trap_standing = building_animated_slp_set(
            BuildingKind::fish_trap,
            RenderBuildingState::completed
        );
        const auto* fish_trap_construction = building_animated_slp_set(
            BuildingKind::fish_trap,
            RenderBuildingState::construction
        );
        attempt_animation(
            sprites.fish_trap_standing,
            fish_trap_standing->slp_id,
            static_cast<std::size_t>(fish_trap_standing->frame_count)
        );
        attempt_animation(
            sprites.fish_trap_construction,
            fish_trap_construction->slp_id,
            static_cast<std::size_t>(fish_trap_construction->frame_count)
        );
        attempt_animation(sprites.outpost_construction, 236, 1);
        attempt_animation(sprites.outpost_death, 73, 10);
        attempt_animation(sprites.wonder_construction, 241, 1);
        attempt_animation(sprites.wonder_death, 75, 10);
        attempt_animation(sprites.longboat_attack, 689, 1);
        attempt_animation(sprites.longboat_idle, 695, 1);
        attempt_animation(sprites.longboat_move, 699, 1);
        for (const UnitDeathAnimationSet& death :
             canonical_unit_death_animation_sets()) {
            attempt_animation(
                sprites.death[death.kind],
                death.slp,
                static_cast<std::size_t>(death.frames)
            );
        }
        attempt_animation(sprites.deer_animation, 342, 5);
        attempt_animation(sprites.boar_animation, 2557, 10);

        struct MilitaryMapping {
            UnitKind kind;
            std::int32_t slp;
            std::size_t frames_per_angle;
        };
        constexpr std::array military_mappings{
            MilitaryMapping{UnitKind::elite_skirmisher, 613, 9},
            MilitaryMapping{UnitKind::skirmisher, 1650, 8},
            MilitaryMapping{UnitKind::crossbowman, 192, 10},
            MilitaryMapping{UnitKind::battering_ram, 179, 1},
            MilitaryMapping{UnitKind::capped_ram, 1689, 1},
            MilitaryMapping{UnitKind::siege_ram, 3035, 1},
            MilitaryMapping{UnitKind::knight, 669, 10},
            MilitaryMapping{UnitKind::militia, 993, 6},
            MilitaryMapping{UnitKind::man_at_arms, 1044, 11},
            MilitaryMapping{UnitKind::long_swordsman, 1181, 6},
            MilitaryMapping{UnitKind::spearman, 873, 9},
            MilitaryMapping{UnitKind::mangonel, 722, 1},
            MilitaryMapping{UnitKind::cavalier, 855, 10},
            MilitaryMapping{UnitKind::pikeman, 2832, 8},
            MilitaryMapping{UnitKind::halberdier, 2793, 10},
            MilitaryMapping{UnitKind::hand_cannoneer, 587, 10},
            MilitaryMapping{UnitKind::bombard_cannon, 67, 10},
            MilitaryMapping{UnitKind::petard, 4497, 10},
            MilitaryMapping{UnitKind::hussar, 4855, 14},
            MilitaryMapping{UnitKind::scout_cavalry, 2085, 10},
            MilitaryMapping{UnitKind::two_handed_swordsman, 2806, 6},
            MilitaryMapping{UnitKind::arbalester, 2704, 11},
            MilitaryMapping{UnitKind::light_cavalry, 3004, 10},
            MilitaryMapping{UnitKind::champion, 3091, 10},
            MilitaryMapping{UnitKind::paladin, 3078, 10},
            MilitaryMapping{UnitKind::monk, 774, 6},
        };
        for (const MilitaryMapping& mapping : military_mappings) {
            const UnitAnimationSet canonical =
                *unit_animation_set(mapping.kind);
            attempt_animation(
                sprites.military[canonical.kind],
                canonical.idle_slp,
                static_cast<std::size_t>(canonical.idle_frames)
            );
        }
        attempt_animation(sprites.military[UnitKind::archer], 8, 10);
        for (std::size_t index = 0; index < 8; ++index) {
            if (!required_owner_slots[index]) continue;
            LegacyAnimation& archer_idle =
                sprites.military[UnitKind::archer].slot(index);
            archer_idle.angle_offsets = {0, 10, 20, 30, 41};
            archer_idle.angle_frame_counts = {10, 10, 10, 11, 11};
        }
        struct ActionMapping {
            UnitKind kind;
            std::int32_t move_slp;
            std::size_t move_frames;
            std::int32_t attack_slp;
            std::size_t attack_frames;
        };
        constexpr std::array action_mappings{
            ActionMapping{UnitKind::villager, 1484, 15, 1473, 15},
            ActionMapping{UnitKind::sheep, 3634, 16, 3623, 15},
            ActionMapping{UnitKind::deer, 348, 15, 336, 15},
            ActionMapping{UnitKind::boar, 2559, 10, 2555, 17},
            ActionMapping{UnitKind::archer, 12, 10, 2, 10},
            ActionMapping{UnitKind::elite_skirmisher, 617, 10, 607, 12},
            ActionMapping{UnitKind::skirmisher, 1654, 10, 1644, 12},
            ActionMapping{UnitKind::crossbowman, 196, 15, 186, 10},
            ActionMapping{UnitKind::battering_ram, 183, 15, 173, 15},
            ActionMapping{UnitKind::capped_ram, 1693, 15, 1683, 15},
            ActionMapping{UnitKind::siege_ram, 3039, 15, 3029, 15},
            ActionMapping{UnitKind::knight, 673, 10, 663, 10},
            ActionMapping{UnitKind::militia, 997, 12, 987, 10},
            ActionMapping{UnitKind::man_at_arms, 1048, 11, 1038, 11},
            ActionMapping{UnitKind::long_swordsman, 1185, 10, 1175, 10},
            ActionMapping{UnitKind::spearman, 877, 10, 867, 10},
            ActionMapping{UnitKind::mangonel, 726, 10, 716, 10},
            ActionMapping{UnitKind::cavalier, 859, 10, 849, 10},
            ActionMapping{UnitKind::pikeman, 2836, 10, 2826, 10},
            ActionMapping{UnitKind::halberdier, 2797, 15, 2787, 10},
            ActionMapping{UnitKind::hand_cannoneer, 591, 10, 581, 10},
            ActionMapping{UnitKind::bombard_cannon, 71, 10, 61, 10},
            ActionMapping{UnitKind::petard, 4498, 15, 4605, 10},
            ActionMapping{UnitKind::hussar, 4857, 11, 4853, 14},
            ActionMapping{UnitKind::scout_cavalry, 2089, 10, 2079, 10},
            ActionMapping{UnitKind::two_handed_swordsman, 2810, 10, 2800, 10},
            ActionMapping{UnitKind::arbalester, 2708, 11, 2698, 11},
            ActionMapping{UnitKind::light_cavalry, 3008, 10, 2998, 10},
            ActionMapping{UnitKind::champion, 3095, 10, 3085, 10},
            ActionMapping{UnitKind::paladin, 3082, 10, 3072, 10},
            ActionMapping{UnitKind::monk, 779, 10, 768, 10},
        };
        for (const ActionMapping& mapping : action_mappings) {
            const UnitAnimationSet canonical =
                *unit_animation_set(mapping.kind);
            attempt_animation(
                sprites.movement[canonical.kind],
                canonical.move_slp,
                static_cast<std::size_t>(canonical.move_frames)
            );
            attempt_animation(
                sprites.attack[canonical.kind],
                canonical.attack_slp,
                static_cast<std::size_t>(canonical.attack_frames)
            );
        }
        struct DeathMapping {
            UnitKind kind;
            std::int32_t slp;
            std::size_t frames_per_angle;
        };
        constexpr std::array standard_land_deaths{
            DeathMapping{UnitKind::archer, 5, 10},
            DeathMapping{UnitKind::elite_skirmisher, 610, 10},
            DeathMapping{UnitKind::skirmisher, 1647, 10},
            DeathMapping{UnitKind::crossbowman, 189, 10},
            DeathMapping{UnitKind::battering_ram, 176, 12},
            DeathMapping{UnitKind::capped_ram, 1686, 10},
            DeathMapping{UnitKind::siege_ram, 3032, 10},
            DeathMapping{UnitKind::knight, 666, 10},
            DeathMapping{UnitKind::militia, 990, 10},
            DeathMapping{UnitKind::man_at_arms, 1041, 11},
            DeathMapping{UnitKind::long_swordsman, 1178, 10},
            DeathMapping{UnitKind::spearman, 870, 10},
            DeathMapping{UnitKind::monk, 771, 10},
            DeathMapping{UnitKind::mangonel, 719, 10},
            DeathMapping{UnitKind::cavalier, 852, 10},
            DeathMapping{UnitKind::pikeman, 2829, 10},
            DeathMapping{UnitKind::halberdier, 2790, 10},
            DeathMapping{UnitKind::hand_cannoneer, 584, 10},
            DeathMapping{UnitKind::bombard_cannon, 64, 15},
            DeathMapping{UnitKind::petard, 4605, 10},
            DeathMapping{UnitKind::hussar, 4854, 14},
            DeathMapping{UnitKind::scout_cavalry, 2082, 10},
            DeathMapping{UnitKind::two_handed_swordsman, 2803, 10},
            DeathMapping{UnitKind::arbalester, 2701, 10},
            DeathMapping{UnitKind::light_cavalry, 3001, 10},
            DeathMapping{UnitKind::champion, 3088, 10},
            DeathMapping{UnitKind::paladin, 3075, 10},
        };
        for (const DeathMapping& mapping : standard_land_deaths) {
            const UnitAnimationSet canonical =
                *unit_animation_set(mapping.kind);
            attempt_animation(
                sprites.death[canonical.kind],
                canonical.death_slp,
                static_cast<std::size_t>(canonical.death_frames)
            );
        }
        struct UniqueInfantryMapping {
            UnitKind base;
            UnitKind elite;
            std::int32_t idle_slp;
            std::size_t idle_frames;
            std::int32_t move_slp;
            std::size_t move_frames;
            std::int32_t attack_slp;
            std::size_t attack_frames;
            std::int32_t death_slp;
            std::size_t death_frames;
        };
        constexpr std::array<UniqueInfantryMapping, 9> unique_infantry{{
            {UnitKind::longbowman, UnitKind::elite_longbowman,
             708, 15, 713, 15, 702, 15, 705, 15},
            {UnitKind::throwing_axeman, UnitKind::elite_throwing_axeman,
             1057, 10, 1061, 15, 1051, 16, 1054, 13},
            {UnitKind::huskarl, UnitKind::elite_huskarl,
             4539, 6, 4541, 10, 4537, 10, 4538, 10},
            {UnitKind::teutonic_knight, UnitKind::elite_teutonic_knight,
             1194, 10, 1198, 10, 1188, 15, 1191, 15},
            {UnitKind::samurai, UnitKind::elite_samurai,
             980, 10, 984, 10, 974, 10, 977, 10},
            {UnitKind::chu_ko_nu, UnitKind::elite_chu_ko_nu,
             221, 10, 225, 15, 215, 10, 218, 10},
            {UnitKind::cataphract, UnitKind::elite_cataphract,
             205, 10, 209, 10, 199, 10, 202, 10},
            {UnitKind::war_elephant, UnitKind::elite_war_elephant,
             801, 7, 805, 10, 795, 7, 798, 15},
            // DAT graphics 1375/1379/1369/1372 resolve to these SLPs.
            {UnitKind::woad_raider, UnitKind::elite_woad_raider,
             1598, 8, 1602, 12, 1592, 12, 1595, 10},
        }};
        for (const UniqueInfantryMapping& mapping : unique_infantry) {
            for (UnitKind kind : {mapping.base, mapping.elite}) {
                const UnitAnimationSet canonical =
                    *unit_animation_set(kind);
                attempt_animation(
                    sprites.military[kind],
                    canonical.idle_slp,
                    static_cast<std::size_t>(canonical.idle_frames)
                );
                attempt_animation(
                    sprites.movement[kind],
                    canonical.move_slp,
                    static_cast<std::size_t>(canonical.move_frames)
                );
                attempt_animation(
                    sprites.attack[kind],
                    canonical.attack_slp,
                    static_cast<std::size_t>(canonical.attack_frames)
                );
                attempt_animation(
                    sprites.death[kind],
                    canonical.death_slp,
                    static_cast<std::size_t>(canonical.death_frames)
                );
            }
        }
        struct UniqueUnitAnimationMapping {
            UnitKind kind;
            std::int32_t idle_slp;
            std::size_t idle_frames;
            std::int32_t move_slp;
            std::size_t move_frames;
            std::int32_t attack_slp;
            std::size_t attack_frames;
            std::int32_t death_slp;
            std::size_t death_frames;
        };
        constexpr std::array<UniqueUnitAnimationMapping, 29>
            unique_unit_animations{{
                // Strict VER 5.7 unit links:
                // 39 Cavalry Archer and 474 Heavy Cavalry Archer.
                {UnitKind::cavalry_archer,
                 326, 10, 330, 10, 320, 13, 323, 10},
                {UnitKind::heavy_cavalry_archer,
                 3763, 10, 3767, 10, 3757, 13, 3760, 10},
                {UnitKind::mameluke,
                 357, 6, 361, 10, 351, 10, 354, 10},
                {UnitKind::elite_mameluke,
                 357, 6, 361, 10, 351, 10, 354, 10},
                {UnitKind::janissary,
                 640, 10, 644, 10, 634, 10, 637, 10},
                {UnitKind::elite_janissary,
                 640, 10, 644, 10, 634, 10, 637, 10},
                {UnitKind::berserk,
                 4392, 6, 4396, 12, 4386, 10, 4389, 10},
                {UnitKind::elite_berserk,
                 4379, 6, 4383, 12, 4373, 10, 4376, 10},
                {UnitKind::mangudai,
                 788, 10, 792, 10, 782, 13, 785, 10},
                {UnitKind::elite_mangudai,
                 788, 10, 792, 10, 782, 13, 785, 10},
                {UnitKind::jaguar_warrior,
                 4860, 10, 4862, 15, 4858, 10, 4859, 10},
                {UnitKind::elite_jaguar_warrior,
                 4860, 10, 4862, 15, 4858, 10, 4859, 10},
                {UnitKind::plumed_archer,
                 4873, 10, 4875, 15, 4871, 15, 4872, 10},
                {UnitKind::elite_plumed_archer,
                 4873, 10, 4875, 15, 4871, 15, 4872, 10},
                {UnitKind::conquistador,
                 4722, 14, 4726, 14, 4716, 14, 4719, 14},
                {UnitKind::elite_conquistador,
                 4722, 14, 4726, 14, 4716, 14, 4719, 14},
                {UnitKind::tarkan,
                 4918, 14, 4920, 10, 4916, 14, 4917, 14},
                {UnitKind::elite_tarkan,
                 4918, 14, 4920, 10, 4916, 14, 4917, 14},
                {UnitKind::eagle_warrior,
                 4828, 10, 4830, 15, 4826, 10, 4827, 10},
                {UnitKind::elite_eagle_warrior,
                 4828, 10, 4830, 15, 4826, 10, 4827, 10},
                {UnitKind::scorpion,
                 942, 1, 946, 10, 936, 10, 939, 10},
                {UnitKind::heavy_scorpion,
                 2819, 1, 2823, 10, 2813, 8, 2816, 8},
                {UnitKind::onager,
                 3023, 1, 3026, 10, 3017, 10, 3020, 10},
                {UnitKind::siege_onager,
                 3559, 1, 3563, 10, 3553, 10, 3556, 10},
                {UnitKind::packed_trebuchet,
                 2279, 10, 2279, 10, 4573, 5, 4572, 10},
                {UnitKind::trebuchet,
                 1244, 1, 1244, 1, 1237, 22, 1241, 12},
                {UnitKind::camel_rider,
                 682, 5, 686, 10, 676, 10, 679, 10},
                {UnitKind::heavy_camel,
                 2768, 6, 2772, 12, 2762, 10, 2765, 11},
                {UnitKind::missionary,
                 4867, 12, 4870, 12, 4865, 13, 4866, 12},
            }};
        for (const UniqueUnitAnimationMapping& mapping :
             unique_unit_animations) {
            const UnitAnimationSet canonical =
                *unit_animation_set(mapping.kind);
            attempt_animation(
                sprites.military[canonical.kind],
                canonical.idle_slp,
                static_cast<std::size_t>(canonical.idle_frames)
            );
            attempt_animation(
                sprites.movement[canonical.kind],
                canonical.move_slp,
                static_cast<std::size_t>(canonical.move_frames)
            );
            attempt_animation(
                sprites.attack[canonical.kind],
                canonical.attack_slp,
                static_cast<std::size_t>(canonical.attack_frames)
            );
            attempt_animation(
                sprites.death[canonical.kind],
                canonical.death_slp,
                static_cast<std::size_t>(canonical.death_frames)
            );
        }
        attempt_animation(
            sprites.packed_trebuchet_transform, 4573, 5
        );
        attempt_animation(
            sprites.unpacked_trebuchet_transform, 1246, 5
        );
        attempt_animation(sprites.villager_gather, 1528, 15);
        attempt_animation(sprites.monk_convert, 768, 10);
        attempt_animation(sprites.missionary_heal, 4869, 14);
        if (sprites.sheep_blue.texture != nullptr ||
            sprites.villager_blue.texture != nullptr ||
            sprites.town_center_blue[0].texture != nullptr ||
            sprites.tree.texture != nullptr ||
            sprites.gold_states[0].texture != nullptr) {
            SDL_Log(
                "using optional original sprites from %s",
                data_root.string().c_str()
            );
        }
    } catch (const std::exception& error) {
        SDL_LogWarn(
            SDL_LOG_CATEGORY_APPLICATION,
            "optional original sprites unavailable: %s",
            error.what()
        );
    }
    return sprites;
}

bool render_legacy_sprite(
    SDL_Renderer* renderer,
    const LegacySprite& sprite,
    SDL_FPoint ground,
    bool visible = true,
    bool flip_horizontal = false
) {
    if (sprite.texture == nullptr) {
        return false;
    }
    SDL_SetTextureColorMod(
        sprite.texture,
        visible ? 255 : 82,
        visible ? 255 : 82,
        visible ? 255 : 82
    );
    const int draw_hotspot_x = flip_horizontal
        ? sprite.width - sprite.hotspot_x
        : sprite.hotspot_x;
    const SDL_FRect destination{
        ground.x - static_cast<float>(draw_hotspot_x),
        ground.y - static_cast<float>(sprite.hotspot_y),
        static_cast<float>(sprite.width),
        static_cast<float>(sprite.height),
    };
    SDL_RenderTextureRotated(
        renderer,
        sprite.texture,
        nullptr,
        &destination,
        0.0,
        nullptr,
        flip_horizontal ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE
    );
    return true;
}

bool render_legacy_building_sprite(
    SDL_Renderer* renderer,
    const LegacySprite& sprite,
    SDL_FPoint ground
) {
    const auto found =
        active_legacy_sprites.building_static_shadows.find(
            sprite.texture
        );
    if (found !=
            active_legacy_sprites.building_static_shadows.end()) {
        if (!render_legacy_sprite(
                renderer,
                found->second.sprite,
                {
                    ground.x + static_cast<float>(
                        found->second.offset_x
                    ),
                    ground.y + static_cast<float>(
                        found->second.offset_y
                    ),
                }
            )) {
            return false;
        }
    }
    return render_legacy_sprite(renderer, sprite, ground);
}

[[maybe_unused]] bool render_legacy_building_animation_frame(
    SDL_Renderer* renderer,
    const LegacyAnimation& animation,
    std::size_t frame,
    SDL_FPoint ground
) {
    if (frame >= animation.frames.size()) return false;
    if (!animation.shadow_frames.empty()) {
        const std::size_t shadow_frame =
            animation.shadow_frames_per_angle == 1
            ? 0U : frame;
        if (shadow_frame >= animation.shadow_frames.size() ||
            !render_legacy_sprite(
                renderer,
                animation.shadow_frames[shadow_frame],
                {
                    ground.x + static_cast<float>(
                        animation.shadow_offset_x
                    ),
                    ground.y + static_cast<float>(
                        animation.shadow_offset_y
                    ),
                }
            )) {
            return false;
        }
    }
    return render_legacy_sprite(
        renderer, animation.frames[frame], ground
    );
}

bool render_legacy_composite(
    SDL_Renderer* renderer,
    const LegacyComposite& composite,
    SDL_FPoint ground
) {
    if (composite.parts.empty()) {
        return false;
    }
    for (const LegacyCompositePart& part : composite.parts) {
        if (!render_legacy_sprite(
                renderer,
                part.sprite,
                {
                    ground.x + static_cast<float>(part.offset_x),
                    ground.y + static_cast<float>(part.offset_y),
                }
            )) {
            return false;
        }
    }
    return true;
}

bool render_legacy_animated_composite(
    SDL_Renderer* renderer,
    const LegacyAnimatedComposite& composite,
    SDL_FPoint ground,
    TilePosition previous,
    TilePosition current,
    std::uint64_t animation_tick,
    bool active
) {
    if (composite.parts.empty()) {
        return false;
    }
    const int dx = std::clamp(current.x - previous.x, -1, 1);
    const int dy = std::clamp(current.y - previous.y, -1, 1);
    int direction = 0;
    if (dx == -1 && dy == 1) direction = 1;
    else if (dx == -1 && dy == 0) direction = 2;
    else if (dx == -1 && dy == -1) direction = 3;
    else if (dx == 0 && dy == -1) direction = 4;
    else if (dx == 1 && dy == -1) direction = 5;
    else if (dx == 1 && dy == 0) direction = 6;
    else if (dx == 1 && dy == 1) direction = 7;

    for (const LegacyAnimatedCompositePart& part : composite.parts) {
        const LegacyAnimation& animation = part.animation;
        if (animation.frames.empty() ||
            animation.frames_per_angle == 0) {
            return false;
        }
        const std::size_t stored_angles =
            animation.frames.size() / animation.frames_per_angle;
        if (stored_angles == 0) {
            return false;
        }
        int angle =
            part.display_angle >= 0
            ? part.display_angle
            : (part.angle_count >= 16 ? direction * 2 : direction);
        angle %= std::max(part.angle_count, 1);
        bool flip_horizontal = false;
        if (static_cast<std::size_t>(angle) >= stored_angles) {
            const int mirrored = part.angle_count - angle;
            if (mirrored >= 0 &&
                static_cast<std::size_t>(mirrored) < stored_angles) {
                angle = mirrored;
                flip_horizontal = true;
            } else {
                angle %= static_cast<int>(stored_angles);
            }
        }
        const std::size_t action_frame =
            active && animation.frames_per_angle > 1
            ? static_cast<std::size_t>(animation_tick / 2) %
                animation.frames_per_angle
            : 0U;
        const std::size_t frame_index =
            static_cast<std::size_t>(angle) *
                animation.frames_per_angle +
            action_frame;
        if (frame_index >= animation.frames.size() ||
            !render_legacy_sprite(
                renderer,
                animation.frames[frame_index],
                {
                    ground.x + static_cast<float>(part.offset_x),
                    ground.y + static_cast<float>(part.offset_y),
                },
                true,
                flip_horizontal
            )) {
            return false;
        }
    }
    return true;
}

[[maybe_unused]] bool render_legacy_construction_composite(
    SDL_Renderer* renderer,
    const LegacyAnimatedComposite& composite,
    SDL_FPoint ground,
    int elapsed,
    int total
) {
    if (composite.parts.empty() ||
        composite.parts.front().animation.frames_per_angle == 0) {
        return false;
    }
    const std::size_t frames =
        composite.parts.front().animation.frames_per_angle;
    if (!std::ranges::all_of(
            composite.parts,
            [frames](const LegacyAnimatedCompositePart& part) {
                return part.animation.frames_per_angle == frames;
            }
        )) {
        return false;
    }
    const std::size_t frame = std::min(
        static_cast<std::size_t>(
            std::max(elapsed, 0) *
            static_cast<int>(frames) /
            std::max(total, 1)
        ),
        frames - 1
    );
    return render_legacy_animated_composite(
        renderer,
        composite,
        ground,
        {},
        {},
        static_cast<std::uint64_t>(frame * 2),
        true
    );
}

bool render_legacy_footprint_construction(
    SDL_Renderer* renderer,
    const LegacyAnimatedComposite& composite,
    SDL_FPoint ground,
    int footprint_size
) {
    if (composite.parts.empty()) return false;
    for (const LegacyAnimatedCompositePart& part : composite.parts) {
        const LegacyAnimation& animation = part.animation;
        if (animation.frames.empty()) return false;
        const std::size_t frame = std::min(
            static_cast<std::size_t>(std::max(footprint_size - 1, 0)),
            animation.frames.size() - 1
        );
        if (!render_legacy_sprite(
                renderer,
                animation.frames[frame],
                {
                    ground.x + static_cast<float>(part.offset_x),
                    ground.y + static_cast<float>(part.offset_y),
                },
                true
            )) {
            return false;
        }
    }
    return true;
}

bool render_legacy_animation(
    SDL_Renderer* renderer,
    const LegacyAnimation& animation,
    SDL_FPoint ground,
    TilePosition previous,
    TilePosition current,
    std::uint64_t animation_tick,
    bool active
) {
    if (animation.frames.empty() || animation.frames_per_angle == 0) {
        return false;
    }
    const int dx = std::clamp(current.x - previous.x, -1, 1);
    const int dy = std::clamp(current.y - previous.y, -1, 1);
    int direction = 0;
    if (dx == -1 && dy == 1) {
        direction = 1;
    } else if (dx == -1 && dy == 0) {
        direction = 2;
    } else if (dx == -1 && dy == -1) {
        direction = 3;
    } else if (dx == 0 && dy == -1) {
        direction = 4;
    } else if (dx == 1 && dy == -1) {
        direction = 5;
    } else if (dx == 1 && dy == 0) {
        direction = 6;
    } else if (dx == 1 && dy == 1) {
        direction = 7;
    }
    const bool flip = direction > 4;
    const std::size_t stored_angle = flip
        ? static_cast<std::size_t>(8 - direction)
        : static_cast<std::size_t>(direction);
    const std::size_t available_angles =
        animation.angle_offsets.empty()
        ? animation.frames.size() / animation.frames_per_angle
        : animation.angle_offsets.size();
    if (available_angles == 0) {
        return false;
    }
    const std::size_t angle =
        std::min(stored_angle, available_angles - 1);
    const std::size_t angle_frames =
        animation.angle_frame_counts.empty()
        ? animation.frames_per_angle
        : animation.angle_frame_counts[angle];
    if (angle_frames == 0) {
        return false;
    }
    const std::size_t action_frame = active
        ? static_cast<std::size_t>(animation_tick) % angle_frames
        : 0;
    const std::size_t index =
        (animation.angle_offsets.empty()
            ? angle * animation.frames_per_angle
            : animation.angle_offsets[angle]) +
        action_frame;
    if (index >= animation.frames.size()) {
        return false;
    }
    if (!animation.shadow_frames.empty() &&
        animation.shadow_frames_per_angle > 0) {
        const std::size_t stored_shadow_angles =
            animation.shadow_frames.size() /
            animation.shadow_frames_per_angle;
        if (stored_shadow_angles > 0) {
            int shadow_angle =
                animation.shadow_display_angle >= 0
                ? animation.shadow_display_angle
                : animation.shadow_angle_count >= 16
                    ? direction * 2 : direction;
            shadow_angle %= std::max(
                animation.shadow_angle_count, 1
            );
            bool shadow_flip = false;
            if (static_cast<std::size_t>(shadow_angle) >=
                stored_shadow_angles) {
                const int mirrored =
                    animation.shadow_angle_count - shadow_angle;
                if (mirrored >= 0 &&
                    static_cast<std::size_t>(mirrored) <
                        stored_shadow_angles) {
                    shadow_angle = mirrored;
                    shadow_flip = true;
                } else {
                    shadow_angle %= static_cast<int>(
                        stored_shadow_angles
                    );
                }
            }
            const std::size_t shadow_action =
                active &&
                    animation.shadow_frames_per_angle > 1
                ? static_cast<std::size_t>(animation_tick) %
                    animation.shadow_frames_per_angle
                : 0U;
            const std::size_t shadow_index =
                static_cast<std::size_t>(shadow_angle) *
                    animation.shadow_frames_per_angle +
                shadow_action;
            if (shadow_index < animation.shadow_frames.size()) {
                (void)render_legacy_sprite(
                    renderer,
                    animation.shadow_frames[shadow_index],
                    {
                        ground.x +
                            static_cast<float>(
                                animation.shadow_offset_x),
                        ground.y +
                            static_cast<float>(
                                animation.shadow_offset_y),
                    },
                    true,
                    shadow_flip
                );
            }
        }
    }
    return render_legacy_sprite(
        renderer,
        animation.frames[index],
        ground,
        true,
        flip
    );
}

bool render_exact_projectile_animation(
    SDL_Renderer* renderer,
    const LegacyAnimation& animation,
    SDL_FPoint ground,
    TilePosition origin,
    TilePosition destination,
    std::uint64_t animation_tick,
    std::int16_t angle_count
) {
    const auto body = select_projectile_frame(
        origin,
        destination,
        static_cast<std::int16_t>(
            animation.frames_per_angle
        ),
        angle_count,
        animation.frames.size(),
        animation_tick
    );
    if (!body || body->frame_index >= animation.frames.size()) {
        return false;
    }
    if (!animation.shadow_frames.empty()) {
        const auto shadow = select_projectile_frame(
            origin,
            destination,
            static_cast<std::int16_t>(
                animation.shadow_frames_per_angle
            ),
            static_cast<std::int16_t>(
                animation.shadow_angle_count
            ),
            animation.shadow_frames.size(),
            animation_tick
        );
        if (!shadow ||
            shadow->frame_index >= animation.shadow_frames.size()) {
            return false;
        }
        const LegacySprite& sprite =
            animation.shadow_frames[shadow->frame_index];
        SDL_FPoint shadow_ground{
            ground.x + static_cast<float>(
                animation.shadow_offset_x
            ),
            ground.y + static_cast<float>(
                animation.shadow_offset_y
            ),
        };
        if (!render_legacy_sprite(
                renderer,
                sprite,
                shadow_ground,
                true,
                shadow->flip_horizontal
            )) {
            return false;
        }
    }
    const LegacySprite& sprite =
        animation.frames[body->frame_index];
    return render_legacy_sprite(
        renderer,
        sprite,
        ground,
        true,
        body->flip_horizontal
    );
}

const PlayerLegacySprites* legacy_sprites_for(UnitKind kind) {
    if (kind == UnitKind::villager) {
        return &active_legacy_sprites.villager_animation;
    }
    if (kind == UnitKind::sheep) {
        return &active_legacy_sprites.sheep_animation;
    }
    if (kind == UnitKind::deer) {
        return &active_legacy_sprites.deer_animation;
    }
    if (kind == UnitKind::boar) {
        return &active_legacy_sprites.boar_animation;
    }
    const auto mapped = active_legacy_sprites.military.find(kind);
    return mapped == active_legacy_sprites.military.end()
        ? nullptr
        : &mapped->second;
}

const LegacyAnimation* legacy_action_for(
    const Simulation& simulation,
    const Unit& unit,
    bool moving
) {
    const auto for_player = [&unit](
        const PlayerLegacySprites& sprites
    ) -> const LegacyAnimation* {
        if (const LegacyAnimation* owned = sprites.owner(unit.owner)) {
            return owned;
        }
        if (unit.owner == EntityOwner{Player::neutral} &&
            (unit.kind == UnitKind::sheep ||
             unit.kind == UnitKind::deer ||
             unit.kind == UnitKind::boar)) {
            return &sprites.blue;
        }
        return nullptr;
    };
    if (unit.kind == UnitKind::monk &&
        unit.conversion_target_id != 0) {
        return for_player(active_legacy_sprites.monk_convert);
    }
    if (unit.kind == UnitKind::missionary) {
        if (unit.healing_target_id != 0) {
            return for_player(active_legacy_sprites.missionary_heal);
        }
        if (unit.conversion_target_id != 0) {
            const auto found =
                active_legacy_sprites.attack.find(unit.kind);
            if (found != active_legacy_sprites.attack.end()) {
                return for_player(found->second);
            }
        }
    }
    if (unit.kind == UnitKind::villager) {
        if (render_action_detail_for(simulation, unit) ==
            RenderActionDetail::animal_resource) {
            return for_player(active_legacy_sprites.villager_gather);
        }
    }
    if (unit.trebuchet_transform_ticks_remaining > 0) {
        const PlayerLegacySprites& transform =
            unit.kind == UnitKind::packed_trebuchet
            ? active_legacy_sprites.packed_trebuchet_transform
            : active_legacy_sprites.unpacked_trebuchet_transform;
        return for_player(transform);
    }
    if (unit.attack_target_id != 0 ||
        unit.attacking_ground) {
        const auto found =
            active_legacy_sprites.attack.find(unit.kind);
        if (found != active_legacy_sprites.attack.end()) {
            return for_player(found->second);
        }
    }
    if (moving || unit.moving) {
        const auto found =
            active_legacy_sprites.movement.find(unit.kind);
        if (found != active_legacy_sprites.movement.end()) {
            return for_player(found->second);
        }
    }
    const PlayerLegacySprites* idle = legacy_sprites_for(unit.kind);
    return idle == nullptr ? nullptr : for_player(*idle);
}

void render_water_detail(
    SDL_Renderer* renderer,
    const Simulation& simulation,
    TilePosition position,
    SDL_FPoint top,
    float animation_tick
) {
    const float phase = animation_tick * 0.55F +
        static_cast<float>(position.x * 3 + position.y * 5);
    const float drift = std::sin(phase) * 4.0F;
    const float second_drift = std::sin(phase + 2.1F) * 3.0F;
    set_color(renderer, {119, 170, 211, 255});
    SDL_RenderLine(
        renderer,
        top.x - 13.0F + drift,
        top.y + 11.0F,
        top.x - 3.0F + drift,
        top.y + 11.0F
    );
    SDL_RenderLine(
        renderer,
        top.x + 2.0F + second_drift,
        top.y + 22.0F,
        top.x + 12.0F + second_drift,
        top.y + 22.0F
    );

    struct ShoreEdge {
        TilePosition neighbor;
        SDL_FPoint first;
        SDL_FPoint second;
    };
    const std::array<ShoreEdge, 4> edges{{
        {{position.x - 1, position.y},
         {top.x, top.y}, {top.x - half_tile_width, top.y + half_tile_height}},
        {{position.x + 1, position.y},
         {top.x + half_tile_width, top.y + half_tile_height},
         {top.x, top.y + tile_height}},
        {{position.x, position.y - 1},
         {top.x, top.y}, {top.x + half_tile_width, top.y + half_tile_height}},
        {{position.x, position.y + 1},
         {top.x - half_tile_width, top.y + half_tile_height},
         {top.x, top.y + tile_height}},
    }};
    for (const ShoreEdge& edge : edges) {
        if (!simulation.map().contains(edge.neighbor) ||
            !simulation.is_explored_to_controller(active_view_player, edge.neighbor) ||
            is_water_surface(
                simulation.map().terrain_at(edge.neighbor)
            )) {
            continue;
        }
        set_color(renderer, {194, 183, 128, 255});
        SDL_RenderLine(
            renderer,
            edge.first.x,
            edge.first.y,
            edge.second.x,
            edge.second.y
        );
        set_color(renderer, {174, 210, 222, 255});
        SDL_RenderLine(
            renderer,
            edge.first.x,
            edge.first.y + 1.0F,
            edge.second.x,
            edge.second.y + 1.0F
        );
    }
}

void fill_triangle(
    SDL_Renderer* renderer,
    SDL_FPoint first,
    SDL_FPoint second,
    SDL_FPoint third,
    SDL_Color color
) {
    const SDL_FColor vertex_color{
        color.r / 255.0F,
        color.g / 255.0F,
        color.b / 255.0F,
        color.a / 255.0F,
    };
    const std::array<SDL_Vertex, 3> vertices{{
        {first, vertex_color, {}},
        {second, vertex_color, {}},
        {third, vertex_color, {}},
    }};
    constexpr std::array<int, 3> indices{{0, 1, 2}};
    SDL_RenderGeometry(
        renderer,
        nullptr,
        vertices.data(),
        static_cast<int>(vertices.size()),
        indices.data(),
        static_cast<int>(indices.size())
    );
}

void outline_diamond(
    SDL_Renderer* renderer,
    SDL_FPoint top,
    SDL_Color color
) {
    set_color(renderer, color);
    const std::array<SDL_FPoint, 5> points{{
        {top.x, top.y},
        {top.x + half_tile_width, top.y + half_tile_height},
        {top.x, top.y + tile_height},
        {top.x - half_tile_width, top.y + half_tile_height},
        {top.x, top.y},
    }};
    SDL_RenderLines(renderer, points.data(), static_cast<int>(points.size()));
}

void render_procedural_terrain_transitions(
    SDL_Renderer* renderer,
    const Simulation& simulation,
    TilePosition position,
    SDL_FPoint top,
    SDL_Color center_color
) {
    struct Edge {
        TilePosition offset;
        SDL_FPoint first;
        SDL_FPoint second;
    };
    const std::array<Edge, 4> edges{{
        {{0, -1}, {top.x, top.y},
         {top.x + half_tile_width, top.y + half_tile_height}},
        {{1, 0}, {top.x + half_tile_width, top.y + half_tile_height},
         {top.x, top.y + tile_height}},
        {{0, 1}, {top.x - half_tile_width, top.y + half_tile_height},
         {top.x, top.y + tile_height}},
        {{-1, 0}, {top.x, top.y},
         {top.x - half_tile_width, top.y + half_tile_height}},
    }};
    const auto center_evidence = terrain_blend_evidence(
        simulation.map().terrain_at(position)
    );
    if (!center_evidence) return;
    const SDL_FPoint center{
        top.x, top.y + half_tile_height
    };
    for (const Edge& edge : edges) {
        const TilePosition neighbor{
            position.x + edge.offset.x,
            position.y + edge.offset.y,
        };
        if (!simulation.map().contains(neighbor) ||
            (active_settings.fog &&
             !simulation.is_explored_to_controller(
                 active_view_player, neighbor
             ))) {
            continue;
        }
        const auto neighbor_evidence = terrain_blend_evidence(
            simulation.map().terrain_at(neighbor)
        );
        if (!neighbor_evidence ||
            neighbor_evidence->terrain == center_evidence->terrain) {
            continue;
        }
        SDL_Color neighbor_color = terrain_color(
            simulation.map().terrain_at(neighbor)
        );
        const int variation =
            (neighbor.x * 17 + neighbor.y * 31) % 7 - 3;
        neighbor_color = shade_color(neighbor_color, variation);
        const auto band = procedural_transition_band(
            {center_color.r, center_color.g, center_color.b},
            {neighbor_color.r, neighbor_color.g, neighbor_color.b}
        );
        for (std::size_t index = 0; index < band.size(); ++index) {
            const float inset =
                static_cast<float>(index) /
                static_cast<float>(band.size() * 5);
            const auto toward_center = [center, inset](SDL_FPoint point) {
                return SDL_FPoint{
                    point.x + (center.x - point.x) * inset,
                    point.y + (center.y - point.y) * inset,
                };
            };
            const SDL_FPoint first = toward_center(edge.first);
            const SDL_FPoint second = toward_center(edge.second);
            set_color(renderer, {
                band[index].red,
                band[index].green,
                band[index].blue,
                255,
            });
            SDL_RenderLine(
                renderer, first.x, first.y, second.x, second.y
            );
        }
    }
}

void render_beveled_panel(
    SDL_Renderer* renderer,
    const SDL_FRect& rect,
    SDL_Color fill
) {
    set_color(renderer, {19, 15, 12, 255});
    SDL_RenderFillRect(renderer, &rect);

    const SDL_FRect inset{
        rect.x + 2.0F,
        rect.y + 2.0F,
        std::max(0.0F, rect.w - 4.0F),
        std::max(0.0F, rect.h - 4.0F),
    };
    set_color(renderer, fill);
    SDL_RenderFillRect(renderer, &inset);

    set_color(renderer, {184, 153, 91, 255});
    SDL_RenderLine(renderer, rect.x, rect.y, rect.x + rect.w, rect.y);
    SDL_RenderLine(renderer, rect.x, rect.y, rect.x, rect.y + rect.h);
    set_color(renderer, {63, 43, 28, 255});
    SDL_RenderLine(
        renderer,
        rect.x,
        rect.y + rect.h,
        rect.x + rect.w,
        rect.y + rect.h
    );
    SDL_RenderLine(
        renderer,
        rect.x + rect.w,
        rect.y,
        rect.x + rect.w,
        rect.y + rect.h
    );
}

void render_tree(
    SDL_Renderer* renderer,
    const Simulation& simulation,
    SDL_FPoint top,
    bool visible
) {
    if (render_legacy_sprite(
            renderer,
            active_legacy_sprites.tree,
            {top.x, top.y + half_tile_height},
            visible
        )) {
        return;
    }
    record_resource_procedural_fallback(
        simulation, ResourceRenderKind::forest, 0
    );
    const SDL_FRect trunk{
        top.x - 2.5F,
        top.y + 8.0F,
        5.0F,
        19.0F,
    };
    set_color(
        renderer,
        visible
            ? SDL_Color{83, 57, 31, 255}
            : SDL_Color{35, 31, 25, 255}
    );
    SDL_RenderFillRect(renderer, &trunk);

    const SDL_FColor canopy_color{
        (visible ? 32.0F : 18.0F) / 255.0F,
        (visible ? 82.0F : 37.0F) / 255.0F,
        (visible ? 44.0F : 25.0F) / 255.0F,
        1.0F,
    };
    const std::array<SDL_Vertex, 5> canopy{{
        {{top.x, top.y - 13.0F}, canopy_color, {}},
        {{top.x + 13.0F, top.y + 2.0F}, canopy_color, {}},
        {{top.x + 9.0F, top.y + 17.0F}, canopy_color, {}},
        {{top.x - 9.0F, top.y + 17.0F}, canopy_color, {}},
        {{top.x - 13.0F, top.y + 2.0F}, canopy_color, {}},
    }};
    constexpr std::array<int, 9> indices{{0, 1, 2, 0, 2, 3, 0, 3, 4}};
    SDL_RenderGeometry(
        renderer,
        nullptr,
        canopy.data(),
        static_cast<int>(canopy.size()),
        indices.data(),
        static_cast<int>(indices.size())
    );
}

void render_resource_node(
    SDL_Renderer* renderer,
    const Simulation& simulation,
    SDL_FPoint top,
    Terrain terrain,
    bool visible,
    int remaining
) {
    if (terrain == Terrain::fish) {
        if (render_legacy_sprite(
                renderer,
                active_legacy_sprites.fish,
                {top.x, top.y + half_tile_height},
                visible
            )) {
            return;
        }
        record_resource_procedural_fallback(
            simulation, ResourceRenderKind::fish, 0
        );
        set_color(
            renderer,
            visible
                ? SDL_Color{202, 218, 224, 255}
                : SDL_Color{55, 70, 78, 255}
        );
        for (float offset : {-8.0F, 7.0F}) {
            SDL_RenderLine(
                renderer,
                top.x + offset - 5.0F,
                top.y + half_tile_height,
                top.x + offset + 5.0F,
                top.y + half_tile_height
            );
            SDL_RenderLine(
                renderer,
                top.x + offset + 5.0F,
                top.y + half_tile_height,
                top.x + offset + 9.0F,
                top.y + half_tile_height - 4.0F
            );
            SDL_RenderLine(
                renderer,
                top.x + offset + 5.0F,
                top.y + half_tile_height,
                top.x + offset + 9.0F,
                top.y + half_tile_height + 4.0F
            );
        }
        return;
    }
    if (terrain == Terrain::gold_mine) {
        const std::size_t frame = static_cast<std::size_t>(std::clamp(
            render_resource_frame(
                ResourceRenderKind::gold_mine, remaining
            ),
            0,
            6
        ));
        if (render_legacy_sprite(
                renderer,
                active_legacy_sprites.gold_states[frame],
                {top.x, top.y + half_tile_height},
                visible
            )) {
            return;
        }
        record_resource_procedural_fallback(
            simulation,
            ResourceRenderKind::gold_mine,
            static_cast<int>(frame)
        );
    }
    if (terrain == Terrain::berry_bush) {
        const std::size_t frame = static_cast<std::size_t>(std::clamp(
            render_resource_frame(
                ResourceRenderKind::berry_bush, remaining
            ),
            0,
            3
        ));
        if (render_legacy_sprite(
                renderer,
                active_legacy_sprites.berry_states[frame],
                {top.x, top.y + half_tile_height},
                visible
            )) {
            return;
        }
        record_resource_procedural_fallback(
            simulation,
            ResourceRenderKind::berry_bush,
            static_cast<int>(frame)
        );
        set_color(
            renderer,
            visible
                ? SDL_Color{115, 45, 120, 255}
                : SDL_Color{45, 28, 48, 255}
        );
        for (int offset : {-8, 0, 8}) {
            const SDL_FRect berry{
                top.x + static_cast<float>(offset) - 3.0F,
                top.y + 9.0F + std::abs(offset) / 3.0F,
                7.0F,
                7.0F,
            };
            SDL_RenderFillRect(renderer, &berry);
        }
        return;
    }
    if (terrain == Terrain::stone_mine) {
        const std::size_t frame = static_cast<std::size_t>(std::clamp(
            render_resource_frame(
                ResourceRenderKind::stone_mine, remaining
            ),
            0,
            6
        ));
        if (render_legacy_sprite(
                renderer,
                active_legacy_sprites.stone_states[frame],
                {top.x, top.y + half_tile_height},
                visible
            )) {
            return;
        }
        record_resource_procedural_fallback(
            simulation,
            ResourceRenderKind::stone_mine,
            static_cast<int>(frame)
        );
    }

    const SDL_Color color = terrain == Terrain::gold_mine
        ? (visible
            ? SDL_Color{225, 185, 55, 255}
            : SDL_Color{70, 61, 35, 255})
        : (visible
            ? SDL_Color{145, 150, 150, 255}
            : SDL_Color{52, 55, 55, 255});
    set_color(renderer, color);
    const SDL_FRect base{top.x - 12.0F, top.y + 7.0F, 24.0F, 13.0F};
    const SDL_FRect peak{top.x - 6.0F, top.y, 12.0F, 10.0F};
    SDL_RenderFillRect(renderer, &base);
    SDL_RenderFillRect(renderer, &peak);
}

void render_health_bar(
    SDL_Renderer* renderer,
    float center_x,
    float top,
    int current,
    int maximum
) {
    const auto green_width = exact_health_fill_pixels(current, maximum);
    if (!green_width) {
        return;
    }
    constexpr float width = 25.0F;
    const SDL_FRect background{center_x - 12.0F, top, width, 2.0F};
    set_color(renderer, SDL_Color{255, 0, 0, 255});
    SDL_RenderFillRect(renderer, &background);

    const SDL_FRect health{
        background.x,
        background.y,
        static_cast<float>(*green_width),
        background.h,
    };
    set_color(renderer, SDL_Color{0, 255, 0, 255});
    SDL_RenderFillRect(renderer, &health);
}

bool is_defensive_garrison_building(BuildingKind kind) {
    return kind == BuildingKind::town_center ||
           kind == BuildingKind::castle ||
           kind == BuildingKind::watch_tower ||
           kind == BuildingKind::bombard_tower;
}

void render_garrison_presentation(
    SDL_Renderer* renderer,
    const Simulation& simulation,
    const Building& building
) {
    if (!building.completed() ||
        !is_defensive_garrison_building(building.kind)) {
        return;
    }
    const int occupants = simulation.garrison_count(building.id);
    const BuildingRules& rules = rules_for(building.kind);
    const int active_volley = static_cast<int>(std::ranges::count_if(
        simulation.projectiles(),
        [&building, &rules](const Projectile& projectile) {
            return projectile.source_is_building &&
                projectile.source_building_kind == building.kind &&
                projectile.origin.x >= building.position.x &&
                projectile.origin.y >= building.position.y &&
                projectile.origin.x <
                    building.position.x + rules.footprint_width &&
                projectile.origin.y <
                    building.position.y + rules.footprint_height;
        }
    ));
    if (occupants == 0 && active_volley == 0) {
        return;
    }
    const SDL_FPoint top = building_top(building);
    const SDL_FRect badge{
        top.x - 25.0F,
        top.y - 38.0F,
        active_volley > 0 ? 50.0F : 38.0F,
        13.0F,
    };
    set_color(renderer, {18, 16, 13, 224});
    SDL_RenderFillRect(renderer, &badge);
    set_color(
        renderer,
        building.owner == Player::blue
            ? SDL_Color{92, 151, 235, 255}
            : SDL_Color{220, 92, 76, 255}
    );
    SDL_RenderRect(renderer, &badge);
    std::ostringstream text;
    if (building.owner == Player::blue) {
        text << "G" << occupants;
    }
    if (active_volley > 0) {
        if (building.owner == Player::blue) text << ' ';
        text << "V" << active_volley;
    }
    SDL_RenderDebugText(
        renderer,
        badge.x + 4.0F,
        badge.y + 3.0F,
        text.str().c_str()
    );
}

void render_attack_range_diagnostic(
    SDL_Renderer* renderer,
    const Simulation& simulation
) {
    const char* requested = SDL_getenv("AOE_RANGE_DIAGNOSTIC");
    if (requested == nullptr ||
        (std::string_view{requested} != "1" &&
         std::string_view{requested} != "true") ||
        simulation.selected_units().empty()) {
        return;
    }
    const EntityId selected = simulation.selected_units().front();
    const auto found = std::ranges::find_if(
        simulation.units(),
        [selected](const Unit& unit) { return unit.id == selected; }
    );
    if (found == simulation.units().end()) return;
    const int range = simulation.effective_attack_range(*found);
    if (range <= 0) return;

    for (int dy = -range; dy <= range; ++dy) {
        for (int dx = -range; dx <= range; ++dx) {
            const TilePosition tile{
                found->position.x + dx,
                found->position.y + dy,
            };
            if (!simulation.map().contains(tile)) continue;
            const int manhattan = std::abs(dx) + std::abs(dy);
            const int squared = dx * dx + dy * dy;
            const bool in_actual_range = squared <= range * range;
            const bool on_actual_boundary = in_actual_range &&
                ((dx + 1) * (dx + 1) + dy * dy > range * range ||
                 (dx - 1) * (dx - 1) + dy * dy > range * range ||
                 dx * dx + (dy + 1) * (dy + 1) > range * range ||
                 dx * dx + (dy - 1) * (dy - 1) > range * range);
            if (on_actual_boundary) {
                outline_diamond(
                    renderer,
                    tile_top(tile),
                    {75, 205, 235, 220}
                );
            } else if (in_actual_range && manhattan > range) {
                outline_diamond(
                    renderer,
                    tile_top(tile),
                    {90, 220, 120, 220}
                );
            }
        }
    }
}

std::uint64_t building_damage_elapsed(
    EntityId building,
    std::optional<std::int16_t> graphic,
    std::uint64_t tick
) {
    if (!graphic) {
        active_building_damage_animation.erase(building);
        return 0;
    }
    auto [found, inserted] = active_building_damage_animation.try_emplace(
        building,
        std::pair{*graphic, tick}
    );
    if (!inserted && found->second.first != *graphic) {
        found->second = {*graphic, tick};
    }
    return tick - found->second.second;
}

void render_building(
    SDL_Renderer* renderer,
    const Simulation& simulation,
    const Building& building
) {
    const BuildingRules& rules = rules_for(building.kind);
    const auto owner_slot = building.owner.slot_index();
    const bool legacy_two_player_owner =
        owner_slot && *owner_slot < 2;
    const int maximum_hit_points =
        simulation.maximum_hit_points(building);
    const bool fortified =
        simulation.has_technology(
            building.owner, Technology::fortified_wall
        ) &&
        (building.kind == BuildingKind::stone_wall ||
         building.kind == BuildingKind::stone_gate_x ||
         building.kind == BuildingKind::stone_gate_y);
    const bool keep =
        building.kind == BuildingKind::watch_tower &&
        simulation.has_technology(
            building.owner, Technology::keep
        );
    const bool guard_tower =
        building.kind == BuildingKind::watch_tower &&
        simulation.has_technology(
            building.owner, Technology::guard_tower
        ) && !keep;
    const SDL_FPoint top = building_top(building);
    const auto damage_records = canonical_building_damage_records(
        building.kind,
        simulation.civilization(building.owner)
    );
    const auto damage_index = select_building_damage_record(
        building.hit_points,
        maximum_hit_points,
        damage_records
    );
    const std::uint64_t damage_elapsed = building_damage_elapsed(
        building.id,
        damage_index
            ? std::optional<std::int16_t>{
                  damage_records[*damage_index].graphic_id
              }
            : std::nullopt,
        simulation.tick_number()
    );
    if (damage_index && damage_records[*damage_index].flag == 2) {
        const auto found = active_legacy_sprites.building_damage_graphics.find(
            damage_records[*damage_index].graphic_id
        );
        if (found !=
            active_legacy_sprites.building_damage_graphics.end()) {
            const LegacyAnimatedComposite* composite =
                found->second.owner(building.owner);
            if (composite != nullptr &&
                render_legacy_animated_composite(
                    renderer,
                    *composite,
                    {top.x, top.y + half_tile_height},
                    building.position,
                    building.position,
                    damage_elapsed,
                    true
                )) {
                return;
            }
        }
    }
    if (!building.completed()) {
        const auto found =
            active_legacy_sprites.building_construction_composites.find(
                building.kind
            );
        if (found !=
                active_legacy_sprites
                    .building_construction_composites.end()) {
            const LegacyAnimatedComposite* composite =
                found->second.owner(building.owner);
            if (composite != nullptr &&
                render_legacy_footprint_construction(
                    renderer,
                    *composite,
                    {top.x, top.y + half_tile_height},
                    std::max(
                        rules.footprint_width,
                        rules.footprint_height
                    )
                )) {
                if (simulation.selected_building() == building.id) {
                    for (int y = 0; y < rules.footprint_height; ++y) {
                        for (int x = 0; x < rules.footprint_width; ++x) {
                            outline_diamond(
                                renderer,
                                tile_top({
                                    building.position.x + x,
                                    building.position.y + y,
                                }),
                                {250, 220, 65, 255}
                            );
                        }
                    }
                }
                return;
            }
        }
    }
    if (building.kind == BuildingKind::wonder) {
        if (building.completed()) {
            const auto found =
                active_legacy_sprites.wonder_standing.find(
                    simulation.civilization(building.owner)
                );
            if (found !=
                    active_legacy_sprites.wonder_standing.end()) {
                const LegacyComposite* composite =
                    found->second.owner(building.owner);
                if (composite != nullptr && render_legacy_composite(
                        renderer,
                        *composite,
                        {top.x, top.y + half_tile_height}
                    )) {
                    if (simulation.selected_building() == building.id) {
                        outline_diamond(
                            renderer,
                            tile_top(building.position),
                            {250, 220, 65, 255}
                        );
                    }
                    return;
                }
            }
        }
    }
    if (building.kind == BuildingKind::fish_trap) {
        const LegacyAnimation* animation =
            (building.completed()
                ? active_legacy_sprites.fish_trap_standing
                : active_legacy_sprites.fish_trap_construction
            ).owner(building.owner);
        const bool rendered = animation != nullptr && render_legacy_animation(
            renderer,
            *animation,
            {top.x, top.y + half_tile_height},
            building.position,
            building.position,
            simulation.tick_number() + building.id,
            false
        );
        if (rendered) {
            if (simulation.selected_building() == building.id) {
                outline_diamond(
                    renderer,
                    tile_top(building.position),
                    {250, 220, 65, 255}
                );
            }
            return;
        }
    }
    if (building.kind == BuildingKind::bombard_tower &&
        building.completed()) {
        const auto owner = building.owner.slot_index();
        const LegacySprite* sprite =
            owner == 0
            ? &active_legacy_sprites.bombard_tower_standing_blue
            : owner == 1
                ? &active_legacy_sprites.bombard_tower_standing_red
                : owner && *owner < 8
                    ? [&]() -> const LegacySprite* {
                          const auto found = active_legacy_sprites
                              .direct_building_player_variants.find({
                                  building.kind,
                                  static_cast<std::size_t>(
                                      render_building_visual_age(
                                          building.kind,
                                          simulation.age(building.owner)
                                      )
                                  ),
                                  static_cast<std::size_t>(
                                      render_architecture_family(
                                          simulation.civilization(
                                              building.owner
                                          )
                                      )
                                  ),
                                  *owner,
                              });
                          return found == active_legacy_sprites
                                  .direct_building_player_variants.end()
                              ? nullptr
                              : &found->second;
                      }()
                    : nullptr;
        if (sprite != nullptr && render_legacy_building_sprite(
                renderer, *sprite, {top.x, top.y + half_tile_height}
            )) {
            if (building.hit_points < maximum_hit_points ||
                simulation.selected_building() == building.id) {
                render_health_bar(
                    renderer, top.x, top.y - 72.0F,
                    building.hit_points, maximum_hit_points
                );
            }
            if (simulation.selected_building() == building.id) {
                outline_diamond(renderer, tile_top(building.position),
                                {250, 220, 65, 255});
            }
            return;
        }
    }
    if (building.kind == BuildingKind::palisade_wall &&
        building.completed() &&
        building.hit_points >= maximum_hit_points) {
        const std::size_t frame = static_cast<std::size_t>(
            render_building_topology_frame(simulation, building)
        );
        const SDL_FPoint ground{
            top.x, top.y + half_tile_height
        };
        const auto owner_slot = building.owner.slot_index();
        const LegacySprite* wall = owner_slot && *owner_slot < 8
            ? &active_legacy_sprites
                   .palisade_wall_by_owner[*owner_slot][frame]
            : nullptr;
        bool rendered = wall != nullptr && render_legacy_sprite(
            renderer,
            active_legacy_sprites.palisade_wall_shadow[frame],
            ground
        );
        rendered = wall != nullptr && render_legacy_sprite(
            renderer,
            *wall,
            ground
        ) && rendered;
        if (frame == 2U) {
            const LegacyAnimation* flags =
                active_legacy_sprites.palisade_wall_flags.owner(
                    building.owner
                );
            rendered = flags != nullptr && render_legacy_animation(
                renderer,
                *flags,
                ground,
                {building.position.x + 1, building.position.y},
                building.position,
                simulation.tick_number() + building.id,
                true
            ) && rendered;
        }
        if (rendered) {
            if (simulation.selected_building() == building.id) {
                outline_diamond(
                    renderer,
                    tile_top(building.position),
                    {250, 220, 65, 255}
                );
            }
            return;
        }
    }
    if (building.kind == BuildingKind::stone_wall &&
        !building.completed()) {
        const std::size_t family = static_cast<std::size_t>(
            render_building_architecture_family(
                building.kind,
                simulation.civilization(building.owner)
            )
        );
        const LegacyComposite* composite =
            active_legacy_sprites
                .stone_wall_construction[family]
                .owner(building.owner);
        if (composite != nullptr && render_legacy_composite(
                renderer,
                *composite,
                {top.x, top.y + half_tile_height}
            )) {
            return;
        }
    }
    if (gate_construction_set(building.kind) != nullptr &&
        !building.completed()) {
        const Civilization civilization =
            simulation.civilization(building.owner);
        const std::size_t family = static_cast<std::size_t>(
            civilization == Civilization::aztecs ||
                civilization == Civilization::mayans
            ? 4
            : render_architecture_family(civilization)
        );
        const auto found =
            active_legacy_sprites.gate_construction.find(
                building.kind
            );
        const LegacyComposite* composite =
            found == active_legacy_sprites.gate_construction.end()
            ? nullptr
            : found->second[family].owner(building.owner);
        if (composite != nullptr && render_legacy_composite(
                renderer,
                *composite,
                {top.x, top.y + half_tile_height}
            )) {
            return;
        }
    }
    if (building.kind == BuildingKind::stone_wall &&
        building.completed()) {
        const Civilization civilization =
            simulation.civilization(building.owner);
        const std::size_t family = static_cast<std::size_t>(
            render_building_architecture_family(
                building.kind, civilization
            )
        );
        const std::size_t frame = static_cast<std::size_t>(
            render_building_topology_frame(simulation, building)
        );
        const auto slot = building.owner.slot_index();
        const LegacySprite* sprite = slot && *slot < 8
            ? &active_legacy_sprites.stone_wall_by_owner[
                  *slot
              ][family][frame]
            : nullptr;
        if (sprite != nullptr && render_legacy_building_sprite(
                renderer,
                *sprite,
                {top.x, top.y + half_tile_height}
            )) {
            if (building.hit_points < maximum_hit_points ||
                simulation.selected_building() == building.id) {
                render_health_bar(
                    renderer, top.x, top.y - 62.0F,
                    building.hit_points, maximum_hit_points
                );
            }
            if (simulation.selected_building() == building.id) {
                outline_diamond(
                    renderer,
                    tile_top(building.position),
                    {250, 220, 65, 255}
                );
            }
            return;
        }
    }
    const bool exact_composite_kind =
        building.kind == BuildingKind::barracks ||
        building.kind == BuildingKind::mill ||
        building.kind == BuildingKind::archery_range ||
        building.kind == BuildingKind::watch_tower ||
        building.kind == BuildingKind::stable ||
        building.kind == BuildingKind::castle ||
        building.kind == BuildingKind::siege_workshop ||
        building.kind == BuildingKind::dock ||
        building.kind == BuildingKind::outpost ||
        building.kind == BuildingKind::monastery ||
        building.kind == BuildingKind::stone_gate_x ||
        building.kind == BuildingKind::stone_gate_y ||
        building.kind == BuildingKind::palisade_gate_x ||
        building.kind == BuildingKind::palisade_gate_y;
    if (exact_composite_kind && building.completed()) {
        const Civilization civilization =
            simulation.civilization(building.owner);
        const std::size_t family =
            civilization == Civilization::teutons ||
            civilization == Civilization::goths ||
            civilization == Civilization::vikings
            ? 1U
            : civilization == Civilization::byzantines ||
              civilization == Civilization::persians ||
              civilization == Civilization::saracens
                ? 2U
                : civilization == Civilization::japanese ||
                  civilization == Civilization::chinese
                    ? 3U
                    : 0U;
        const std::size_t age = static_cast<std::size_t>(
            render_building_composite_variant(
                building.kind,
                simulation.age(building.owner),
                render_building_upgrade_variant(
                    simulation, building
                )
            )
        );
        const auto found =
            active_legacy_sprites.building_composites.find(building.kind);
        if (found != active_legacy_sprites.building_composites.end()) {
            const PlayerLegacyComposite& players =
                found->second[age][family];
            const LegacyComposite* composite =
                players.owner(building.owner);
            if (composite != nullptr && render_legacy_composite(
                    renderer,
                    *composite,
                    {top.x, top.y + half_tile_height}
                )) {
                if (building.hit_points < maximum_hit_points ||
                    simulation.selected_building() == building.id) {
                    render_health_bar(
                        renderer,
                        top.x,
                        top.y - (
                            building.kind == BuildingKind::town_center
                            ? 82.0F
                            : 58.0F
                        ),
                        building.hit_points,
                        maximum_hit_points
                    );
                }
                if (simulation.selected_building() == building.id) {
                    for (int y = 0; y < rules.footprint_height; ++y) {
                        for (int x = 0; x < rules.footprint_width; ++x) {
                            outline_diamond(
                                renderer,
                                tile_top({
                                    building.position.x + x,
                                    building.position.y + y,
                                }),
                                {250, 220, 65, 255}
                            );
                        }
                    }
                }
                return;
            }
        }
    }
    if (building.completed() &&
        building_direct_slp_set(building.kind) != nullptr) {
        const auto owner = building.owner.slot_index();
        if (owner && *owner >= 2) {
            const std::size_t age = static_cast<std::size_t>(
                render_building_visual_age(
                    building.kind,
                    simulation.age(building.owner)
                )
            );
            const std::size_t family = static_cast<std::size_t>(
                render_architecture_family(
                    simulation.civilization(building.owner)
                )
            );
            const auto found =
                active_legacy_sprites.direct_building_player_variants.find(
                    {building.kind, age, family, *owner}
                );
            if (found !=
                    active_legacy_sprites
                        .direct_building_player_variants.end() &&
                render_legacy_building_sprite(
                    renderer,
                    found->second,
                    {top.x, top.y + half_tile_height}
                )) {
                if (building.hit_points < maximum_hit_points ||
                    simulation.selected_building() == building.id) {
                    render_health_bar(
                        renderer,
                        top.x,
                        top.y - (
                            building.kind == BuildingKind::market
                            ? 66.0F
                            : 54.0F
                        ),
                        building.hit_points,
                        maximum_hit_points
                    );
                }
                if (simulation.selected_building() == building.id) {
                    for (int y = 0; y < rules.footprint_height; ++y) {
                        for (int x = 0; x < rules.footprint_width; ++x) {
                            outline_diamond(
                                renderer,
                                tile_top({
                                    building.position.x + x,
                                    building.position.y + y,
                                }),
                                {250, 220, 65, 255}
                            );
                        }
                    }
                }
                return;
            }
        }
    }
    if (legacy_two_player_owner && building.completed() &&
        (building.kind == BuildingKind::blacksmith ||
         building.kind == BuildingKind::lumber_camp ||
         building.kind == BuildingKind::mining_camp ||
         building.kind == BuildingKind::university)) {
        const Civilization civilization =
            simulation.civilization(building.owner);
        const std::size_t family =
            civilization == Civilization::teutons ||
            civilization == Civilization::goths ||
            civilization == Civilization::vikings
            ? 1U
            : civilization == Civilization::byzantines ||
              civilization == Civilization::persians ||
              civilization == Civilization::saracens
                ? 2U
                : civilization == Civilization::japanese ||
                  civilization == Civilization::chinese
                    ? 3U
                    : 0U;
        const LegacySprite* sprite = nullptr;
        if (building.kind == BuildingKind::blacksmith) {
            const std::size_t style =
                simulation.age(building.owner) >= Age::castle ? 1U : 0U;
            sprite = building.owner == Player::blue
                ? &active_legacy_sprites.blacksmith_blue[style][family]
                : &active_legacy_sprites.blacksmith_red[style][family];
        } else if (building.kind == BuildingKind::university) {
            const std::size_t style =
                simulation.age(building.owner) == Age::imperial ? 1U : 0U;
            sprite = building.owner == Player::blue
                ? &active_legacy_sprites.university_blue[style][family]
                : &active_legacy_sprites.university_red[style][family];
        } else if (building.kind == BuildingKind::lumber_camp) {
            sprite = building.owner == Player::blue
                ? &active_legacy_sprites.lumber_camp_blue[family]
                : &active_legacy_sprites.lumber_camp_red[family];
        } else {
            sprite = building.owner == Player::blue
                ? &active_legacy_sprites.mining_camp_blue[family]
                : &active_legacy_sprites.mining_camp_red[family];
        }
        if (render_legacy_building_sprite(
                renderer,
                *sprite,
                {top.x, top.y + half_tile_height}
            )) {
            if (building.hit_points < maximum_hit_points ||
                simulation.selected_building() == building.id) {
                render_health_bar(
                    renderer, top.x, top.y - 54.0F,
                    building.hit_points, maximum_hit_points
                );
            }
            if (simulation.selected_building() == building.id) {
                for (int y = 0; y < rules.footprint_height; ++y) {
                    for (int x = 0; x < rules.footprint_width; ++x) {
                        outline_diamond(
                            renderer,
                            tile_top({
                                building.position.x + x,
                                building.position.y + y,
                            }),
                            {250, 220, 65, 255}
                        );
                    }
                }
            }
            return;
        }
    }
    if (legacy_two_player_owner &&
        (building.kind == BuildingKind::house ||
         building.kind == BuildingKind::town_center) &&
        building.completed()) {
        const Civilization civilization =
            simulation.civilization(building.owner);
        const std::size_t family =
            civilization == Civilization::teutons ||
            civilization == Civilization::goths ||
            civilization == Civilization::vikings
            ? 1U
            : civilization == Civilization::byzantines ||
              civilization == Civilization::persians ||
              civilization == Civilization::saracens
                ? 2U
                : civilization == Civilization::japanese ||
                  civilization == Civilization::chinese
                    ? 3U
                    : 0U;
        const std::size_t age = static_cast<std::size_t>(
            render_building_visual_age(
                building.kind, simulation.age(building.owner)
            )
        );
        bool rendered_original = false;
        if (building.kind == BuildingKind::house) {
            const auto& table =
                building.owner == Player::blue
                ? active_legacy_sprites.house_blue
                : active_legacy_sprites.house_red;
            rendered_original = render_legacy_building_sprite(
                renderer,
                table[age][family],
                {top.x, top.y + half_tile_height}
            );
        } else {
            const auto& base =
                building.owner == Player::blue
                ? active_legacy_sprites.town_center_age_blue
                : active_legacy_sprites.town_center_age_red;
            const auto& layers =
                building.owner == Player::blue
                ? active_legacy_sprites.town_center_layers_blue
                : active_legacy_sprites.town_center_layers_red;
            const SDL_FPoint ground{
                top.x, top.y + half_tile_height
            };
            // Supplied archives omit several documented Town Center
            // component SLPs. A valid current-age base remains authoritative;
            // draw only present current-age layers and never mix a
            // procedural/future-age building over that partial composite.
            rendered_original = render_legacy_sprite(
                renderer, base[age][family], ground
            );
            if (rendered_original) {
                for (const LegacySprite& layer : layers[age][family]) {
                    if (layer.texture != nullptr) {
                        (void)render_legacy_sprite(
                            renderer, layer, ground
                        );
                    }
                }
            }
        }
        if (rendered_original) {
            if (building.hit_points < maximum_hit_points ||
                simulation.selected_building() == building.id) {
                render_health_bar(
                    renderer,
                    top.x,
                    top.y - (
                        building.kind == BuildingKind::town_center
                        ? 82.0F
                        : 48.0F
                    ),
                    building.hit_points,
                    maximum_hit_points
                );
            }
            if (simulation.selected_building() == building.id) {
                for (int y = 0; y < rules.footprint_height; ++y) {
                    for (int x = 0; x < rules.footprint_width; ++x) {
                        outline_diamond(
                            renderer,
                            tile_top({
                                building.position.x + x,
                                building.position.y + y,
                            }),
                            {250, 220, 65, 255}
                        );
                    }
                }
            }
            return;
        }
    }
    if (legacy_two_player_owner &&
        building.kind == BuildingKind::market &&
        building.completed()) {
        const Civilization civilization =
            simulation.civilization(building.owner);
        const std::size_t family =
            civilization == Civilization::teutons ||
            civilization == Civilization::goths ||
            civilization == Civilization::vikings
            ? 1U
            : civilization == Civilization::byzantines ||
              civilization == Civilization::persians ||
              civilization == Civilization::saracens
                ? 2U
                : civilization == Civilization::japanese ||
                  civilization == Civilization::chinese
                    ? 3U
                    : 0U;
        const std::size_t age = std::clamp(
            static_cast<std::size_t>(simulation.age(building.owner)),
            std::size_t{1},
            std::size_t{3}
        ) - std::size_t{1};
        const LegacySprite& market =
            building.owner == Player::blue
            ? active_legacy_sprites.market_age_blue[age][family]
            : active_legacy_sprites.market_age_red[age][family];
        if (render_legacy_building_sprite(
                renderer,
                market,
                {top.x, top.y + half_tile_height}
            )) {
            if (building.hit_points < maximum_hit_points ||
                simulation.selected_building() == building.id) {
                render_health_bar(
                    renderer,
                    top.x,
                    top.y - 66.0F,
                    building.hit_points,
                    maximum_hit_points
                );
            }
            if (simulation.selected_building() == building.id) {
                for (int y = 0; y < rules.footprint_height; ++y) {
                    for (int x = 0; x < rules.footprint_width; ++x) {
                        outline_diamond(
                            renderer,
                            tile_top({
                                building.position.x + x,
                                building.position.y + y,
                            }),
                            {250, 220, 65, 255}
                        );
                    }
                }
            }
            return;
        }
    }
    const bool textured_farm =
        building.kind == BuildingKind::farm &&
        active_terrain_textures.farm_growing != nullptr &&
        active_terrain_textures.farm_harvested != nullptr;
    if (!textured_farm) {
        record_building_procedural_fallback(
            simulation, building, maximum_hit_points
        );
    }
    const float full_height =
        building.kind == BuildingKind::town_center
            ? 62.0F
            : (building.kind == BuildingKind::castle
                ? 78.0F
                : (building.kind == BuildingKind::watch_tower
                    ? (keep ? 76.0F : (guard_tower ? 68.0F : 60.0F))
                : (building.kind == BuildingKind::university
                    ? 48.0F
                    : (building.kind == BuildingKind::siege_workshop
                        ? 48.0F
                    : (building.kind == BuildingKind::palisade_wall
                        ? 22.0F
                    : (building.kind == BuildingKind::stone_wall
                        ? 24.0F
                    : ((building.kind == BuildingKind::palisade_gate_x ||
                        building.kind == BuildingKind::palisade_gate_y ||
                        building.kind == BuildingKind::stone_gate_x ||
                        building.kind == BuildingKind::stone_gate_y)
                        ? 28.0F
                    : (building.kind == BuildingKind::farm
                        ? 10.0F
                        : 32.0F))))))));
    const float body_width =
        building.kind == BuildingKind::castle
            ? 128.0F
            : (building.kind == BuildingKind::town_center
                ? 112.0F
            : ((building.kind == BuildingKind::palisade_gate_x ||
                building.kind == BuildingKind::palisade_gate_y ||
                building.kind == BuildingKind::stone_gate_x ||
                building.kind == BuildingKind::stone_gate_y)
                ? 112.0F
            : (building.kind == BuildingKind::watch_tower
                ? 34.0F
            : ((building.kind == BuildingKind::university ||
                building.kind == BuildingKind::siege_workshop)
                ? 96.0F
                : 40.0F))));
    const float progress = building.completed()
        ? 1.0F
        : static_cast<float>(
            rules.construction_ticks -
            building.construction_ticks_remaining
        ) / static_cast<float>(rules.construction_ticks);
    SDL_FRect body{
        top.x - body_width / 2.0F,
        building.kind == BuildingKind::farm
            ? top.y + 7.0F
            : top.y - (
                building.kind == BuildingKind::castle
                    ? 30.0F
                    : (building.kind == BuildingKind::watch_tower
                        ? 28.0F
                    : ((building.kind == BuildingKind::university ||
                        building.kind == BuildingKind::siege_workshop)
                        ? 18.0F
                    : (building.kind == BuildingKind::town_center
                    ? 18.0F
                    : 8.0F)))),
        body_width,
        full_height,
    };
    if (!building.completed()) {
        const float visible_height =
            std::max(5.0F, full_height * progress);
        body.y += full_height - visible_height;
        body.h = visible_height;
        fill_diamond(renderer, top, {104, 84, 58, 255});
    }

    const bool palisade_gate =
        building.kind == BuildingKind::palisade_gate_x ||
        building.kind == BuildingKind::palisade_gate_y ||
        building.kind == BuildingKind::stone_gate_x ||
        building.kind == BuildingKind::stone_gate_y;
    if (building.kind == BuildingKind::farm) {
        const float remaining = std::clamp(
            static_cast<float>(building.resource_amount) /
                static_cast<float>(
                    simulation.farm_capacity(building.owner)
                ),
            0.0F,
            1.0F
        );
        const SDL_Color soil = building.completed()
            ? SDL_Color{
                static_cast<Uint8>(112.0F - remaining * 20.0F),
                static_cast<Uint8>(76.0F + remaining * 8.0F),
                static_cast<Uint8>(38.0F - remaining * 8.0F),
                255,
            }
            : SDL_Color{104, 84, 58, 255};
        SDL_Texture* farm_texture = nullptr;
        if (textured_farm) {
            farm_texture =
                !building.completed() || remaining >= 0.35F
                ? active_terrain_textures.farm_growing
                : active_terrain_textures.farm_harvested;
        }
        fill_diamond(
            renderer,
            top,
            textured_farm ? SDL_Color{255, 255, 255, 255} : soil,
            farm_texture,
            building.position
        );
        outline_diamond(renderer, top, {63, 46, 27, 255});

        if (!textured_farm) {
            set_color(renderer, {70, 48, 26, 255});
            for (int row = 0; row < 5; ++row) {
                const float start_x = top.x - 26.0F + row * 7.0F;
                const float start_y = top.y + 13.0F - row * 3.5F;
                SDL_RenderLine(
                    renderer,
                    start_x,
                    start_y,
                    start_x + 36.0F,
                    start_y + 18.0F
                );
            }
        }

        if (building.completed() && !textured_farm) {
            const int active_rows = static_cast<int>(
                std::ceil(remaining * 5.0F)
            );
            const float stalk_height = 3.0F + remaining * 5.0F;
            for (int row = 0; row < active_rows; ++row) {
                const float start_x = top.x - 23.0F + row * 7.0F;
                const float start_y = top.y + 11.5F - row * 3.5F;
                for (int plant = 0; plant < 5; ++plant) {
                    const float crop_x = start_x + plant * 8.0F;
                    const float crop_y = start_y + plant * 4.0F;
                    set_color(renderer, {190, 158, 48, 255});
                    SDL_RenderLine(
                        renderer,
                        crop_x,
                        crop_y,
                        crop_x,
                        crop_y - stalk_height
                    );
                    set_color(renderer, {112, 124, 42, 255});
                    SDL_RenderLine(
                        renderer,
                        crop_x,
                        crop_y - stalk_height * 0.55F,
                        crop_x - 2.5F,
                        crop_y - stalk_height * 0.8F
                    );
                    SDL_RenderLine(
                        renderer,
                        crop_x,
                        crop_y - stalk_height * 0.45F,
                        crop_x + 2.5F,
                        crop_y - stalk_height * 0.7F
                    );
                }
            }
        } else if (!textured_farm) {
            set_color(renderer, {132, 104, 62, 255});
            for (float offset : {-18.0F, 0.0F, 18.0F}) {
                SDL_RenderLine(
                    renderer,
                    top.x + offset,
                    top.y + 7.0F + std::abs(offset) * 0.25F,
                    top.x + offset,
                    top.y + 20.0F
                );
            }
        }
    }

    if (building.completed() &&
        building.kind != BuildingKind::farm &&
        building.kind != BuildingKind::palisade_wall &&
        !palisade_gate &&
        building.kind != BuildingKind::watch_tower &&
        building.kind != BuildingKind::stone_wall) {
        // Procedural fallback must not imitate an SLP player-color mask with
        // a large opaque owner-colored polygon. Exact archive sprites retain
        // their decoded player pixels; fallback uses neutral roof material.
        const SDL_FRect roof{
            body.x - 3.0F,
            body.y - 6.0F,
            body.w + 6.0F,
            9.0F,
        };
        set_color(renderer, {91, 63, 39, 255});
        SDL_RenderFillRect(renderer, &roof);
        set_color(renderer, {54, 38, 25, 255});
        SDL_RenderRect(renderer, &roof);
        const SDL_FRect door{
            top.x - 5.0F,
            body.y + body.h - 14.0F,
            10.0F,
            14.0F,
        };
        set_color(renderer, {66, 45, 29, 255});
        SDL_RenderFillRect(renderer, &door);

        if (building.kind == BuildingKind::town_center) {
            const SDL_FRect tower{
                top.x - 7.0F,
                body.y - 17.0F,
                14.0F,
                19.0F,
            };
            set_color(renderer, building_color(building));
            SDL_RenderFillRect(renderer, &tower);
            set_color(renderer, {55, 39, 25, 255});
            SDL_RenderRect(renderer, &tower);
            const int occupants = simulation.garrison_count(building.id);
            set_color(renderer, {238, 195, 73, 255});
            for (int index = 0; index < std::min(occupants, 5); ++index) {
                const SDL_FRect lit_window{
                    tower.x + 2.0F + index * 2.0F,
                    tower.y + 6.0F,
                    1.0F,
                    4.0F,
                };
                SDL_RenderFillRect(renderer, &lit_window);
            }
        } else if (building.kind == BuildingKind::stable) {
            const SDL_FRect hay{
                body.x + 4.0F,
                body.y + body.h - 9.0F,
                8.0F,
                5.0F,
            };
            set_color(renderer, {202, 168, 70, 255});
            SDL_RenderFillRect(renderer, &hay);
        } else if (building.kind == BuildingKind::blacksmith) {
            const SDL_FRect chimney{
                body.x + body.w - 10.0F,
                body.y - 17.0F,
                7.0F,
                19.0F,
            };
            set_color(renderer, {62, 57, 55, 255});
            SDL_RenderFillRect(renderer, &chimney);
            const SDL_FRect anvil{
                body.x + 4.0F,
                body.y + body.h - 11.0F,
                11.0F,
                5.0F,
            };
            set_color(renderer, {185, 190, 195, 255});
            SDL_RenderFillRect(renderer, &anvil);
        } else if (building.kind == BuildingKind::castle) {
            set_color(renderer, building_color(building));
            for (float tower_x : {
                     body.x - 6.0F,
                     body.x + body.w - 18.0F,
                 }) {
                const SDL_FRect tower{
                    tower_x,
                    body.y - 12.0F,
                    24.0F,
                    body.h + 12.0F,
                };
                SDL_RenderFillRect(renderer, &tower);
                for (float merlon = tower.x;
                     merlon < tower.x + tower.w;
                     merlon += 6.0F) {
                    const SDL_FRect battlement{
                        merlon,
                        tower.y - 5.0F,
                        4.0F,
                        6.0F,
                    };
                    SDL_RenderFillRect(renderer, &battlement);
                }
            }
        } else if (building.kind == BuildingKind::university) {
            set_color(renderer, {220, 211, 180, 255});
            for (float column_x : {
                     body.x + 14.0F,
                     body.x + body.w / 2.0F - 3.0F,
                     body.x + body.w - 20.0F,
                 }) {
                const SDL_FRect column{
                    column_x,
                    body.y + 12.0F,
                    6.0F,
                    body.h - 16.0F,
                };
                SDL_RenderFillRect(renderer, &column);
            }
        } else if (building.kind == BuildingKind::siege_workshop) {
            set_color(renderer, {78, 61, 43, 255});
            const SDL_FRect frame{
                body.x + 12.0F,
                body.y + 10.0F,
                body.w - 24.0F,
                body.h - 15.0F,
            };
            SDL_RenderRect(renderer, &frame);
            SDL_RenderLine(
                renderer,
                frame.x,
                frame.y,
                frame.x + frame.w,
                frame.y + frame.h
            );
            SDL_RenderLine(
                renderer,
                frame.x + frame.w,
                frame.y,
                frame.x,
                frame.y + frame.h
            );
        } else if (building.kind == BuildingKind::archery_range) {
            set_color(renderer, {220, 195, 125, 255});
            SDL_RenderLine(
                renderer,
                body.x + 7.0F,
                body.y + 8.0F,
                body.x + 7.0F,
                body.y + 22.0F
            );
            SDL_RenderLine(
                renderer,
                body.x + 3.0F,
                body.y + 15.0F,
                body.x + 11.0F,
                body.y + 15.0F
            );
        }
    } else if (
        building.completed() &&
        building.kind == BuildingKind::palisade_wall
    ) {
        const SDL_Color timber = building_color(building);
        for (float offset = -16.0F; offset <= 16.0F; offset += 8.0F) {
            set_color(renderer, timber);
            const SDL_FRect stake{
                top.x + offset - 3.0F,
                top.y - 18.0F,
                6.0F,
                25.0F,
            };
            SDL_RenderFillRect(renderer, &stake);
            fill_triangle(
                renderer,
                {stake.x, stake.y},
                {stake.x + stake.w / 2.0F, stake.y - 7.0F},
                {stake.x + stake.w, stake.y},
                timber
            );
        }
        set_color(renderer, {75, 54, 33, 255});
        SDL_RenderLine(
            renderer,
            top.x - 20.0F,
            top.y - 7.0F,
            top.x + 20.0F,
            top.y - 7.0F
        );
    } else if (building.completed() && palisade_gate) {
        const bool along_x =
            building.kind == BuildingKind::palisade_gate_x ||
            building.kind == BuildingKind::stone_gate_x;
        const bool stone_gate =
            building.kind == BuildingKind::stone_gate_x ||
            building.kind == BuildingKind::stone_gate_y;
        const SDL_Color material = building_color(building);
        std::array<SDL_FPoint, 4> posts{};
        for (int index = 0; index < 4; ++index) {
            const TilePosition tile{
                building.position.x + (along_x ? index : 0),
                building.position.y + (along_x ? 0 : index),
            };
            const SDL_FPoint tile_position = tile_top(tile);
            posts[index] = {
                tile_position.x,
                tile_position.y + half_tile_height,
            };
            const bool tall =
                !building.gate_open || index == 0 || index == 3;
            const float post_height = tall
                ? (stone_gate ? (fortified ? 40.0F : 34.0F) : 28.0F)
                : 5.0F;
            set_color(renderer, material);
            const SDL_FRect post{
                posts[index].x - (stone_gate ? 4.0F : 3.0F),
                posts[index].y - post_height,
                stone_gate ? 8.0F : 6.0F,
                post_height,
            };
            SDL_RenderFillRect(renderer, &post);
            if (tall && !stone_gate) {
                fill_triangle(
                    renderer,
                    {post.x, post.y},
                    {post.x + post.w / 2.0F, post.y - 7.0F},
                    {post.x + post.w, post.y},
                    material
                );
            } else if (tall) {
                const SDL_FRect battlement{
                    post.x - 2.0F,
                    post.y - 5.0F,
                    post.w + 4.0F,
                    6.0F,
                };
                SDL_RenderFillRect(renderer, &battlement);
                if (fortified) {
                    set_color(renderer, {54, 55, 53, 255});
                    const SDL_FRect band{
                        post.x - 1.0F, post.y + 8.0F,
                        post.w + 2.0F, 3.0F,
                    };
                    SDL_RenderFillRect(renderer, &band);
                }
            }
        }
        set_color(
            renderer,
            stone_gate
                ? SDL_Color{74, 73, 68, 255}
                : SDL_Color{75, 54, 33, 255}
        );
        if (building.gate_open) {
            SDL_RenderLine(
                renderer,
                posts[0].x,
                posts[0].y - 4.0F,
                posts[1].x,
                posts[1].y - 4.0F
            );
            SDL_RenderLine(
                renderer,
                posts[2].x,
                posts[2].y - 4.0F,
                posts[3].x,
                posts[3].y - 4.0F
            );
        } else {
            for (float lift : {10.0F, 19.0F}) {
                const int thickness = stone_gate ? 4 : 1;
                for (int offset = 0; offset < thickness; ++offset) {
                    SDL_RenderLine(
                        renderer,
                        posts[0].x,
                        posts[0].y - lift + offset,
                        posts[3].x,
                        posts[3].y - lift + offset
                    );
                }
            }
        }
    } else if (
        building.completed() &&
        building.kind == BuildingKind::watch_tower
    ) {
        const SDL_Color stone = building_color(building);
        set_color(renderer, stone);
        const SDL_FRect crown{
            body.x - 5.0F,
            body.y - 5.0F,
            body.w + 10.0F,
            14.0F,
        };
        SDL_RenderFillRect(renderer, &crown);
        for (float x = crown.x; x < crown.x + crown.w; x += 10.0F) {
            const SDL_FRect merlon{x, crown.y - 7.0F, 6.0F, 8.0F};
            SDL_RenderFillRect(renderer, &merlon);
        }
        if (guard_tower || keep) {
            set_color(renderer, {72, 70, 65, 255});
            const SDL_FRect guard_band{
                body.x - 3.0F, body.y + 13.0F,
                body.w + 6.0F, 5.0F,
            };
            SDL_RenderFillRect(renderer, &guard_band);
            if (keep) {
                const SDL_FRect keep_band{
                    body.x - 5.0F, body.y + 27.0F,
                    body.w + 10.0F, 5.0F,
                };
                SDL_RenderFillRect(renderer, &keep_band);
            }
        }
        set_color(renderer, {45, 40, 35, 255});
        const SDL_FRect arrow_slit{
            top.x - 2.0F,
            body.y + 22.0F,
            4.0F,
            12.0F,
        };
        SDL_RenderFillRect(renderer, &arrow_slit);
    } else if (
        building.completed() &&
        building.kind == BuildingKind::stone_wall
    ) {
        const SDL_Color stone = building_color(building);
        set_color(renderer, stone);
        for (float x = body.x; x < body.x + body.w; x += 10.0F) {
            const SDL_FRect merlon{x, body.y - 7.0F, 7.0F, 8.0F};
            SDL_RenderFillRect(renderer, &merlon);
        }
        set_color(renderer, {74, 73, 68, 255});
        SDL_RenderLine(
            renderer,
            body.x,
            body.y + 10.0F,
            body.x + body.w,
            body.y + 10.0F
        );
        SDL_RenderLine(
            renderer,
            body.x + body.w / 2.0F,
            body.y,
            body.x + body.w / 2.0F,
            body.y + body.h
        );
        if (fortified) {
            set_color(renderer, {54, 55, 53, 255});
            SDL_RenderLine(
                renderer, body.x, body.y + 17.0F,
                body.x + body.w, body.y + 17.0F
            );
            for (float x = body.x + 5.0F;
                 x < body.x + body.w; x += 10.0F) {
                SDL_RenderLine(
                    renderer, x, body.y + 10.0F,
                    x, body.y + 17.0F
                );
            }
        }
    }

    if (building.kind == BuildingKind::town_center &&
        building.completed()) {
        const std::array<LegacySprite, 4>& sprites =
            building.owner == Player::blue
            ? active_legacy_sprites.town_center_blue
            : active_legacy_sprites.town_center_red;
        const bool complete_composite = std::ranges::all_of(
            sprites,
            [](const LegacySprite& sprite) {
                return sprite.texture != nullptr;
            }
        );
        if (complete_composite) {
            for (const LegacySprite& sprite : sprites) {
                render_legacy_sprite(
                    renderer,
                    sprite,
                    {top.x, top.y + half_tile_height}
                );
            }
        }
    }
    if (building.hit_points < maximum_hit_points ||
        simulation.selected_building() == building.id) {
        render_health_bar(
            renderer,
            top.x,
            body.y - 18.0F,
            building.hit_points,
            maximum_hit_points
        );
    }
    if (simulation.selected_building() == building.id) {
        for (int y = 0; y < rules.footprint_height; ++y) {
            for (int x = 0; x < rules.footprint_width; ++x) {
                outline_diamond(
                    renderer,
                    tile_top({
                        building.position.x + x,
                        building.position.y + y,
                    }),
                    {250, 220, 65, 255}
                );
            }
        }
        if (building.has_rally_point) {
            const SDL_FPoint rally = tile_top(building.rally_point);
            set_color(renderer, {250, 220, 65, 220});
            SDL_RenderLine(
                renderer,
                top.x,
                top.y,
                rally.x,
                rally.y + half_tile_height
            );
            SDL_RenderLine(
                renderer,
                rally.x,
                rally.y - 11.0F,
                rally.x,
                rally.y + half_tile_height
            );
            const SDL_FRect flag{
                rally.x,
                rally.y - 11.0F,
                12.0F,
                7.0F,
            };
            SDL_RenderFillRect(renderer, &flag);
            outline_diamond(
                renderer,
                rally,
                {250, 220, 65, 255}
            );
        }
    }
}

void render_building_damage_overlay(
    SDL_Renderer* renderer,
    const Simulation& simulation,
    const Building& building
) {
    const auto records = canonical_building_damage_records(
        building.kind,
        simulation.civilization(building.owner)
    );
    const auto selected = select_building_damage_record(
        building.hit_points,
        simulation.maximum_hit_points(building),
        records
    );
    if (!selected || records[*selected].flag != 0) {
        return;
    }
    const std::uint64_t elapsed = building_damage_elapsed(
        building.id,
        records[*selected].graphic_id,
        simulation.tick_number()
    );
    const auto found = active_legacy_sprites.building_damage_graphics.find(
        records[*selected].graphic_id
    );
    if (found == active_legacy_sprites.building_damage_graphics.end()) {
        record_building_procedural_fallback(
            simulation,
            building,
            simulation.maximum_hit_points(building),
            "render_building_damage_overlay:procedural_or_missing"
        );
        return;
    }
    const LegacyAnimatedComposite* composite =
        found->second.owner(building.owner);
    if (composite == nullptr) {
        record_building_procedural_fallback(
            simulation,
            building,
            simulation.maximum_hit_points(building),
            "render_building_damage_overlay:procedural_or_missing"
        );
        return;
    }
    const SDL_FPoint top = building_top(building);
    if (!render_legacy_animated_composite(
        renderer,
        *composite,
        {top.x, top.y + half_tile_height},
        building.position,
        building.position,
        elapsed,
        true
    )) {
        record_building_procedural_fallback(
            simulation,
            building,
            simulation.maximum_hit_points(building),
            "render_building_damage_overlay:procedural_or_missing"
        );
    }
}

void render_unit(
    SDL_Renderer* renderer,
    const Simulation& simulation,
    const Unit& unit,
    float movement_alpha,
    std::uint64_t presentation_time_ms
) {
    const UnitRules& unit_rules = rules_for(unit.kind);
    const RenderUnitElevationEndpoints elevation_endpoints =
        render_unit_elevation_endpoints(simulation, unit);
    const SDL_FPoint current = unit.render_subtile_initialized
        ? subtile_top(
            unit.render_current_subtile,
            elevation_endpoints.current
        )
        : tile_top(unit.position);
    SDL_FPoint top = current;
    const bool interpolating =
        render_unit_is_interpolating(simulation, unit);
    const std::uint64_t unit_animation_tick =
        unit_animation_tick_from_milliseconds(
            presentation_time_ms,
            unit.id
        );
    const std::uint64_t unit_animation_frame =
        unit_animation_frame_from_milliseconds(
            presentation_time_ms,
            unit.id
        );
    if (interpolating) {
        const SDL_FPoint previous = subtile_top(
            unit.render_previous_subtile,
            elevation_endpoints.previous
        );
        top = {
            previous.x + (current.x - previous.x) * movement_alpha,
            previous.y + (current.y - previous.y) * movement_alpha,
        };
    }
    const bool cavalry = is_cavalry(unit.kind);
    const bool ram = unit.kind == UnitKind::battering_ram;
    const bool mangonel = unit.kind == UnitKind::mangonel;
    const bool siege = ram || mangonel;
    const SDL_FPoint ground_top = top;
    const float gait_phase = interpolating
        ? std::sin(
              movement_alpha * 6.28318531F +
              static_cast<float>((simulation.tick_number() + unit.id) % 2) *
                  3.14159265F
          )
        : 0.0F;
    if (!siege && interpolating) {
        top.y -= std::abs(std::sin(movement_alpha * 3.14159265F)) * 2.0F;
    }
    if (unit.kind == UnitKind::relic) {
        if (render_legacy_sprite(
                renderer,
                active_legacy_sprites.relic,
                {ground_top.x, ground_top.y + half_tile_height}
            )) {
            if (simulation.is_unit_selected(unit.id)) {
                outline_diamond(
                    renderer, ground_top, {250, 220, 65, 255}
                );
            }
            return;
        }
        record_unit_procedural_fallback(simulation, unit, true);
        set_color(renderer, {225, 190, 65, 255});
        const SDL_FRect shrine{
            ground_top.x - 7.0F,
            ground_top.y + half_tile_height - 14.0F,
            14.0F,
            12.0F,
        };
        SDL_RenderFillRect(renderer, &shrine);
        set_color(renderer, {255, 236, 145, 255});
        SDL_RenderLine(
            renderer,
            ground_top.x,
            shrine.y - 9.0F,
            ground_top.x,
            shrine.y + 3.0F
        );
        SDL_RenderLine(
            renderer,
            ground_top.x - 5.0F,
            shrine.y - 4.0F,
            ground_top.x + 5.0F,
            shrine.y - 4.0F
        );
        if (simulation.is_unit_selected(unit.id)) {
            outline_diamond(
                renderer, ground_top, {250, 220, 65, 255}
            );
        }
        return;
    }
    const bool naval_unit =
        unit.kind == UnitKind::fishing_ship ||
        unit.kind == UnitKind::galley ||
        unit.kind == UnitKind::war_galley ||
        unit.kind == UnitKind::galleon ||
        unit.kind == UnitKind::transport_ship ||
        unit.kind == UnitKind::fire_ship ||
        unit.kind == UnitKind::fast_fire_ship ||
        unit.kind == UnitKind::demolition_ship ||
        unit.kind == UnitKind::heavy_demolition_ship ||
        unit.kind == UnitKind::cannon_galleon ||
        unit.kind == UnitKind::elite_cannon_galleon ||
        unit.kind == UnitKind::longboat ||
        unit.kind == UnitKind::elite_longboat ||
        unit.kind == UnitKind::turtle_ship ||
        unit.kind == UnitKind::elite_turtle_ship ||
        unit.kind == UnitKind::trade_cog;
    if (naval_unit) {
        bool legacy_selection_existed{};
        if (unit.kind != UnitKind::fishing_ship) {
            const Civilization civilization =
                simulation.civilization(unit.owner);
            const std::size_t family =
                static_cast<std::size_t>(
                    render_architecture_family(civilization)
                );
            const auto& table =
                unit.attack_target_id != 0
                ? active_legacy_sprites.naval_attack
                : interpolating
                    ? active_legacy_sprites.naval_move
                    : active_legacy_sprites.naval_idle;
            const auto found = table.find(unit.kind);
            if (found != table.end()) {
                legacy_selection_existed = true;
                const PlayerLegacyAnimatedComposite& players =
                    found->second[family];
                const LegacyAnimatedComposite* composite =
                    players.owner(unit.owner);
                if (composite != nullptr) {
                    TilePosition facing_previous = unit.previous_position;
                    TilePosition facing_current = unit.position;
                    if (unit.attack_target_id != 0) {
                    TilePosition target = unit.destination;
                    if (unit.attack_target_is_building) {
                        const auto target_building =
                            std::ranges::find_if(
                                simulation.buildings(),
                                [&unit](const Building& building) {
                                    return building.id ==
                                        unit.attack_target_id;
                                }
                            );
                        if (target_building !=
                            simulation.buildings().end()) {
                            target = target_building->position;
                        }
                    } else {
                        const auto target_unit = std::ranges::find_if(
                            simulation.units(),
                            [&unit](const Unit& candidate) {
                                return candidate.id ==
                                    unit.attack_target_id;
                            }
                        );
                        if (target_unit != simulation.units().end()) {
                            target = target_unit->position;
                        }
                    }
                        facing_previous = unit.position;
                        facing_current = {
                            unit.position.x + std::clamp(
                                target.x - unit.position.x, -1, 1
                            ),
                            unit.position.y + std::clamp(
                                target.y - unit.position.y, -1, 1
                            ),
                        };
                    }
                    if (render_legacy_animated_composite(
                        renderer,
                        *composite,
                        {
                            ground_top.x,
                            ground_top.y + half_tile_height,
                        },
                        facing_previous,
                        facing_current,
                        unit_animation_tick,
                        true
                    )) {
                    if (unit.hit_points < unit_rules.hit_points ||
                        simulation.is_unit_selected(unit.id)) {
                        render_health_bar(
                            renderer,
                            ground_top.x,
                            ground_top.y + half_tile_height - 46.0F,
                            unit.hit_points,
                            unit_rules.hit_points
                        );
                    }
                    if (simulation.is_unit_selected(unit.id)) {
                        outline_diamond(
                            renderer,
                            ground_top,
                            {250, 220, 65, 255}
                        );
                    }
                        return;
                    }
                }
            }
        }
        if (unit.kind == UnitKind::fishing_ship) {
            const PlayerLegacySprites& ship_sprites =
                interpolating
                ? active_legacy_sprites.fishing_ship_moving
                : active_legacy_sprites.fishing_ship_standing;
            const LegacyAnimation* ship_animation =
                ship_sprites.owner(unit.owner);
            legacy_selection_existed = true;
            if (ship_animation != nullptr && render_legacy_animation(
                    renderer,
                    *ship_animation,
                    {ground_top.x, ground_top.y + half_tile_height},
                    unit.previous_position,
                    unit.position,
                    unit_animation_frame,
                    interpolating
                )) {
                if (unit.hit_points < unit_rules.hit_points ||
                    simulation.is_unit_selected(unit.id)) {
                    render_health_bar(
                        renderer,
                        ground_top.x,
                        ground_top.y + half_tile_height - 38.0F,
                        unit.hit_points,
                        unit_rules.hit_points
                    );
                }
                if (simulation.is_unit_selected(unit.id)) {
                    outline_diamond(
                        renderer, ground_top, {250, 220, 65, 255}
                    );
                }
                return;
            }
        }
        record_unit_procedural_fallback(
            simulation, unit, legacy_selection_existed
        );
        const bool transport = unit.kind == UnitKind::transport_ship;
        const bool warship =
            unit.kind == UnitKind::galley ||
            unit.kind == UnitKind::war_galley ||
            unit.kind == UnitKind::galleon ||
            unit.kind == UnitKind::fire_ship ||
            unit.kind == UnitKind::fast_fire_ship ||
            unit.kind == UnitKind::cannon_galleon ||
            unit.kind == UnitKind::elite_cannon_galleon ||
            unit.kind == UnitKind::longboat ||
            unit.kind == UnitKind::elite_longboat ||
            unit.kind == UnitKind::turtle_ship ||
            unit.kind == UnitKind::elite_turtle_ship;
        const float hull_width =
            unit.kind == UnitKind::galleon ? 40.0F
            : transport ? 36.0F
            : warship ? 34.0F
            : 32.0F;
        const SDL_Color player_color =
            unit.owner == Player::blue
            ? SDL_Color{45, 90, 190, 255}
            : SDL_Color{190, 55, 45, 255};
        const SDL_FRect hull{
            ground_top.x - hull_width / 2.0F,
            ground_top.y + half_tile_height - 7.0F,
            hull_width,
            transport ? 10.0F : 8.0F,
        };
        set_color(renderer, warship
            ? SDL_Color{74, 55, 40, 255}
            : SDL_Color{95, 61, 34, 255});
        SDL_RenderFillRect(renderer, &hull);
        set_color(renderer, {55, 38, 25, 255});
        SDL_RenderLine(
            renderer,
            hull.x + 3.0F,
            hull.y + hull.h,
            hull.x + hull.w - 4.0F,
            hull.y + hull.h
        );
        SDL_RenderLine(
            renderer,
            ground_top.x,
            hull.y,
            ground_top.x,
            hull.y - 25.0F
        );
        const std::array<SDL_Vertex, 3> sail{{
            {{ground_top.x + 1.0F, hull.y - 23.0F}, {
                player_color.r / 255.0F,
                player_color.g / 255.0F,
                player_color.b / 255.0F,
                1.0F,
            }, {}},
            {{ground_top.x + 1.0F, hull.y - 2.0F}, {
                player_color.r / 255.0F,
                player_color.g / 255.0F,
                player_color.b / 255.0F,
                1.0F,
            }, {}},
            {{ground_top.x + 14.0F, hull.y - 2.0F}, {
                0.9F, 0.85F, 0.68F, 1.0F
            }, {}},
        }};
        SDL_RenderGeometry(
            renderer,
            nullptr,
            sail.data(),
            static_cast<int>(sail.size()),
            nullptr,
            0
        );
        if (warship) {
            set_color(renderer, {28, 28, 31, 255});
            const int cannon_count =
                unit.kind == UnitKind::galleon ? 3
                : unit.kind == UnitKind::war_galley ? 2
                : 1;
            for (int cannon = 0; cannon < cannon_count; ++cannon) {
                const float x =
                    hull.x + 9.0F + static_cast<float>(cannon) * 9.0F;
                SDL_RenderLine(
                    renderer, x, hull.y + 2.0F, x - 7.0F, hull.y - 3.0F
                );
            }
        }
        if (transport) {
            int passengers{};
            for (const Unit& candidate : simulation.units()) {
                passengers += candidate.garrisoned_in == unit.id ? 1 : 0;
            }
            set_color(renderer, {220, 185, 125, 255});
            for (int passenger = 0; passenger < passengers; ++passenger) {
                const SDL_FRect marker{
                    hull.x + 5.0F + passenger * 5.0F,
                    hull.y - 4.0F,
                    3.0F,
                    4.0F,
                };
                SDL_RenderFillRect(renderer, &marker);
            }
        }
        if (unit.hit_points < unit_rules.hit_points ||
            simulation.is_unit_selected(unit.id)) {
            render_health_bar(
                renderer,
                ground_top.x,
                hull.y - 31.0F,
                unit.hit_points,
                unit_rules.hit_points
            );
        }
        if (simulation.is_unit_selected(unit.id)) {
            outline_diamond(
                renderer, ground_top, {250, 220, 65, 255}
            );
        }
        return;
    }
    if (unit.kind == UnitKind::trade_cart) {
        const SDL_FPoint ground{
            ground_top.x,
            ground_top.y + half_tile_height
        };
        {
            const PlayerLegacySprites& sprites =
                interpolating
                ? active_legacy_sprites.trade_cart_moving
                : active_legacy_sprites.trade_cart_standing;
            const LegacyAnimation* animation =
                sprites.owner(unit.owner);
            if (animation != nullptr && render_legacy_animation(
                    renderer,
                    *animation,
                    ground,
                    unit.previous_position,
                    unit.position,
                    unit_animation_frame,
                    true
                )) {
                const int maximum_hit_points =
                    simulation.maximum_hit_points(unit);
                if (unit.hit_points < maximum_hit_points ||
                    simulation.is_unit_selected(unit.id)) {
                    render_health_bar(
                        renderer,
                        ground_top.x,
                        ground_top.y - 36.0F,
                        unit.hit_points,
                        maximum_hit_points
                    );
                }
                if (simulation.is_unit_selected(unit.id)) {
                    outline_diamond(
                        renderer,
                        ground_top,
                        {250, 220, 65, 255}
                    );
                }
                return;
            }
        }
        record_unit_procedural_fallback(simulation, unit, true);
        set_color(renderer, {65, 45, 27, 255});
        const SDL_FRect cart{
            ground.x - 14.0F,
            ground.y - 18.0F,
            28.0F,
            13.0F,
        };
        SDL_RenderFillRect(renderer, &cart);
        set_color(
            renderer,
            unit.owner == Player::blue
                ? SDL_Color{55, 110, 205, 255}
                : SDL_Color{190, 65, 55, 255}
        );
        const SDL_FRect canopy{
            ground.x - 11.0F,
            ground.y - 26.0F,
            22.0F,
            9.0F,
        };
        SDL_RenderFillRect(renderer, &canopy);
        set_color(renderer, {28, 25, 22, 255});
        for (float wheel_x : {ground.x - 9.0F, ground.x + 9.0F}) {
            const SDL_FRect wheel{wheel_x - 4.0F, ground.y - 7.0F, 8.0F, 8.0F};
            SDL_RenderFillRect(renderer, &wheel);
        }
        const int maximum_hit_points =
            simulation.maximum_hit_points(unit);
        if (unit.hit_points < maximum_hit_points ||
            simulation.is_unit_selected(unit.id)) {
            render_health_bar(
                renderer,
                ground_top.x,
                ground_top.y - 36.0F,
                unit.hit_points,
                maximum_hit_points
            );
        }
        if (simulation.is_unit_selected(unit.id)) {
            outline_diamond(
                renderer, ground_top, {250, 220, 65, 255}
            );
        }
        return;
    }
    if (unit.kind == UnitKind::monk && unit.carrying_relic) {
        const PlayerLegacySprites& carried_players =
            interpolating
            ? active_legacy_sprites.carried_relic_moving
            : active_legacy_sprites.carried_relic_standing;
        const LegacyAnimation* carried =
            carried_players.owner(unit.owner);
        if (carried != nullptr && render_legacy_animation(
                renderer,
                *carried,
                {ground_top.x, ground_top.y + half_tile_height},
                unit.previous_position,
                unit.position,
                unit_animation_frame,
                true
            )) {
            const int maximum_hit_points =
                simulation.maximum_hit_points(unit);
            if (unit.hit_points < maximum_hit_points ||
                simulation.is_unit_selected(unit.id)) {
                render_health_bar(
                    renderer,
                    ground_top.x,
                    ground_top.y - 36.0F,
                    unit.hit_points,
                    maximum_hit_points
                );
            }
            if (simulation.is_unit_selected(unit.id)) {
                outline_diamond(
                    renderer, ground_top, {250, 220, 65, 255}
                );
            }
            return;
        }
    }
    const LegacyAnimation* early_animation =
        legacy_action_for(simulation, unit, interpolating);
    if (early_animation != nullptr) {
        const std::uint64_t action_tick =
            unit.trebuchet_transform_ticks_remaining > 0
            ? static_cast<std::uint64_t>(
                (2 - unit.trebuchet_transform_ticks_remaining) * 4
            )
            : unit_animation_frame;
        if (render_legacy_animation(
                renderer,
                *early_animation,
                {ground_top.x, ground_top.y + half_tile_height},
                unit.previous_position,
                unit.position,
                action_tick,
                true
            )) {
            const int maximum_hit_points =
                simulation.maximum_hit_points(unit);
            if (unit.hit_points < maximum_hit_points ||
                simulation.is_unit_selected(unit.id)) {
                render_health_bar(
                    renderer,
                    ground_top.x,
                    ground_top.y - 36.0F,
                    unit.hit_points,
                    maximum_hit_points
                );
            }
            if (simulation.is_unit_selected(unit.id)) {
                outline_diamond(
                    renderer,
                    ground_top,
                    {250, 220, 65, 255}
                );
            }
            if (unit.carrying_relic) {
                set_color(renderer, {255, 220, 75, 255});
                SDL_RenderLine(
                    renderer,
                    ground_top.x,
                    ground_top.y - 42.0F,
                    ground_top.x,
                    ground_top.y - 31.0F
                );
                SDL_RenderLine(
                    renderer,
                    ground_top.x - 4.0F,
                    ground_top.y - 38.0F,
                    ground_top.x + 4.0F,
                    ground_top.y - 38.0F
                );
            }
            return;
        }
    }
    record_unit_procedural_fallback(
        simulation, unit, early_animation != nullptr
    );
    const SDL_Color player_color = unit.owner == Player::blue
        ? SDL_Color{70, 135, 240, 255}
        : SDL_Color{220, 70, 60, 255};

    const SDL_FRect shadow{
        ground_top.x - (siege ? 15.0F : (cavalry ? 11.0F : 7.0F)),
        ground_top.y + half_tile_height - 3.0F,
        siege ? 30.0F : (cavalry ? 22.0F : 14.0F),
        5.0F,
    };
    set_color(renderer, {25, 24, 20, 120});
    SDL_RenderFillRect(renderer, &shadow);

    float health_top{};
    if (ram) {
        const SDL_FRect chassis{
            top.x - 14.0F,
            top.y + half_tile_height - 15.0F,
            28.0F,
            12.0F,
        };
        set_color(renderer, {105, 77, 45, 255});
        SDL_RenderFillRect(renderer, &chassis);
        set_color(renderer, player_color);
        SDL_RenderLine(
            renderer,
            chassis.x,
            chassis.y,
            chassis.x + chassis.w,
            chassis.y
        );
        set_color(renderer, {58, 46, 34, 255});
        for (float wheel_x : {chassis.x + 4.0F, chassis.x + 22.0F}) {
            const SDL_FRect wheel{
                wheel_x,
                chassis.y + chassis.h - 1.0F,
                5.0F,
                5.0F,
            };
            SDL_RenderFillRect(renderer, &wheel);
            if (interpolating) {
                set_color(renderer, {145, 113, 72, 255});
                SDL_RenderLine(
                    renderer,
                    wheel.x,
                    wheel.y + (gait_phase >= 0.0F ? 0.0F : wheel.h),
                    wheel.x + wheel.w,
                    wheel.y + (gait_phase >= 0.0F ? wheel.h : 0.0F)
                );
                set_color(renderer, {58, 46, 34, 255});
            }
        }
        SDL_RenderLine(
            renderer,
            chassis.x + chassis.w,
            chassis.y + 6.0F,
            chassis.x + chassis.w + 10.0F,
            chassis.y + 6.0F
        );
        health_top = chassis.y - 8.0F;
    } else if (mangonel) {
        const SDL_FRect frame{
            top.x - 13.0F,
            top.y + half_tile_height - 15.0F,
            26.0F,
            12.0F,
        };
        set_color(renderer, {112, 80, 45, 255});
        SDL_RenderRect(renderer, &frame);
        SDL_RenderLine(
            renderer,
            frame.x + 4.0F,
            frame.y + frame.h,
            frame.x + frame.w - 3.0F,
            frame.y - 9.0F
        );
        set_color(renderer, player_color);
        SDL_RenderLine(
            renderer,
            frame.x,
            frame.y,
            frame.x + frame.w,
            frame.y
        );
        set_color(renderer, {58, 46, 34, 255});
        for (float wheel_x : {frame.x + 3.0F, frame.x + 19.0F}) {
            const SDL_FRect wheel{
                wheel_x,
                frame.y + frame.h - 1.0F,
                6.0F,
                6.0F,
            };
            SDL_RenderFillRect(renderer, &wheel);
            if (interpolating) {
                set_color(renderer, {145, 113, 72, 255});
                SDL_RenderLine(
                    renderer,
                    wheel.x,
                    wheel.y + (gait_phase >= 0.0F ? 0.0F : wheel.h),
                    wheel.x + wheel.w,
                    wheel.y + (gait_phase >= 0.0F ? wheel.h : 0.0F)
                );
                set_color(renderer, {58, 46, 34, 255});
            }
        }
        const SDL_FRect stone{
            frame.x + frame.w - 4.0F,
            frame.y - 13.0F,
            7.0F,
            7.0F,
        };
        set_color(renderer, {105, 105, 100, 255});
        SDL_RenderFillRect(renderer, &stone);
        health_top = frame.y - 18.0F;
    } else if (cavalry) {
        const SDL_FRect horse{
            top.x - 10.0F,
            top.y + half_tile_height - 14.0F,
            20.0F,
            10.0F,
        };
        set_color(
            renderer,
            unit.kind == UnitKind::paladin
                ? SDL_Color{60, 68, 82, 255}
                : (unit.kind == UnitKind::cavalier
                ? SDL_Color{72, 76, 84, 255}
                : (unit.kind == UnitKind::knight
                    ? SDL_Color{100, 92, 82, 255}
                    : (unit.kind == UnitKind::hussar
                        ? SDL_Color{74, 54, 42, 255}
                    : (unit.kind == UnitKind::light_cavalry
                        ? SDL_Color{112, 76, 48, 255}
                    : SDL_Color{132, 92, 54, 255}))
                ))
        );
        SDL_RenderFillRect(renderer, &horse);
        const SDL_FRect head{
            top.x + 7.0F,
            horse.y - 4.0F,
            7.0F,
            8.0F,
        };
        SDL_RenderFillRect(renderer, &head);
        const std::array<float, 2> leg_positions{
            horse.x + 3.0F,
            horse.x + 15.0F,
        };
        for (std::size_t index = 0; index < leg_positions.size(); ++index) {
            const float stride =
                gait_phase * (index == 0 ? 4.0F : -4.0F);
            SDL_RenderLine(
                renderer,
                leg_positions[index],
                horse.y + horse.h,
                leg_positions[index] + stride,
                horse.y + horse.h + 5.0F
            );
        }
        const SDL_FRect rider{
            top.x - 4.0F,
            horse.y - 10.0F,
            8.0F,
            11.0F,
        };
        set_color(renderer, player_color);
        SDL_RenderFillRect(renderer, &rider);
        const SDL_FRect rider_head{
            top.x - 3.0F,
            rider.y - 6.0F,
            6.0F,
            6.0F,
        };
        set_color(renderer, {220, 178, 130, 255});
        SDL_RenderFillRect(renderer, &rider_head);
        if (unit.kind == UnitKind::hussar) {
            set_color(renderer, {210, 180, 72, 255});
            SDL_RenderLine(
                renderer,
                rider_head.x + rider_head.w / 2.0F,
                rider_head.y,
                rider_head.x + rider_head.w / 2.0F + 3.0F,
                rider_head.y - 7.0F
            );
        }
        if (unit.kind == UnitKind::knight ||
            unit.kind == UnitKind::cavalier ||
            unit.kind == UnitKind::paladin) {
            set_color(renderer, {205, 210, 215, 255});
            SDL_RenderLine(
                renderer,
                rider.x + rider.w,
                rider.y + 2.0F,
                rider.x + rider.w + 9.0F,
                rider.y - 5.0F
            );
            if (unit.kind == UnitKind::cavalier ||
                unit.kind == UnitKind::paladin) {
                const SDL_FRect helmet{
                    rider_head.x - 1.0F,
                    rider_head.y - 3.0F,
                    rider_head.w + 2.0F,
                    4.0F,
                };
                SDL_RenderFillRect(renderer, &helmet);
                SDL_RenderLine(
                    renderer,
                    helmet.x + helmet.w / 2.0F,
                    helmet.y,
                    helmet.x + helmet.w / 2.0F + 2.0F,
                    helmet.y - 5.0F
                );
                if (unit.kind == UnitKind::paladin) {
                    SDL_RenderLine(
                        renderer,
                        helmet.x,
                        helmet.y + helmet.h,
                        helmet.x + helmet.w,
                        helmet.y + helmet.h
                    );
                }
            }
        }
        health_top = rider.y - 13.0F;
    } else {
        const float height =
            (unit.kind == UnitKind::archer ||
             unit.kind == UnitKind::crossbowman ||
             unit.kind == UnitKind::arbalester ||
             unit.kind == UnitKind::skirmisher ||
             unit.kind == UnitKind::elite_skirmisher)
                ? 18.0F
                : 15.0F;
        const SDL_FRect body{
            top.x - 5.0F,
            top.y + half_tile_height - height,
            10.0F,
            height,
        };
        set_color(renderer, player_color);
        SDL_RenderFillRect(renderer, &body);
        const SDL_FRect head{
            top.x - 3.5F,
            body.y - 7.0F,
            7.0F,
            7.0F,
        };
        set_color(renderer, {220, 178, 130, 255});
        SDL_RenderFillRect(renderer, &head);
        set_color(renderer, {48, 49, 55, 255});
        for (int leg = 0; leg < 2; ++leg) {
            const float hip_x = top.x + (leg == 0 ? -2.5F : 2.5F);
            const float stride =
                gait_phase * (leg == 0 ? 3.5F : -3.5F);
            SDL_RenderLine(
                renderer,
                hip_x,
                body.y + body.h - 2.0F,
                hip_x + stride,
                body.y + body.h + 4.0F
            );
        }
        if (unit.kind == UnitKind::archer) {
            set_color(renderer, {110, 72, 38, 255});
            SDL_RenderLine(
                renderer,
                body.x + body.w,
                body.y + 2.0F,
                body.x + body.w + 6.0F,
                body.y + 14.0F
            );
            SDL_RenderLine(
                renderer,
                body.x + body.w + 6.0F,
                body.y + 14.0F,
                body.x + body.w,
                body.y + 15.0F
            );
        } else if (unit.kind == UnitKind::arbalester) {
            set_color(renderer, {105, 68, 38, 255});
            SDL_RenderLine(
                renderer,
                body.x + body.w,
                body.y + 8.0F,
                body.x + body.w + 13.0F,
                body.y + 8.0F
            );
            SDL_RenderLine(
                renderer,
                body.x + body.w + 6.0F,
                body.y + 1.0F,
                body.x + body.w + 6.0F,
                body.y + 15.0F
            );
            set_color(renderer, {215, 218, 222, 255});
            const SDL_FRect cap{
                head.x - 1.0F,
                head.y - 3.0F,
                head.w + 2.0F,
                4.0F,
            };
            SDL_RenderFillRect(renderer, &cap);
        } else if (unit.kind == UnitKind::crossbowman) {
            set_color(renderer, {115, 78, 45, 255});
            SDL_RenderLine(
                renderer,
                body.x + body.w,
                body.y + 8.0F,
                body.x + body.w + 10.0F,
                body.y + 8.0F
            );
            SDL_RenderLine(
                renderer,
                body.x + body.w + 5.0F,
                body.y + 3.0F,
                body.x + body.w + 5.0F,
                body.y + 13.0F
            );
            set_color(renderer, {195, 200, 205, 255});
            const SDL_FRect cap{
                head.x,
                head.y - 2.0F,
                head.w,
                3.0F,
            };
            SDL_RenderFillRect(renderer, &cap);
        } else if (unit.kind == UnitKind::elite_skirmisher) {
            set_color(renderer, {205, 190, 130, 255});
            const SDL_FRect shield{
                body.x - 5.0F,
                body.y + 3.0F,
                9.0F,
                13.0F,
            };
            SDL_RenderFillRect(renderer, &shield);
            set_color(renderer, {220, 210, 175, 255});
            SDL_RenderLine(
                renderer,
                body.x + body.w,
                body.y + 13.0F,
                body.x + body.w + 13.0F,
                body.y - 5.0F
            );
            const SDL_FRect cap{
                head.x - 1.0F,
                head.y - 2.0F,
                head.w + 2.0F,
                3.0F,
            };
            SDL_RenderFillRect(renderer, &cap);
        } else if (unit.kind == UnitKind::skirmisher) {
            set_color(renderer, {185, 165, 105, 255});
            const SDL_FRect shield{
                body.x - 4.0F,
                body.y + 5.0F,
                7.0F,
                10.0F,
            };
            SDL_RenderFillRect(renderer, &shield);
            set_color(renderer, {205, 195, 165, 255});
            SDL_RenderLine(
                renderer,
                body.x + body.w,
                body.y + 12.0F,
                body.x + body.w + 10.0F,
                body.y - 3.0F
            );
        } else if (unit.kind == UnitKind::pikeman) {
            set_color(renderer, {205, 210, 215, 255});
            const SDL_FRect shield{
                body.x - 4.0F,
                body.y + 4.0F,
                7.0F,
                11.0F,
            };
            SDL_RenderFillRect(renderer, &shield);
            SDL_RenderLine(
                renderer,
                body.x + body.w,
                body.y + 14.0F,
                body.x + body.w + 17.0F,
                body.y - 13.0F
            );
            fill_triangle(
                renderer,
                {body.x + body.w + 14.0F, body.y - 10.0F},
                {body.x + body.w + 20.0F, body.y - 17.0F},
                {body.x + body.w + 18.0F, body.y - 8.0F},
                {220, 222, 225, 255}
            );
        } else if (unit.kind == UnitKind::spearman) {
            set_color(renderer, {190, 175, 145, 255});
            SDL_RenderLine(
                renderer,
                body.x + body.w,
                body.y + 13.0F,
                body.x + body.w + 13.0F,
                body.y - 9.0F
            );
            fill_triangle(
                renderer,
                {body.x + body.w + 10.0F, body.y - 6.0F},
                {body.x + body.w + 16.0F, body.y - 13.0F},
                {body.x + body.w + 14.0F, body.y - 4.0F},
                {205, 210, 215, 255}
            );
        } else if (unit.kind == UnitKind::militia) {
            set_color(renderer, {205, 210, 215, 255});
            SDL_RenderLine(
                renderer,
                body.x + body.w,
                body.y + 11.0F,
                body.x + body.w + 8.0F,
                body.y + 1.0F
            );
        } else if (unit.kind == UnitKind::champion) {
            set_color(renderer, {225, 226, 230, 255});
            const SDL_FRect helmet{
                head.x - 2.0F,
                head.y - 4.0F,
                head.w + 4.0F,
                6.0F,
            };
            SDL_RenderFillRect(renderer, &helmet);
            set_color(renderer, {205, 175, 65, 255});
            SDL_RenderLine(
                renderer,
                helmet.x + helmet.w / 2.0F,
                helmet.y,
                helmet.x + helmet.w / 2.0F + 2.0F,
                helmet.y - 6.0F
            );
            set_color(renderer, {225, 226, 230, 255});
            SDL_RenderLine(
                renderer,
                body.x + body.w - 1.0F,
                body.y + 15.0F,
                body.x + body.w + 16.0F,
                body.y - 9.0F
            );
            SDL_RenderLine(
                renderer,
                body.x + body.w + 13.0F,
                body.y - 8.0F,
                body.x + body.w + 19.0F,
                body.y - 4.0F
            );
        } else if (unit.kind == UnitKind::two_handed_swordsman) {
            set_color(renderer, {215, 218, 222, 255});
            const SDL_FRect helmet{
                head.x - 1.0F,
                head.y - 3.0F,
                head.w + 2.0F,
                5.0F,
            };
            SDL_RenderFillRect(renderer, &helmet);
            SDL_RenderLine(
                renderer,
                body.x + body.w - 1.0F,
                body.y + 15.0F,
                body.x + body.w + 15.0F,
                body.y - 8.0F
            );
            SDL_RenderLine(
                renderer,
                body.x + body.w + 12.0F,
                body.y - 7.0F,
                body.x + body.w + 18.0F,
                body.y - 3.0F
            );
        } else if (unit.kind == UnitKind::long_swordsman) {
            set_color(renderer, {205, 210, 215, 255});
            const SDL_FRect helmet{
                head.x - 1.0F,
                head.y - 3.0F,
                head.w + 2.0F,
                5.0F,
            };
            SDL_RenderFillRect(renderer, &helmet);
            const SDL_FRect shield{
                body.x - 5.0F,
                body.y + 4.0F,
                8.0F,
                11.0F,
            };
            SDL_RenderFillRect(renderer, &shield);
            SDL_RenderLine(
                renderer,
                body.x + body.w,
                body.y + 13.0F,
                body.x + body.w + 13.0F,
                body.y - 5.0F
            );
            SDL_RenderLine(
                renderer,
                body.x + body.w + 11.0F,
                body.y - 4.0F,
                body.x + body.w + 15.0F,
                body.y - 1.0F
            );
        } else if (unit.kind == UnitKind::man_at_arms) {
            set_color(renderer, {195, 200, 205, 255});
            const SDL_FRect helmet{
                head.x - 1.0F,
                head.y - 2.0F,
                head.w + 2.0F,
                4.0F,
            };
            SDL_RenderFillRect(renderer, &helmet);
            const SDL_FRect shield{
                body.x - 4.0F,
                body.y + 5.0F,
                7.0F,
                10.0F,
            };
            SDL_RenderFillRect(renderer, &shield);
            SDL_RenderLine(
                renderer,
                body.x + body.w,
                body.y + 12.0F,
                body.x + body.w + 9.0F,
                body.y
            );
        } else {
            const bool constructing = !unit.moving &&
                std::ranges::any_of(
                    simulation.buildings(),
                    [&unit](const Building& building) {
                        return !building.completed() &&
                            std::ranges::find(
                                building.builder_ids,
                                unit.id
                            ) != building.builder_ids.end();
                    }
                );
            const bool repairing =
                !unit.moving && unit.repair_target_id != 0;
            const bool gathering =
                !unit.moving && unit.has_resource_target &&
                !unit.returning_resource;
            const int work_frame = static_cast<int>(
                (simulation.tick_number() + unit.id) % 4
            );
            if (constructing || repairing || gathering) {
                const bool raised = work_frame < 2;
                const float tool_x =
                    body.x + body.w + (raised ? 9.0F : 5.0F);
                const float tool_y =
                    body.y + (raised ? -4.0F : 13.0F);
                set_color(renderer, {128, 83, 43, 255});
                SDL_RenderLine(
                    renderer,
                    body.x + body.w - 1.0F,
                    body.y + 7.0F,
                    tool_x,
                    tool_y
                );
                const Terrain target_terrain =
                    simulation.map().contains(unit.resource_target)
                        ? simulation.map().terrain_at(unit.resource_target)
                        : Terrain::grass;
                if (constructing || repairing) {
                    set_color(renderer, {185, 190, 195, 255});
                    const SDL_FRect hammer{
                        tool_x - 4.0F,
                        tool_y - 2.0F,
                        9.0F,
                        4.0F,
                    };
                    SDL_RenderFillRect(renderer, &hammer);
                } else if (target_terrain == Terrain::forest) {
                    set_color(renderer, {190, 195, 198, 255});
                    fill_triangle(
                        renderer,
                        {tool_x - 1.0F, tool_y - 5.0F},
                        {tool_x + 6.0F, tool_y},
                        {tool_x - 1.0F, tool_y + 3.0F},
                        {190, 195, 198, 255}
                    );
                } else if (
                    target_terrain == Terrain::gold_mine ||
                    target_terrain == Terrain::stone_mine
                ) {
                    set_color(renderer, {190, 195, 198, 255});
                    SDL_RenderLine(
                        renderer,
                        tool_x - 5.0F,
                        tool_y - 2.0F,
                        tool_x + 5.0F,
                        tool_y + 2.0F
                    );
                } else {
                    set_color(renderer, {214, 204, 145, 255});
                    SDL_RenderLine(
                        renderer,
                        tool_x - 3.0F,
                        tool_y - 4.0F,
                        tool_x + 4.0F,
                        tool_y + 3.0F
                    );
                }
            } else {
                const SDL_FRect tool{
                    body.x + body.w,
                    body.y + 5.0F,
                    7.0F,
                    2.0F,
                };
                set_color(renderer, {190, 175, 145, 255});
                SDL_RenderFillRect(renderer, &tool);
            }
            if (unit.carried_amount > 0) {
                SDL_Color cargo_color{125, 82, 42, 255};
                switch (unit.carried_resource) {
                    case ResourceKind::food:
                        cargo_color = {190, 70, 55, 255};
                        break;
                    case ResourceKind::gold:
                        cargo_color = {230, 184, 45, 255};
                        break;
                    case ResourceKind::stone:
                        cargo_color = {145, 145, 140, 255};
                        break;
                    case ResourceKind::wood:
                    case ResourceKind::none:
                        break;
                }
                const SDL_FRect cargo{
                    body.x - 7.0F,
                    body.y + 5.0F,
                    7.0F,
                    9.0F,
                };
                set_color(renderer, cargo_color);
                SDL_RenderFillRect(renderer, &cargo);
                if (unit.carried_resource == ResourceKind::wood) {
                    set_color(renderer, {74, 49, 28, 255});
                    SDL_RenderLine(
                        renderer,
                        cargo.x,
                        cargo.y + 3.0F,
                        cargo.x + cargo.w,
                        cargo.y + 3.0F
                    );
                    SDL_RenderLine(
                        renderer,
                        cargo.x,
                        cargo.y + 6.0F,
                        cargo.x + cargo.w,
                        cargo.y + 6.0F
                    );
                }
            }
        }
        health_top = head.y - 7.0F;
    }

    const bool attack_pose =
        unit.attack_cooldown >=
        std::max(1, unit_rules.attack_interval_ticks - 1);
    if (attack_pose) {
        TilePosition pose_target = unit.destination;
        if (unit.attacking_ground) {
            pose_target = unit.attack_ground_target;
        } else if (unit.attack_target_id != 0) {
            if (unit.attack_target_is_building) {
                const auto target = std::ranges::find_if(
                    simulation.buildings(),
                    [&unit](const Building& building) {
                        return building.id == unit.attack_target_id;
                    }
                );
                if (target != simulation.buildings().end()) {
                    pose_target = target->position;
                }
            } else {
                const auto target = std::ranges::find_if(
                    simulation.units(),
                    [&unit](const Unit& candidate) {
                        return candidate.id == unit.attack_target_id;
                    }
                );
                if (target != simulation.units().end()) {
                    pose_target = target->position;
                }
            }
        }
        const SDL_FPoint target_top = tile_top(pose_target);
        const float facing = target_top.x < top.x ? -1.0F : 1.0F;
        const bool release =
            unit.attack_cooldown == unit_rules.attack_interval_ticks;
        const float hand_y = top.y + half_tile_height - 11.0F;
        if (unit.kind == UnitKind::battering_ram) {
            set_color(renderer, {79, 56, 35, 255});
            SDL_RenderLine(
                renderer,
                top.x + facing * 8.0F,
                hand_y + 5.0F,
                top.x + facing * (release ? 27.0F : 18.0F),
                hand_y + 5.0F
            );
            set_color(renderer, {120, 108, 90, 210});
            const SDL_FRect dust{
                top.x - facing * 15.0F - 4.0F,
                top.y + half_tile_height - 1.0F,
                8.0F,
                5.0F,
            };
            SDL_RenderFillRect(renderer, &dust);
        } else if (unit.kind == UnitKind::mangonel) {
            set_color(renderer, {105, 76, 43, 255});
            SDL_RenderLine(
                renderer,
                top.x - facing * 3.0F,
                hand_y + 7.0F,
                top.x + facing * (release ? 6.0F : 18.0F),
                hand_y + (release ? -12.0F : 5.0F)
            );
        } else if (
            unit.kind == UnitKind::archer ||
            unit.kind == UnitKind::crossbowman ||
            unit.kind == UnitKind::arbalester ||
            unit.kind == UnitKind::skirmisher ||
            unit.kind == UnitKind::elite_skirmisher
        ) {
            const float weapon_x = top.x + facing * 10.0F;
            set_color(renderer, {120, 78, 42, 255});
            SDL_RenderLine(
                renderer,
                weapon_x,
                hand_y - 8.0F,
                weapon_x + facing * 5.0F,
                hand_y
            );
            SDL_RenderLine(
                renderer,
                weapon_x + facing * 5.0F,
                hand_y,
                weapon_x,
                hand_y + 8.0F
            );
            SDL_RenderLine(
                renderer,
                weapon_x,
                hand_y - 8.0F,
                top.x + facing * (release ? 8.0F : 2.0F),
                hand_y
            );
            SDL_RenderLine(
                renderer,
                top.x + facing * (release ? 8.0F : 2.0F),
                hand_y,
                weapon_x,
                hand_y + 8.0F
            );
        } else {
            const float reach = release ? 20.0F : 13.0F;
            const float weapon_y = release ? hand_y + 7.0F : hand_y - 12.0F;
            set_color(renderer, {210, 214, 218, 255});
            SDL_RenderLine(
                renderer,
                top.x + facing * 3.0F,
                hand_y,
                top.x + facing * reach,
                weapon_y
            );
            SDL_RenderLine(
                renderer,
                top.x + facing * (reach - 3.0F),
                weapon_y - 3.0F,
                top.x + facing * (reach + 2.0F),
                weapon_y + 2.0F
            );
        }
    }

    if (unit.carrying_relic) {
        set_color(renderer, {255, 220, 75, 255});
        SDL_RenderLine(
            renderer,
            ground_top.x,
            ground_top.y - 42.0F,
            ground_top.x,
            ground_top.y - 31.0F
        );
        SDL_RenderLine(
            renderer,
            ground_top.x - 4.0F,
            ground_top.y - 38.0F,
            ground_top.x + 4.0F,
            ground_top.y - 38.0F
        );
    }
    const int maximum_hit_points = simulation.maximum_hit_points(unit);
    if (unit.hit_points < maximum_hit_points ||
        simulation.is_unit_selected(unit.id)) {
        render_health_bar(
            renderer,
            top.x,
            health_top,
            unit.hit_points,
            maximum_hit_points
        );
    }
    if (simulation.is_unit_selected(unit.id)) {
        outline_diamond(renderer, ground_top, {250, 220, 65, 255});
        const SDL_Color stance_color =
            unit.stance == UnitStance::aggressive
                ? SDL_Color{220, 65, 55, 255}
                : unit.stance == UnitStance::defensive
                ? SDL_Color{65, 145, 235, 255}
                : unit.stance == UnitStance::stand_ground
                ? SDL_Color{235, 235, 220, 255}
                : SDL_Color{125, 125, 125, 255};
        set_color(renderer, stance_color);
        const SDL_FRect stance_badge{
            ground_top.x - 4.0F,
            ground_top.y - 9.0F,
            8.0F,
            5.0F,
        };
        SDL_RenderFillRect(renderer, &stance_badge);
        if (unit.attack_moving) {
            const TilePosition line_origin = unit.patrolling
                ? unit.patrol_origin
                : unit.position;
            const TilePosition line_destination = unit.patrolling
                ? unit.patrol_destination
                : unit.attack_move_destination;
            const SDL_FPoint origin = tile_top(line_origin);
            const SDL_FPoint destination = tile_top(line_destination);
            const SDL_Color order_color = unit.patrolling
                ? SDL_Color{55, 205, 215, 255}
                : SDL_Color{220, 65, 55, 255};
            set_color(renderer, order_color);
            SDL_RenderLine(
                renderer,
                origin.x,
                origin.y + half_tile_height,
                destination.x,
                destination.y + half_tile_height
            );
            outline_diamond(
                renderer,
                destination,
                order_color
            );
            if (unit.patrolling) {
                outline_diamond(renderer, origin, order_color);
            }
        }
        if (unit.attacking_ground) {
            const SDL_FPoint destination =
                tile_top(unit.attack_ground_target);
            const SDL_Color order_color{225, 125, 35, 255};
            set_color(renderer, order_color);
            SDL_RenderLine(
                renderer,
                ground_top.x,
                ground_top.y + half_tile_height,
                destination.x,
                destination.y + half_tile_height
            );
            outline_diamond(renderer, destination, order_color);
            SDL_RenderLine(
                renderer,
                destination.x - 8.0F,
                destination.y + half_tile_height - 8.0F,
                destination.x + 8.0F,
                destination.y + half_tile_height + 8.0F
            );
            SDL_RenderLine(
                renderer,
                destination.x + 8.0F,
                destination.y + half_tile_height - 8.0F,
                destination.x - 8.0F,
                destination.y + half_tile_height + 8.0F
            );
        }
        if (unit.guard_target_id != 0) {
            std::optional<TilePosition> guard_position;
            if (unit.guard_target_is_building) {
                const auto target = std::ranges::find_if(
                    simulation.buildings(),
                    [&unit](const Building& building) {
                        return building.id == unit.guard_target_id;
                    }
                );
                if (target != simulation.buildings().end()) {
                    guard_position = target->position;
                }
            } else {
                const auto target = std::ranges::find_if(
                    simulation.units(),
                    [&unit](const Unit& candidate) {
                        return candidate.id == unit.guard_target_id;
                    }
                );
                if (target != simulation.units().end()) {
                    guard_position = target->position;
                }
            }
            if (guard_position) {
                const SDL_FPoint target = tile_top(*guard_position);
                set_color(renderer, {65, 220, 105, 255});
                SDL_RenderLine(
                    renderer,
                    ground_top.x,
                    ground_top.y + half_tile_height,
                    target.x,
                    target.y + half_tile_height
                );
                outline_diamond(
                    renderer,
                    target,
                    {65, 220, 105, 255}
                );
            }
        }
        if (!unit.waypoints.empty()) {
            SDL_FPoint previous = tile_top(unit.destination);
            const SDL_Color waypoint_color{245, 155, 45, 255};
            outline_diamond(
                renderer,
                previous,
                waypoint_color
            );
            for (TilePosition waypoint : unit.waypoints) {
                const SDL_FPoint next = tile_top(waypoint);
                set_color(renderer, waypoint_color);
                SDL_RenderLine(
                    renderer,
                    previous.x,
                    previous.y + half_tile_height,
                    next.x,
                    next.y + half_tile_height
                );
                outline_diamond(renderer, next, waypoint_color);
                previous = next;
            }
        }
    }
}

// Modern choice: no original diagnostic dump exists. Headless runs need a
// way to report the dimensions of the map the app actually loaded, because
// a screenshot cannot distinguish a small map from a large one that happens
// to be scrolled to a corner.
void report_map_dimensions(const Simulation& simulation) {
    const char* path = SDL_getenv("AOE_MAP_DIMENSION_PATH");
    if (path == nullptr || path[0] == '\0') return;
    static int reported_width = -1;
    static int reported_height = -1;
    static int reported_preview_width = -1;
    static int reported_preview_height = -1;
    const int width = simulation.map().width();
    const int height = simulation.map().height();
    const int preview_width = active_random_preview != nullptr
        ? active_random_preview->map.width()
        : -1;
    const int preview_height = active_random_preview != nullptr
        ? active_random_preview->map.height()
        : -1;
    if (width == reported_width &&
        height == reported_height &&
        preview_width == reported_preview_width &&
        preview_height == reported_preview_height) {
        return;
    }
    reported_width = width;
    reported_height = height;
    reported_preview_width = preview_width;
    reported_preview_height = preview_height;
    std::ofstream output(path, std::ios::trunc);
    if (!output) {
        SDL_Log("Could not write map dimensions to %s", path);
        return;
    }
    output << "map " << width << ' ' << height << '\n'
           << "tiles " << static_cast<long long>(width) * height << '\n';
    if (active_random_preview != nullptr) {
        output << "preview " << preview_width << ' '
               << preview_height << '\n'
               << "preview_tiles "
               << static_cast<long long>(preview_width) * preview_height
               << '\n';
    }
}

void capture_requested_frame(
    SDL_Renderer* renderer,
    const Simulation& simulation,
    float movement_alpha
) {
    static bool captured = false;
    const char* path = SDL_getenv("AOE_SCREENSHOT_PATH");
    if (captured || path == nullptr || path[0] == '\0') {
        return;
    }
    if (const char* requested_tick = SDL_getenv("AOE_SCREENSHOT_TICK")) {
        if (simulation.tick_number() <
            static_cast<std::uint64_t>(SDL_atoi(requested_tick))) {
            return;
        }
    }
    if (const char* requested_alpha = SDL_getenv("AOE_SCREENSHOT_ALPHA")) {
        if (movement_alpha < SDL_atof(requested_alpha)) {
            return;
        }
    }
    SDL_Surface* surface = SDL_RenderReadPixels(renderer, nullptr);
    if (surface == nullptr) {
        SDL_Log("Could not capture frame: %s", SDL_GetError());
        return;
    }
    if (!SDL_SaveBMP(surface, path)) {
        SDL_Log("Could not save captured frame: %s", SDL_GetError());
    } else {
        captured = true;
        const char* exit_after_capture =
            SDL_getenv("AOE_EXIT_AFTER_SCREENSHOT");
        if (exit_after_capture != nullptr &&
            exit_after_capture[0] != '\0' &&
            exit_after_capture[0] != '0') {
            SDL_Event quit{};
            quit.type = SDL_EVENT_QUIT;
            SDL_PushEvent(&quit);
        }
    }
    SDL_DestroySurface(surface);
}

std::string_view formation_name(FormationKind kind);

std::string selection_text(const Simulation& simulation) {
    if (simulation.selected_units().size() > 1) {
        std::ostringstream text;
        text << "Selected: " << simulation.selected_units().size()
             << " blue units  Formation "
             << formation_name(
                    simulation.formation_kind(active_view_player)
                )
             << "  Ctrl+F1..F5";
        const int regrouping = static_cast<int>(
            std::ranges::count_if(
                simulation.units(),
                [&simulation](const Unit& unit) {
                    return unit.moving &&
                        std::ranges::find(
                            simulation.selected_units(),
                            unit.id
                        ) != simulation.selected_units().end();
                }
            )
        );
        if (regrouping > 0) {
            text << "  Regrouping " << regrouping;
        }
        return text.str();
    }
    if (simulation.selected_unit()) {
        for (const Unit& unit : simulation.units()) {
            if (unit.id == simulation.selected_unit()) {
                std::ostringstream text;
                const UnitRules& rules = rules_for(unit.kind);
                text << "Selected: blue " << name(unit.kind)
                     << "  HP " << unit.hit_points
                     << '/' << simulation.maximum_hit_points(unit)
                     << "  ATK " << unit.attack
                     << "  ARM " << simulation.melee_armor(unit) << '/'
                     << simulation.pierce_armor(unit)
                     << "  Speed ";
                if (unit.kind == UnitKind::scout_cavalry ||
                    unit.kind == UnitKind::knight ||
                    unit.kind == UnitKind::cavalier ||
                    unit.kind == UnitKind::paladin ||
                    unit.kind == UnitKind::light_cavalry ||
                    unit.kind == UnitKind::hussar ||
                    unit.kind == UnitKind::war_elephant ||
                    unit.kind == UnitKind::elite_war_elephant) {
                    double speed =
                        (unit.kind == UnitKind::war_elephant ||
                         unit.kind == UnitKind::elite_war_elephant)
                        ? static_cast<double>(
                              simulation.unique_unit_movement_numerator(unit)
                          ) / 100.0
                        : (unit.kind == UnitKind::light_cavalry ||
                         unit.kind == UnitKind::hussar)
                        ? 1.50
                        : (unit.kind == UnitKind::knight ||
                         unit.kind == UnitKind::cavalier ||
                         unit.kind == UnitKind::paladin)
                        ? 1.35
                        : (simulation.age(unit.owner) >= Age::feudal
                            ? 1.55
                            : 1.20);
                    if (simulation.has_technology(
                            unit.owner, Technology::husbandry
                        )) {
                        speed *= 1.10;
                    }
                    text << std::fixed << std::setprecision(2) << speed;
                } else if (
                    unit.kind == UnitKind::battering_ram ||
                    unit.kind == UnitKind::mangonel
                ) {
                    text << 'x' << std::fixed << std::setprecision(2)
                         << static_cast<double>(
                                simulation
                                    .effective_siege_movement_numerator(unit)
                            ) / 100.0;
                } else if (unit.kind == UnitKind::missionary) {
                    int speed = rules.movement_speed_percent;
                    if (simulation.has_technology(
                            unit.owner, Technology::fervor
                        )) {
                        speed = speed * 115 / 100;
                    }
                    text << 'x' << std::fixed << std::setprecision(2)
                         << static_cast<double>(speed) / 100.0;
                } else {
                    text << (rules.movement_interval_ticks == 1
                                ? "fast"
                                : "normal");
                }
                const int regeneration =
                    simulation.berserk_regeneration_per_three_ticks(unit);
                if (regeneration > 0) {
                    text << "  Regen +" << regeneration << "/3 ticks";
                }
                if (rules.attack_range > 1) {
                    text << "  RNG "
                         << simulation.effective_attack_range(unit);
                    if (unit.kind == UnitKind::cannon_galleon ||
                        unit.kind == UnitKind::elite_cannon_galleon) {
                        text << "  MIN 3";
                    }
                }
                if (siege_engineers_unit(unit.kind)) {
                    int building_bonus = rules.bonus_vs_buildings;
                    if (simulation.has_technology(
                            unit.owner, Technology::siege_engineers
                        )) {
                        building_bonus = building_bonus * 120 / 100;
                    }
                    text << "  LOS "
                         << simulation.effective_unit_vision_range(unit)
                         << "  BLD +" << building_bonus;
                } else if (unit.kind == UnitKind::petard) {
                    const int building_bonus =
                        rules.bonus_vs_buildings +
                        (simulation.has_technology(
                             unit.owner, Technology::siege_engineers
                         ) ? 200 : 0);
                    text << "  LOS "
                         << simulation.effective_unit_vision_range(unit)
                         << "  BLD +" << building_bonus;
                }
                if (unit.kind == UnitKind::missionary) {
                    text << "  RECH " << unit.conversion_cooldown << '/'
                         << (simulation.has_technology(
                                 unit.owner, Technology::illumination
                             ) ? 10 : 20)
                         << "  Targets units";
                    if (simulation.has_technology(
                            unit.owner, Technology::redemption
                        )) {
                        text << "+buildings";
                    }
                    if (simulation.has_technology(
                            unit.owner, Technology::atonement
                        )) {
                        text << "+monks";
                    }
                    if (unit.conversion_target_id != 0) {
                        text << "  Converting #"
                             << unit.conversion_target_id;
                    } else if (unit.healing_target_id != 0) {
                        text << "  Healing #" << unit.healing_target_id;
                    }
                }
                if (unit.carried_amount > 0) {
                    text << "  Carrying " << unit.carried_amount << ' '
                         << name(unit.carried_resource);
                }
                if (unit.kind == UnitKind::villager) {
                    double gather_multiplier = 1.0;
                    if (unit.carried_resource == ResourceKind::wood) {
                        gather_multiplier *= simulation.has_technology(
                            unit.owner, Technology::double_bit_axe
                        ) ? 1.20 : 1.0;
                        gather_multiplier *= simulation.has_technology(
                            unit.owner, Technology::bow_saw
                        ) ? 1.20 : 1.0;
                        gather_multiplier *= simulation.has_technology(
                            unit.owner, Technology::two_man_saw
                        ) ? 1.10 : 1.0;
                    } else if (
                        unit.carried_resource == ResourceKind::gold
                    ) {
                        gather_multiplier *= simulation.has_technology(
                            unit.owner, Technology::gold_mining
                        ) ? 1.15 : 1.0;
                        gather_multiplier *= simulation.has_technology(
                            unit.owner, Technology::gold_shaft_mining
                        ) ? 1.15 : 1.0;
                    } else if (
                        unit.carried_resource == ResourceKind::stone
                    ) {
                        gather_multiplier *= simulation.has_technology(
                            unit.owner, Technology::stone_mining
                        ) ? 1.15 : 1.0;
                        gather_multiplier *= simulation.has_technology(
                            unit.owner, Technology::stone_shaft_mining
                        ) ? 1.15 : 1.0;
                    }
                    text << "  Gather x" << std::fixed
                         << std::setprecision(4) << gather_multiplier
                         << "  Carry " << unit.carried_amount << '/'
                         << ((simulation.has_technology(
                                  unit.owner, Technology::hand_cart
                              ) ? 18
                              : simulation.has_technology(
                                    unit.owner, Technology::wheelbarrow
                                ) ? 12 : 10) +
                             (unit.carried_resource == ResourceKind::food &&
                              simulation.has_technology(
                                  unit.owner, Technology::heavy_plow
                              ) ? 1 : 0) +
                             (simulation.civilization(unit.owner) ==
                                  Civilization::aztecs ? 3 : 0));
                    if (unit.has_resource_target) {
                        text << "  Remaining "
                             << simulation.map().resource_amount_at(
                                    unit.resource_target
                                );
                    }
                    if (simulation.has_technology(
                            unit.owner, Technology::sappers
                        )) {
                        const Building* target{};
                        if (unit.attack_target_is_building &&
                            unit.attack_target_id != 0) {
                            const auto found = std::ranges::find_if(
                                simulation.buildings(),
                                [&unit](const Building& building) {
                                    return building.id ==
                                        unit.attack_target_id;
                                }
                            );
                            if (found != simulation.buildings().end()) {
                                target = &*found;
                            }
                        }
                        if (target == nullptr) {
                            text << "  Sappers target-aware";
                        } else if (
                            target->kind == BuildingKind::fish_trap
                        ) {
                            text << "  Sappers +0 Fish Trap excluded";
                        } else {
                            text << "  Sappers class11 +15";
                            if (target->kind ==
                                    BuildingKind::watch_tower ||
                                target->kind ==
                                    BuildingKind::bombard_tower ||
                                target->kind ==
                                    BuildingKind::stone_wall) {
                                text << " class13 +15";
                            }
                        }
                    }
                }
                if (unit.attack_target_id != 0) {
                    text << "  Target #" << unit.attack_target_id;
                }
                if (unit.repair_target_id != 0) {
                    text << "  Repairing #" << unit.repair_target_id;
                }
                if (unit.attack_moving) {
                    if (unit.patrolling) {
                        text << "  Patrol " << unit.patrol_origin.x << ','
                             << unit.patrol_origin.y << " <> "
                             << unit.patrol_destination.x << ','
                             << unit.patrol_destination.y;
                    } else {
                        text << "  Attack-move "
                             << unit.attack_move_destination.x << ','
                             << unit.attack_move_destination.y;
                    }
                }
                if (unit.guard_target_id != 0) {
                    text << "  Guard #"
                         << unit.guard_target_id;
                }
                if (unit.attacking_ground) {
                    text << "  Attack ground "
                         << unit.attack_ground_target.x << ','
                         << unit.attack_ground_target.y;
                }
                if (!unit.waypoints.empty()) {
                    text << "  Waypoints " << unit.waypoints.size();
                }
                if (unit.kind == UnitKind::trade_cart ||
                    unit.kind == UnitKind::trade_cog) {
                    if (unit.trade_target_market_id != 0) {
                        text << "  Trade #"
                             << unit.trade_home_market_id
                             << "<->#" << unit.trade_target_market_id
                             << (unit.trade_returning
                                 ? " returning"
                                 : " outbound");
                        const Building* home{};
                        const Building* target{};
                        for (const Building& building :
                             simulation.buildings()) {
                            if (building.id ==
                                unit.trade_home_market_id) {
                                home = &building;
                            } else if (
                                building.id ==
                                unit.trade_target_market_id
                            ) {
                                target = &building;
                            }
                        }
                        if (home != nullptr && target != nullptr) {
                            const int distance =
                                std::abs(
                                    home->position.x -
                                    target->position.x
                                ) +
                                std::abs(
                                    home->position.y -
                                    target->position.y
                                );
                            text << "  Distance " << distance
                                 << "  Gold " << std::max(1, distance * 2);
                        }
                        if (unit.kind == UnitKind::trade_cog) {
                            int speed = 132;
                            if (simulation.has_technology(
                                    unit.owner, Technology::dry_dock
                                )) {
                                speed = speed * 115 / 100;
                            }
                            if (simulation.has_technology(
                                    unit.owner, Technology::caravan
                                )) {
                                speed = speed * 3 / 2;
                            }
                            text << "  Speed x" << std::fixed
                                 << std::setprecision(2)
                                 << static_cast<double>(speed) / 100.0;
                        }
                    } else {
                        text << "  No trade route";
                    }
                }
                if (unit.kind == UnitKind::fishing_ship &&
                    simulation.has_technology(
                        unit.owner, Technology::fish_trap_gate
                    )) {
                    text << "  P Fish Trap";
                }
                if (unit.kind == UnitKind::transport_ship) {
                    int passengers{};
                    for (const Unit& candidate : simulation.units()) {
                        passengers +=
                            candidate.garrisoned_in == unit.id ? 1 : 0;
                    }
                    text << "  Passengers " << passengers << "/5"
                         << (passengers > 0
                             ? "  Right-click shore to disembark"
                             : "  Units right-click ship to embark");
                }
                if (unit.kind == UnitKind::packed_trebuchet) {
                    text << "  \\ Unpack";
                } else if (unit.kind == UnitKind::trebuchet) {
                    text << "  \\ Pack";
                }
                text << "  Stance " << name(unit.stance);
                if (simulation.selected_units().size() > 1) {
                    text << "  Group "
                         << simulation.selected_units().size()
                         << "  Formation "
                         << formation_name(
                                simulation.formation_kind(unit.owner)
                            )
                         << "  Ctrl+F1..F5";
                }
                return text.str();
            }
        }
    }
    if (simulation.selected_building()) {
        for (const Building& building : simulation.buildings()) {
            if (building.id == simulation.selected_building()) {
                std::ostringstream text;
                const BuildingRules& rules = rules_for(building.kind);
                const bool defense_class_3_or_52 =
                    building.kind != BuildingKind::farm &&
                    building.kind != BuildingKind::fish_trap &&
                    building.kind != BuildingKind::palisade_wall &&
                    building.kind != BuildingKind::stone_wall &&
                    building.kind != BuildingKind::palisade_gate_x &&
                    building.kind != BuildingKind::palisade_gate_y &&
                    building.kind != BuildingKind::stone_gate_x &&
                    building.kind != BuildingKind::stone_gate_y;
                text << "Selected: blue " << name(building.kind)
                     << "  HP " << building.hit_points
                     << '/' << simulation.maximum_hit_points(building);
                if (is_defensive_garrison_building(building.kind)) {
                    text << "  Garrison "
                         << simulation.garrison_count(building.id);
                    if (building.kind == BuildingKind::town_center) {
                        text << "/15";
                    }
                }
                text << "  ARM " << simulation.melee_armor(building) << '/'
                     << simulation.pierce_armor(building);
                int effective_los = rules.vision_range;
                if (defense_class_3_or_52) {
                    effective_los += simulation.has_technology(
                        building.owner, Technology::town_watch
                    ) ? 4 : 0;
                    effective_los += simulation.has_technology(
                        building.owner, Technology::town_patrol
                    ) ? 4 : 0;
                }
                text << "  LOS " << effective_los;
                if (defense_class_3_or_52) {
                    const int class_11_armor =
                        (simulation.has_technology(
                             building.owner, Technology::masonry
                         ) ? 3 : 0) +
                        (simulation.has_technology(
                             building.owner, Technology::architecture
                         ) ? 3 : 0);
                    if (class_11_armor > 0) {
                        text << "  Class11 ARM +"
                             << class_11_armor;
                    }
                }
                if (rules.attack > 0) {
                    const bool fletching =
                        (building.kind == BuildingKind::castle ||
                         building.kind == BuildingKind::watch_tower ||
                         building.kind == BuildingKind::town_center) &&
                        simulation.has_technology(
                            building.owner,
                            Technology::fletching
                        );
                    const bool bodkin =
                        (building.kind == BuildingKind::castle ||
                         building.kind == BuildingKind::watch_tower ||
                         building.kind == BuildingKind::town_center) &&
                        simulation.has_technology(
                            building.owner,
                            Technology::bodkin_arrow
                        );
                    const bool bracer =
                        (building.kind == BuildingKind::castle ||
                         building.kind == BuildingKind::watch_tower ||
                         building.kind == BuildingKind::town_center) &&
                        simulation.has_technology(
                            building.owner,
                            Technology::bracer
                        );
                    const bool guard_tower =
                        building.kind == BuildingKind::watch_tower &&
                        simulation.has_technology(
                            building.owner,
                            Technology::guard_tower
                        );
                    const bool keep =
                        building.kind == BuildingKind::watch_tower &&
                        simulation.has_technology(
                            building.owner,
                            Technology::keep
                        );
                    text << "  ATK " << rules.attack +
                            (fletching ? 1 : 0) +
                            (bodkin ? 1 : 0) +
                            (bracer ? 1 : 0) +
                            (keep ? 3 : (guard_tower ? 2 : 0))
                         << "  RNG "
                         << simulation.effective_building_attack_range(
                                building
                            )
                         << "  ACC " << rules.accuracy_percent << '%';
                    if (simulation.has_technology(
                            building.owner, Technology::ballistics
                        ) &&
                        (building.kind == BuildingKind::town_center ||
                         building.kind == BuildingKind::castle ||
                         building.kind == BuildingKind::watch_tower ||
                         building.kind ==
                            BuildingKind::bombard_tower)) {
                        text << "  Ballistics tracking";
                    }
                    if (simulation.has_technology(
                            building.owner, Technology::heated_shot
                        )) {
                        if (building.kind == BuildingKind::castle) {
                            text << "  Ship +4";
                        } else if (
                            building.kind == BuildingKind::watch_tower ||
                            building.kind ==
                                BuildingKind::bombard_tower
                        ) {
                            text << "  Ship x2.25";
                        }
                    }
                    if (building.kind == BuildingKind::bombard_tower) {
                        text << "  MIN " << rules.minimum_attack_range;
                    }
                }
                if (!building.completed()) {
                    const int progress =
                        100 * (rules.construction_ticks -
                               building.construction_ticks_remaining) /
                        rules.construction_ticks;
                    text << "  Building " << progress << '%'
                         << "  Builders "
                         << building.builder_ids.size();
                } else if (building.age_research_ticks_remaining > 0) {
                    const AgeRules& age_rules =
                        rules_for(building.age_research_target);
                    const int progress =
                        100 * (age_rules.research_ticks -
                               building.age_research_ticks_remaining) /
                        age_rules.research_ticks;
                    text << "  " << name(building.age_research_target)
                         << ' ' << progress << '%';
                } else if (
                    building.technology_research_ticks_remaining > 0
                ) {
                    const TechnologyRules& technology_rules =
                        rules_for(building.technology_research_target);
                    const int progress =
                        100 * (technology_rules.research_ticks -
                               building.technology_research_ticks_remaining) /
                        technology_rules.research_ticks;
                    text << "  " << name(building.technology_research_target)
                         << ' ' << progress << '%';
                } else if (building.kind == BuildingKind::farm) {
                    text << "  Food " << building.resource_amount << '/'
                         << simulation.farm_capacity(building.owner);
                } else if (
                    building.kind == BuildingKind::fish_trap
                ) {
                    text << "  Food remaining "
                         << building.resource_amount;
                } else if (
                    building.kind == BuildingKind::wonder
                ) {
                    const int countdown =
                        simulation.victory_countdown(building.owner);
                    const VictoryCountdownKind kind =
                        simulation.countdown_kind(building.owner);
                    text << "  Wonder victory ";
                    if (kind == VictoryCountdownKind::wonder &&
                        countdown > 0) {
                        text << countdown << " ticks";
                    } else {
                        text << (building.completed()
                            ? "countdown pending"
                            : "starts on completion");
                    }
                } else if (
                    is_defensive_garrison_building(building.kind)
                ) {
                    text << "  Queue "
                         << building.production_queue.size();
                } else if (
                    building.kind == BuildingKind::monastery
                ) {
                    text << "  Relics " << building.relic_count
                         << "  Gold income +" << building.relic_count
                         << "  Queue "
                         << building.production_queue.size();
                    if (simulation.countdown_kind(building.owner) ==
                        VictoryCountdownKind::relic) {
                        text << "  Relic victory "
                             << simulation.victory_countdown(
                                    building.owner
                                )
                             << " ticks";
                    }
                } else if (
                    building.kind == BuildingKind::market
                ) {
                    const int fee =
                        simulation.civilization(building.owner) ==
                            Civilization::saracens
                        ? 5
                        : simulation.has_technology(
                              building.owner, Technology::guilds
                          ) ? 15 : 30;
                    const int tribute_fee =
                        simulation.has_technology(
                            building.owner, Technology::banking
                        ) ? 0
                        : simulation.has_technology(
                              building.owner, Technology::coinage
                          ) ? 20 : 30;
                    text << "  Fee " << fee << "%"
                         << "  Tribute fee " << tribute_fee << "%"
                         << "  Prices B/S"
                         << " F "
                         << simulation.market_buy_price(
                                MarketResource::food
                            )
                         << '/'
                         << simulation.market_sell_price(
                                MarketResource::food
                            )
                         << " W "
                         << simulation.market_buy_price(
                                MarketResource::wood
                            )
                         << '/'
                         << simulation.market_sell_price(
                                MarketResource::wood
                            )
                         << " S "
                         << simulation.market_buy_price(
                                MarketResource::stone
                            )
                         << '/'
                         << simulation.market_sell_price(
                                MarketResource::stone
                            );
                } else {
                    text << "  Queue " << building.production_queue.size();
                    if (!building.production_queue.empty()) {
                        const ProductionOrder& order =
                            building.production_queue.front();
                        const int total =
                            std::max(rules_for(order.kind).training_ticks, 1);
                        const int progress = std::clamp(
                            100 * (total - order.ticks_remaining) / total,
                            0,
                            100
                        );
                        text << "  " << name(order.kind)
                             << ' ' << progress << '%';
                    }
                }
                if (building.has_rally_point) {
                    text << "  Rally " << building.rally_point.x << ','
                         << building.rally_point.y;
                }
                return text.str();
            }
        }
    }
    return "Selected: none";
}

void render_minimap(
    SDL_Renderer* renderer,
    const Simulation& simulation,
    float hud_top,
    const CameraView& camera
) {
    static_cast<void>(hud_top);
    constexpr float padding = 7.0F;
    const int screen_width = view_pixel_width;
    const int screen_height = view_pixel_height + hud_height;
    const hud_layout::Rect exact_frame =
        hud_layout::anchored_large_panel(screen_width, screen_height);
    const SDL_FRect panel{
        static_cast<float>(exact_frame.x),
        static_cast<float>(exact_frame.y),
        static_cast<float>(exact_frame.width),
        static_cast<float>(exact_frame.height),
    };
    render_beveled_panel(renderer, panel, {42, 35, 27, 255});

    const float available_width = panel.w - padding * 2.0F;
    const float available_height = panel.h - padding * 2.0F;
    const float horizontal_scale = available_width /
        static_cast<float>(simulation.map().width() + simulation.map().height());
    const float vertical_scale = available_height /
        static_cast<float>(simulation.map().width() + simulation.map().height());
    const float cell_half_width = std::min(horizontal_scale, vertical_scale * 2.0F);
    const float cell_half_height = cell_half_width * 0.5F;
    const float center_x = panel.x + panel.w * 0.5F +
        static_cast<float>(simulation.map().height() - simulation.map().width()) *
            cell_half_width * 0.5F;
    const float top = panel.y + padding;
    const auto scaling_rows = minimap::build_scaling_rows(
        simulation.map().width(),
        simulation.map().height(),
        std::max(1, static_cast<int>(available_height))
    );
    const auto output_row_for_diagonal = [&scaling_rows](int diagonal) {
        const auto found = std::ranges::lower_bound(
            scaling_rows,
            diagonal,
            {},
            &minimap::ScalingRow::source_diagonal
        );
        if (found == scaling_rows.end()) {
            return scaling_rows.back().output_row;
        }
        return found->output_row;
    };
    const auto minimap_top = [=](TilePosition position) {
        return SDL_FPoint{
            center_x +
                static_cast<float>(position.x - position.y) * cell_half_width,
            top +
                static_cast<float>(
                    output_row_for_diagonal(position.x + position.y)
                ),
        };
    };
    const std::array<SDL_Color, 8> marker_colors = [] {
        std::array<SDL_Color, 8> colors{};
        for (std::size_t index = 0; index < colors.size(); ++index) {
            colors[index] = {
                minimap::player_marker_rgb[index][0],
                minimap::player_marker_rgb[index][1],
                minimap::player_marker_rgb[index][2],
                255,
            };
        }
        return colors;
    }();
    const auto marker_color = [&marker_colors](
        EntityOwner owner
    ) -> const SDL_Color* {
        const auto slot = owner.slot_index();
        return slot ? &marker_colors[*slot] : nullptr;
    };

    // One diamond per tile disappears once a tile is narrower than a pixel,
    // which is what happens from roughly 200 tiles up in an 80px HUD strip:
    // the geometry degenerates and the minimap renders empty. Group tiles
    // into square blocks just wide enough to stay above a pixel. A block
    // step of one reproduces the per-tile pass exactly, so smaller maps are
    // unchanged.
    const int block_step = std::max(
        1,
        static_cast<int>(std::ceil(1.0F / std::max(cell_half_width, 0.001F)))
    );
    const float block_half_width =
        cell_half_width * static_cast<float>(block_step);
    const float block_half_height =
        cell_half_height * static_cast<float>(block_step);
    for (int y = 0; y < simulation.map().height(); y += block_step) {
        for (int x = 0; x < simulation.map().width(); x += block_step) {
            const TilePosition position{x, y};
            SDL_Color color{5, 7, 6, 255};
            // Original executable proves an explored-tile minimap pass, but
            // not its colors/masks. Keep this procedural contract until
            // generated/fog_rendering_catalog.json's missing links close.
            if (!active_settings.fog ||
                simulation.is_explored_to_controller(
                    active_view_player, position
                )) {
                color = terrain_color(
                    simulation.map().terrain_at(position)
                );
                if (active_settings.fog &&
                    !simulation.is_visible_to_controller(active_view_player, position)) {
                    color = {
                        static_cast<Uint8>(color.r * 0.35F),
                        static_cast<Uint8>(color.g * 0.35F),
                        static_cast<Uint8>(color.b * 0.35F),
                        255,
                    };
                }
            }
            const SDL_FPoint cell_top = minimap_top(position);
            const SDL_FColor vertex_color{
                color.r / 255.0F,
                color.g / 255.0F,
                color.b / 255.0F,
                1.0F,
            };
            const std::array<SDL_Vertex, 4> vertices{{
                {{cell_top.x, cell_top.y}, vertex_color, {}},
                {{cell_top.x + block_half_width,
                  cell_top.y + block_half_height}, vertex_color, {}},
                {{cell_top.x,
                  cell_top.y + block_half_height * 2.0F}, vertex_color, {}},
                {{cell_top.x - block_half_width,
                  cell_top.y + block_half_height}, vertex_color, {}},
            }};
            constexpr std::array<int, 6> indices{{0, 1, 2, 0, 2, 3}};
            SDL_RenderGeometry(
                renderer,
                nullptr,
                vertices.data(),
                static_cast<int>(vertices.size()),
                indices.data(),
                static_cast<int>(indices.size())
            );
        }
    }

    for (const Building& building : simulation.buildings()) {
        if (building.owner != active_view_player &&
            !simulation.is_building_visible(active_view_player, building)) {
            continue;
        }
        const SDL_FPoint position = minimap_top(building.position);
        const SDL_Color* color = marker_color(building.owner);
        if (color == nullptr) continue;
        const minimap::InclusiveRect bounds = minimap::size_one_marker_rect(
            static_cast<int>(std::lround(position.x)),
            static_cast<int>(std::lround(position.y))
        );
        const SDL_FRect marker{
            static_cast<float>(bounds.left),
            static_cast<float>(bounds.top),
            static_cast<float>(bounds.right - bounds.left + 1),
            static_cast<float>(bounds.bottom - bounds.top + 1),
        };
        set_color(renderer, *color);
        SDL_RenderFillRect(renderer, &marker);
    }
    for (const Unit& unit : simulation.units()) {
        if (unit.garrisoned_in != 0 ||
            (unit.owner != active_view_player &&
             !simulation.is_visible_to_controller(active_view_player, unit.position))) {
            continue;
        }
        const SDL_FPoint position = minimap_top(unit.position);
        const SDL_Color* color = marker_color(unit.owner);
        if (color == nullptr) continue;
        const minimap::InclusiveRect bounds = minimap::size_one_marker_rect(
            static_cast<int>(std::lround(position.x)),
            static_cast<int>(std::lround(position.y))
        );
        const SDL_FRect marker{
            static_cast<float>(bounds.left),
            static_cast<float>(bounds.top),
            static_cast<float>(bounds.right - bounds.left + 1),
            static_cast<float>(bounds.bottom - bounds.top + 1),
        };
        set_color(renderer, *color);
        SDL_RenderFillRect(renderer, &marker);
    }
    for (const VisibleMapSignal& visible : active_map_signals) {
        const Uint64 age = SDL_GetTicks() - visible.received_ms;
        if (age >= 6000) continue;
        const SDL_FPoint position = minimap_top(visible.signal.tile);
        const SDL_Color* primary = marker_color(visible.signal.sender);
        if (primary == nullptr) continue;
        const minimap::SignalPhase phase =
            minimap::advance_type_0x112_signal_phase(
                false,
                static_cast<std::uint32_t>(age % 666U)
            );
        const minimap::InclusiveRect bounds =
            minimap::type_0x112_signal_outline(
                static_cast<int>(std::lround(position.x)),
                static_cast<int>(std::lround(position.y))
            );
        const SDL_FRect marker{
            static_cast<float>(bounds.left),
            static_cast<float>(bounds.top),
            static_cast<float>(bounds.right - bounds.left + 1),
            static_cast<float>(bounds.bottom - bounds.top + 1),
        };
        set_color(renderer, {0, 0, 0, 255});
        SDL_RenderRect(renderer, &marker);
        const SDL_Color phase_color = phase.alternate
            ? SDL_Color{0, 0, 0, 255}
            : *primary;
        set_color(renderer, phase_color);
        SDL_RenderRect(renderer, &marker);
    }

    const auto world_to_minimap = [=](float world_x, float world_y) {
        const float projected_x =
            (world_x - static_cast<float>(map_origin_x())) / half_tile_width;
        const float projected_y =
            (world_y - static_cast<float>(map_origin_y)) / half_tile_height;
        const float tile_x = (projected_y + projected_x) * 0.5F;
        const float tile_y = (projected_y - projected_x) * 0.5F;
        return SDL_FPoint{
            center_x + (tile_x - tile_y) * cell_half_width,
            top + static_cast<float>(output_row_for_diagonal(
                std::clamp(
                    static_cast<int>(tile_x + tile_y),
                    0,
                    simulation.map().width() +
                        simulation.map().height() - 2
                )
            )),
        };
    };
    const float view_width =
        static_cast<float>(view_pixel_width) / camera.zoom;
    const float view_height =
        static_cast<float>(view_pixel_height) / camera.zoom;
    const SDL_FPoint transformed_camera =
        world_to_minimap(camera.x, camera.y);
    const minimap::ViewportBounds viewport_bounds =
        minimap::proved_viewport_bounds(
            static_cast<int>(std::lround(transformed_camera.x)),
            static_cast<int>(std::lround(transformed_camera.y)),
            static_cast<int>(view_width / tile_width),
            static_cast<int>(view_height / tile_height),
            static_cast<double>(cell_half_width),
            1.0
        );
    static_cast<void>(viewport_bounds);
    // Bounds above are exact. Scanline polygon and 640 anchor remain
    // deliberately disabled because their raster/crop contracts are unproved.
    static_assert(!minimap::viewport_scanline_polygon_proved);
    static_assert(!minimap::map640_anchor_proved);
}

bool unit_available_to_player(
    const Simulation& simulation,
    Player player,
    UnitKind kind
) {
    return civilization_has_unit(simulation.civilization(player), kind) &&
        simulation.age(player) >= rules_for(kind).minimum_age;
}

bool building_available_to_player(
    const Simulation& simulation,
    Player player,
    BuildingKind kind
) {
    return civilization_has_building(
               simulation.civilization(player), kind
           ) &&
        simulation.age(player) >= rules_for(kind).minimum_age;
}

bool technology_available_to_player(
    const Simulation& simulation,
    Player player,
    Technology technology
) {
    return civilization_has_technology(
               simulation.civilization(player), technology
           ) &&
        simulation.age(player) >= rules_for(technology).minimum_age;
}

std::string_view formation_name(FormationKind kind) {
    switch (kind) {
        case FormationKind::compact: return "compact";
        case FormationKind::line: return "line";
        case FormationKind::box: return "box";
        case FormationKind::staggered: return "staggered";
        case FormationKind::flank: return "flank";
    }
    return "compact";
}

std::string_view computer_difficulty_name(ComputerDifficulty difficulty) {
    switch (difficulty) {
        case ComputerDifficulty::easiest: return "easiest";
        case ComputerDifficulty::easy: return "easy";
        case ComputerDifficulty::moderate: return "moderate";
        case ComputerDifficulty::hard: return "hard";
        case ComputerDifficulty::hardest: return "hardest";
    }
    return "moderate";
}

std::string_view computer_phase_name(ComputerStrategyPhase phase) {
    switch (phase) {
        case ComputerStrategyPhase::opening: return "opening";
        case ComputerStrategyPhase::developing: return "developing";
        case ComputerStrategyPhase::pressure: return "pressure";
        case ComputerStrategyPhase::conquest: return "conquest";
    }
    return "opening";
}

std::string_view computer_objective_name(ComputerObjective objective) {
    switch (objective) {
        case ComputerObjective::scout: return "scout";
        case ComputerObjective::defend: return "defend";
        case ComputerObjective::attack: return "attack";
        case ComputerObjective::naval: return "naval";
        case ComputerObjective::transport: return "transport";
        case ComputerObjective::trade: return "trade";
        case ComputerObjective::relic: return "relic";
        case ComputerObjective::wonder: return "wonder";
        case ComputerObjective::regroup: return "regroup";
    }
    return "scout";
}

void render_computer_status(
    SDL_Renderer* renderer,
    const ComputerPlayer& computer
) {
    const ComputerPlayerStatus& status = computer.status();
    const SDL_FRect panel{12.0F, 12.0F, 520.0F, 72.0F};
    set_color(renderer, {12, 14, 18, 224});
    SDL_RenderFillRect(renderer, &panel);
    set_color(renderer, {218, 174, 66, 255});
    SDL_RenderRect(renderer, &panel);

    std::ostringstream first;
    first << "AI DEBUG  " << computer_difficulty_name(computer.difficulty())
          << "  Phase " << computer_phase_name(status.phase)
          << "  Age goal " << name(status.age_goal)
          << "  Objective " << computer_objective_name(status.objective);
    SDL_RenderDebugText(renderer, 22.0F, 22.0F, first.str().c_str());

    std::ostringstream second;
    second << "Workers W/F/G/S "
           << status.resource_workers[0] << '/'
           << status.resource_workers[1] << '/'
           << status.resource_workers[2] << '/'
           << status.resource_workers[3]
           << "  Army M/R/C/S/N "
           << status.melee_units << '/' << status.ranged_units << '/'
           << status.cavalry_units << '/' << status.siege_units << '/'
           << status.naval_units;
    SDL_RenderDebugText(renderer, 22.0F, 42.0F, second.str().c_str());

    std::ostringstream third;
    third << "Desired " << name(status.desired_counter)
          << "  Stance " << (status.retreating ? "retreat" : "advance")
          << "  H " << status.home.x << ',' << status.home.y
          << "  R " << status.rally.x << ',' << status.rally.y;
    if (status.target) {
        third << "  T " << status.target->x << ',' << status.target->y;
    }
    SDL_RenderDebugText(renderer, 22.0F, 62.0F, third.str().c_str());
}

bool render_original_hud_background(
    SDL_Renderer* renderer,
    Civilization civilization
) {
    const int file_index =
        hud_layout::civilization_file_index(civilization);
    const LegacyHudBackground& background =
        active_legacy_sprites.civilization_hud_backgrounds[
            static_cast<std::size_t>(file_index)
        ];
    if (!background.complete()) return false;

    std::array<
        hud_layout::FrameMetrics,
        hud_layout::game_background_frame_count
    > metrics{};
    for (std::size_t index = 0; index < metrics.size(); ++index) {
        const LegacySprite& frame = background.frames[index];
        metrics[index] = {
            frame.width,
            frame.height,
            frame.hotspot_x,
            frame.hotspot_y,
        };
    }
    const hud_layout::Rect sibling =
        hud_layout::frame7_sibling_view(
            view_pixel_width, metrics[6], metrics[7]
        );
    const auto draws = hud_layout::background_composition(
        view_pixel_width,
        logical_screen_height,
        sibling.x,
        sibling.width,
        metrics
    );
    for (const hud_layout::BackgroundDraw& draw : draws) {
        // Frame 5 is a large legacy center ornament. Without the original
        // widgets layered over it, it obscures both the world and info panel.
        if (draw.frame == 5) continue;
        const LegacySprite& frame =
            background.frames[static_cast<std::size_t>(draw.frame)];
        const SDL_FRect destination{
            static_cast<float>(draw.anchor_x - frame.hotspot_x),
            static_cast<float>(draw.anchor_y - frame.hotspot_y),
            static_cast<float>(frame.width),
            static_cast<float>(frame.height),
        };
        SDL_RenderTexture(
            renderer, frame.texture, nullptr, &destination
        );
    }
    return true;
}

void render_hud(
    SDL_Renderer* renderer,
    const Simulation& simulation,
    std::optional<BuildingKind> pending_building,
    bool pending_attack_move,
    bool pending_attack_ground,
    bool pending_patrol,
    bool pending_guard,
    bool pending_conversion,
    bool pending_trade_route,
    bool paused,
    const std::string& control_group_status,
    const CameraView& camera
) {
    const float top = static_cast<float>(view_pixel_height);
    SDL_FRect background{
        0.0F,
        top,
        static_cast<float>(view_pixel_width),
        static_cast<float>(hud_height),
    };
    const bool original_background = render_original_hud_background(
        renderer,
        simulation.civilization(active_view_player)
    );
    if (!original_background) {
        set_color(renderer, {33, 25, 18, 255});
        SDL_RenderFillRect(renderer, &background);
        set_color(renderer, {196, 164, 98, 255});
        SDL_RenderLine(
            renderer,
            0.0F,
            top,
            static_cast<float>(view_pixel_width),
            top
        );
    }

    const SDL_FRect command_panel{
        5.0F,
        top + 4.0F,
        245.0F,
        static_cast<float>(hud_height) - 8.0F,
    };
    const SDL_FRect information_panel{
        255.0F,
        top + 4.0F,
        static_cast<float>(view_pixel_width) - 601.0F,
        static_cast<float>(hud_height) - 8.0F,
    };
    if (!original_background) {
        render_beveled_panel(renderer, command_panel, {55, 45, 32, 255});
        render_beveled_panel(
            renderer, information_panel, {48, 42, 34, 255}
        );
    }
    set_color(renderer, {239, 226, 185, 255});
    const SDL_Rect information_clip{
        static_cast<int>(information_panel.x + 8.0F),
        static_cast<int>(information_panel.y + 8.0F),
        std::max(0, static_cast<int>(information_panel.w - 16.0F)),
        std::max(0, static_cast<int>(information_panel.h - 16.0F)),
    };
    SDL_SetRenderClipRect(renderer, &information_clip);

    std::ostringstream economy;
    economy << ui_text("hud.wood") << ' '
            << simulation.economy(active_view_player).wood
            << "   " << ui_text("hud.food") << ' '
            << simulation.economy(active_view_player).food
            << "   " << ui_text("hud.gold") << ' '
            << simulation.economy(active_view_player).gold
            << "   " << ui_text("hud.stone") << ' '
            << simulation.economy(active_view_player).stone
            << "   " << ui_text("hud.population") << ' '
            << simulation.population(active_view_player)
            << '/' << simulation.population_capacity(active_view_player)
            << "   " << ui_text("hud.idle") << ' '
            << simulation.idle_villagers(active_view_player).size()
            << '/' << simulation.idle_military(active_view_player).size()
            << "   FARM QUEUE "
            << simulation.farm_reseed_queue(active_view_player);
    if (paused && simulation.outcome() == MatchOutcome::ongoing) {
        economy << "   PAUSED";
    }
    const bool complete_resource_icons = std::ranges::all_of(
        active_legacy_sprites.resource_icons,
        [](const LegacySprite& icon) {
            return icon.texture != nullptr;
        }
    );
    SDL_SetRenderClipRect(renderer, nullptr);
    if (complete_resource_icons) {
        const auto fields =
            hud_layout::resource_status_fields(view_pixel_width);
        const Economy& blue = simulation.economy(active_view_player);
        const std::array<int, 4> amounts{
            blue.wood, blue.food, blue.gold, blue.stone
        };
        const std::array<const char*, 4> labels{
            "WOOD", "FOOD", "GOLD", "STONE"
        };
        for (std::size_t index = 0; index < amounts.size(); ++index) {
            const hud_layout::Rect field = fields[index];
            const SDL_FRect icon{
                static_cast<float>(field.x),
                static_cast<float>(field.y),
                16.0F,
                16.0F,
            };
            SDL_RenderTexture(
                renderer,
                active_legacy_sprites.resource_icons[index].texture,
                nullptr,
                &icon
            );
            std::ostringstream amount;
            amount << labels[index] << ' ' << amounts[index];
            const std::string amount_text =
                hud_layout::truncate_debug_text(
                    amount.str(), std::max(0, field.width - 20)
                );
            SDL_RenderDebugText(
                renderer,
                static_cast<float>(field.x + 20),
                8.0F,
                amount_text.c_str()
            );
        }
        std::ostringstream population;
        population << "POP " << simulation.population(active_view_player)
                   << '/' << simulation.population_capacity(active_view_player)
                   << "  IDLE "
                   << simulation.idle_villagers(active_view_player).size()
                   << '/' << simulation.idle_military(active_view_player).size();
        if (paused && simulation.outcome() == MatchOutcome::ongoing) {
            population << " PAUSED";
        }
        const hud_layout::Rect population_field = fields.back();
        const std::string population_text =
            hud_layout::truncate_debug_text(
                population.str(), population_field.width
            );
        SDL_RenderDebugText(
            renderer,
            static_cast<float>(population_field.x),
            8.0F,
            population_text.c_str()
        );
    } else {
        const std::string economy_text =
            hud_layout::truncate_debug_text(
                economy.str(), view_pixel_width - 20
            );
        SDL_RenderDebugText(
            renderer, 10.0F, 8.0F, economy_text.c_str()
        );
    }
    SDL_SetRenderClipRect(renderer, &information_clip);
    set_color(
        renderer,
        original_background
            ? SDL_Color{54, 38, 23, 255}
            : SDL_Color{239, 226, 185, 255}
    );
    const SelectionPanelModel selection_panel =
        build_selection_panel(
            simulation,
            active_view_player,
            active_command_page,
            active_command_subpage);
    const bool observer_mode =
        simulation.observer_perspective(active_view_player);
    const bool has_selection =
        simulation.selected_unit() || simulation.selected_building();
    if (observer_mode) {
        SDL_SetRenderClipRect(renderer, nullptr);
        const SDL_FRect badge{270.0F, top + 16.0F, 142.0F, 30.0F};
        render_beveled_panel(renderer, badge, {75, 61, 40, 255});
        set_color(renderer, {245, 215, 122, 255});
        SDL_RenderDebugText(
            renderer, badge.x + 18.0F, badge.y + 10.0F, "OBSERVER"
        );
        SDL_SetRenderClipRect(renderer, &information_clip);
    }
    if (has_selection) {
        if (information_clip.w >= 180) {
            const SDL_FRect portrait{
                information_panel.x + 15.0F,
                top + 30.0F,
                72.0F,
                72.0F
            };
            set_color(renderer, {26, 20, 14, 255});
            SDL_RenderFillRect(renderer, &portrait);
            set_color(renderer, {196, 164, 98, 255});
            SDL_RenderRect(renderer, &portrait);
            const LegacySprite* portrait_sprite = nullptr;
            if (simulation.selected_unit()) {
                const auto selected = std::ranges::find(
                    simulation.units(),
                    *simulation.selected_unit(),
                    &Unit::id
                );
                if (selected != simulation.units().end()) {
                    const LegacyAnimation* animation =
                        legacy_action_for(simulation, *selected, false);
                    if (animation != nullptr && !animation->frames.empty() &&
                        animation->frames.front().texture != nullptr) {
                        portrait_sprite = &animation->frames.front();
                    }
                }
            } else if (simulation.selected_building()) {
                const auto selected = std::ranges::find(
                    simulation.buildings(),
                    *simulation.selected_building(),
                    &Building::id
                );
                if (selected != simulation.buildings().end()) {
                    const auto binding =
                        ui_icons::building(selected->kind);
                    if (binding) {
                        const auto icon =
                            active_legacy_sprites.building_command_icons.find(
                                binding->frame
                            );
                        if (icon !=
                            active_legacy_sprites
                                .building_command_icons.end()) {
                            portrait_sprite = &icon->second;
                        }
                    }
                }
            }
            if (portrait_sprite != nullptr) {
                const SDL_Rect portrait_clip{
                    static_cast<int>(portrait.x + 2.0F),
                    static_cast<int>(portrait.y + 2.0F),
                    static_cast<int>(portrait.w - 4.0F),
                    static_cast<int>(portrait.h - 4.0F),
                };
                SDL_SetRenderClipRect(renderer, &portrait_clip);
                const hud_layout::FloatRect fitted = hud_layout::contain(
                    portrait_sprite->width,
                    portrait_sprite->height,
                    {
                        portrait.x + 2.0F,
                        portrait.y + 2.0F,
                        portrait.w - 4.0F,
                        portrait.h - 4.0F,
                    }
                );
                const SDL_FRect destination{
                    fitted.x, fitted.y, fitted.width, fitted.height
                };
                SDL_RenderTexture(
                    renderer,
                    portrait_sprite->texture,
                    nullptr,
                    &destination
                );
                SDL_SetRenderClipRect(renderer, &information_clip);
            } else {
                set_color(renderer, {104, 91, 68, 255});
                const SDL_FRect head{
                    portrait.x + 28.0F, portrait.y + 14.0F, 16.0F, 16.0F
                };
                const SDL_FRect body{
                    portrait.x + 20.0F, portrait.y + 34.0F, 32.0F, 26.0F
                };
                SDL_RenderFillRect(renderer, &head);
                SDL_RenderFillRect(renderer, &body);
            }
            set_color(
                renderer,
                original_background
                    ? SDL_Color{54, 38, 23, 255}
                    : SDL_Color{239, 226, 185, 255}
            );
            const float text_x = portrait.x + portrait.w + 12.0F;
            const int text_width = std::max(
                0,
                information_clip.x + information_clip.w -
                    static_cast<int>(text_x) - 8
            );
            const std::string title = hud_layout::truncate_debug_text(
                selection_panel.title, text_width
            );
            SDL_RenderDebugText(
                renderer, text_x, top + 31.0F, title.c_str()
            );
            std::ostringstream detail;
            detail << "HP " << selection_panel.hit_points << '/'
                   << selection_panel.maximum_hit_points
                   << "  " << selection_panel.status;
            if (selection_panel.garrison_count > 0) {
                detail << "  GARRISON " << selection_panel.garrison_count;
            }
            if (selection_panel.carried_amount > 0) {
                detail << "  CARRY " << selection_panel.carried_amount << ' '
                       << name(selection_panel.carried_resource);
            }
            const std::string detail_text = hud_layout::truncate_debug_text(
                detail.str(), text_width
            );
            SDL_RenderDebugText(
                renderer, text_x, top + 51.0F, detail_text.c_str()
            );
            if (selection_panel.progress_percent >= 0) {
                const SDL_FRect track{
                    text_x,
                    top + 75.0F,
                    static_cast<float>(std::min(260, text_width)),
                    8.0F
                };
                set_color(renderer, {8, 10, 12, 255});
                SDL_RenderFillRect(renderer, &track);
                const SDL_FRect fill{
                    track.x, track.y,
                    track.w * selection_panel.progress_percent / 100.0F,
                    track.h,
                };
                set_color(renderer, {196, 160, 58, 255});
                SDL_RenderFillRect(renderer, &fill);
            }
        }
        SDL_SetRenderClipRect(renderer, nullptr);
        for (std::size_t index = 0;
             index < selection_panel.commands.size();
             ++index) {
            const CommandButtonModel& command =
                selection_panel.commands[index];
            if (command.grid_slot >= 15) continue;
            const int column = static_cast<int>(command.grid_slot % 5);
            const int row = static_cast<int>(command.grid_slot / 5);
            const SDL_FRect button{
                37.0F + column * 41.0F,
                top + 31.0F + row * 41.0F,
                40.0F, 40.0F,
            };
            const bool command_enabled =
                command.enabled && !observer_mode;
            const bool pressed =
                command_enabled &&
                active_command_hover ==
                    static_cast<int>(command.grid_slot) &&
                (SDL_GetMouseState(nullptr, nullptr) &
                 SDL_BUTTON_MASK(SDL_BUTTON_LEFT)) != 0;
            render_beveled_panel(
                renderer, button,
                !command_enabled ? SDL_Color{45, 43, 39, 255} :
                command.selected ? SDL_Color{128, 91, 31, 255} :
                active_command_hover ==
                    static_cast<int>(command.grid_slot)
                    ? SDL_Color{104, 78, 38, 255}
                    : SDL_Color{64, 51, 34, 255}
            );
            set_color(
                renderer,
                command_enabled ? SDL_Color{236, 220, 178, 255}
                                : SDL_Color{117, 112, 102, 255}
            );
            float label_x = button.x + 3.0F;
            const LegacySprite* icon_sprite = nullptr;
            if (command.icon &&
                command.icon->sheet == ui_icons::command_sheet) {
                const auto icon =
                    active_legacy_sprites.action_command_icons.find(
                        command.icon->frame);
                if (icon !=
                    active_legacy_sprites.action_command_icons.end()) {
                    icon_sprite = &icon->second;
                }
            } else if (command.icon &&
                command.icon->evidence ==
                    ui_icons::Evidence::exact_executable_dispatch &&
                command.icon->sheet == ui_icons::unit_sheet) {
                const auto icon =
                    active_legacy_sprites.unit_command_icons.find(
                        command.icon->frame);
                if (icon != active_legacy_sprites.unit_command_icons.end()) {
                    icon_sprite = &icon->second;
                }
            } else if (command.icon &&
                command.icon->evidence ==
                    ui_icons::Evidence::exact_executable_dispatch &&
                command.icon->sheet == ui_icons::technology_sheet) {
                const auto icon =
                    active_legacy_sprites.technology_command_icons.find(
                        command.icon->frame);
                if (icon !=
                    active_legacy_sprites.technology_command_icons.end()) {
                    icon_sprite = &icon->second;
                }
            } else if (command.icon &&
                command.icon->evidence ==
                    ui_icons::Evidence::exact_executable_dispatch &&
                command.icon->sheet == ui_icons::building_sheet) {
                const auto icon =
                    active_legacy_sprites.building_command_icons.find(
                        command.icon->frame);
                if (icon !=
                    active_legacy_sprites.building_command_icons.end()) {
                    icon_sprite = &icon->second;
                }
            }
            if (icon_sprite != nullptr && icon_sprite->texture != nullptr) {
                SDL_SetTextureColorMod(
                    icon_sprite->texture,
                    command_enabled ? 255 : 112,
                    command_enabled ? 255 : 112,
                    command_enabled ? 255 : 112
                );
                const SDL_FRect icon_box{
                    button.x + 2.0F + (pressed ? 1.0F : 0.0F),
                    button.y + 2.0F + (pressed ? 1.0F : 0.0F),
                    36.0F,
                    36.0F,
                };
                SDL_RenderTexture(
                    renderer, icon_sprite->texture, nullptr, &icon_box
                );
                SDL_SetTextureColorMod(icon_sprite->texture, 255, 255, 255);
            }
            SDL_RenderDebugText(
                renderer, label_x, button.y + 27.0F,
                command.hotkey.substr(0, 4).c_str()
            );
        }
        const auto hovered_command = std::ranges::find_if(
            selection_panel.commands,
            [](const CommandButtonModel& command) {
                return active_command_hover >= 0 &&
                    command.grid_slot ==
                        static_cast<std::size_t>(active_command_hover);
            }
        );
        if (hovered_command != selection_panel.commands.end()) {
            const CommandButtonModel& hovered = *hovered_command;
            const SDL_FRect tooltip{5.0F, top + 2.0F, 240.0F, 38.0F};
            render_beveled_panel(renderer, tooltip, {31, 25, 18, 245});
            const std::string heading =
                hovered.label + " [" + hovered.hotkey + "]";
            set_color(renderer, {239, 226, 185, 255});
            SDL_RenderDebugText(
                renderer, tooltip.x + 4.0F, tooltip.y + 5.0F,
                heading.c_str()
            );
            SDL_RenderDebugText(
                renderer, tooltip.x + 4.0F, tooltip.y + 19.0F,
                hovered.tooltip.substr(0, 38).c_str()
            );
        }
        SDL_SetRenderClipRect(renderer, &information_clip);
    } else {
        SDL_RenderDebugText(
            renderer, 270.0F, top + 30.0F,
            selection_text(simulation).c_str()
        );
    }
    const bool selected_dock =
        simulation.selected_building() &&
        std::ranges::any_of(
            simulation.buildings(),
            [&simulation](const Building& building) {
                return building.id == *simulation.selected_building() &&
                    building.kind == BuildingKind::dock;
            }
        );
    const bool selected_castle =
        simulation.selected_building() &&
        std::ranges::any_of(
            simulation.buildings(),
            [&simulation](const Building& building) {
                return building.id == *simulation.selected_building() &&
                    building.kind == BuildingKind::castle;
            }
        );
    const bool selected_blacksmith =
        simulation.selected_building() &&
        std::ranges::any_of(
            simulation.buildings(),
            [&simulation](const Building& building) {
                return building.id == *simulation.selected_building() &&
                    building.kind == BuildingKind::blacksmith;
            }
        );
    const bool selected_barracks =
        simulation.selected_building() &&
        std::ranges::any_of(
            simulation.buildings(),
            [&simulation](const Building& building) {
                return building.id == *simulation.selected_building() &&
                    building.kind == BuildingKind::barracks;
            }
        );
    const bool selected_siege_workshop =
        simulation.selected_building() &&
        std::ranges::any_of(
            simulation.buildings(),
            [&simulation](const Building& building) {
                return building.id == *simulation.selected_building() &&
                    building.kind == BuildingKind::siege_workshop;
            }
        );
    const bool selected_stable =
        simulation.selected_building() &&
        std::ranges::any_of(
            simulation.buildings(),
            [&simulation](const Building& building) {
                return building.id == *simulation.selected_building() &&
                    building.kind == BuildingKind::stable;
            }
        );
    const bool selected_archery_range =
        simulation.selected_building() &&
        std::ranges::any_of(
            simulation.buildings(),
            [&simulation](const Building& building) {
                return building.id == *simulation.selected_building() &&
                    building.kind == BuildingKind::archery_range;
            }
        );
    const bool selected_university =
        simulation.selected_building() &&
        std::ranges::any_of(
            simulation.buildings(),
            [&simulation](const Building& building) {
                return building.id == *simulation.selected_building() &&
                    building.kind == BuildingKind::university;
            }
        );
    const bool selected_monastery =
        simulation.selected_building() &&
        std::ranges::any_of(
            simulation.buildings(),
            [&simulation](const Building& building) {
                return building.id == *simulation.selected_building() &&
                    building.kind == BuildingKind::monastery;
            }
        );
    const auto selected_kind = [&simulation](BuildingKind kind) {
        return simulation.selected_building() &&
            std::ranges::any_of(
                simulation.buildings(),
                [&simulation, kind](const Building& building) {
                    return building.id == *simulation.selected_building() &&
                        building.kind == kind;
                }
            );
    };
    const bool selected_mill = selected_kind(BuildingKind::mill);
    const bool selected_lumber_camp =
        selected_kind(BuildingKind::lumber_camp);
    const bool selected_mining_camp =
        selected_kind(BuildingKind::mining_camp);
    const bool selected_town_center =
        selected_kind(BuildingKind::town_center);
    const bool selected_market = selected_kind(BuildingKind::market);
    std::string building_controls;
    const auto append_control = [&building_controls](
        std::string_view key,
        std::string_view label
    ) {
        if (!building_controls.empty()) {
            building_controls += "  ";
        }
        building_controls += key;
        building_controls += ' ';
        building_controls += label;
    };
    if (selected_dock) {
        building_controls = "Dock:";
        constexpr std::array dock_units{
            std::pair{"F", UnitKind::fishing_ship},
            std::pair{"Q", UnitKind::trade_cog},
            std::pair{"G", UnitKind::galley},
            std::pair{"W", UnitKind::war_galley},
            std::pair{"E", UnitKind::galleon},
            std::pair{"T", UnitKind::transport_ship},
            std::pair{"R", UnitKind::fire_ship},
            std::pair{"D", UnitKind::demolition_ship},
            std::pair{"C", UnitKind::cannon_galleon},
            std::pair{"L", UnitKind::longboat},
            std::pair{"K", UnitKind::turtle_ship},
        };
        for (const auto& [key, kind] : dock_units) {
            if (unit_available_to_player(
                    simulation, active_view_player, kind
                )) {
                append_control(key, name(kind));
            }
        }
        constexpr std::array dock_technologies{
            std::pair{"Y", Technology::careening},
            std::pair{"U", Technology::dry_dock},
            std::pair{"I", Technology::shipwright},
        };
        for (const auto& [key, technology] : dock_technologies) {
            if (technology_available_to_player(
                    simulation, active_view_player, technology
                ) &&
                !simulation.has_technology(active_view_player, technology)) {
                append_control(key, name(technology));
            }
        }
    } else if (selected_market) {
        building_controls = "Market:";
        constexpr std::array market_technologies{
            std::pair{"C", Technology::coinage},
            std::pair{"B", Technology::banking},
            std::pair{"Alt+C", Technology::cartography},
            std::pair{"V", Technology::caravan},
            std::pair{"G", Technology::guilds},
        };
        for (const auto& [key, technology] : market_technologies) {
            if (technology_available_to_player(
                    simulation, active_view_player, technology
                ) &&
                !simulation.has_technology(active_view_player, technology)) {
                append_control(key, name(technology));
            }
        }
        append_control("Alt+F/W/S", "buy");
        append_control("Shift+Alt+F/W/S", "sell");
        if (simulation.diplomacy(
                active_view_player,
                opposing_player(active_view_player)
            ) ==
            Diplomacy::ally) {
            append_control("Shift+1/2/3/4", "tribute 100 W/F/G/S");
        }
    } else if (selected_castle) {
        building_controls = "Castle:";
        struct UniqueControl {
            std::string_view key;
            UnitKind unit;
            Technology elite;
        };
        constexpr std::array unique_controls{
            UniqueControl{"B", UnitKind::longbowman,
                          Technology::elite_longbowman},
            UniqueControl{"X", UnitKind::throwing_axeman,
                          Technology::elite_throwing_axeman},
            UniqueControl{"H", UnitKind::huskarl,
                          Technology::elite_huskarl},
            UniqueControl{"Q", UnitKind::teutonic_knight,
                          Technology::elite_teutonic_knight},
            UniqueControl{"J", UnitKind::samurai,
                          Technology::elite_samurai},
            UniqueControl{"N", UnitKind::chu_ko_nu,
                          Technology::elite_chu_ko_nu},
            UniqueControl{"C", UnitKind::cataphract,
                          Technology::elite_cataphract},
            UniqueControl{"P", UnitKind::war_elephant,
                          Technology::elite_war_elephant},
            UniqueControl{"M", UnitKind::mameluke,
                          Technology::elite_mameluke},
            UniqueControl{"Y", UnitKind::janissary,
                          Technology::elite_janissary},
            UniqueControl{"Z", UnitKind::berserk,
                          Technology::elite_berserk},
            UniqueControl{"G", UnitKind::mangudai,
                          Technology::elite_mangudai},
            UniqueControl{"A", UnitKind::jaguar_warrior,
                          Technology::elite_jaguar_warrior},
            UniqueControl{"U", UnitKind::plumed_archer,
                          Technology::elite_plumed_archer},
            UniqueControl{"O", UnitKind::conquistador,
                          Technology::elite_conquistador},
            UniqueControl{"T", UnitKind::tarkan,
                          Technology::elite_tarkan},
            UniqueControl{"W", UnitKind::woad_raider,
                          Technology::elite_woad_raider},
        };
        for (const UniqueControl& control : unique_controls) {
            if (!unit_available_to_player(
                    simulation, active_view_player, control.unit
                )) {
                continue;
            }
            append_control(control.key, name(control.unit));
            if (technology_available_to_player(
                    simulation, active_view_player, control.elite
                ) &&
                !simulation.has_technology(
                    active_view_player, control.elite
                )) {
                append_control(
                    std::string{"Shift+"} + std::string{control.key},
                    "elite"
                );
            }
        }
        constexpr std::array unique_technologies{
            std::pair{"8", Technology::yeomen},
            std::pair{"9", Technology::bearded_axe},
            std::pair{"0", Technology::anarchy},
            std::pair{"F7", Technology::crenellations},
            std::pair{"F6", Technology::kataparuto},
            std::pair{"F4", Technology::rocketry},
            std::pair{"F3", Technology::logistica},
            std::pair{"F2", Technology::mahouts},
            std::pair{"F1", Technology::zealotry},
            std::pair{"F5", Technology::artillery},
            std::pair{"F12", Technology::drill},
            std::pair{"=", Technology::berserkergang},
            std::pair{"F10", Technology::supremacy},
            std::pair{"F9", Technology::atheism},
            std::pair{"F8", Technology::shinkichon},
            std::pair{"F11", Technology::el_dorado},
        };
        for (const auto& [key, technology] : unique_technologies) {
            if (!technology_available_to_player(
                    simulation, active_view_player, technology
                ) ||
                simulation.has_technology(active_view_player, technology)) {
                continue;
            }
            const TechnologyRules& technology_rules =
                rules_for(technology);
            std::ostringstream label;
            label << name(technology) << " (";
            bool wrote_cost = false;
            const auto add_cost = [&label, &wrote_cost](
                int amount,
                std::string_view resource
            ) {
                if (amount <= 0) return;
                if (wrote_cost) label << '/';
                label << amount << resource;
                wrote_cost = true;
            };
            add_cost(technology_rules.wood_cost, "W");
            add_cost(technology_rules.food_cost, "F");
            add_cost(technology_rules.gold_cost, "G");
            add_cost(technology_rules.stone_cost, "S");
            label << ')';
            append_control(key, label.str());
        }
        if (technology_available_to_player(
                simulation, active_view_player, Technology::conscription
            ) &&
            !simulation.has_technology(
                active_view_player, Technology::conscription
            )) {
            append_control("Alt+V", "Conscription");
        }
        constexpr std::array castle_defensive_technologies{
            std::pair{"Alt+O", Technology::hoardings},
            std::pair{"Alt+P", Technology::sappers},
        };
        for (const auto& [key, technology] :
             castle_defensive_technologies) {
            if (technology_available_to_player(
                    simulation, active_view_player, technology
                ) &&
                !simulation.has_technology(active_view_player, technology)) {
                append_control(key, name(technology));
            }
        }
        if (unit_available_to_player(
                simulation, active_view_player, UnitKind::trebuchet
            )) {
            append_control("[", "trebuchet");
        }
        if (unit_available_to_player(
                simulation, active_view_player, UnitKind::petard
            )) {
            append_control("R", "petard");
        }
        if (building_controls == "Castle:") {
            building_controls += " unique unit unavailable";
        }
    } else if (selected_blacksmith) {
        building_controls = "Blacksmith:";
        constexpr std::array blacksmith_technologies{
            std::pair{"F", Technology::fletching},
            std::pair{"Alt+F", Technology::bodkin_arrow},
            std::pair{"Shift+Alt+F", Technology::bracer},
            std::pair{"G", Technology::forging},
            std::pair{"Alt+G", Technology::iron_casting},
            std::pair{"Shift+Alt+G", Technology::blast_furnace},
            std::pair{"Alt+1", Technology::scale_mail_armor},
            std::pair{"Alt+2", Technology::chain_mail_armor},
            std::pair{"Alt+3", Technology::plate_mail_armor},
            std::pair{"Alt+4", Technology::scale_barding_armor},
            std::pair{"Alt+5", Technology::chain_barding_armor},
            std::pair{"Alt+6", Technology::plate_barding_armor},
        };
        for (const auto& [key, technology] : blacksmith_technologies) {
            if (technology_available_to_player(
                    simulation, active_view_player, technology
                ) &&
                !simulation.has_technology(active_view_player, technology)) {
                append_control(key, name(technology));
            }
        }
        if (building_controls == "Blacksmith:") {
            building_controls += " no available research";
        }
    } else if (selected_barracks) {
        building_controls = "Barracks:";
        if (unit_available_to_player(
                simulation, active_view_player, UnitKind::spearman
            )) {
            append_control("Z", "spearman");
        }
        if (technology_available_to_player(
                simulation, active_view_player, Technology::pikeman
            ) &&
            !simulation.has_technology(active_view_player, Technology::pikeman)) {
            append_control("Alt+Z", "pikeman");
        }
        if (technology_available_to_player(
                simulation, active_view_player, Technology::halberdier
            ) &&
            !simulation.has_technology(
                active_view_player, Technology::halberdier
            )) {
            append_control("Shift+Alt+Z", "halberdier");
        }
        if (unit_available_to_player(
                simulation, active_view_player, UnitKind::eagle_warrior
            )) {
            append_control("E", "eagle warrior");
        }
        if (technology_available_to_player(
                simulation,
                active_view_player,
                Technology::elite_eagle_warrior
            ) &&
            !simulation.has_technology(
                active_view_player, Technology::elite_eagle_warrior
            )) {
            append_control("Alt+E", "elite eagle");
        }
    } else if (selected_archery_range) {
        building_controls = "Archery Range:";
        if (simulation.has_technology(
                active_view_player, Technology::chemistry
            ) &&
            unit_available_to_player(
                simulation, active_view_player, UnitKind::hand_cannoneer
            )) {
            append_control("Alt+H", "hand cannoneer");
        } else {
            append_control("-", "Chemistry required");
        }
    } else if (selected_university) {
        building_controls = "University:";
        constexpr std::array defensive_university_technologies{
            std::pair{"Alt+M", Technology::masonry},
            std::pair{"Shift+Alt+M", Technology::architecture},
            std::pair{"Alt+B", Technology::ballistics},
            std::pair{"Alt+H", Technology::heated_shot},
        };
        for (const auto& [key, technology] :
             defensive_university_technologies) {
            if (technology_available_to_player(
                    simulation, active_view_player, technology
                ) &&
                !simulation.has_technology(active_view_player, technology)) {
                append_control(key, name(technology));
            }
        }
        if (technology_available_to_player(
                simulation, active_view_player, Technology::chemistry
            ) &&
            !simulation.has_technology(
                active_view_player, Technology::chemistry
            )) {
            append_control("Alt+0", "Chemistry");
        }
        if (technology_available_to_player(
                simulation, active_view_player, Technology::siege_engineers
            ) &&
            !simulation.has_technology(
                active_view_player, Technology::siege_engineers
            )) {
            append_control("Alt+S", "Siege Engineers");
        }
        if (technology_available_to_player(
                simulation, active_view_player, Technology::bombard_tower
            ) &&
            !simulation.has_technology(
                active_view_player, Technology::bombard_tower
            )) {
            append_control("Alt+7", "Bombard Tower");
        }
    } else if (selected_monastery) {
        building_controls = "Monastery:";
        if (unit_available_to_player(
                simulation, active_view_player, UnitKind::monk
            )) {
            append_control("M", "monk");
        }
        if (unit_available_to_player(
                simulation, active_view_player, UnitKind::missionary
            )) {
            append_control("R", "missionary");
        }
        constexpr std::array monastery_technologies{
            std::pair{"F1", Technology::sanctity},
            std::pair{"F2", Technology::fervor},
            std::pair{"F3", Technology::redemption},
            std::pair{"F4", Technology::atonement},
            std::pair{"F5", Technology::illumination},
            std::pair{"F6", Technology::block_printing},
            std::pair{"F7", Technology::faith},
            std::pair{"F8", Technology::theocracy},
            std::pair{"F9", Technology::heresy},
        };
        for (const auto& [key, technology] : monastery_technologies) {
            if (technology_available_to_player(
                    simulation, active_view_player, technology
                ) &&
                !simulation.has_technology(active_view_player, technology)) {
                append_control(key, name(technology));
            }
        }
    } else if (selected_mill || selected_lumber_camp ||
               selected_mining_camp || selected_town_center) {
        struct EconomyControl {
            std::string_view key;
            Technology technology;
        };
        const std::array<EconomyControl, 2> mill_controls{{
            {"Alt+1", Technology::heavy_plow},
            {"Alt+2", Technology::crop_rotation},
        }};
        const std::array<EconomyControl, 2> lumber_controls{{
            {"Alt+1", Technology::bow_saw},
            {"Alt+2", Technology::two_man_saw},
        }};
        const std::array<EconomyControl, 4> mining_controls{{
            {"G", Technology::gold_mining},
            {"Alt+G", Technology::gold_shaft_mining},
            {"S", Technology::stone_mining},
            {"Alt+S", Technology::stone_shaft_mining},
        }};
        building_controls = selected_mill ? "Mill:"
            : selected_lumber_camp ? "Lumber Camp:"
            : selected_mining_camp ? "Mining Camp:"
            : "Town Center:";
        const auto add_technology = [&](const EconomyControl& control) {
            if (technology_available_to_player(
                    simulation, active_view_player, control.technology
                ) &&
                !simulation.has_technology(
                    active_view_player, control.technology
                )) {
                append_control(control.key, name(control.technology));
            }
        };
        if (selected_mill) {
            for (const auto& control : mill_controls) add_technology(control);
        } else if (selected_lumber_camp) {
            for (const auto& control : lumber_controls)
                add_technology(control);
        } else if (selected_mining_camp) {
            for (const auto& control : mining_controls)
                add_technology(control);
        } else {
            add_technology({"Alt+C", Technology::hand_cart});
            add_technology({"Alt+W", Technology::town_watch});
            add_technology({"Shift+Alt+W", Technology::town_patrol});
        }
    } else if (selected_stable) {
        building_controls = "Stable:";
        if (unit_available_to_player(
                simulation, active_view_player, UnitKind::camel_rider
            )) {
            append_control("C", "camel rider");
        }
        if (technology_available_to_player(
                simulation, active_view_player, Technology::heavy_camel
            ) &&
            !simulation.has_technology(
                active_view_player, Technology::heavy_camel
            )) {
            append_control("Alt+C", "heavy camel");
        }
        if (unit_available_to_player(
                simulation, active_view_player, UnitKind::knight
            )) {
            append_control("K", "knight");
        }
        if (unit_available_to_player(
                simulation, active_view_player, UnitKind::scout_cavalry
            )) {
            append_control("Q", "scout cavalry");
        }
    } else if (selected_siege_workshop) {
        building_controls = "Siege Workshop:";
        if (simulation.has_technology(
                active_view_player, Technology::chemistry
            ) &&
            unit_available_to_player(
                simulation, active_view_player, UnitKind::bombard_cannon
            )) {
            append_control("Alt+8", "bombard cannon");
        } else {
            append_control("-", "Chemistry required");
        }
        if (unit_available_to_player(
                simulation, active_view_player, UnitKind::battering_ram
            )) {
            append_control("3", "battering ram");
        }
        if (technology_available_to_player(
                simulation, active_view_player, Technology::capped_ram
            ) &&
            !simulation.has_technology(
                active_view_player, Technology::capped_ram
            )) {
            append_control("Alt+3", "capped ram");
        }
        if (technology_available_to_player(
                simulation, active_view_player, Technology::siege_ram
            ) &&
            !simulation.has_technology(
                active_view_player, Technology::siege_ram
            )) {
            append_control("Shift+Alt+3", "siege ram");
        }
        if (unit_available_to_player(
                simulation, active_view_player, UnitKind::scorpion
            )) {
            append_control("X", "scorpion");
        }
        if (technology_available_to_player(
                simulation, active_view_player, Technology::heavy_scorpion
            ) &&
            !simulation.has_technology(
                active_view_player, Technology::heavy_scorpion
            )) {
            append_control("Alt+X", "heavy");
        }
        if (unit_available_to_player(
                simulation, active_view_player, UnitKind::mangonel
            )) {
            append_control("O", "mangonel");
        }
        if (technology_available_to_player(
                simulation, active_view_player, Technology::onager
            ) &&
            !simulation.has_technology(active_view_player, Technology::onager)) {
            append_control("Alt+O", "onager");
        }
        if (technology_available_to_player(
                simulation, active_view_player, Technology::siege_onager
            ) &&
            !simulation.has_technology(
                active_view_player, Technology::siege_onager
            )) {
            append_control("Shift+Alt+O", "siege onager");
        }
    }
    const bool selected_transport = std::ranges::any_of(
        simulation.selected_units(),
        [&simulation](EntityId id) {
            const auto found = std::ranges::find_if(
                simulation.units(),
                [id](const Unit& unit) {
                    return unit.id == id &&
                        unit.kind == UnitKind::transport_ship;
                }
            );
            return found != simulation.units().end();
        }
    );
    const char* controls = pending_trade_route
        ? "TRADE ROUTE: right click an allied Market"
        : pending_conversion
        ? "CONVERT MODE: right click visible enemy unit"
        : pending_guard
        ? "GUARD MODE: right click friendly unit or building"
        : pending_attack_ground
        ? "ATTACK-GROUND MODE: right click target tile"
        : pending_patrol
        ? "PATROL MODE: right click second endpoint"
        : pending_attack_move
        ? "ATTACK-MOVE MODE: right click destination"
        : pending_building
        ? "BUILD MODE: right click nearby tile   Esc quit"
        : selected_dock
        ? building_controls.c_str()
        : selected_transport
        ? "Transport: units right-click ship to embark; right-click shore to disembark"
        : selected_castle
        ? building_controls.c_str()
        : selected_blacksmith
        ? building_controls.c_str()
        : selected_barracks
        ? building_controls.c_str()
        : selected_archery_range
        ? building_controls.c_str()
        : selected_university
        ? building_controls.c_str()
        : selected_monastery
        ? building_controls.c_str()
        : (selected_mill || selected_lumber_camp ||
           selected_mining_camp || selected_town_center)
        ? building_controls.c_str()
        : selected_stable
        ? building_controls.c_str()
        : selected_siege_workshop
        ? building_controls.c_str()
        : "Villager 3 Outpost F12 Wonder  Market Alt+T cart  Cart Alt+T route  Ctrl+Alt+A/N/E diplomacy  S stop";
    if (!has_selection && !observer_mode) {
        SDL_RenderDebugText(renderer, 270.0F, top + 108.0F, controls);
    }
    SDL_SetRenderClipRect(renderer, nullptr);
    std::ostringstream status;
    status << name(simulation.civilization(active_view_player))
           << ' ' << name(simulation.age(active_view_player))
           << " T" << simulation.tick_number()
           << ' ' << name(simulation.outcome())
           << ' '
           << (simulation.diplomacy(
                   active_view_player,
                   opposing_player(active_view_player)
               ) ==
                       Diplomacy::ally
                   ? "ALLY"
                   : simulation.diplomacy(
                         active_view_player,
                         opposing_player(active_view_player)
                     ) == Diplomacy::neutral
                       ? "NEUTRAL"
                       : "ENEMY");
    if (!has_selection && !observer_mode) {
        SDL_RenderDebugText(
            renderer, 270.0F, top + 52.0F, status.str().c_str()
        );
    }
    const auto countdown_name = [](VictoryCountdownKind kind) {
        return kind == VictoryCountdownKind::wonder
            ? "WONDER"
            : kind == VictoryCountdownKind::relic
                ? "RELIC"
                : "VICTORY";
    };
    std::ostringstream countdown_status;
    const int red_countdown =
        simulation.victory_countdown(Player::red);
    const int blue_countdown =
        simulation.victory_countdown(active_view_player);
    if (red_countdown > 0) {
        countdown_status << "WARNING RED "
                         << countdown_name(
                                simulation.countdown_kind(Player::red)
                            )
                         << ' ' << red_countdown << " TICKS";
    } else if (blue_countdown > 0) {
        countdown_status << "BLUE "
                         << countdown_name(
                                simulation.countdown_kind(active_view_player)
                            )
                         << ' ' << blue_countdown << " TICKS";
    } else if (simulation.match_rules().wonder_enabled ||
               simulation.match_rules().relic_enabled) {
        countdown_status << "VICTORY: "
                         << (simulation.match_rules().wonder_enabled
                             ? "WONDER "
                             : "")
                         << (simulation.match_rules().relic_enabled
                             ? "RELIC"
                             : "");
    }
    if (!has_selection && !observer_mode) {
        SDL_RenderDebugText(
            renderer, 270.0F, top + 72.0F,
            countdown_status.str().c_str()
        );
    }
    if (simulation.outcome() != MatchOutcome::ongoing &&
        !observer_mode) {
        const SDL_FRect terminal{
            360.0F,
            210.0F,
            560.0F,
            92.0F,
        };
        set_color(renderer, {18, 12, 8, 224});
        SDL_RenderFillRect(renderer, &terminal);
        set_color(renderer, {221, 188, 103, 255});
        SDL_RenderRect(renderer, &terminal);
        std::ostringstream terminal_text;
        terminal_text << "MATCH COMPLETE: "
                      << name(simulation.outcome());
        SDL_RenderDebugText(
            renderer,
            terminal.x + 24.0F,
            terminal.y + 38.0F,
            terminal_text.str().c_str()
        );
    }
    if (false && (simulation.selected_unit() ||
         simulation.selected_building()) &&
        active_legacy_sprites.portrait_frame.texture != nullptr) {
        const SDL_FRect portrait{
            923.0F,
            top + 12.0F,
            54.0F,
            54.0F,
        };
        SDL_RenderTexture(
            renderer,
            active_legacy_sprites.portrait_frame.texture,
            nullptr,
            &portrait
        );
    }
    if (!control_group_status.empty() && information_clip.w >= 180) {
        SDL_SetRenderClipRect(renderer, &information_clip);
        const std::string group_text = hud_layout::truncate_debug_text(
            control_group_status, information_clip.w - 16
        );
        SDL_RenderDebugText(
            renderer, static_cast<float>(information_clip.x + 8),
            top + 92.0F,
            group_text.c_str()
        );
        SDL_SetRenderClipRect(renderer, nullptr);
    }
    if (false && simulation.selected_building()) {
        const auto selected = std::ranges::find_if(
            simulation.buildings(),
            [&simulation](const Building& building) {
                return building.id == *simulation.selected_building();
            }
        );
        if (selected != simulation.buildings().end() &&
            !selected->production_queue.empty()) {
            const ProductionOrder& order =
                selected->production_queue.front();
            const int total = rules_for(order.kind).training_ticks;
            const float progress = std::clamp(
                1.0F - static_cast<float>(order.ticks_remaining) /
                    static_cast<float>(total),
                0.0F,
                1.0F
            );
            const SDL_FRect track{728.0F, top + 48.0F, 190.0F, 8.0F};
            set_color(renderer, {8, 10, 12, 255});
            SDL_RenderFillRect(renderer, &track);
            const SDL_FRect fill{
                track.x,
                track.y,
                track.w * progress,
                track.h,
            };
            set_color(renderer, {196, 160, 58, 255});
            SDL_RenderFillRect(renderer, &fill);
            set_color(renderer, {235, 235, 220, 255});
            std::ostringstream queue;
            queue << name(order.kind) << ' '
                  << static_cast<int>(progress * 100.0F) << "%  +"
                  << selected->production_queue.size() - 1;
            SDL_RenderDebugText(
                renderer,
                track.x + track.w + 8.0F,
                top + 59.0F,
                queue.str().c_str()
            );
        }
    }
    if (active_settings.minimap) {
        render_minimap(renderer, simulation, top, camera);
    }
}

std::vector<std::string> wrap_scenario_text(
    std::string_view text,
    std::size_t width
) {
    std::vector<std::string> lines;
    std::string line;
    std::istringstream words{std::string{text}};
    std::string word;
    while (words >> word) {
        for (char& character : word) {
            const unsigned char byte =
                static_cast<unsigned char>(character);
            if (byte < 32 || byte > 126) character = '?';
        }
        while (word.size() > width) {
            if (!line.empty()) {
                lines.push_back(std::move(line));
                line.clear();
            }
            lines.push_back(word.substr(0, width));
            word.erase(0, width);
        }
        if (line.empty()) {
            line = std::move(word);
        } else if (!word.empty() &&
                   line.size() + 1 + word.size() <= width) {
            line += ' ';
            line += word;
        } else if (!word.empty()) {
            lines.push_back(std::move(line));
            line = std::move(word);
        }
    }
    if (!line.empty()) lines.push_back(std::move(line));
    if (lines.empty()) lines.emplace_back();
    return lines;
}

void render_scenario_presentation(
    SDL_Renderer* renderer,
    const Simulation& simulation,
    const ScenarioPresentation& presentation
) {
    const auto& messages = simulation.scenario_messages();
    const ScenarioMessage* message = nullptr;
    for (auto candidate = messages.rbegin();
         candidate != messages.rend(); ++candidate) {
        if (candidate->player == active_view_player &&
            simulation.tick_number() < candidate->expires_tick) {
            message = &*candidate;
            break;
        }
    }
    if (message != nullptr) {
        const std::vector<std::string> lines =
            wrap_scenario_text(message->text, 60);
        const float box_width = 512.0F;
        const float box_height =
            34.0F + static_cast<float>(lines.size()) * 13.0F;
        const SDL_FRect message_box{
            presentation.objectives_visible
                ? 490.0F
                : (static_cast<float>(view_pixel_width) - box_width) * 0.5F,
            26.0F,
            box_width,
            box_height,
        };
        set_color(renderer, {18, 12, 8, 224});
        SDL_RenderFillRect(renderer, &message_box);
        set_color(renderer, {221, 188, 103, 255});
        SDL_RenderRect(renderer, &message_box);
        SDL_RenderDebugText(
            renderer,
            message_box.x + 16.0F,
            message_box.y + 10.0F,
            ui_text("ui.message").data()
        );
        set_color(renderer, {238, 230, 198, 255});
        float y = message_box.y + 27.0F;
        for (const std::string& line : lines) {
            SDL_RenderDebugText(
                renderer, message_box.x + 16.0F, y, line.c_str()
            );
            y += 13.0F;
        }
    }

    if (!presentation.objectives_visible) return;

    std::vector<const ObjectiveState*> visible;
    for (const ObjectiveState& objective : simulation.objectives()) {
        if (objective.player != active_view_player || objective.hidden) {
            continue;
        }
        visible.push_back(&objective);
    }
    std::vector<std::string> lines;
    if (visible.empty()) {
        lines.emplace_back("No visible objectives.");
    }
    const bool defeated =
        simulation.outcome() == MatchOutcome::red_victory;
    for (const ObjectiveState* objective : visible) {
        const std::string status = objective->completed
            ? "[" + std::string{ui_text("objective.done")} + "] "
            : defeated
                ? "[" + std::string{ui_text("objective.failed")} + "] "
                : "[ ] ";
        const std::string kind =
            std::string{objective->required
                ? ui_text("objective.required")
                : ui_text("objective.optional")} + ": ";
        std::vector<std::string> wrapped =
            wrap_scenario_text(
                status + kind + objective->description, 52
            );
        lines.insert(
            lines.end(),
            std::make_move_iterator(wrapped.begin()),
            std::make_move_iterator(wrapped.end())
        );
    }
    const float box_height =
        50.0F + static_cast<float>(lines.size()) * 14.0F;
    const SDL_FRect objective_box{
        18.0F,
        18.0F,
        454.0F,
        std::min(box_height, 300.0F),
    };
    set_color(renderer, {18, 12, 8, 232});
    SDL_RenderFillRect(renderer, &objective_box);
    set_color(renderer, {221, 188, 103, 255});
    SDL_RenderRect(renderer, &objective_box);
    SDL_RenderDebugText(
        renderer,
        objective_box.x + 14.0F,
        objective_box.y + 11.0F,
        ui_text("ui.objectives").data()
    );
    set_color(renderer, {160, 142, 104, 255});
    SDL_RenderDebugText(
        renderer,
        objective_box.x + objective_box.w - 92.0F,
        objective_box.y + 11.0F,
        ui_text("ui.tab_close").data()
    );
    float y = objective_box.y + 34.0F;
    for (const std::string& line : lines) {
        if (y + 12.0F >= objective_box.y + objective_box.h) break;
        set_color(
            renderer,
            line.starts_with("[DONE]")
                ? SDL_Color{116, 210, 126, 255}
                : line.starts_with("[FAILED]")
                    ? SDL_Color{232, 106, 86, 255}
                    : SDL_Color{238, 230, 198, 255}
        );
        SDL_RenderDebugText(
            renderer, objective_box.x + 14.0F, y, line.c_str()
        );
        y += 14.0F;
    }
}

void render_campaign_presentation(
    SDL_Renderer* renderer,
    const Simulation& simulation,
    const CampaignPresentation* presentation
) {
    if (presentation == nullptr || !presentation->visible) return;

    const auto current = std::ranges::find_if(
        presentation->campaign.scenarios,
        [presentation](const CampaignScenarioEntry& entry) {
            return entry.id == presentation->scenario.id;
        }
    );
    const std::size_t scenario_number =
        current == presentation->campaign.scenarios.end()
        ? 0
        : static_cast<std::size_t>(std::distance(
              presentation->campaign.scenarios.begin(), current
          )) + 1;
    const bool completed = std::ranges::find(
        presentation->progress.completed,
        presentation->scenario.id
    ) != presentation->progress.completed.end();
    const bool unlocked =
        presentation->scenario.id <=
        presentation->progress.highest_unlocked;

    if (presentation->screen !=
            CampaignPresentation::Screen::status) {
        if (active_legacy_sprites.campaign_background.texture != nullptr) {
            const SDL_FRect destination{
                0.0F, 0.0F,
                static_cast<float>(view_pixel_width),
                static_cast<float>(view_pixel_height),
            };
            SDL_RenderTexture(
                renderer,
                active_legacy_sprites.campaign_background.texture,
                nullptr,
                &destination
            );
        } else if (active_legacy_sprites.scenario_background != nullptr) {
            const SDL_FRect destination{
                0.0F, 0.0F,
                static_cast<float>(view_pixel_width),
                static_cast<float>(view_pixel_height),
            };
            SDL_RenderTexture(
                renderer,
                active_legacy_sprites.scenario_background,
                nullptr,
                &destination
            );
        }
        const bool debrief =
            presentation->screen ==
            CampaignPresentation::Screen::debrief;
        const SDL_FRect shade{
            0.0F, 0.0F,
            static_cast<float>(view_pixel_width),
            static_cast<float>(view_pixel_height),
        };
        set_color(renderer, {8, 7, 5, 238});
        SDL_RenderFillRect(renderer, &shade);
        const SDL_FRect panel{170.0F, 54.0F, 940.0F, 590.0F};
        render_beveled_panel(renderer, panel, {43, 34, 23, 255});
        set_color(renderer, {238, 214, 145, 255});
        SDL_RenderDebugText(
            renderer, panel.x + 28.0F, panel.y + 24.0F,
            debrief
                ? ui_text("campaign.debrief").data()
                : ui_text("campaign.briefing").data()
        );
        set_color(renderer, {238, 230, 198, 255});
        std::ostringstream order;
        order << "MISSION " << scenario_number << " OF "
              << presentation->campaign.scenarios.size()
              << "  " << presentation->scenario.name;
        SDL_RenderDebugText(
            renderer, panel.x + 28.0F, panel.y + 55.0F,
            order.str().c_str()
        );
        float y = panel.y + 86.0F;
        const std::string outcome =
            simulation.outcome() == MatchOutcome::blue_victory
            ? "VICTORY"
            : simulation.outcome() == MatchOutcome::red_victory
                ? "DEFEAT" : "MISSION BRIEFING";
        set_color(
            renderer,
            outcome == "VICTORY"
                ? SDL_Color{120, 218, 132, 255}
                : outcome == "DEFEAT"
                    ? SDL_Color{238, 108, 88, 255}
                    : SDL_Color{221, 188, 103, 255}
        );
        SDL_RenderDebugText(
            renderer, panel.x + 28.0F, y, outcome.c_str()
        );
        y += 28.0F;
        set_color(renderer, {226, 218, 190, 255});
        std::ostringstream metadata;
        metadata << "CIVILIZATION "
                 << name(simulation.civilization(
                        presentation->campaign.human_player))
                 << "   MAP " << simulation.map().width() << 'x'
                 << simulation.map().height();
        SDL_RenderDebugText(
            renderer, panel.x + 28.0F, y,
            metadata.str().c_str()
        );
        y += 28.0F;
        for (const std::string& line : wrap_scenario_text(
                 presentation->campaign.description, 92)) {
            SDL_RenderDebugText(
                renderer, panel.x + 28.0F, y, line.c_str()
            );
            y += 15.0F;
        }
        y += 12.0F;
        SDL_RenderDebugText(
            renderer, panel.x + 28.0F, y,
            ui_text("ui.objectives").data()
        );
        y += 21.0F;
        for (const ObjectiveState& objective : simulation.objectives()) {
            if (objective.player != presentation->campaign.human_player ||
                objective.hidden) continue;
            const std::string prefix =
                objective.completed ? "[DONE] " :
                debrief ? "[FAILED] " : "[ ] ";
            for (const std::string& line : wrap_scenario_text(
                     prefix + objective.description, 88)) {
                if (y > panel.y + panel.h - 92.0F) break;
                SDL_RenderDebugText(
                    renderer, panel.x + 28.0F, y, line.c_str()
                );
                y += 15.0F;
            }
        }
        if (debrief) {
            y += 12.0F;
            const auto next = next_campaign_scenario(
                presentation->campaign, presentation->progress
            );
            const std::string progression =
                completed && next
                ? "NEXT UNLOCKED: " + next->name
                : completed
                    ? "CAMPAIGN COMPLETE"
                    : "DEFEAT: RETRY CURRENT MISSION";
            set_color(renderer, {221, 188, 103, 255});
            SDL_RenderDebugText(
                renderer, panel.x + 28.0F, y,
                progression.c_str()
            );
        }
        set_color(renderer, {221, 188, 103, 255});
        SDL_RenderDebugText(
            renderer, panel.x + 28.0F,
            panel.y + panel.h - 55.0F,
            debrief
                ? "ENTER / CLICK: CONTINUE   ESC: BACK"
                : "ENTER / CLICK: BEGIN MISSION   ESC: BACK"
        );
        if (!presentation->optional_narration_path.empty() ||
            !presentation->optional_cinematic_path.empty()) {
            set_color(renderer, {151, 139, 110, 255});
            SDL_RenderDebugText(
                renderer, panel.x + 28.0F,
                panel.y + panel.h - 31.0F,
                "OPTIONAL USER-OWNED MEDIA PATH CONFIGURED"
            );
        }
        return;
    }

    std::vector<std::string> lines;
    {
        std::ostringstream scenario_line;
        scenario_line << "SCENARIO " << scenario_number << '/'
                      << presentation->campaign.scenarios.size()
                      << ": " << presentation->scenario.name;
        std::vector<std::string> wrapped =
            wrap_scenario_text(scenario_line.str(), 47);
        lines.insert(
            lines.end(),
            std::make_move_iterator(wrapped.begin()),
            std::make_move_iterator(wrapped.end())
        );
    }
    lines.push_back(
        std::string{"STATE: "} +
        (completed ? "COMPLETED" : unlocked ? "UNLOCKED" : "LOCKED")
    );
    if (simulation.outcome() != MatchOutcome::ongoing) {
        const auto next =
            next_campaign_scenario(
                presentation->campaign, presentation->progress
            );
        if (completed && next &&
            next->id != presentation->scenario.id) {
            lines.push_back("NEXT UNLOCKED:");
            std::vector<std::string> wrapped =
                wrap_scenario_text(next->name, 47);
            lines.insert(
                lines.end(),
                std::make_move_iterator(wrapped.begin()),
                std::make_move_iterator(wrapped.end())
            );
        } else if (
            completed &&
            presentation->progress.completed.size() ==
                presentation->campaign.scenarios.size()
        ) {
            lines.emplace_back("CAMPAIGN COMPLETE");
        } else {
            lines.emplace_back("RETRY CURRENT SCENARIO");
        }
    }

    const float box_height =
        55.0F + static_cast<float>(lines.size()) * 14.0F;
    const SDL_FRect box{
        static_cast<float>(view_pixel_width) - 438.0F,
        static_cast<float>(view_pixel_height) - box_height - 18.0F,
        420.0F,
        box_height,
    };
    set_color(renderer, {18, 12, 8, 232});
    SDL_RenderFillRect(renderer, &box);
    set_color(renderer, {221, 188, 103, 255});
    SDL_RenderRect(renderer, &box);
    SDL_RenderDebugText(
        renderer, box.x + 14.0F, box.y + 10.0F,
        ui_text("campaign.title").data()
    );
    set_color(renderer, {238, 230, 198, 255});
    const std::vector<std::string> title =
        wrap_scenario_text(presentation->campaign.name, 47);
    float y = box.y + 29.0F;
    for (const std::string& line : title) {
        SDL_RenderDebugText(renderer, box.x + 14.0F, y, line.c_str());
        y += 14.0F;
    }
    for (const std::string& line : lines) {
        SDL_RenderDebugText(renderer, box.x + 14.0F, y, line.c_str());
        y += 14.0F;
    }
}

std::string_view lockstep_status_name(LockstepStatus status) {
    switch (status) {
        case LockstepStatus::handshaking:
            return "CONNECTING / HANDSHAKE";
        case LockstepStatus::ready: return "READY";
        case LockstepStatus::running: return "RUNNING";
        case LockstepStatus::desync: return "DESYNC";
        case LockstepStatus::timed_out: return "PEER TIMEOUT";
        case LockstepStatus::disconnected: return "DISCONNECTED";
        case LockstepStatus::protocol_mismatch:
            return "PROTOCOL MISMATCH";
        case LockstepStatus::build_mismatch:
            return "BUILD MISMATCH";
        case LockstepStatus::schema_mismatch:
            return "SCHEMA MISMATCH";
        case LockstepStatus::scenario_mismatch:
            return "SCENARIO MISMATCH";
        case LockstepStatus::content_mismatch:
            return "CONTENT MISMATCH";
        case LockstepStatus::settings_mismatch:
            return "SETTINGS MISMATCH";
        case LockstepStatus::roster_mismatch:
            return "ROSTER MISMATCH";
        case LockstepStatus::invalid_command: return "INVALID COMMAND";
    }
    return "UNKNOWN";
}

void render_chat_panel(
    SDL_Renderer* renderer,
    const MultiplayerPresentation& presentation,
    SDL_FRect box
) {
    render_beveled_panel(renderer, box, {31, 24, 17, 242});
    set_color(renderer, {221, 188, 103, 255});
    SDL_RenderDebugText(
        renderer, box.x + 10.0F, box.y + 8.0F,
        presentation.chat_audience == ChatAudience::all
            ? "CHAT [ALL]  TAB: ALLIES"
            : "CHAT [ALLIES]  TAB: ALL"
    );
    const std::size_t visible_count = std::min<std::size_t>(
        6, presentation.chat_log.size()
    );
    float y = box.y + 25.0F;
    for (std::size_t index =
             presentation.chat_log.size() - visible_count;
         index < presentation.chat_log.size();
         ++index) {
        const LockstepChatMessage& message =
            presentation.chat_log[index];
        std::string line =
            std::to_string(message.sequence) + " " +
            (message.sender == Player::blue ? "BLUE" : "RED") +
            (message.audience == ChatAudience::allies
                 ? " [ALLIES]: " : ": ") +
            message.text;
        if (line.size() > 66) line.resize(66);
        set_color(
            renderer,
            message.sender == Player::blue
                ? SDL_Color{130, 179, 245, 255}
                : SDL_Color{239, 130, 110, 255}
        );
        SDL_RenderDebugText(renderer, box.x + 10.0F, y, line.c_str());
        y += 13.0F;
    }
    if (!presentation.signal_log.empty() && y < box.y + box.h - 32.0F) {
        const LockstepMapSignal& signal =
            presentation.signal_log.back();
        const std::string line =
            std::to_string(signal.sequence) + " " +
            (signal.sender == Player::blue ? "BLUE" : "RED") +
            " [SIGNAL] " + std::to_string(signal.tile.x) + "," +
            std::to_string(signal.tile.y) + "  CLICK TO VIEW";
        set_color(
            renderer,
            signal.sender == Player::blue
                ? SDL_Color{130, 179, 245, 255}
                : SDL_Color{239, 130, 110, 255}
        );
        SDL_RenderDebugText(renderer, box.x + 10.0F, y, line.c_str());
    }
    set_color(renderer, {238, 230, 198, 255});
    const std::string input =
        presentation.chat_input_active
        ? "> " + presentation.chat_input + "_"
        : "ENTER: TYPE MESSAGE";
    SDL_RenderDebugText(
        renderer, box.x + 10.0F, box.y + box.h - 27.0F,
        input.substr(0, 72).c_str()
    );
    if (!presentation.chat_feedback.empty()) {
        set_color(renderer, {245, 170, 102, 255});
        SDL_RenderDebugText(
            renderer, box.x + 10.0F, box.y + box.h - 14.0F,
            presentation.chat_feedback.substr(0, 72).c_str()
        );
    }
}

std::string_view latency_band_name(LatencyBand band) {
    switch (band) {
        case LatencyBand::green: return "GREEN";
        case LatencyBand::yellow: return "YELLOW";
        case LatencyBand::red: return "RED";
        case LatencyBand::unknown: return "UNKNOWN";
    }
    return "UNKNOWN";
}

std::string_view checkpoint_status_name(SaveBarrierStatus status) {
    switch (status) {
        case SaveBarrierStatus::idle: return "IDLE";
        case SaveBarrierStatus::collecting: return "COLLECTING";
        case SaveBarrierStatus::matched: return "MATCHED";
        case SaveBarrierStatus::hash_mismatch: return "HASH MISMATCH";
    }
    return "UNKNOWN";
}

std::string_view game_speed_name(GameSpeed speed) {
    switch (speed) {
        case GameSpeed::slow: return "SLOW";
        case GameSpeed::normal: return "NORMAL";
        case GameSpeed::fast: return "FAST";
    }
    return "NORMAL";
}

void render_multiplayer_presentation(
    SDL_Renderer* renderer,
    const MultiplayerPresentation* presentation
) {
    if (presentation == nullptr || !presentation->visible) return;

    const Player peer = presentation->local_slot == Player::blue
        ? Player::red : Player::blue;
    const LockstepStatus status = presentation->live_transport
        ? presentation->live_status
        : presentation->session.status();
    if (status != LockstepStatus::running) {
        const SDL_FRect lobby{
            238.0F, 72.0F, 804.0F, 520.0F,
        };
        render_beveled_panel(
            renderer, lobby, {42, 33, 23, 248}
        );
        set_color(renderer, {238, 214, 145, 255});
        SDL_RenderDebugText(
            renderer, lobby.x + 24.0F, lobby.y + 20.0F,
            "LOCALHOST MULTIPLAYER LOBBY"
        );
        set_color(renderer, {159, 133, 78, 255});
        SDL_RenderLine(
            renderer,
            lobby.x + 20.0F,
            lobby.y + 43.0F,
            lobby.x + lobby.w - 20.0F,
            lobby.y + 43.0F
        );
        const LockstepSessionConfig& config = presentation->config;
        const auto player_line = [&](const LockstepPlayerConfig& player,
                                     bool ready,
                                     float y) {
            const bool local =
                player.slot == presentation->local_slot;
            std::ostringstream line;
            line << (player.slot == Player::blue ? "BLUE" : "RED")
                 << (local ? "  [LOCAL]" : "  [PEER]")
                 << "   CIV " << name(player.civilization)
                 << "   TEAM " << player.team
                 << "   " << (ready ? "READY" : "NOT READY");
            set_color(
                renderer,
                player.slot == Player::blue
                    ? SDL_Color{112, 166, 245, 255}
                    : SDL_Color{235, 105, 88, 255}
            );
            SDL_RenderDebugText(
                renderer, lobby.x + 30.0F, y,
                line.str().c_str()
            );
        };
        player_line(
            config.blue,
            presentation->blue_ready,
            lobby.y + 70.0F
        );
        player_line(
            config.red,
            presentation->red_ready,
            lobby.y + 96.0F
        );
        set_color(renderer, {226, 218, 190, 255});
        std::vector<std::string> metadata;
        metadata.push_back(
            "SCENARIO: " + config.scenario_digest.substr(
                0, std::min<std::size_t>(
                    config.scenario_digest.size(), 58
                )
            )
        );
        metadata.push_back(
            "RULES: " + config.content_rules_digest
        );
        metadata.push_back(
            "BUILD " + config.build_id +
            "   COMMAND SCHEMA " +
            std::to_string(config.command_schema_version) +
            "   SAVE " + std::to_string(config.save_version)
        );
        metadata.push_back(
            "TICK " + std::to_string(config.tick_cadence_ms) +
            "ms   INPUT DELAY " +
            std::to_string(config.input_delay_ticks) +
            "   SEED " +
            std::to_string(config.deterministic_seed)
        );
        metadata.push_back(
            "LINK: " +
            std::string{
                presentation->transport_connected
                    ? "LOCALHOST PEER CONNECTED"
                    : "CONNECTING TO LOCALHOST"
            }
        );
        metadata.push_back(
            "DELAY " + std::to_string(config.input_delay_ticks) +
            " TICKS   RTT " +
            (presentation->network_metrics.round_trip_ms
                 ? std::to_string(
                       *presentation->network_metrics.round_trip_ms) + "ms"
                 : std::string{"PENDING"}) +
            " " + std::string{latency_band_name(
                presentation->network_metrics.latency_band)}
        );
        float metadata_y = lobby.y + 146.0F;
        for (const std::string& line : metadata) {
            SDL_RenderDebugText(
                renderer, lobby.x + 30.0F,
                metadata_y, line.c_str()
            );
            metadata_y += 23.0F;
        }
        render_chat_panel(
            renderer,
            *presentation,
            {lobby.x + 24.0F, lobby.y + 260.0F,
             lobby.w - 48.0F, 144.0F}
        );
        const SDL_FRect ready_button{
            lobby.x + 76.0F, lobby.y + 418.0F,
            248.0F, 52.0F,
        };
        const SDL_FRect start_button{
            lobby.x + 480.0F, lobby.y + 418.0F,
            248.0F, 52.0F,
        };
        render_beveled_panel(
            renderer, ready_button,
            presentation->local_ready
                ? SDL_Color{49, 92, 48, 255}
                : SDL_Color{68, 54, 36, 255}
        );
        render_beveled_panel(
            renderer, start_button,
            presentation->hosting &&
                presentation->blue_ready &&
                presentation->red_ready
                ? SDL_Color{49, 92, 48, 255}
                : SDL_Color{47, 43, 38, 255}
        );
        set_color(renderer, {240, 224, 173, 255});
        SDL_RenderDebugText(
            renderer,
            ready_button.x + 45.0F,
            ready_button.y + 20.0F,
            presentation->local_ready
                ? "READY LOCKED"
                : "R / CLICK: READY"
        );
        SDL_RenderDebugText(
            renderer,
            start_button.x + 43.0F,
            start_button.y + 20.0F,
            presentation->hosting
                ? "CTRL+ENTER / CLICK: START"
                : "WAITING FOR HOST START"
        );
        set_color(renderer, {161, 146, 111, 255});
        SDL_RenderDebugText(
            renderer,
            lobby.x + 24.0F,
            lobby.y + lobby.h - 24.0F,
            "Reconstruction lobby; no claim of original wire compatibility."
        );
        return;
    }
    std::vector<std::string> lines;
    lines.push_back(
        "MODE: " + presentation->mode +
        "  LOCAL SLOT: " +
        std::string{name(presentation->local_slot)}
    );
    lines.push_back(
        "PROTOCOL: " +
        std::to_string(lockstep_protocol_version)
    );
    lines.emplace_back(
        presentation->live_transport
            ? "TRANSPORT: LOCALHOST TCP PORT " +
                  std::to_string(presentation->port)
            : "TRANSPORT: CAPTURE SIMULATION"
    );
    std::string digest = presentation->scenario_digest;
    if (digest.size() > 43) {
        digest = digest.substr(0, 40) + "...";
    }
    lines.push_back("SCENARIO DIGEST: " + digest);
    lines.push_back(
        "SESSION: " +
        std::string{lockstep_status_name(
            presentation->live_transport
                ? presentation->live_status
                : presentation->session.status()
        )}
    );
    lines.push_back(
        std::string{"PEER: "} +
        ((presentation->live_transport
              ? presentation->transport_connected
              : presentation->session.connected(peer))
            ? "CONNECTED" : "DISCONNECTED")
    );
    lines.push_back(
        "CONFIRMED TICK: " +
        std::to_string(
            presentation->live_transport
                ? presentation->live_tick
                : presentation->session.current_tick()
        )
    );
    lines.push_back(
        "INPUT DELAY: " +
        std::to_string(presentation->config.input_delay_ticks) +
        " TICKS"
    );
    lines.push_back(
        "RTT: " +
        (presentation->network_metrics.round_trip_ms
             ? std::to_string(
                   *presentation->network_metrics.round_trip_ms) + "ms"
             : std::string{"PENDING"}) +
        "  BAND: " +
        std::string{latency_band_name(
            presentation->network_metrics.latency_band)}
    );
    lines.push_back(
        "PEER TRAFFIC AGE: " +
        std::to_string(
            presentation->network_metrics.milliseconds_since_peer_traffic
        ) + "ms" +
        (presentation->network_metrics.waiting ? "  WAITING" : "")
    );
    lines.push_back(
        "CHECKPOINT: " +
        std::string{checkpoint_status_name(
            presentation->checkpoint_status)} +
        (presentation->checkpoint_tick > 0
             ? " TICK " +
                   std::to_string(presentation->checkpoint_tick)
             : "")
    );
    lines.push_back(presentation->checkpoint_feedback);
    lines.push_back(
        "CONTROL: " +
        std::string{presentation->network_paused ? "PAUSED " : "RUNNING "} +
        std::string{game_speed_name(presentation->game_speed)} +
        "  " + std::to_string(presentation->effective_cadence_ms) + "ms"
    );
    lines.push_back(presentation->control_feedback);
    if (status == LockstepStatus::desync) {
        lines.emplace_back("INTEGRITY: STATE HASH MISMATCH");
    } else if (status == LockstepStatus::timed_out) {
        lines.emplace_back("PEER LAG: TIMEOUT");
    } else if (presentation->waiting_for_turn) {
        lines.emplace_back("PEER LAG: WAITING FOR TURN");
    }

    const float box_height =
        45.0F + static_cast<float>(lines.size()) * 14.0F;
    const SDL_FRect box{
        18.0F,
        static_cast<float>(view_pixel_height) - box_height - 18.0F,
        462.0F,
        box_height,
    };
    set_color(renderer, {18, 12, 8, 232});
    SDL_RenderFillRect(renderer, &box);
    set_color(renderer, {221, 188, 103, 255});
    SDL_RenderRect(renderer, &box);
    SDL_RenderDebugText(
        renderer, box.x + 14.0F, box.y + 10.0F,
        ui_text("multiplayer.title").data()
    );
    float y = box.y + 30.0F;
    for (const std::string& line : lines) {
        SDL_RenderDebugText(renderer, box.x + 14.0F, y, line.c_str());
        y += 14.0F;
    }
    render_chat_panel(
        renderer,
        *presentation,
        {box.x + box.w + 10.0F, box.y, 566.0F, box.h}
    );
    if (presentation->network_paused) {
        const SDL_FRect paused_box{
            static_cast<float>(view_pixel_width) * 0.5F - 120.0F,
            36.0F, 240.0F, 48.0F,
        };
        render_beveled_panel(renderer, paused_box, {72, 38, 28, 244});
        set_color(renderer, {248, 220, 150, 255});
        SDL_RenderDebugText(
            renderer, paused_box.x + 66.0F, paused_box.y + 18.0F,
            "GAME PAUSED"
        );
    }
}

void render_editor_overlay(SDL_Renderer* renderer) {
    if (!active_editor_overlay) return;
    const SDL_FRect panel{18.0F, 18.0F, 680.0F, 218.0F};
    render_beveled_panel(renderer, panel, {42, 33, 23, 248});
    set_color(renderer, {238, 214, 145, 255});
    SDL_RenderDebugText(
        renderer, panel.x + 14.0F, panel.y + 12.0F,
        ui_debug_text("editor.title").c_str()
    );
    const char* tool =
        active_editor_tool == EditorTool::grass ? "GRASS" :
        active_editor_tool == EditorTool::water ? "WATER" :
        active_editor_tool == EditorTool::forest ? "FOREST" :
        active_editor_tool == EditorTool::elevation ? "ELEVATION +" :
        active_editor_tool == EditorTool::villager ? "BLUE VILLAGER" :
        active_editor_tool == EditorTool::house ? "BLUE HOUSE" : "ERASE";
    set_color(renderer, {238, 230, 198, 255});
    const std::string selected =
        std::string{"TOOL: "} + tool +
        "  PLAYER: " + std::string{name(active_editor_player)} +
        "  CURSOR: " + std::to_string(active_editor_cursor.x) + "," +
        std::to_string(active_editor_cursor.y);
    SDL_RenderDebugText(
        renderer, panel.x + 14.0F, panel.y + 36.0F,
        selected.c_str()
    );
    SDL_RenderDebugText(
        renderer, panel.x + 14.0F, panel.y + 58.0F,
        ui_debug_text("editor.tabs").c_str()
    );
    SDL_RenderDebugText(
        renderer, panel.x + 14.0F, panel.y + 78.0F,
        ui_debug_text("editor.tools").c_str()
    );
    SDL_RenderDebugText(
        renderer, panel.x + 14.0F, panel.y + 100.0F,
        ui_debug_text("editor.actions").c_str()
    );
    SDL_RenderDebugText(
        renderer, panel.x + 14.0F, panel.y + 122.0F,
        ui_debug_text("editor.files").c_str()
    );
    constexpr std::array<const char*, 5> tabs{
        "TERRAIN", "PLAYERS", "OBJECTIVES", "TRIGGERS", "FILE"
    };
    std::string focus_line{"FOCUS: "};
    for (std::size_t index = 0; index < tabs.size(); ++index) {
        focus_line += index == active_editor_focus
            ? "[" + std::string{tabs[index]} + "] "
            : std::string{tabs[index]} + " ";
    }
    SDL_RenderDebugText(
        renderer, panel.x + 14.0F, panel.y + 146.0F,
        focus_line.c_str()
    );
    set_color(renderer, {169, 204, 139, 255});
    SDL_RenderDebugText(
        renderer, panel.x + 14.0F, panel.y + 174.0F,
        active_editor_status.substr(0, 78).c_str()
    );
}

void render_frontend_overlay(SDL_Renderer* renderer) {
    if (active_frontend_screen == FrontendScreen::hidden) return;
    if (active_legacy_sprites.frontend_background != nullptr) {
        const SDL_FRect destination{
            0.0F, 0.0F,
            static_cast<float>(view_pixel_width),
            static_cast<float>(view_pixel_height + hud_height),
        };
        SDL_RenderTexture(
            renderer,
            active_legacy_sprites.frontend_background,
            nullptr,
            &destination
        );
    }
    const SDL_FRect shade{
        0.0F, 0.0F,
        static_cast<float>(view_pixel_width),
        static_cast<float>(view_pixel_height + hud_height),
    };
    set_color(renderer, {7, 7, 5, 246});
    SDL_RenderFillRect(renderer, &shade);
    const SDL_FRect panel{330.0F, 92.0F, 620.0F, 520.0F};
    render_beveled_panel(renderer, panel, {44, 34, 23, 255});
    set_color(renderer, {238, 214, 145, 255});
    SDL_RenderDebugText(
        renderer, panel.x + 42.0F, panel.y + 32.0F,
        "AGE OF EMPIRES II ARCHAEOLOGY"
    );
    set_color(renderer, {158, 137, 91, 255});
    SDL_RenderDebugText(
        renderer, panel.x + 42.0F, panel.y + 56.0F,
        "BOUNDED RECONSTRUCTION FRONT END"
    );
    set_color(renderer, {238, 230, 198, 255});
    if (active_frontend_screen == FrontendScreen::main_menu) {
        SDL_RenderDebugText(
            renderer, panel.x + 70.0F, panel.y + 112.0F,
            "1  SINGLE PLAYER SETUP"
        );
        SDL_RenderDebugText(
            renderer, panel.x + 70.0F, panel.y + 158.0F,
            "2  LOADED SCENARIO"
        );
        SDL_RenderDebugText(
            renderer, panel.x + 70.0F, panel.y + 204.0F,
            "3  CAMPAIGN BRIEFING"
        );
        SDL_RenderDebugText(
            renderer, panel.x + 70.0F, panel.y + 250.0F,
            "4  SCENARIO63 EDITOR"
        );
        SDL_RenderDebugText(
            renderer, panel.x + 70.0F, panel.y + 296.0F,
            "H / J  PRECONFIGURED HOST / JOIN"
        );
        SDL_RenderDebugText(
            renderer, panel.x + 70.0F, panel.y + 342.0F,
            "O  OPTIONS"
        );
        SDL_RenderDebugText(
            renderer, panel.x + 70.0F, panel.y + 388.0F,
            "L  SAVE / LOAD / REPLAY BROWSER"
        );
    } else {
        const char* map_kind =
            active_random_settings.kind == RandomMapKind::arabia
                ? "ARABIA" :
            active_random_settings.kind == RandomMapKind::black_forest
                ? "BLACK FOREST" :
            active_random_settings.kind == RandomMapKind::islands
                ? "ISLANDS" : "RIVERS";
        const char* map_size =
            random_map_size_label(active_random_settings.size);
        const char* difficulty =
            active_setup_difficulty == ComputerDifficulty::easiest
                ? "EASIEST" :
            active_setup_difficulty == ComputerDifficulty::easy ? "EASY" :
            active_setup_difficulty == ComputerDifficulty::moderate
                ? "MODERATE" :
            active_setup_difficulty == ComputerDifficulty::hard
                ? "HARD" : "HARDEST";
        SDL_RenderDebugText(
            renderer, panel.x + 70.0F, panel.y + 122.0F,
            "SINGLE PLAYER RANDOM MAP"
        );
        const std::string map_line =
            std::string{"M MAP: "} + map_kind +
            " [RMS]   Z SIZE: " + map_size;
        SDL_RenderDebugText(
            renderer, panel.x + 70.0F, panel.y + 166.0F,
            map_line.c_str()
        );
        const std::string seed_line =
            "SEED -/+ : " +
            std::to_string(active_random_settings.seed);
        SDL_RenderDebugText(
            renderer, panel.x + 70.0F, panel.y + 202.0F,
            seed_line.c_str()
        );
        const std::string player_line =
            "C CIV: " +
            std::string{name(active_setup_civilization)} +
            "   D AI: " + difficulty;
        SDL_RenderDebugText(
            renderer, panel.x + 70.0F, panel.y + 238.0F,
            player_line.c_str()
        );
        const char* victory =
            active_setup_victory == 0 ? "CONQUEST" :
            active_setup_victory == 1 ? "WONDER" : "RELIC";
        const std::string victory_line =
            std::string{"V VICTORY: "} + victory;
        SDL_RenderDebugText(
            renderer, panel.x + 70.0F, panel.y + 274.0F,
            victory_line.c_str()
        );
        SDL_RenderDebugText(
            renderer, panel.x + 70.0F, panel.y + 322.0F,
            "ENTER: GENERATE / START   ESC: MAIN MENU"
        );
        if (active_random_preview != nullptr) {
            // Every generated map is square, so the preview is square too.
            // A 128x96 rect stretched one by 4:3, and one filled rect per
            // tile meant 65,025 draws per frame at the maximum preset;
            // sampling one rect per preview pixel is both square and
            // independent of the map size.
            constexpr int preview_pixels = 96;
            const SDL_FRect preview{
                panel.x + panel.w - 170.0F,
                panel.y + 350.0F,
                static_cast<float>(preview_pixels),
                static_cast<float>(preview_pixels),
            };
            set_color(renderer, {8, 10, 8, 255});
            SDL_RenderFillRect(renderer, &preview);
            const GameMap& map = active_random_preview->map;
            for (int row = 0; row < preview_pixels; ++row) {
                for (int column = 0; column < preview_pixels; ++column) {
                    const TilePosition tile{
                        column * map.width() / preview_pixels,
                        row * map.height() / preview_pixels,
                    };
                    if (!map.contains(tile)) continue;
                    set_color(renderer, terrain_color(map.terrain_at(tile)));
                    const SDL_FRect cell{
                        preview.x + static_cast<float>(column),
                        preview.y + static_cast<float>(row),
                        1.0F,
                        1.0F,
                    };
                    SDL_RenderFillRect(renderer, &cell);
                }
            }
            set_color(renderer, {221, 188, 103, 255});
            SDL_RenderRect(renderer, &preview);
        }
    }
    set_color(renderer, {169, 204, 139, 255});
    SDL_RenderDebugText(
        renderer, panel.x + 42.0F, panel.y + panel.h - 54.0F,
        active_frontend_status.substr(0, 68).c_str()
    );
    set_color(renderer, {145, 132, 103, 255});
    SDL_RenderDebugText(
        renderer, panel.x + 42.0F, panel.y + panel.h - 28.0F,
        "NO CLAIM OF COMMERCIAL MENU OR NETWORK COMPATIBILITY"
    );
}

const char* on_off(bool value) {
    return value ? "ON" : "OFF";
}

void render_options_overlay(SDL_Renderer* renderer) {
    if (!active_options_visible) return;
    const SDL_FRect shade{0.0F, 0.0F, static_cast<float>(view_pixel_width),
                          static_cast<float>(view_pixel_height + hud_height)};
    set_color(renderer, {6, 6, 4, 238});
    SDL_RenderFillRect(renderer, &shade);
    const SDL_FRect panel{260.0F, 42.0F, 760.0F, 620.0F};
    render_beveled_panel(renderer, panel, {43, 34, 24, 255});
    set_color(renderer, {238, 214, 145, 255});
    SDL_RenderDebugText(
        renderer, panel.x + 34.0F, panel.y + 24.0F,
        active_options_hotkeys ? "HOTKEY REFERENCE" : "OPTIONS"
    );
    set_color(renderer, {232, 225, 196, 255});
    if (active_options_hotkeys) {
        const std::array<const char*, 10> lines{{
            "ESC OPTIONS / CLOSE OVERLAY     F11 FULLSCREEN",
            "ARROWS CAMERA     WHEEL ZOOM     DRAG SELECT",
            "F5 SAVE GAME      F6 CHECKPOINT/REPLAY",
            "F7 PAUSE/REPLAY    F8 SPEED (MULTIPLAYER HOST)",
            "F9 TECHNOLOGY TREE              F10 DIPLOMACY",
            "ENTER CHAT        TAB CHAT AUDIENCE",
            "1-9 PRODUCTION / BUILD COMMANDS",
            "CTRL+0..9 STORE GROUP   0..9 RECALL GROUP",
            "H RETURNS TO OPTIONS",
            "Procedural panel; no archive options artwork proven.",
        }};
        float y = panel.y + 76.0F;
        for (const char* line : lines) {
            SDL_RenderDebugText(renderer, panel.x + 34.0F, y, line);
            y += 42.0F;
        }
        return;
    }
    const char* speed =
        draft_settings.game_speed == SinglePlayerSpeed::slow ? "SLOW" :
        draft_settings.game_speed == SinglePlayerSpeed::fast ? "FAST" :
        "NORMAL";
    const std::array<std::string, 13> lines{{
        std::string{"G  SINGLE-PLAYER SPEED: "} + speed,
        "M  MUSIC VOLUME: " + std::to_string(draft_settings.music_volume),
        "E  EFFECTS VOLUME: " + std::to_string(draft_settings.effects_volume),
        "C  COMBAT CATEGORY: " + std::to_string(draft_settings.combat_volume),
        "I  INTERFACE CATEGORY: " +
            std::to_string(draft_settings.interface_volume),
        "B  AMBIENT CATEGORY: " +
            std::to_string(draft_settings.ambient_volume),
        std::string{"F  FULLSCREEN: "} + on_off(draft_settings.fullscreen),
        "R  SCROLL SPEED: " + std::to_string(draft_settings.scroll_speed) + "%",
        std::string{"X  EDGE SCROLL: "} + on_off(draft_settings.edge_scroll),
        std::string{"V  FOG DISPLAY: "} + on_off(draft_settings.fog),
        std::string{"P  MINIMAP: "} + on_off(draft_settings.minimap),
        "LOCALE: " + draft_settings.locale +
            "   FILE: " +
            (draft_settings.language_file.empty()
                 ? std::string{"BUILT-IN/AUTO"}
                 : draft_settings.language_file),
        "H HOTKEYS   A APPLY   S SAVE+APPLY   ESC CANCEL",
    }};
    float y = panel.y + 68.0F;
    for (const auto& line : lines) {
        SDL_RenderDebugText(renderer, panel.x + 34.0F, y, line.c_str());
        y += 36.0F;
    }
    set_color(renderer, {169, 204, 139, 255});
    SDL_RenderDebugText(
        renderer, panel.x + 34.0F, panel.y + 562.0F,
        active_options_status.substr(0, 82).c_str()
    );
    set_color(renderer, {158, 137, 91, 255});
    SDL_RenderDebugText(
        renderer, panel.x + 34.0F, panel.y + 588.0F,
        "AUDIO MIX APPLIES LIVE; FOCUS LOSS MUTES ALL CATEGORIES"
    );
}

void render_statistics_overlay(
    SDL_Renderer* renderer,
    const Simulation& simulation
) {
    if (!active_statistics_visible) return;
    const SDL_FRect shade{0.0F, 0.0F, static_cast<float>(view_pixel_width),
                          static_cast<float>(view_pixel_height + hud_height)};
    set_color(renderer, {5, 5, 4, 244});
    SDL_RenderFillRect(renderer, &shade);
    const SDL_FRect panel{120.0F, 36.0F, 1040.0F, 640.0F};
    render_beveled_panel(renderer, panel, {42, 33, 23, 255});
    const MatchStatistics statistics = simulation.match_statistics();
    const char* outcome =
        simulation.outcome() == MatchOutcome::blue_victory ? "BLUE VICTORY" :
        simulation.outcome() == MatchOutcome::red_victory ? "RED VICTORY" :
        simulation.outcome() == MatchOutcome::allied_victory
            ? "ALLIED VICTORY" :
        simulation.outcome() == MatchOutcome::draw ? "DRAW" :
        "MATCH IN PROGRESS";
    set_color(renderer, {238, 214, 145, 255});
    SDL_RenderDebugText(
        renderer, panel.x + 28.0F, panel.y + 22.0F,
        ui_text("statistics.title").data()
    );
    set_color(renderer, {226, 218, 190, 255});
    SDL_RenderDebugText(renderer, panel.x + 360.0F, panel.y + 22.0F, outcome);
    const std::string cause = statistics_victory_cause(
        simulation.outcome(),
        simulation.countdown_kind(Player::blue),
        simulation.countdown_kind(Player::red)
    );
    SDL_RenderDebugText(
        renderer, panel.x + 560.0F, panel.y + 22.0F, cause.c_str()
    );
    const std::array<std::string, 5> tabs{{
        "1 " + std::string{ui_text("statistics.economy")},
        "2 " + std::string{ui_text("statistics.military")},
        "3 " + std::string{ui_text("statistics.society")},
        "4 " + std::string{ui_text("statistics.technology")},
        "5 " + std::string{ui_text("statistics.timeline")},
    }};
    float tab_x = panel.x + 28.0F;
    for (std::size_t index = 0; index < tabs.size(); ++index) {
        set_color(
            renderer,
            static_cast<std::size_t>(active_statistics_tab) == index
                ? SDL_Color{245, 215, 122, 255}
                : SDL_Color{150, 136, 104, 255}
        );
        SDL_RenderDebugText(
            renderer, tab_x, panel.y + 60.0F, tabs[index].c_str()
        );
        tab_x += 190.0F;
    }
    set_color(renderer, {76, 142, 236, 255});
    SDL_RenderDebugText(
        renderer, panel.x + 630.0F, panel.y + 98.0F, "BLUE"
    );
    set_color(renderer, {222, 76, 68, 255});
    SDL_RenderDebugText(
        renderer, panel.x + 820.0F, panel.y + 98.0F, "RED"
    );
    if (active_statistics_tab == StatisticsTab::timeline) {
        const SDL_FRect graph{
            panel.x + 54.0F, panel.y + 142.0F, 900.0F, 360.0F,
        };
        set_color(renderer, {15, 15, 12, 255});
        SDL_RenderFillRect(renderer, &graph);
        set_color(renderer, {112, 101, 75, 255});
        SDL_RenderRect(renderer, &graph);
        const auto points = score_graph_points(statistics);
        for (std::size_t index = 1; index < points.size(); ++index) {
            const auto draw_segment = [&](float first, float second,
                                          SDL_Color color) {
                set_color(renderer, color);
                SDL_RenderLine(
                    renderer,
                    graph.x + points[index - 1].x * graph.w,
                    graph.y + graph.h - first * graph.h,
                    graph.x + points[index].x * graph.w,
                    graph.y + graph.h - second * graph.h
                );
            };
            draw_segment(
                points[index - 1].blue, points[index].blue,
                {76, 142, 236, 255}
            );
            draw_segment(
                points[index - 1].red, points[index].red,
                {222, 76, 68, 255}
            );
        }
        if (points.empty()) {
            set_color(renderer, {166, 151, 116, 255});
            SDL_RenderDebugText(
                renderer, graph.x + 330.0F, graph.y + 170.0F,
                "TIMELINE SAMPLES UNAVAILABLE"
            );
        }
        const std::string summary =
            "FINAL SCORE     " +
            std::to_string(statistics.current_score[0]) + "       " +
            std::to_string(statistics.current_score[1]) +
            "     SAMPLES " + std::to_string(points.size());
        SDL_RenderDebugText(
            renderer, graph.x + 250.0F, graph.y + graph.h + 24.0F,
            summary.c_str()
        );
    } else {
        const auto rows =
            statistics_rows(statistics, active_statistics_tab);
        float y = panel.y + 138.0F;
        for (const auto& row : rows) {
            set_color(renderer, {226, 218, 190, 255});
            SDL_RenderDebugText(
                renderer, panel.x + 72.0F, y, row.label.c_str()
            );
            const std::string blue =
                row.blue ? std::to_string(*row.blue)
                         : std::string{ui_text("statistics.unavailable")};
            const std::string red =
                row.red ? std::to_string(*row.red)
                        : std::string{ui_text("statistics.unavailable")};
            set_color(renderer, {116, 170, 244, 255});
            SDL_RenderDebugText(
                renderer, panel.x + 630.0F, y, blue.c_str()
            );
            set_color(renderer, {232, 112, 100, 255});
            SDL_RenderDebugText(
                renderer, panel.x + 820.0F, y, red.c_str()
            );
            y += 48.0F;
        }
    }
    set_color(renderer, {169, 204, 139, 255});
    SDL_RenderDebugText(
        renderer, panel.x + 220.0F, panel.y + 592.0F,
        active_statistics_postgame
            ? ("C " + std::string{ui_text("statistics.continue")} +
               "   R " + std::string{ui_text("statistics.rematch")} +
               "   B " + std::string{ui_text("statistics.back")}).c_str()
            : "F12 / ESC CLOSE   READ-ONLY LIVE SNAPSHOT"
    );
    set_color(renderer, {150, 136, 104, 255});
    SDL_RenderDebugText(
        renderer, panel.x + 255.0F, panel.y + 615.0F,
        "PROCEDURAL PANEL; NO MATCHING ARCHIVE STATISTICS ART PROVEN"
    );
}

void render_save_browser_overlay(SDL_Renderer* renderer) {
    if (!active_save_browser_visible) return;
    const SDL_FRect shade{0.0F, 0.0F, static_cast<float>(view_pixel_width),
                          static_cast<float>(view_pixel_height + hud_height)};
    set_color(renderer, {5, 5, 4, 244});
    SDL_RenderFillRect(renderer, &shade);
    const SDL_FRect panel{150.0F, 54.0F, 980.0F, 590.0F};
    render_beveled_panel(renderer, panel, {42, 33, 23, 255});
    set_color(renderer, {238, 214, 145, 255});
    SDL_RenderDebugText(
        renderer, panel.x + 28.0F, panel.y + 22.0F,
        ui_debug_text("browser.title").c_str()
    );
    const std::string entry_count = active_string_table != nullptr
        ? debug_font_fallback(active_string_table->count_text(
              "browser.entry_one",
              "browser.entry_other",
              static_cast<std::int64_t>(active_browser_entries.size())
          ))
        : std::to_string(active_browser_entries.size()) + " ENTRIES";
    SDL_RenderDebugText(
        renderer, panel.x + 790.0F, panel.y + 22.0F,
        entry_count.c_str()
    );
    set_color(renderer, {160, 142, 104, 255});
    SDL_RenderDebugText(
        renderer, panel.x + 28.0F, panel.y + 48.0F,
        "BOUNDED SDL USER-DATA ROOT; NO DELETE OR ARBITRARY PATH"
    );
    float y = panel.y + 88.0F;
    if (active_browser_entries.empty()) {
        set_color(renderer, {180, 164, 126, 255});
        SDL_RenderDebugText(
            renderer, panel.x + 44.0F, y, "NO BOUNDED SAVE OR REPLAY FILES"
        );
    }
    const std::size_t first =
        active_browser_selection > 6 ? active_browser_selection - 6 : 0;
    for (std::size_t index = first;
         index < active_browser_entries.size() && index < first + 8;
         ++index) {
        const BrowserEntry& entry = active_browser_entries[index];
        set_color(
            renderer,
            index == active_browser_selection
                ? SDL_Color{245, 215, 122, 255}
                : entry.status == BrowserFileStatus::compatible
                    ? SDL_Color{226, 218, 190, 255}
                    : SDL_Color{211, 132, 100, 255}
        );
        const char* kind =
            entry.kind == BrowserFileKind::save ? "SAVE" :
            entry.kind == BrowserFileKind::replay ? "REPLAY" :
            entry.kind == BrowserFileKind::legacy_commercial
                ? "LEGACY INSPECT-ONLY" : "UNKNOWN";
        const std::string line =
            (index == active_browser_selection ? "> " : "  ") +
            entry.filename + "  [" + kind + "]  " + entry.modified_time;
        SDL_RenderDebugText(
            renderer, panel.x + 44.0F, y, line.substr(0, 105).c_str()
        );
        y += 46.0F;
    }
    if (!active_browser_entries.empty()) {
        const BrowserEntry& entry =
            active_browser_entries[active_browser_selection];
        std::ostringstream metadata;
        metadata << "VERSION "
                 << (entry.version
                         ? std::to_string(*entry.version) : "N/A")
                 << "  TICK "
                 << (entry.tick ? std::to_string(*entry.tick) : "N/A")
                 << "  CIV "
                 << (entry.civilization
                         ? std::string{name(*entry.civilization)} : "N/A")
                 << "  COMMANDS "
                 << (entry.command_count
                         ? std::to_string(*entry.command_count) : "N/A");
        const char* saved_outcome =
            !entry.outcome ? "N/A" :
            *entry.outcome == MatchOutcome::ongoing ? "ONGOING" :
            *entry.outcome == MatchOutcome::blue_victory ? "BLUE VICTORY" :
            *entry.outcome == MatchOutcome::red_victory ? "RED VICTORY" :
            *entry.outcome == MatchOutcome::allied_victory
                ? "ALLIED VICTORY" : "DRAW";
        metadata << "  OUTCOME " << saved_outcome;
        set_color(renderer, {169, 204, 139, 255});
        SDL_RenderDebugText(
            renderer, panel.x + 44.0F, panel.y + 470.0F,
            metadata.str().c_str()
        );
        set_color(renderer, {214, 157, 113, 255});
        SDL_RenderDebugText(
            renderer, panel.x + 44.0F, panel.y + 494.0F,
            entry.diagnostic.substr(0, 108).c_str()
        );
    }
    set_color(renderer, {226, 218, 190, 255});
    const std::string slot =
        std::string{"SLOT: "} + active_save_slot +
        (active_save_slot_input ? "_" : "");
    SDL_RenderDebugText(
        renderer, panel.x + 44.0F, panel.y + 526.0F, slot.c_str()
    );
    SDL_RenderDebugText(
        renderer, panel.x + 360.0F, panel.y + 526.0F,
        "N NAME/SAVE   ENTER LOAD/PLAY   UP/DOWN SELECT   ESC CLOSE"
    );
    set_color(renderer, {169, 204, 139, 255});
    SDL_RenderDebugText(
        renderer, panel.x + 44.0F, panel.y + 554.0F,
        active_save_browser_status.substr(0, 112).c_str()
    );
}

void render_technology_tree_overlay(SDL_Renderer* renderer) {
    if (!active_technology_tree_visible) return;
    const SDL_FRect background{
        0.0F, 0.0F,
        static_cast<float>(view_pixel_width),
        static_cast<float>(view_pixel_height + hud_height),
    };
    set_color(renderer, {13, 11, 8, 250});
    SDL_RenderFillRect(renderer, &background);
    set_color(renderer, {238, 214, 145, 255});
    const std::string title =
        debug_font_fallback(ui_text("technology_tree.title")) + ": " +
        std::string{name(active_technology_tree.civilization)};
    SDL_RenderDebugText(renderer, 24.0F, 18.0F, title.c_str());
    SDL_RenderDebugText(
        renderer, 24.0F, 38.0F,
        ui_debug_text("technology_tree.help").c_str()
    );
    const std::array<std::string, 4> ages{
        ui_debug_text("technology_tree.dark_age"),
        ui_debug_text("technology_tree.feudal_age"),
        ui_debug_text("technology_tree.castle_age"),
        ui_debug_text("technology_tree.imperial_age")
    };
    for (int age = 0; age < 4; ++age) {
        const float x =
            80.0F + age * 310.0F * active_tree_zoom -
            active_tree_pan_x;
        set_color(renderer, {190, 158, 86, 255});
        SDL_RenderDebugText(renderer, x, 66.0F, ages[age].data());
    }
    set_color(renderer, {91, 78, 54, 150});
    for (const auto& [from_index, to_index] :
         active_technology_tree.dependencies) {
        if (from_index >= active_technology_tree.nodes.size() ||
            to_index >= active_technology_tree.nodes.size()) continue;
        const TechnologyTreeNode& from =
            active_technology_tree.nodes[from_index];
        const TechnologyTreeNode& to =
            active_technology_tree.nodes[to_index];
        SDL_RenderLine(
            renderer,
            24.0F + (from.x + 138) * active_tree_zoom -
                active_tree_pan_x,
            72.0F + (from.y + 21) * active_tree_zoom -
                active_tree_pan_y,
            24.0F + to.x * active_tree_zoom - active_tree_pan_x,
            72.0F + (to.y + 21) * active_tree_zoom -
                active_tree_pan_y
        );
    }
    for (const TechnologyTreeNode& node :
         active_technology_tree.nodes) {
        const SDL_FRect box{
            24.0F + node.x * active_tree_zoom - active_tree_pan_x,
            72.0F + node.y * active_tree_zoom - active_tree_pan_y,
            138.0F * active_tree_zoom,
            42.0F * active_tree_zoom,
        };
        if (box.x + box.w < 0.0F ||
            box.y + box.h < 62.0F ||
            box.x > view_pixel_width ||
            box.y > view_pixel_height + hud_height) continue;
        const SDL_Color fill =
            node.state == TechnologyTreeNodeState::disabled
                ? SDL_Color{48, 46, 42, 245}
            : node.state == TechnologyTreeNodeState::upgraded
                ? SDL_Color{40, 96, 52, 245}
                : node.kind == TechnologyTreeNodeKind::unit
                    ? SDL_Color{49, 70, 104, 245}
                : node.kind == TechnologyTreeNodeKind::building
                    ? SDL_Color{98, 70, 42, 245}
                    : SDL_Color{82, 58, 102, 245};
        const bool focused =
            &node == &active_technology_tree.nodes[
                std::min(
                    active_tree_focus,
                    active_technology_tree.nodes.size() - 1
                )
            ];
        render_beveled_panel(
            renderer, box,
            focused ? SDL_Color{132, 102, 42, 255} : fill
        );
        if (focused) {
            set_color(renderer, {255, 230, 126, 255});
            SDL_RenderRect(renderer, &box);
        }
        set_color(
            renderer,
            node.state == TechnologyTreeNodeState::disabled
                ? SDL_Color{132, 128, 118, 255}
                : SDL_Color{238, 230, 198, 255}
        );
        std::string label = node.label;
        if (label.size() > 19) label.resize(19);
        SDL_RenderDebugText(
            renderer, box.x + 5.0F, box.y + 6.0F,
            label.c_str()
        );
        const std::string state =
            node.state == TechnologyTreeNodeState::disabled
                ? ui_debug_text("technology_tree.disabled")
            : node.state == TechnologyTreeNodeState::upgraded
                ? ui_debug_text("technology_tree.researched")
                : ui_debug_text("technology_tree.available");
        SDL_RenderDebugText(
            renderer, box.x + 5.0F, box.y + box.h - 13.0F,
            state.c_str()
        );
    }
    std::string detail = active_tree_hover;
    if (detail.empty() && !active_technology_tree.nodes.empty()) {
        const TechnologyTreeNode& node =
            active_technology_tree.nodes[std::min(
                active_tree_focus,
                active_technology_tree.nodes.size() - 1
            )];
        std::ostringstream text;
        text << node.label << "  COST W" << node.wood
             << " F" << node.food << " G" << node.gold
             << " S" << node.stone << "  " << node.requirement;
        detail = text.str();
    }
    if (!detail.empty()) {
        const SDL_FRect help{
            18.0F,
            static_cast<float>(view_pixel_height + hud_height) - 48.0F,
            860.0F, 34.0F,
        };
        render_beveled_panel(renderer, help, {35, 28, 20, 250});
        set_color(renderer, {238, 230, 198, 255});
        SDL_RenderDebugText(
            renderer, help.x + 10.0F, help.y + 11.0F,
            detail.substr(0, 104).c_str()
        );
    }
    set_color(renderer, {135, 126, 102, 255});
    SDL_RenderDebugText(
        renderer, 900.0F,
        static_cast<float>(view_pixel_height + hud_height) - 24.0F,
        ui_debug_text("technology_tree.missing_evidence").c_str()
    );
}

void render_diplomacy_panel(
    SDL_Renderer* renderer,
    const Simulation& simulation,
    const MultiplayerPresentation* multiplayer
) {
    if (!active_diplomacy_panel_visible) return;
    const SDL_FRect shade{
        0.0F, 0.0F,
        static_cast<float>(view_pixel_width),
        static_cast<float>(view_pixel_height + hud_height),
    };
    set_color(renderer, {8, 7, 5, 220});
    SDL_RenderFillRect(renderer, &shade);
    const SDL_FRect panel{250.0F, 74.0F, 780.0F, 560.0F};
    render_beveled_panel(renderer, panel, {43, 34, 23, 255});
    set_color(renderer, {238, 214, 145, 255});
    SDL_RenderDebugText(
        renderer, panel.x + 28.0F, panel.y + 24.0F,
        ui_text("diplomacy.title").data()
    );
    const Diplomacy relation =
        simulation.diplomacy(Player::blue, Player::red);
    const char* stance =
        relation == Diplomacy::ally ? "ALLY" :
        relation == Diplomacy::neutral ? "NEUTRAL" : "ENEMY";
    const int blue_team = multiplayer ? multiplayer->config.blue.team :
        relation == Diplomacy::ally ? 1 : 1;
    const int red_team = multiplayer ? multiplayer->config.red.team :
        relation == Diplomacy::ally ? 1 : 2;
    set_color(renderer, {112, 166, 245, 255});
    const std::string blue =
        "BLUE  CIV " + std::string{name(
            simulation.civilization(Player::blue))} +
        "  TEAM " + std::to_string(blue_team);
    SDL_RenderDebugText(
        renderer, panel.x + 38.0F, panel.y + 72.0F,
        blue.c_str()
    );
    set_color(renderer, {235, 105, 88, 255});
    const std::string red =
        "RED   CIV " + std::string{name(
            simulation.civilization(Player::red))} +
        "  TEAM " + std::to_string(red_team);
    SDL_RenderDebugText(
        renderer, panel.x + 38.0F, panel.y + 100.0F,
        red.c_str()
    );
    set_color(renderer, {238, 230, 198, 255});
    const bool shared_vision =
        relation == Diplomacy::ally &&
        simulation.has_technology(
            active_view_player, Technology::cartography);
    SDL_RenderDebugText(
        renderer, panel.x + 38.0F, panel.y + 142.0F,
        (std::string{"STANCE: "} + stance +
         "   ALLIED VICTORY: " +
         (relation == Diplomacy::ally ? "ON" : "OFF") +
         "   SHARED VISION: " +
         (shared_vision ? "ON" : "OFF")).c_str()
    );
    SDL_RenderDebugText(
        renderer, panel.x + 38.0F, panel.y + 180.0F,
        "A ALLY   N NEUTRAL   E ENEMY"
    );
    std::ostringstream rates;
    rates << "MARKET BUY/SELL  F "
          << simulation.market_buy_price(
                 active_view_player, MarketResource::food) << '/'
          << simulation.market_sell_price(
                 active_view_player, MarketResource::food)
          << "  W "
          << simulation.market_buy_price(
                 active_view_player, MarketResource::wood) << '/'
          << simulation.market_sell_price(
                 active_view_player, MarketResource::wood)
          << "  S "
          << simulation.market_buy_price(
                 active_view_player, MarketResource::stone) << '/'
          << simulation.market_sell_price(
                 active_view_player, MarketResource::stone);
    SDL_RenderDebugText(
        renderer, panel.x + 38.0F, panel.y + 224.0F,
        rates.str().c_str()
    );
    const int tribute_fee =
        simulation.has_technology(
            active_view_player, Technology::banking) ? 0 :
        simulation.has_technology(
            active_view_player, Technology::coinage) ? 20 : 30;
    const std::string tribute =
        "TRIBUTE " + std::to_string(active_tribute_amount) + " " +
        std::string{name(active_tribute_resource)} +
        "   FEE " + std::to_string(tribute_fee) + "%";
    SDL_RenderDebugText(
        renderer, panel.x + 38.0F, panel.y + 276.0F,
        tribute.c_str()
    );
    SDL_RenderDebugText(
        renderer, panel.x + 38.0F, panel.y + 308.0F,
        "1 FOOD  2 WOOD  3 GOLD  4 STONE   -/+ AMOUNT"
    );
    const bool available =
        relation == Diplomacy::ally &&
        (active_tribute_resource == ResourceKind::food
             ? simulation.economy(active_view_player).food
         : active_tribute_resource == ResourceKind::wood
             ? simulation.economy(active_view_player).wood
         : active_tribute_resource == ResourceKind::gold
             ? simulation.economy(active_view_player).gold
             : simulation.economy(active_view_player).stone)
            >= active_tribute_amount;
    set_color(
        renderer,
        available
            ? SDL_Color{169, 204, 139, 255}
            : SDL_Color{150, 118, 105, 255}
    );
    SDL_RenderDebugText(
        renderer, panel.x + 38.0F, panel.y + 346.0F,
        available
            ? "ENTER: SEND TRIBUTE"
            : "TRIBUTE DISABLED: ALLY + RESOURCES REQUIRED"
    );
    set_color(renderer, {238, 230, 198, 255});
    SDL_RenderDebugText(
        renderer, panel.x + 38.0F, panel.y + 392.0F,
        "T CHAT ALL   Y CHAT ALLIES   F10/ESC CLOSE"
    );
    set_color(renderer, {169, 204, 139, 255});
    SDL_RenderDebugText(
        renderer, panel.x + 38.0F, panel.y + 438.0F,
        active_diplomacy_status.substr(0, 80).c_str()
    );
    set_color(renderer, {135, 126, 102, 255});
    SDL_RenderDebugText(
        renderer, panel.x + 38.0F, panel.y + panel.h - 30.0F,
        "RECONSTRUCTION PANEL; NO ORIGINAL DIPLOMACY ART CLAIM"
    );
}

std::size_t render(
    SDL_Renderer* renderer,
    const Simulation& simulation,
    const ComputerPlayer& computer,
    bool computer_debug,
    std::optional<BuildingKind> pending_building,
    bool pending_attack_move,
    bool pending_attack_ground,
    bool pending_patrol,
    bool pending_guard,
    bool pending_conversion,
    bool pending_trade_route,
    bool paused,
    const std::string& control_group_status,
    const std::optional<SelectionDrag>& selection_drag,
    std::optional<TilePosition> formation_preview_center,
    const ScenarioPresentation& scenario_presentation,
    const CampaignPresentation* campaign_presentation,
    const MultiplayerPresentation* multiplayer_presentation,
    float movement_alpha,
    std::uint64_t presentation_time_ms,
    const CameraView& camera
) {
    std::size_t rendered_tiles{};
    active_camera = camera;
    active_render_map = &simulation.map();
    const Uint64 signal_now = SDL_GetTicks();
    std::erase_if(
        active_map_signals,
        [signal_now](const VisibleMapSignal& visible) {
            return signal_now - visible.received_ms >= 6000;
        }
    );
    set_color(renderer, {22, 24, 20, 255});
    SDL_RenderClear(renderer);
    if (active_frontend_screen != FrontendScreen::hidden) {
        // Frontend background is opaque. Rendering full simulation behind it
        // delays event polling on large maps without producing visible pixels.
        SDL_SetRenderScale(renderer, 1.0F, 1.0F);
        SDL_SetRenderViewport(renderer, nullptr);
        render_frontend_overlay(renderer);
        render_options_overlay(renderer);
        render_save_browser_overlay(renderer);
        report_map_dimensions(simulation);
        capture_requested_frame(renderer, simulation, movement_alpha);
        SDL_RenderPresent(renderer);
        return 0;
    }
    const SDL_Rect world_viewport{
        0,
        0,
        view_pixel_width,
        view_pixel_height,
    };
    SDL_SetRenderViewport(renderer, &world_viewport);
    SDL_SetRenderScale(renderer, camera.zoom, camera.zoom);

    for (int y = 0; y < simulation.map().height(); ++y) {
        for (int x = 0; x < simulation.map().width(); ++x) {
            const TilePosition position{x, y};
            if (!tile_near_world_view(position, 96.0F)) {
                continue;
            }
            ++rendered_tiles;
            const bool visible =
                !active_settings.fog ||
                simulation.is_visible_to_controller(active_view_player, position);
            const bool explored =
                !active_settings.fog ||
                simulation.is_explored_to_controller(active_view_player, position);
            SDL_Color color =
                terrain_color(simulation.map().terrain_at(position));
            // TileEdge.Dat/BlkEdge.Dat dimensions and 47-class normalization
            // are proved, but state/compass/shape/palette/blend mappings are
            // not. Partial archive use would invent the state-to-frame map.
            if (!explored) {
                color = {9, 11, 10, 255};
            } else if (!visible) {
                color = {
                    static_cast<Uint8>(color.r * 0.32F),
                    static_cast<Uint8>(color.g * 0.32F),
                    static_cast<Uint8>(color.b * 0.32F),
                    255,
                };
            } else {
                const int variation =
                    (x * 17 + y * 31) % 7 - 3;
                color = shade_color(color, variation);
            }
            const SDL_FPoint top = tile_top(position);
            SDL_Texture* texture = nullptr;
            const Terrain terrain =
                simulation.map().terrain_at(position);
            bool full_texture_diamond = false;
            if (explored) {
                const std::vector<SDL_Texture*>* archive_frames =
                    &active_terrain_textures.grass_archive_frames;
                if (terrain == Terrain::water ||
                    terrain == Terrain::fish) {
                    texture = active_terrain_textures.water;
                    archive_frames =
                        &active_terrain_textures.water_archive_frames;
                } else if (terrain == Terrain::beach) {
                    texture = active_terrain_textures.beach;
                    archive_frames =
                        &active_terrain_textures.beach_archive_frames;
                } else if (terrain == Terrain::shallows) {
                    texture = active_terrain_textures.shallows;
                    archive_frames =
                        &active_terrain_textures.shallows_archive_frames;
                } else {
                    texture = active_terrain_textures.grass;
                }
                if (texture == nullptr && !archive_frames->empty()) {
                    const std::size_t flat_frame_count =
                        std::min<std::size_t>(
                            100, archive_frames->size()
                        );
                    const std::size_t frame_index =
                        static_cast<std::size_t>(
                            (x % 10) + (y % 10) * 10
                        ) % flat_frame_count;
                    texture = (*archive_frames)[frame_index];
                    full_texture_diamond = true;
                    if (SDL_Texture* transition =
                            terrain_transition_texture(
                                renderer,
                                simulation,
                                position,
                                frame_index
                            )) {
                        texture = transition;
                    }
                }
            }
            if (texture != nullptr) {
                if (visible) {
                    const Uint8 brightness = static_cast<Uint8>(
                        245 + ((x * 17 + y * 31) % 7 - 3)
                    );
                    color = {
                        brightness,
                        brightness,
                        brightness,
                        255,
                    };
                } else {
                    color = {82, 82, 82, 255};
                }
            }
            if (explored) {
                render_elevation_faces(
                    renderer, simulation, position, top
                );
            }
            fill_diamond(
                renderer,
                top,
                color,
                texture,
                position,
                full_texture_diamond
            );
            if (visible && texture == nullptr) {
                render_procedural_terrain_transitions(
                    renderer, simulation, position, top, color
                );
            }
            if (visible && is_water_surface(terrain)) {
                render_water_detail(
                    renderer,
                    simulation,
                    position,
                    top,
                    static_cast<float>(simulation.tick_number()) +
                        movement_alpha
                );
            }
        }
    }

    for (int depth = 0;
         depth < simulation.map().width() + simulation.map().height() - 1;
         ++depth) {
        for (int y = 0; y < simulation.map().height(); ++y) {
            const int x = depth - y;
            const TilePosition position{x, y};
            if (x >= 0 && x < simulation.map().width() &&
                tile_near_world_view(position) &&
                (!active_settings.fog ||
                 simulation.is_explored_to_controller(active_view_player, position)) &&
                is_resource_terrain(
                    simulation.map().terrain_at(position)
                )) {
                const Terrain terrain =
                    simulation.map().terrain_at(position);
                if (terrain == Terrain::forest) {
                    render_tree(
                        renderer,
                        simulation,
                        tile_top(position),
                        !active_settings.fog ||
                        simulation.is_visible_to_controller(active_view_player, position)
                    );
                } else {
                    render_resource_node(
                        renderer,
                        simulation,
                        tile_top(position),
                        terrain,
                        !active_settings.fog ||
                        simulation.is_visible_to_controller(active_view_player, position),
                        simulation.map().resource_amount_at(position)
                    );
                }
            }
        }

        for (const BuildingRubbleEffect& effect :
             simulation.rubble_effects()) {
            if (effect.position.x + effect.position.y != depth ||
                !tile_near_world_view(effect.position) ||
                (active_settings.fog &&
                 !simulation.is_visible_to_controller(active_view_player, effect.position))) {
                continue;
            }
            const auto exact_building_death =
                active_legacy_sprites.building_death_composites.find(
                    effect.kind
                );
            bool legacy_selection_existed =
                exact_building_death !=
                active_legacy_sprites
                    .building_death_composites.end();
            if (exact_building_death !=
                    active_legacy_sprites
                        .building_death_composites.end()) {
                const LegacyAnimatedComposite* composite =
                    exact_building_death->second.owner(effect.owner);
                const std::uint64_t elapsed =
                    static_cast<std::uint64_t>(std::max(
                        effect.total_ticks - effect.ticks_remaining,
                        0
                    ));
                const SDL_FPoint top = tile_top(effect.position);
                if (composite != nullptr &&
                    render_legacy_animated_composite(
                        renderer,
                        *composite,
                        {top.x, top.y + half_tile_height},
                        effect.position,
                        effect.position,
                        elapsed,
                        true
                    )) {
                    continue;
                }
            }
            if (effect.kind == BuildingKind::bombard_tower) {
                legacy_selection_existed = true;
                const LegacySprite& dying =
                    effect.owner == Player::blue
                    ? active_legacy_sprites.bombard_tower_dying_blue
                    : active_legacy_sprites.bombard_tower_dying_red;
                const SDL_FPoint top = tile_top(effect.position);
                if (render_legacy_building_sprite(
                        renderer, dying,
                        {top.x, top.y + half_tile_height}
                    )) {
                    continue;
                }
            }
            if (effect.kind == BuildingKind::outpost) {
                legacy_selection_existed = true;
                const LegacyAnimation* animation =
                    active_legacy_sprites.outpost_death.owner(effect.owner);
                const std::uint64_t elapsed =
                    static_cast<std::uint64_t>(std::max(
                        effect.total_ticks - effect.ticks_remaining,
                        0
                    ));
                const SDL_FPoint top = tile_top(effect.position);
                if (animation != nullptr && render_legacy_animation(
                        renderer,
                        *animation,
                        {top.x, top.y + half_tile_height},
                        effect.position,
                        effect.position,
                        elapsed,
                        true
                    )) {
                    continue;
                }
            }
            if (effect.kind == BuildingKind::wonder) {
                legacy_selection_existed = true;
                const LegacyAnimation* animation =
                    active_legacy_sprites.wonder_death.owner(effect.owner);
                const std::uint64_t elapsed =
                    static_cast<std::uint64_t>(std::max(
                        effect.total_ticks - effect.ticks_remaining,
                        0
                    ));
                const SDL_FPoint top = tile_top(effect.position);
                if (animation != nullptr && render_legacy_animation(
                        renderer,
                        *animation,
                        {top.x, top.y + half_tile_height},
                        effect.position,
                        effect.position,
                        elapsed,
                        true
                    )) {
                    continue;
                }
            }
            record_building_rubble_procedural_fallback(
                simulation, effect, legacy_selection_existed
            );
            const BuildingRules& rubble_rules = rules_for(effect.kind);
            const float life = std::clamp(
                static_cast<float>(effect.ticks_remaining) /
                    static_cast<float>(effect.total_ticks),
                0.0F,
                1.0F
            );
            const Uint8 base = static_cast<Uint8>(62 + 34 * life);
            for (int y = 0; y < rubble_rules.footprint_height; ++y) {
                for (int x = 0; x < rubble_rules.footprint_width; ++x) {
                    const TilePosition tile{
                        effect.position.x + x,
                        effect.position.y + y,
                    };
                    const SDL_FPoint tile_position = tile_top(tile);
                    fill_diamond(
                        renderer,
                        tile_position,
                        {base, static_cast<Uint8>(base - 8),
                         static_cast<Uint8>(base - 14), 255}
                    );
                    const int seed =
                        x * 13 + y * 19 + static_cast<int>(effect.kind) * 7;
                    const SDL_FRect stone{
                        tile_position.x - 9.0F + seed % 9,
                        tile_position.y + half_tile_height - 5.0F +
                            seed % 4,
                        8.0F + static_cast<float>(seed % 5),
                        5.0F,
                    };
                    set_color(renderer, {
                        static_cast<Uint8>(base + 28),
                        static_cast<Uint8>(base + 20),
                        static_cast<Uint8>(base + 10),
                        255,
                    });
                    SDL_RenderFillRect(renderer, &stone);
                    SDL_RenderLine(
                        renderer,
                        tile_position.x - 11.0F,
                        tile_position.y + half_tile_height + 4.0F,
                        tile_position.x + 10.0F,
                        tile_position.y + half_tile_height - 3.0F
                    );
                }
            }
        }

        for (const UnitDeathEffect& effect : simulation.death_effects()) {
            if (effect.position.x + effect.position.y != depth ||
                !tile_near_world_view(effect.position) ||
                !simulation.is_visible_to_controller(active_view_player, effect.position)) {
                continue;
            }
            const SDL_FPoint top = tile_top(effect.position);
            const auto exact_death =
                active_legacy_sprites.death.find(effect.kind);
            const bool legacy_selection_existed =
                exact_death != active_legacy_sprites.death.end();
            if (exact_death != active_legacy_sprites.death.end()) {
                const LegacyAnimation* animation =
                    exact_death->second.owner(effect.owner);
                const std::uint64_t elapsed =
                    static_cast<std::uint64_t>(std::max(
                        effect.total_ticks - effect.ticks_remaining,
                        0
                    ));
                if (animation != nullptr && render_legacy_animation(
                        renderer,
                        *animation,
                        {top.x, top.y + half_tile_height},
                        effect.previous_position,
                        effect.position,
                        elapsed * 2,
                        true
                    )) {
                    if (effect.kind == UnitKind::demolition_ship ||
                        effect.kind ==
                            UnitKind::heavy_demolition_ship) {
                        const float flare =
                            8.0F + 2.5F * static_cast<float>(elapsed);
                        set_color(renderer, {250, 132, 35, 220});
                        SDL_RenderLine(
                            renderer,
                            top.x - flare,
                            top.y + half_tile_height,
                            top.x + flare,
                            top.y + half_tile_height
                        );
                        SDL_RenderLine(
                            renderer,
                            top.x,
                            top.y + half_tile_height - flare * 0.5F,
                            top.x,
                            top.y + half_tile_height + flare * 0.5F
                        );
                    }
                    continue;
                }
            }
            record_unit_death_procedural_fallback(
                simulation, effect, legacy_selection_existed
            );
            const float age = std::clamp(
                static_cast<float>(
                    effect.total_ticks - effect.ticks_remaining
                ) / 4.0F,
                0.0F,
                1.0F
            );
            const bool siege =
                effect.kind == UnitKind::battering_ram ||
                effect.kind == UnitKind::mangonel;
            const float width = siege ? 22.0F : 14.0F;
            const float standing_height = siege ? 8.0F : 17.0F;
            const float height =
                standing_height * (1.0F - age) + 4.0F * age;
            const SDL_FRect shadow{
                top.x - width * 0.6F,
                top.y + half_tile_height + 3.0F,
                width * 1.2F,
                5.0F,
            };
            set_color(renderer, {42, 36, 30, 190});
            SDL_RenderFillRect(renderer, &shadow);
            SDL_Color body = effect.owner == Player::blue
                ? SDL_Color{70, 135, 240, 255}
                : SDL_Color{220, 70, 60, 255};
            const float fade = std::clamp(
                static_cast<float>(effect.ticks_remaining) / 10.0F,
                0.35F,
                1.0F
            );
            body.r = static_cast<Uint8>(body.r * fade);
            body.g = static_cast<Uint8>(body.g * fade);
            body.b = static_cast<Uint8>(body.b * fade);
            const SDL_FRect fallen{
                top.x - width * 0.5F,
                top.y + half_tile_height - height + 3.0F,
                width,
                height,
            };
            set_color(renderer, body);
            SDL_RenderFillRect(renderer, &fallen);
            if (!siege) {
                const SDL_FRect head{
                    top.x + width * (0.25F + 0.2F * age) - 3.0F,
                    fallen.y - 3.0F * (1.0F - age),
                    6.0F,
                    6.0F,
                };
                set_color(renderer, {212, 174, 126, 255});
                SDL_RenderFillRect(renderer, &head);
            }
        }

        for (const Building& building : simulation.buildings()) {
            if (building.position.x + building.position.y != depth ||
                !tile_near_world_view(building.position) ||
                (!simulation.observer_perspective(active_view_player) &&
                 active_settings.fog &&
                 building.owner != active_view_player &&
                 !simulation.is_building_visible(active_view_player, building))) {
                continue;
            }
            render_building(renderer, simulation, building);
            render_building_damage_overlay(
                renderer,
                simulation,
                building
            );
            render_garrison_presentation(
                renderer,
                simulation,
                building
            );
        }

        for (const Unit& unit : simulation.units()) {
            if (unit.garrisoned_in != 0 ||
                unit.position.x + unit.position.y != depth ||
                !tile_near_world_view(unit.position) ||
                (active_settings.fog &&
                 unit.owner != active_view_player &&
                 !simulation.is_visible_to_controller(active_view_player, unit.position))) {
                continue;
            }
            render_unit(
                renderer,
                simulation,
                unit,
                movement_alpha,
                presentation_time_ms
            );
        }
    }

    for (const Projectile& projectile : simulation.projectiles()) {
        const SDL_FPoint origin = tile_top(projectile.origin);
        const SDL_FPoint destination = tile_top(projectile.destination);
        const float progress = std::clamp(
            1.0F -
                (static_cast<float>(projectile.ticks_remaining) -
                 movement_alpha) /
                    static_cast<float>(projectile.total_ticks),
            0.0F,
            1.0F
        );
        const TilePosition projectile_tile{
            static_cast<int>(std::lround(
                static_cast<float>(projectile.origin.x) +
                static_cast<float>(
                    projectile.destination.x - projectile.origin.x
                ) * progress
            )),
            static_cast<int>(std::lround(
                static_cast<float>(projectile.origin.y) +
                static_cast<float>(
                    projectile.destination.y - projectile.origin.y
                ) * progress
            )),
        };
        if (!simulation.map().contains(projectile_tile) ||
            (active_settings.fog &&
             !simulation.is_visible_to_controller(active_view_player, projectile_tile))) {
            continue;
        }
        const SDL_FPoint position{
            origin.x + (destination.x - origin.x) * progress,
            origin.y + half_tile_height +
                (destination.y - origin.y) * progress -
                8.0F * std::sin(progress * 3.14159265F),
        };
        const float lane = static_cast<float>(projectile.visual_lane);
        const auto projectile_asset =
            projectile_asset_kind_for(projectile);
        const bool fire_stream =
            projectile_asset == ProjectileAssetKind::fire_stream;
        const bool cannonball =
            projectile_asset == ProjectileAssetKind::cannonball;
        const bool gunshot =
            projectile_asset == ProjectileAssetKind::gunpowder_shot;
        const bool throwing_axe =
            projectile_asset == ProjectileAssetKind::throwing_axe;
        const bool scorpion_bolt =
            projectile_asset == ProjectileAssetKind::scorpion_bolt;
        const bool arrow =
            projectile_asset == ProjectileAssetKind::arrow;
        const bool onager_stone =
            projectile_asset == ProjectileAssetKind::onager_primary ||
            projectile_asset == ProjectileAssetKind::onager_volley;
        const bool trebuchet_stone =
            projectile_asset == ProjectileAssetKind::trebuchet_stone;
        const std::uint64_t projectile_frame =
            static_cast<std::uint64_t>(
                projectile.total_ticks -
                projectile.ticks_remaining
            );
        if (scorpion_bolt &&
            render_exact_projectile_animation(
                renderer,
                active_legacy_sprites.scorpion_projectile,
                position,
                projectile.origin,
                projectile.destination,
                projectile_frame,
                18
            )) {
            // Exact 18-direction body/shadow transform rendered.
        } else if (onager_stone &&
            render_legacy_animation(
                renderer,
                projectile.visual_lane == 0
                    ? active_legacy_sprites
                        .onager_primary_projectile
                    : active_legacy_sprites
                        .onager_volley_projectile,
                position,
                projectile.origin,
                projectile.destination,
                projectile_frame,
                true
            )) {
            // Exact primary/volley Onager chain rendered.
        } else if (trebuchet_stone &&
            render_legacy_animation(
                renderer,
                active_legacy_sprites.trebuchet_projectile,
                position,
                projectile.origin,
                projectile.destination,
                projectile_frame,
                true
            )) {
            // Exact Trebuchet projectile rendered.
        } else if (gunshot &&
            render_legacy_animation(
                renderer,
                active_legacy_sprites.gunshot_projectile,
                position,
                projectile.origin,
                projectile.destination,
                projectile_frame,
                true
            )) {
            // Exact Janissary gunshot rendered.
        } else if (throwing_axe &&
            render_legacy_animation(
                renderer,
                active_legacy_sprites.axe_projectile,
                position,
                projectile.origin,
                projectile.destination,
                projectile_frame,
                true
            )) {
            // Exact spinning axe rendered.
        } else if (arrow &&
            render_exact_projectile_animation(
                renderer,
                active_legacy_sprites.arrow_projectile,
                {
                    position.x + lane * 3.0F,
                    position.y + lane * 1.5F,
                },
                projectile.origin,
                projectile.destination,
                projectile_frame,
                72
            )) {
            // Exact static 72-direction archive arrow rendered.
        } else if (cannonball) {
            if (!render_legacy_animation(
                    renderer,
                    active_legacy_sprites.cannonball_projectile,
                    position,
                    projectile.origin,
                    projectile.destination,
                    projectile_frame,
                    true
                )) {
                record_projectile_procedural_fallback(
                    simulation, projectile, projectile_asset
                );
                const SDL_FRect ball{
                    position.x - 5.0F,
                    position.y - 5.0F,
                    10.0F,
                    10.0F,
                };
                set_color(renderer, {42, 42, 38, 255});
                SDL_RenderFillRect(renderer, &ball);
            }
        } else if (fire_stream) {
            if (!render_legacy_animation(
                    renderer,
                    active_legacy_sprites.fire_projectile,
                    {
                        position.x + lane * 3.0F,
                        position.y + lane * 1.5F,
                    },
                    projectile.origin,
                    projectile.destination,
                    projectile_frame,
                    true
                )) {
                record_projectile_procedural_fallback(
                    simulation, projectile, projectile_asset
                );
                for (int flame = 0; flame < 3; ++flame) {
                    const float tail =
                        static_cast<float>(flame) * 5.0F;
                    const SDL_FRect ember{
                        position.x - tail - 3.0F + lane * 3.0F,
                        position.y + tail * 0.25F - 3.0F,
                        7.0F - static_cast<float>(flame),
                        5.0F - static_cast<float>(flame),
                    };
                    set_color(
                        renderer,
                        flame == 0
                        ? SDL_Color{255, 218, 65, 255}
                        : flame == 1
                            ? SDL_Color{245, 112, 28, 235}
                            : SDL_Color{165, 48, 24, 190}
                    );
                    SDL_RenderFillRect(renderer, &ember);
                }
            }
        } else if (projectile.splash_radius > 0) {
            record_projectile_procedural_fallback(
                simulation, projectile, projectile_asset
            );
            const SDL_FRect stone{
                position.x - 5.0F,
                position.y - 5.0F,
                10.0F,
                10.0F,
            };
            set_color(renderer, {105, 105, 100, 255});
            SDL_RenderFillRect(renderer, &stone);
        } else {
            record_projectile_procedural_fallback(
                simulation, projectile, projectile_asset
            );
            const SDL_FRect arrow{
                position.x - 2.0F + lane * 3.0F,
                position.y - 2.0F + lane * 1.5F,
                5.0F,
                3.0F,
            };
            set_color(renderer, {235, 205, 120, 255});
            SDL_RenderFillRect(renderer, &arrow);
        }
    }

    for (const ImpactEffect& effect : simulation.impact_effects()) {
        if (!simulation.is_visible_to_controller(active_view_player, effect.position)) {
            continue;
        }
        const SDL_FPoint top = tile_top(effect.position);
        const SDL_FPoint center{
            top.x,
            top.y + half_tile_height,
        };
        const float progress = std::clamp(
            1.0F -
                static_cast<float>(effect.ticks_remaining) /
                    static_cast<float>(effect.total_ticks),
            0.0F,
            1.0F
        );
        const auto impact_asset = impact_asset_kind_for(effect);
        const bool siege_source =
            impact_asset == ProjectileAssetKind::cannonball;
        const bool gunpowder_explosion =
            impact_asset == ProjectileAssetKind::gunpowder_shot;
        const bool rendered_exact_impact =
            (effect.source_kind == UnitKind::trebuchet ||
             gunpowder_explosion)
            ? render_legacy_animation(
                  renderer,
                  active_legacy_sprites.trebuchet_impact,
                  center,
                  effect.position,
                  {effect.position.x + 1, effect.position.y},
                  static_cast<std::uint64_t>(
                      effect.total_ticks - effect.ticks_remaining
                  ),
                  true
              )
            : (siege_source || !impact_asset) &&
                render_legacy_animation(
                    renderer,
                    active_legacy_sprites.siege_impact,
                    center,
                    effect.position,
                    {effect.position.x + 1, effect.position.y},
                    static_cast<std::uint64_t>(
                        effect.total_ticks - effect.ticks_remaining
                    ),
                    true
                );
        if (rendered_exact_impact) {
            // Exact source-specific siege impact rendered.
        } else if (effect.splash) {
            record_impact_procedural_fallback(
                simulation, effect, impact_asset
            );
            const float radius_x = 11.0F + 25.0F * progress;
            const float radius_y = 5.0F + 11.0F * progress;
            const Uint8 shade = static_cast<Uint8>(170 - 55 * progress);
            set_color(renderer, {shade, static_cast<Uint8>(shade - 20),
                                 static_cast<Uint8>(shade - 38), 255});
            SDL_RenderLine(
                renderer,
                center.x, center.y - radius_y,
                center.x + radius_x, center.y
            );
            SDL_RenderLine(
                renderer,
                center.x + radius_x, center.y,
                center.x, center.y + radius_y
            );
            SDL_RenderLine(
                renderer,
                center.x, center.y + radius_y,
                center.x - radius_x, center.y
            );
            SDL_RenderLine(
                renderer,
                center.x - radius_x, center.y,
                center.x, center.y - radius_y
            );
            const float lift = 10.0F * (1.0F - progress);
            for (int index = 0; index < 4; ++index) {
                const float direction = index % 2 == 0 ? -1.0F : 1.0F;
                const float spread =
                    (5.0F + static_cast<float>(index) * 3.0F) * progress;
                const SDL_FRect debris{
                    center.x + direction * spread - 1.5F,
                    center.y - lift + static_cast<float>(index % 2) * 4.0F,
                    4.0F,
                    4.0F,
                };
                SDL_RenderFillRect(renderer, &debris);
            }
        } else {
            record_impact_procedural_fallback(
                simulation, effect, impact_asset
            );
            const float size = 5.0F - 2.0F * progress;
            set_color(renderer, {225, 190, 105, 255});
            SDL_RenderLine(
                renderer,
                center.x - size, center.y - size,
                center.x + size, center.y + size
            );
            SDL_RenderLine(
                renderer,
                center.x + size, center.y - size,
                center.x - size, center.y + size
            );
            SDL_RenderLine(
                renderer,
                center.x, center.y - 10.0F + 4.0F * progress,
                center.x, center.y
            );
        }
    }

    if (simulation.selected_units().size() > 1) {
        for (EntityId id : simulation.selected_units()) {
            const auto found = std::ranges::find_if(
                simulation.units(),
                [id](const Unit& unit) { return unit.id == id; }
            );
            if (found == simulation.units().end()) continue;
            if (found->moving || !found->waypoints.empty()) {
                outline_diamond(
                    renderer,
                    tile_top(found->destination),
                    {196, 160, 58, 220}
                );
                const SDL_FPoint from = tile_top(found->position);
                const SDL_FPoint to = tile_top(found->destination);
                set_color(renderer, {196, 160, 58, 120});
                SDL_RenderLine(
                    renderer,
                    from.x,
                    from.y + half_tile_height,
                    to.x,
                    to.y + half_tile_height
                );
            }
        }
        if (formation_preview_center) {
            const auto slots = simulation.formation_destinations(
                simulation.selected_units(),
                *formation_preview_center,
                simulation.formation_kind(active_view_player)
            );
            for (TilePosition slot : slots) {
                outline_diamond(
                    renderer,
                    tile_top(slot),
                    {104, 190, 220, 210}
                );
            }
        }
    }

    if (computer_debug) {
        const ComputerPlayerStatus& status = computer.status();
        const auto marker = [renderer](
                                TilePosition position,
                                SDL_Color color,
                                float radius
                            ) {
            if (position.x < 0 || position.y < 0) return;
            const SDL_FPoint center = tile_top(position);
            set_color(renderer, color);
            SDL_RenderLine(
                renderer,
                center.x - radius, center.y + half_tile_height,
                center.x + radius, center.y + half_tile_height
            );
            SDL_RenderLine(
                renderer,
                center.x, center.y + half_tile_height - radius,
                center.x, center.y + half_tile_height + radius
            );
            outline_diamond(renderer, center, color);
        };
        marker(status.home, {80, 170, 255, 240}, 8.0F);
        marker(status.rally, {245, 202, 76, 240}, 10.0F);
        if (status.target) {
            marker(*status.target, {235, 76, 64, 240}, 13.0F);
        }
    }
    render_attack_range_diagnostic(renderer, simulation);

    for (const VisibleMapSignal& visible : active_map_signals) {
        const Uint64 age = SDL_GetTicks() - visible.received_ms;
        if (age >= 6000) continue;
        const SDL_FPoint position = tile_top(visible.signal.tile);
        const float radius = 10.0F +
            static_cast<float>((age / 120) % 12);
        const SDL_Color color =
            visible.signal.sender == Player::blue
            ? SDL_Color{90, 175, 255, 240}
            : SDL_Color{245, 80, 65, 240};
        set_color(renderer, color);
        const SDL_FRect pulse{
            position.x - radius,
            position.y + half_tile_height - radius,
            radius * 2.0F,
            radius * 2.0F,
        };
        SDL_RenderRect(renderer, &pulse);
        SDL_RenderLine(
            renderer,
            position.x - radius - 4.0F,
            position.y + half_tile_height,
            position.x + radius + 4.0F,
            position.y + half_tile_height
        );
    }

    if (pending_building && simulation.selected_unit() &&
        active_build_preview_tile) {
        const PlacementPreview preview = evaluate_building_placement(
            simulation,
            *simulation.selected_unit(),
            *pending_building,
            *active_build_preview_tile
        );
        const SDL_Color color = preview.valid
            ? SDL_Color{80, 232, 112, 220}
            : SDL_Color{238, 72, 58, 220};
        for (TilePosition tile : preview.footprint) {
            const SDL_FPoint top = tile_top(tile);
            outline_diamond(renderer, top, color);
            set_color(renderer, {color.r, color.g, color.b, 72});
            SDL_RenderLine(
                renderer,
                top.x - half_tile_width,
                top.y + half_tile_height,
                top.x + half_tile_width,
                top.y + half_tile_height
            );
        }
        const SDL_FPoint label = tile_top(*active_build_preview_tile);
        set_color(renderer, color);
        SDL_RenderDebugText(
            renderer,
            label.x - 40.0F,
            label.y - 18.0F,
            preview.reason.substr(0, 28).c_str()
        );
    }

    SDL_SetRenderScale(renderer, 1.0F, 1.0F);
    SDL_SetRenderViewport(renderer, nullptr);

    if (selection_drag) {
        const float left =
            std::min(selection_drag->start.x, selection_drag->current.x);
        const float top =
            std::min(selection_drag->start.y, selection_drag->current.y);
        SDL_FRect area{
            left,
            top,
            std::abs(selection_drag->current.x - selection_drag->start.x),
            std::abs(selection_drag->current.y - selection_drag->start.y),
        };
        set_color(renderer, {245, 245, 225, 220});
        SDL_RenderRect(renderer, &area);
    }

    render_hud(
        renderer,
        simulation,
        pending_building,
        pending_attack_move,
        pending_attack_ground,
        pending_patrol,
        pending_guard,
        pending_conversion,
        pending_trade_route,
        paused,
        control_group_status,
        camera
    );
    if (computer_debug) {
        render_computer_status(renderer, computer);
    }
    render_scenario_presentation(
        renderer, simulation, scenario_presentation
    );
    render_campaign_presentation(
        renderer, simulation, campaign_presentation
    );
    render_multiplayer_presentation(
        renderer, multiplayer_presentation
    );
    render_editor_overlay(renderer);
    render_frontend_overlay(renderer);
    render_technology_tree_overlay(renderer);
    render_diplomacy_panel(
        renderer, simulation, multiplayer_presentation
    );
    render_options_overlay(renderer);
    render_statistics_overlay(renderer, simulation);
    render_save_browser_overlay(renderer);
    report_map_dimensions(simulation);
    capture_requested_frame(renderer, simulation, movement_alpha);
    SDL_RenderPresent(renderer);
    return rendered_tiles;
}

TilePosition mouse_tile(
    float mouse_x,
    float mouse_y,
    const CameraView& camera
) {
    if (active_render_map == nullptr) return {};
    return pick_world_tile(
        *active_render_map,
        mouse_x,
        mouse_y,
        {
            camera.x,
            camera.y,
            camera.zoom,
            map_origin_x(),
            map_origin_y,
            half_tile_width,
            half_tile_height,
            elevation_pixel_step,
        }
    );
}

TilePosition mouse_tile(
    const SDL_MouseButtonEvent& button,
    const CameraView& camera
) {
    return mouse_tile(button.x, button.y, camera);
}

bool minimap_tile_at(
    float mouse_x,
    float mouse_y,
    const Simulation& simulation,
    TilePosition& tile
) {
    constexpr float padding = 7.0F;
    const int screen_width = view_pixel_width;
    const int screen_height = view_pixel_height + hud_height;
    const hud_layout::Rect exact_frame =
        hud_layout::anchored_large_panel(screen_width, screen_height);
    const SDL_FRect panel{
        static_cast<float>(exact_frame.x),
        static_cast<float>(exact_frame.y),
        static_cast<float>(exact_frame.width),
        static_cast<float>(exact_frame.height),
    };
    if (mouse_x < panel.x || mouse_x > panel.x + panel.w ||
        mouse_y < panel.y || mouse_y > panel.y + panel.h) {
        return false;
    }
    const float available_width = panel.w - padding * 2.0F;
    const float available_height = panel.h - padding * 2.0F;
    const float horizontal_scale = available_width /
        static_cast<float>(simulation.map().width() + simulation.map().height());
    const float vertical_scale = available_height /
        static_cast<float>(simulation.map().width() + simulation.map().height());
    const float cell_half_width =
        std::min(horizontal_scale, vertical_scale * 2.0F);
    const float center_x = panel.x + panel.w * 0.5F +
        static_cast<float>(
            simulation.map().height() - simulation.map().width()
        ) * cell_half_width * 0.5F;
    const float top = panel.y + padding;
    const float projected_x = (mouse_x - center_x) / cell_half_width;
    const auto rows = minimap::build_scaling_rows(
        simulation.map().width(),
        simulation.map().height(),
        std::max(1, static_cast<int>(available_height))
    );
    const int output_row = std::clamp(
        static_cast<int>(mouse_y - top),
        0,
        static_cast<int>(rows.size()) - 1
    );
    const float projected_y =
        static_cast<float>(rows[output_row].source_diagonal);
    tile = {
        static_cast<int>(std::floor((projected_y + projected_x) * 0.5F)),
        static_cast<int>(std::floor((projected_y - projected_x) * 0.5F)),
    };
    tile.x = std::clamp(tile.x, 0, simulation.map().width() - 1);
    tile.y = std::clamp(tile.y, 0, simulation.map().height() - 1);
    return true;
}

bool contextual_group_target(
    const Simulation& simulation,
    TilePosition target
) {
    if (!simulation.map().contains(target)) {
        return true;
    }
    if (is_resource_terrain(simulation.map().terrain_at(target))) {
        return true;
    }
    const auto farm = std::ranges::find_if(
        simulation.buildings(),
        [target](const Building& building) {
            return building.owner == active_view_player &&
                   building.kind == BuildingKind::farm &&
                   building.completed() &&
                   building.resource_amount > 0 &&
                   building.position == target;
        }
    );
    if (farm != simulation.buildings().end()) {
        return true;
    }
    const auto sheep = std::ranges::find_if(
        simulation.units(),
        [&simulation, target](const Unit& unit) {
            return unit.kind == UnitKind::sheep &&
                   unit.hit_points > 0 &&
                   unit.garrisoned_in == 0 &&
                   unit.position == target &&
                   simulation.is_visible_to_controller(
                       active_view_player, target
                   );
        }
    );
    if (sheep != simulation.units().end()) {
        return true;
    }
    const auto enemy_unit = std::ranges::find_if(
        simulation.units(),
        [&simulation, target](const Unit& unit) {
            return unit.owner == Player::red &&
                   unit.position == target &&
                   simulation.is_visible_to_controller(active_view_player, target);
        }
    );
    const auto enemy_building = std::ranges::find_if(
        simulation.buildings(),
        [&simulation, target](const Building& building) {
            const BuildingRules& rules = rules_for(building.kind);
            const bool contains_target =
                target.x >= building.position.x &&
                target.x <
                    building.position.x + rules.footprint_width &&
                target.y >= building.position.y &&
                target.y <
                    building.position.y + rules.footprint_height;
            return building.owner == Player::red &&
                   contains_target &&
                   simulation.is_building_visible(active_view_player, building);
        }
    );
    return enemy_unit != simulation.units().end() ||
           enemy_building != simulation.buildings().end();
}

// Modern choice: the renderer audit fixtures under resources/ are
// deliberately tiny, single-purpose sprite stages whose telemetry is
// position independent, and regenerating all of them at 255x255 would
// churn the pixel-audit suite for no gameplay gain. This diagnostic lets
// tools/run_renderer_runtime_coverage.py keep loading them while every
// ordinary launch path stays 255x255. Unset, no smaller map is accepted.
[[nodiscard]] bool audit_map_sizes_allowed() {
    const char* requested = SDL_getenv("AOE_AUDIT_ANY_MAP_SIZE");
    return requested != nullptr && requested[0] != '\0' &&
           requested[0] != '0';
}

Scenario load_presentable_scenario(
    const std::filesystem::path& path
) {
    Scenario scenario = load_scenario(path);
    if (!audit_map_sizes_allowed() &&
        (scenario.map.width() != 255 || scenario.map.height() != 255)) {
        throw std::runtime_error(
            "playable scenarios must use a 255x255 map"
        );
    }
    return scenario;
}

Simulation load_presentable_game(
    const std::filesystem::path& path
) {
    Simulation simulation = load_game(path);
    if (!audit_map_sizes_allowed() &&
        (simulation.map().width() != 255 ||
         simulation.map().height() != 255)) {
        throw std::runtime_error(
            "playable saves must use a 255x255 map"
        );
    }
    return simulation;
}

ScenarioStartup load_bundled_scenario() {
    if (const char* requested = SDL_getenv("AOE_CAMPAIGN")) {
        if (requested[0] != '\0') {
            Campaign campaign = load_campaign(requested);
            const std::filesystem::path progress_path =
                SDL_getenv("AOE_CAMPAIGN_PROGRESS") != nullptr
                ? SDL_getenv("AOE_CAMPAIGN_PROGRESS")
                : std::filesystem::path(requested).concat(".progress");
            CampaignProgressLoad loaded =
                load_campaign_progress(campaign, progress_path);
            if (loaded.status == CampaignProgressStatus::stale) {
                throw std::runtime_error(
                    "campaign progress is stale; start fresh explicitly"
                );
            }
            CampaignScenarioEntry selected =
                current_campaign_scenario(campaign, loaded.progress);
            return {
                load_presentable_scenario(selected.path),
                CampaignPresentation{
                    std::move(campaign),
                    std::move(loaded.progress),
                    progress_path,
                    std::move(selected),
                    true,
                    false,
                    false,
                    false,
                    CampaignPresentation::Screen::briefing,
                    {},
                    {},
                },
            };
        }
    }
    if (const char* requested = SDL_getenv("AOE_SCENARIO_PATH")) {
        if (requested[0] != '\0') {
            return {
                load_presentable_scenario(requested), std::nullopt
            };
        }
    }
    const std::filesystem::path base = SDL_GetBasePath();
    const std::filesystem::path candidates[] = {
        base / "resources/demo.scenario",
        base / "../Resources/demo.scenario",
    };
    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate)) {
            return {
                load_presentable_scenario(candidate), std::nullopt
            };
        }
    }
    throw std::runtime_error("bundled demo.scenario not found");
}

std::array<bool, 8> required_legacy_owner_slots(
    const Scenario& scenario
) {
    std::array<bool, 8> result{};
    // Legacy front-end and default scenarios always expose blue/red.
    result[0] = true;
    result[1] = true;
    for (const ScenarioRosterEntry& entry : scenario.roster_entries) {
        const auto slot = entry.roster.slot.index();
        if (entry.roster.occupied && slot) result[*slot] = true;
    }
    const auto include_owner = [&result](EntityOwner owner) {
        const auto slot = owner.slot_index();
        if (slot) result[*slot] = true;
    };
    for (const UnitPlacement& unit : scenario.units) {
        include_owner(unit.owner);
    }
    for (const BuildingPlacement& building : scenario.buildings) {
        include_owner(building.owner);
    }
    return result;
}

std::array<bool, 19> required_legacy_civilizations(
    const Scenario& scenario
) {
    std::array<bool, 19> result{};
    const auto include = [&result](Civilization civilization) {
        result[static_cast<std::size_t>(civilization)] = true;
    };
    if (scenario.roster_schema) {
        for (const ScenarioRosterEntry& entry : scenario.roster_entries) {
            if (entry.roster.occupied) include(entry.civilization);
        }
    } else {
        include(scenario.blue_civilization);
        include(scenario.red_civilization);
    }
    return result;
}

std::filesystem::path user_data_directory() {
    char* raw_path = SDL_GetPrefPath(
        "Software Archaeology",
        "AoE Archaeology"
    );
    if (raw_path == nullptr) {
        throw std::runtime_error(
            std::string{"cannot locate user data directory: "} + SDL_GetError()
        );
    }

    const std::filesystem::path path{raw_path};
    SDL_free(raw_path);
    std::filesystem::create_directories(path);
    return path;
}

}  // namespace

int SdlApp::run() {
    active_view_player = Player::blue;
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        throw std::runtime_error(SDL_GetError());
    }
    auto audio = AudioSystem::start_from_environment();

    if (const char* requested_size = SDL_getenv("AOE_WINDOW_SIZE")) {
        int width{};
        int height{};
        if (SDL_sscanf(requested_size, "%dx%d", &width, &height) == 2 &&
            width >= 640 && height >= 360) {
            view_pixel_width = width;
            logical_screen_height = height;
            view_pixel_height = height - hud_height;
        }
    }
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    if (!SDL_CreateWindowAndRenderer(
            "AoE II HD Archaeology Reconstruction",
            view_pixel_width,
            view_pixel_height + hud_height,
            SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY,
            &window,
            &renderer
        )) {
        SDL_Quit();
        throw std::runtime_error(SDL_GetError());
    }
    if (!SDL_SetRenderLogicalPresentation(
            renderer,
            view_pixel_width,
            view_pixel_height + hud_height,
            SDL_LOGICAL_PRESENTATION_LETTERBOX
        )) {
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        throw std::runtime_error(SDL_GetError());
    }
    const std::string_view requested_locale =
        SDL_getenv("AOE_LOCALE") != nullptr
        ? SDL_getenv("AOE_LOCALE")
        : "en";
    const std::optional<std::filesystem::path> language_file =
        SDL_getenv("AOE_LANGUAGE_FILE") != nullptr &&
        SDL_getenv("AOE_LANGUAGE_FILE")[0] != '\0'
        ? std::optional<std::filesystem::path>{
              SDL_getenv("AOE_LANGUAGE_FILE")
          }
        : std::nullopt;
    LocalizationResult localization =
        negotiate_localization(requested_locale, language_file);
    if (!language_file) {
        if (const auto asset_root = configured_asset_root()) {
            const std::filesystem::path language_root =
                *asset_root / "Bin" / "en";
            const std::vector<std::filesystem::path> sources{
                language_root / "language.dll",
                language_root / "language_x1.dll",
                language_root / "language_x1_p1.dll",
            };
            try {
                LegacyLanguageReport legacy =
                    load_legacy_language_sources(
                        requested_locale, 0x0409, sources, {}
                    );
                localization.table = std::move(legacy.table);
                SDL_Log(
                    "using packaged legacy localization: %zu strings from "
                    "%zu sources",
                    legacy.extracted.size(),
                    legacy.sources.size()
                );
            } catch (const std::exception& error) {
                SDL_LogWarn(
                    SDL_LOG_CATEGORY_APPLICATION,
                    "cannot load packaged legacy localization: %s",
                    error.what()
                );
            }
        }
    }
    active_string_table = &localization.table;
    ScenarioStartup startup = load_bundled_scenario();
    Scenario demo_scenario = std::move(startup.scenario);
    std::optional<CampaignPresentation> campaign_presentation =
        std::move(startup.campaign);
    active_terrain_textures = load_local_terrain_textures(renderer);
    active_legacy_sprites = load_local_legacy_sprites(
        renderer,
        required_legacy_owner_slots(demo_scenario),
        required_legacy_civilizations(demo_scenario)
    );
    SDL_Cursor* archive_cursor =
        load_archive_cursor(aoe::cursor::State::normal);
    if (archive_cursor != nullptr) {
        SDL_SetCursor(archive_cursor);
    }

    if (campaign_presentation) {
        campaign_presentation->visible = true;
        if (const char* narration =
                SDL_getenv("AOE_CAMPAIGN_NARRATION_PATH")) {
            campaign_presentation->optional_narration_path = narration;
        }
        if (const char* cinematic =
                SDL_getenv("AOE_CAMPAIGN_CINEMATIC_PATH")) {
            campaign_presentation->optional_cinematic_path = cinematic;
        }
        if (const char* requested =
                SDL_getenv("AOE_CAMPAIGN_STATUS_VISIBLE");
            requested != nullptr && requested[0] != '0') {
            campaign_presentation->visible = true;
        }
    }
    const auto new_game = [&demo_scenario] {
        return create_simulation(demo_scenario);
    };
    double gameplay_benchmark_command_ms{};
    std::size_t gameplay_benchmark_commanded_units{};
    Simulation simulation = new_game();
    if (const char* benchmark =
            SDL_getenv("AOE_GAMEPLAY_BENCHMARK_PATH");
        benchmark != nullptr && benchmark[0] != '\0') {
        RandomMapSettings benchmark_settings = active_random_settings;
        benchmark_settings.size = RandomMapSize::maximum;
        benchmark_settings.seed = 424242;
        const RmsMapResult benchmark_map =
            generate_rms_map(benchmark_settings);
        if (!benchmark_map.scenario) {
            throw std::runtime_error(
                "could not generate gameplay benchmark map: " +
                benchmark_map.error
            );
        }
        simulation = create_simulation(*benchmark_map.scenario);
        std::vector<EntityId> moving_units;
        TilePosition origin{};
        for (const Unit& unit : simulation.units()) {
            if (unit.owner == active_view_player &&
                unit.garrisoned_in == 0 && !is_ship(unit.kind)) {
                if (moving_units.empty()) {
                    origin = unit.position;
                }
                moving_units.push_back(unit.id);
            }
        }
        TilePosition destination{
            std::clamp(
                origin.x + 40, 0, simulation.map().width() - 1
            ),
            std::clamp(
                origin.y + 20, 0, simulation.map().height() - 1
            ),
        };
        for (int radius = 0; radius <= 20; ++radius) {
            bool found{};
            for (int y = destination.y - radius;
                 y <= destination.y + radius && !found;
                 ++y) {
                for (int x = destination.x - radius;
                     x <= destination.x + radius;
                     ++x) {
                    const TilePosition candidate{x, y};
                    if (simulation.map().contains(candidate) &&
                        simulation.map().walkable(candidate) &&
                        !is_resource_terrain(
                            simulation.map().terrain_at(candidate)
                        )) {
                        destination = candidate;
                        found = true;
                        break;
                    }
                }
            }
            if (found) {
                break;
            }
        }
        simulation.select_units(moving_units, active_view_player);
        const auto command_started =
            std::chrono::steady_clock::now();
        if (simulation.command_formation(
                moving_units,
                destination,
                FormationKind::compact
            )) {
            gameplay_benchmark_commanded_units =
                moving_units.size();
        }
        gameplay_benchmark_command_ms =
            std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - command_started
            ).count();
    }
    std::optional<ScenarioEditor> scenario_editor;
    std::optional<Scenario> random_map_preview;
    std::filesystem::path scenario_editor_path =
        user_data_directory() / "edited-scenario.scenario";
    if (const char* editor = SDL_getenv("AOE_EDITOR");
        editor != nullptr && editor[0] != '0') {
        if (const char* input = SDL_getenv("AOE_EDITOR_INPUT");
            input != nullptr && input[0] != '\0') {
            scenario_editor.emplace(
                load_presentable_scenario(input)
            );
            simulation = create_simulation(
                scenario_editor->scenario()
            );
        } else {
            scenario_editor.emplace(demo_scenario);
        }
        active_editor_overlay = true;
        if (const char* output = SDL_getenv("AOE_EDITOR_PATH")) {
            scenario_editor_path = output;
        }
    }
    if (const char* menu = SDL_getenv("AOE_MAIN_MENU");
        menu != nullptr) {
        active_frontend_screen = menu[0] == '0'
            ? FrontendScreen::hidden
            : FrontendScreen::main_menu;
    }
    if (const char* setup = SDL_getenv("AOE_RANDOM_MAP_SETUP");
        setup != nullptr && setup[0] != '0') {
        active_frontend_screen = FrontendScreen::single_player_setup;
        if (const char* seed = SDL_getenv("AOE_RANDOM_MAP_SEED")) {
            active_random_settings.seed =
                static_cast<std::uint64_t>(
                    std::max(SDL_atoi(seed), 0)
                );
        }
    }
    if (const char* requested =
            SDL_getenv("AOE_SCREENSHOT_SELECT_ALL_UNITS");
        requested != nullptr && requested[0] != '0') {
        std::vector<EntityId> ids;
        for (const Unit& unit : simulation.units()) {
            if (unit.owner == active_view_player &&
                unit.garrisoned_in == 0) {
                ids.push_back(unit.id);
            }
        }
        simulation.select_units(ids, active_view_player);
    }
    if (const char* requested =
            SDL_getenv("AOE_SCREENSHOT_FORMATION")) {
        char kind_name[16]{};
        TilePosition center;
        if (SDL_sscanf(
                requested,
                "%15[^:]:%d,%d",
                kind_name,
                &center.x,
                &center.y
            ) == 3) {
            const std::string_view kind_text{kind_name};
            const FormationKind kind =
                kind_text == "line" ? FormationKind::line
                : kind_text == "box" ? FormationKind::box
                : kind_text == "staggered"
                    ? FormationKind::staggered
                : kind_text == "flank" ? FormationKind::flank
                : FormationKind::compact;
            simulation.set_formation_kind(active_view_player, kind);
            simulation.command_formation(
                simulation.selected_units(),
                center,
                kind
            );
        }
    }
    if (const char* requested = SDL_getenv("AOE_SCREENSHOT_SELECT_TILE")) {
        TilePosition tile;
        if (SDL_sscanf(requested, "%d,%d", &tile.x, &tile.y) == 2) {
            if (!simulation.select_unit_at(tile, active_view_player)) {
                simulation.select_building_at(tile, active_view_player);
            }
        }
    }
    if (const char* requested =
            SDL_getenv("AOE_SCREENSHOT_ATTACK_GROUND")) {
        TilePosition tile;
        if (SDL_sscanf(requested, "%d,%d", &tile.x, &tile.y) == 2) {
            for (EntityId unit : simulation.selected_units()) {
                simulation.command_attack_ground(unit, tile);
            }
        }
    }
    if (const char* requested =
            SDL_getenv("AOE_SCREENSHOT_PACK_TREBUCHET")) {
        const bool pack = requested[0] != '0';
        for (EntityId unit : simulation.selected_units()) {
            simulation.command_pack_trebuchet(unit, pack);
        }
    }
    if (const char* requested = SDL_getenv("AOE_SCREENSHOT_COMMAND_TILE")) {
        TilePosition tile;
        if (SDL_sscanf(requested, "%d,%d", &tile.x, &tile.y) == 2) {
            simulation.command_selected(tile);
        }
    }
    if (const char* requested =
            SDL_getenv("AOE_SCREENSHOT_BUILD_BOMBARD_TOWER")) {
        TilePosition tile;
        if (SDL_sscanf(requested, "%d,%d", &tile.x, &tile.y) == 2 &&
            simulation.selected_unit()) {
            execute(
                simulation,
                ConstructBuildingCommand{
                    *simulation.selected_unit(),
                    BuildingKind::bombard_tower,
                    tile,
                }
            );
        }
    }
    if (const char* requested =
            SDL_getenv("AOE_SCREENSHOT_BUILD_OUTPOST")) {
        TilePosition tile;
        if (SDL_sscanf(requested, "%d,%d", &tile.x, &tile.y) == 2 &&
            simulation.selected_unit()) {
            if (execute(
                    simulation,
                    ConstructBuildingCommand{
                        *simulation.selected_unit(),
                        BuildingKind::outpost,
                        tile,
                    }
                )) {
                simulation.select_building_at(tile, active_view_player);
            }
        }
    }
    if (const char* requested =
            SDL_getenv("AOE_SCREENSHOT_BUILD_WONDER")) {
        TilePosition tile;
        if (SDL_sscanf(requested, "%d,%d", &tile.x, &tile.y) == 2 &&
            simulation.selected_unit()) {
            if (execute(
                    simulation,
                    ConstructBuildingCommand{
                        *simulation.selected_unit(),
                        BuildingKind::wonder,
                        tile,
                    }
                )) {
                simulation.select_building_at(tile, active_view_player);
            }
        }
    }
    if (const char* requested =
            SDL_getenv("AOE_SCREENSHOT_BUILD_FISH_TRAP")) {
        TilePosition tile;
        if (SDL_sscanf(requested, "%d,%d", &tile.x, &tile.y) == 2 &&
            simulation.selected_unit()) {
            if (execute(
                simulation,
                ConstructBuildingCommand{
                    *simulation.selected_unit(),
                    BuildingKind::fish_trap,
                    tile,
                }
            )) {
                simulation.select_building_at(tile, active_view_player);
            }
        }
    }
    if (const char* requested =
            SDL_getenv("AOE_SCREENSHOT_TRADE_ROUTE")) {
        TilePosition tile;
        if (SDL_sscanf(requested, "%d,%d", &tile.x, &tile.y) == 2 &&
            simulation.selected_unit()) {
            const auto endpoint = std::ranges::find_if(
                simulation.buildings(),
                [tile](const Building& building) {
                    const BuildingRules& rules =
                        rules_for(building.kind);
                    return tile.x >= building.position.x &&
                        tile.y >= building.position.y &&
                        tile.x < building.position.x +
                            rules.footprint_width &&
                        tile.y < building.position.y +
                            rules.footprint_height;
                }
            );
            if (endpoint != simulation.buildings().end()) {
                simulation.command_trade_route(
                    *simulation.selected_unit(),
                    endpoint->id
                );
            }
        }
    }
    if (const char* requested =
            SDL_getenv("AOE_SCREENSHOT_MISSIONARY_ACTION")) {
        TilePosition tile;
        char action[16]{};
        if (SDL_sscanf(
                requested, "%15[^:]:%d,%d",
                action, &tile.x, &tile.y
            ) == 3 &&
            simulation.selected_unit()) {
            const auto target = std::ranges::find_if(
                simulation.units(),
                [tile](const Unit& unit) {
                    return unit.position == tile;
                }
            );
            if (target != simulation.units().end()) {
                if (std::string_view{action} == "convert") {
                    simulation.command_convert(
                        *simulation.selected_unit(), target->id
                    );
                } else if (std::string_view{action} == "heal") {
                    simulation.command_heal(
                        *simulation.selected_unit(), target->id
                    );
                }
            }
        }
    }
    if (const char* requested =
            SDL_getenv("AOE_SCREENSHOT_QUEUE_UNIT")) {
        if (simulation.selected_building()) {
            const std::string_view kind{requested};
            if (kind == "trebuchet") {
                simulation.queue_unit_at(
                    *simulation.selected_building(),
                    UnitKind::trebuchet
                );
            } else if (kind == "longbowman") {
                simulation.queue_unit_at(
                    *simulation.selected_building(),
                    UnitKind::longbowman
                );
            } else if (kind == "petard") {
                simulation.queue_unit_at(
                    *simulation.selected_building(),
                    UnitKind::petard
                );
            }
        }
    }
    ComputerDifficulty computer_difficulty = ComputerDifficulty::standard;
    if (const char* requested = SDL_getenv("AOE_AI_DIFFICULTY")) {
        const std::string_view value{requested};
        if (value == "easy") {
            computer_difficulty = ComputerDifficulty::easy;
        } else if (value == "hard") {
            computer_difficulty = ComputerDifficulty::hard;
        } else if (value == "expert") {
            computer_difficulty = ComputerDifficulty::expert;
        }
    }
    ComputerPlayer computer(Player::red, computer_difficulty);
    if (const char* tree = SDL_getenv("AOE_TECH_TREE");
        tree != nullptr && tree[0] != '0') {
        active_technology_tree_visible = true;
        active_technology_tree = build_technology_tree(
            simulation.civilization(active_view_player)
        );
    }
    if (const char* diplomacy =
            SDL_getenv("AOE_DIPLOMACY_PANEL");
        diplomacy != nullptr && diplomacy[0] != '0') {
        active_diplomacy_panel_visible = true;
    }
    const auto refresh_random_map_preview = [&] {
        try {
            RandomMapSettings settings = active_random_settings;
            settings.blue_civilization = active_setup_civilization;
            std::optional<std::string> custom_source;
            if (const char* path = SDL_getenv("AOE_RMS_PATH");
                path != nullptr && path[0] != '\0') {
                std::ifstream input(path, std::ios::binary);
                if (!input) {
                    throw std::runtime_error(
                        std::string{"could not open RMS: "} + path
                    );
                }
                custom_source.emplace(
                    std::istreambuf_iterator<char>{input},
                    std::istreambuf_iterator<char>{}
                );
                active_random_map_source =
                    "RMS " + std::filesystem::path(path).filename().string();
            } else {
                active_random_map_source = "CLASSIC RMS";
            }
            const RmsMapResult generated = generate_rms_map(
                settings,
                custom_source
                    ? std::optional<std::string_view>{*custom_source}
                    : std::nullopt
            );
            if (!generated.scenario) {
                throw std::runtime_error(generated.error);
            }
            random_map_preview = *generated.scenario;
            active_random_preview = &*random_map_preview;
            active_frontend_status =
                active_random_map_source + "  HASH " +
                random_map_hash(*random_map_preview).substr(0, 22);
        } catch (const std::exception& error) {
            random_map_preview.reset();
            active_random_preview = nullptr;
            active_frontend_status =
                std::string{"GENERATION FAILED: "} + error.what();
        }
    };
    if (active_frontend_screen ==
        FrontendScreen::single_player_setup) {
        refresh_random_map_preview();
    }
    bool computer_debug = false;
    if (const char* requested = SDL_getenv("AOE_AI_DEBUG")) {
        computer_debug =
            std::string_view{requested} == "1" ||
            std::string_view{requested} == "true";
    }
    if (const char* requested =
            SDL_getenv("AOE_SCREENSHOT_ADVANCE_TICKS")) {
        const int ticks = std::max(SDL_atoi(requested), 0);
        for (int tick = 0; tick < ticks; ++tick) {
            simulation.update();
            computer.update(simulation);
        }
    }
    std::optional<MultiplayerPresentation> multiplayer_presentation;
    std::optional<LocalhostMultiplayerRuntime> multiplayer_runtime;
    std::uint64_t multiplayer_exit_tick{};
    std::filesystem::path multiplayer_state_path;
    bool multiplayer_state_written = false;
    bool multiplayer_script_chat_sent = false;
    bool multiplayer_script_signal_sent = false;
    bool multiplayer_checkpoint_written = false;
    std::uint64_t multiplayer_script_checkpoint_tick{};
    const bool multiplayer_script_control =
        SDL_getenv("AOE_MULTIPLAYER_SCRIPT_CONTROL") != nullptr;
    int multiplayer_control_stage{};
    int multiplayer_paused_frames{};
    std::uint64_t multiplayer_pause_tick{};
    bool multiplayer_pause_tick_frozen{};
    const std::string multiplayer_script_chat =
        SDL_getenv("AOE_MULTIPLAYER_SCRIPT_CHAT") != nullptr
        ? SDL_getenv("AOE_MULTIPLAYER_SCRIPT_CHAT") : "";
    const std::string multiplayer_script_signal =
        SDL_getenv("AOE_MULTIPLAYER_SCRIPT_SIGNAL") != nullptr
        ? SDL_getenv("AOE_MULTIPLAYER_SCRIPT_SIGNAL") : "";
    if (const char* requested = SDL_getenv("AOE_MULTIPLAYER");
        requested != nullptr &&
        (std::string_view{requested} == "host" ||
         std::string_view{requested} == "join")) {
        const bool hosting = std::string_view{requested} == "host";
        const char* requested_digest =
            SDL_getenv("AOE_MULTIPLAYER_SCENARIO_DIGEST");
        std::string digest =
            requested_digest != nullptr && requested_digest[0] != '\0'
            ? requested_digest
            : deterministic_state_hash(simulation);
        multiplayer_presentation.emplace(
            hosting ? "HOST" : "JOIN",
            std::move(digest),
            multiplayer_local_player(hosting)
        );
        active_view_player =
            multiplayer_presentation->local_slot;
        multiplayer_presentation->hosting = hosting;
        multiplayer_presentation->config.scenario_digest =
            multiplayer_presentation->scenario_digest;
        multiplayer_presentation->config.blue.civilization =
            simulation.civilization(Player::blue);
        multiplayer_presentation->config.red.civilization =
            simulation.civilization(Player::red);
        // The SDL two-player match presents blue and red as opponents unless
        // a future lobby team picker explicitly negotiates otherwise.
        multiplayer_presentation->config.blue.team = 1;
        multiplayer_presentation->config.red.team = 2;
        if (SDL_getenv("AOE_MULTIPLAYER_ALLIED") != nullptr) {
            multiplayer_presentation->config.red.team = 1;
        }
        if (const char* delay =
                SDL_getenv("AOE_MULTIPLAYER_INPUT_DELAY")) {
            multiplayer_presentation->config.input_delay_ticks =
                std::clamp(SDL_atoi(delay), 0, 20);
        }
        if (const char* checkpoint =
                SDL_getenv("AOE_MULTIPLAYER_SCRIPT_CHECKPOINT")) {
            multiplayer_script_checkpoint_tick =
                static_cast<std::uint64_t>(
                    std::max(SDL_atoi(checkpoint), 0)
                );
        }
        const std::string_view capture_state =
            SDL_getenv("AOE_MULTIPLAYER_CAPTURE_STATE") != nullptr
            ? SDL_getenv("AOE_MULTIPLAYER_CAPTURE_STATE")
            : "";
        constexpr std::uint16_t default_multiplayer_port = 48192;
        std::uint16_t multiplayer_port = default_multiplayer_port;
        if (const char* requested_port =
                SDL_getenv("AOE_MULTIPLAYER_PORT")) {
            const int parsed = SDL_atoi(requested_port);
            if (parsed <= 0 || parsed > 65535) {
                throw std::runtime_error(
                    "AOE_MULTIPLAYER_PORT must be between 1 and 65535"
                );
            }
            multiplayer_port = static_cast<std::uint16_t>(parsed);
        }
        multiplayer_presentation->port = multiplayer_port;
        if (capture_state.empty()) {
            multiplayer_presentation->live_transport = true;
            multiplayer_runtime.emplace(
                hosting
                    ? LocalhostMultiplayerRuntime::host(
                          multiplayer_port,
                          multiplayer_presentation->config
                      )
                    : LocalhostMultiplayerRuntime::join(
                          multiplayer_port,
                          multiplayer_presentation->config
                      )
            );
            if (const char* auto_ready =
                    SDL_getenv("AOE_MULTIPLAYER_AUTO_READY");
                auto_ready != nullptr && auto_ready[0] != '0') {
                multiplayer_runtime->set_ready();
                multiplayer_presentation->local_ready = true;
            }
            if (hosting) {
                if (const char* auto_start =
                        SDL_getenv("AOE_MULTIPLAYER_AUTO_START");
                    auto_start != nullptr &&
                    auto_start[0] != '0') {
                    multiplayer_runtime->request_start();
                }
            }
            if (const char* requested_move =
                    SDL_getenv("AOE_MULTIPLAYER_SCRIPT_MOVE")) {
                MoveUnitCommand command{};
                unsigned int unit{};
                if (SDL_sscanf(
                        requested_move,
                        "%u,%d,%d",
                        &unit,
                        &command.destination.x,
                        &command.destination.y
                    ) == 3) {
                    command.unit = unit;
                    multiplayer_runtime->queue_command(command);
                }
            }
            if (const char* requested_tick =
                    SDL_getenv("AOE_MULTIPLAYER_EXIT_TICK")) {
                multiplayer_exit_tick = static_cast<std::uint64_t>(
                    std::max(SDL_atoi(requested_tick), 0)
                );
            }
            if (const char* requested_path =
                    SDL_getenv("AOE_MULTIPLAYER_STATE_PATH")) {
                multiplayer_state_path = requested_path;
            }
        }
        auto receive = [&](LockstepFrameKind kind, Player player) {
            LockstepFrame frame;
            frame.kind = kind;
            frame.player = player;
            frame.scenario_digest =
                multiplayer_presentation->scenario_digest;
            return multiplayer_presentation->session.receive(
                frame, simulation
            );
        };
        if (!capture_state.empty()) {
            (void)receive(LockstepFrameKind::hello, Player::blue);
            (void)receive(LockstepFrameKind::hello, Player::red);
            (void)receive(LockstepFrameKind::ready, Player::blue);
            (void)receive(LockstepFrameKind::ready, Player::red);
        }
        if (capture_state == "running" ||
            capture_state == "lag" ||
            capture_state == "desync" ||
            capture_state == "timeout" ||
            capture_state == "disconnect") {
            (void)receive(
                LockstepFrameKind::start,
                Player::blue
            );
        }
        if (capture_state == "lag") {
            multiplayer_presentation->waiting_for_turn = true;
        } else if (capture_state == "desync") {
            for (const Player player : {Player::blue, Player::red}) {
                LockstepFrame frame;
                frame.kind = LockstepFrameKind::turn;
                frame.player = player;
                frame.scenario_digest =
                    multiplayer_presentation->scenario_digest;
                frame.state_hash = "capture-mismatch";
                (void)multiplayer_presentation->session.receive(
                    frame, simulation
                );
            }
            (void)multiplayer_presentation->session.advance(simulation);
        } else if (capture_state == "timeout") {
            for (int step = 0; step < 3; ++step) {
                multiplayer_presentation->session.elapse();
            }
        } else if (capture_state == "disconnect") {
            multiplayer_presentation->session.disconnect(
                multiplayer_presentation->local_slot == Player::blue
                    ? Player::red : Player::blue
            );
        }
    }
    FrontendAudioEvents audio_events;
    audio_events.prime(simulation);
    if (audio != nullptr) {
        audio->set_listener_civilization(
            simulation.civilization(active_view_player)
        );
        if (const char* proof_context =
                SDL_getenv("AOE_AUDIO_PROOF_CONTEXT");
            proof_context != nullptr &&
            std::string_view{proof_context} == "gameplay") {
            audio->set_music_context(AudioMusicContext::gameplay, true);
        } else {
            audio->set_music_context(
                AudioMusicContext::civilization,
                true
            );
        }
        if (const char* requested =
                SDL_getenv("AOE_AUDIO_PROOF_SOUND")) {
            audio->play_effect(SDL_atoi(requested));
        }
        if (const char* requested =
                SDL_getenv("AOE_AUDIO_PROOF_TAUNT")) {
            audio->play_taunt(
                static_cast<unsigned>(SDL_atoi(requested))
            );
        }
        if (const char* requested =
                SDL_getenv("AOE_AUDIO_PROOF_NARRATION")) {
            audio->play_narration(requested);
        }
    }
    Replay replay;
    bool replaying = false;
    const std::filesystem::path user_data = user_data_directory();
    active_browser_root = user_data;
    active_browser_entries = browse_user_data_files(active_browser_root);
    active_browser_selection = 0;
    if (const char* browser = SDL_getenv("AOE_SAVE_BROWSER");
        browser != nullptr && browser[0] != '0') {
        active_save_browser_visible = true;
        active_frontend_screen = FrontendScreen::hidden;
    }
    if (const char* panel = SDL_getenv("AOE_COMMAND_PANEL")) {
        const std::string_view panel_name{panel};
        if (panel_name == "unit" || panel_name == "villager" ||
            panel_name == "scout") {
            const auto unit = std::ranges::find_if(
                simulation.units(),
                [panel_name](const Unit& candidate) {
                    const bool requested_kind =
                        panel_name == "unit" ||
                        (panel_name == "villager" &&
                         candidate.kind == UnitKind::villager) ||
                        (panel_name == "scout" &&
                         candidate.kind == UnitKind::scout_cavalry);
                    return candidate.owner == active_view_player &&
                        requested_kind;
                }
            );
            if (unit != simulation.units().end()) {
                simulation.select_units({unit->id}, active_view_player);
            }
        } else if (std::string_view{panel} == "building") {
            const auto building = std::ranges::find_if(
                simulation.buildings(),
                [](const Building& candidate) {
                    return candidate.owner == active_view_player;
                }
            );
            if (building != simulation.buildings().end()) {
                simulation.select_building_at(
                    building->position, active_view_player
                );
            }
        }
    }
    if (const char* observer = SDL_getenv("AOE_OBSERVER_PANEL");
        observer != nullptr && observer[0] != '0') {
        simulation.resign(active_view_player);
    }
    active_settings_path = user_data / "reconstruction-settings.txt";
    const SettingsLoadResult loaded_settings =
        load_settings(active_settings_path);
    if (loaded_settings.status == SettingsLoadStatus::current ||
        loaded_settings.status == SettingsLoadStatus::migrated) {
        active_settings = loaded_settings.settings;
    } else {
        active_settings = {};
    }
    // Modern choice: no original equivalent. The fog display toggle already
    // exists in the options panel; this exposes it to headless captures,
    // which otherwise only ever see the explored disc around a start.
    if (const char* requested = SDL_getenv("AOE_FOG");
        requested != nullptr && requested[0] != '\0') {
        active_settings.fog = requested[0] != '0';
    }
    draft_settings = active_settings;
    if (audio) {
        audio->apply_mix(AudioMix::from_settings(active_settings));
    }
    if (loaded_settings.status == SettingsLoadStatus::invalid) {
        active_options_status =
            "INVALID FILE IGNORED: " + loaded_settings.message;
    } else if (loaded_settings.status == SettingsLoadStatus::migrated) {
        active_options_status = "VERSION 1 LOADED; SAVE MIGRATES TO VERSION 2";
    }
    if (const char* options = SDL_getenv("AOE_OPTIONS_PANEL");
        options != nullptr && options[0] != '0') {
        active_options_visible = true;
        active_frontend_screen = FrontendScreen::hidden;
    }
    if (const char* statistics = SDL_getenv("AOE_STATISTICS_PANEL");
        statistics != nullptr && statistics[0] != '0') {
        active_statistics_visible = true;
        active_statistics_postgame = false;
        active_frontend_screen = FrontendScreen::hidden;
        if (std::string_view{statistics} == "timeline") {
            active_statistics_tab = StatisticsTab::timeline;
        }
    }
    const std::filesystem::path save_path =
        user_data / "archaeology-save.txt";
    const std::filesystem::path replay_path =
        user_data / "archaeology-replay.txt";
    const std::filesystem::path multiplayer_checkpoint_save =
        SDL_getenv("AOE_MULTIPLAYER_CHECKPOINT_PATH") != nullptr
        ? std::filesystem::path{
              SDL_getenv("AOE_MULTIPLAYER_CHECKPOINT_PATH")}
        : user_data / "multiplayer-checkpoint.save";
    std::filesystem::path multiplayer_checkpoint_envelope =
        multiplayer_checkpoint_save;
    multiplayer_checkpoint_envelope += ".envelope";
    if (multiplayer_presentation) {
        multiplayer_presentation->checkpoint_path =
            multiplayer_checkpoint_save.string();
    }
    bool running = true;
    bool pending_map_signal = false;
    std::uint64_t local_signal_sequence = 1;
    std::vector<Uint64> local_signal_times;
    bool outcome_statistics_seen =
        simulation.outcome() != MatchOutcome::ongoing;
    bool paused = false;
    bool fullscreen = active_settings.fullscreen;
    if (fullscreen && !SDL_SetWindowFullscreen(window, true)) {
        fullscreen = false;
        active_settings.fullscreen = false;
        draft_settings.fullscreen = false;
        active_options_status = "FULLSCREEN REQUEST FAILED";
    }
    auto apply_options = [&]() {
        active_settings = draft_settings;
        if (audio) {
            audio->apply_mix(AudioMix::from_settings(active_settings));
        }
        if (fullscreen != active_settings.fullscreen) {
            if (SDL_SetWindowFullscreen(
                    window, active_settings.fullscreen)) {
                fullscreen = active_settings.fullscreen;
            } else {
                active_settings.fullscreen = fullscreen;
                draft_settings.fullscreen = fullscreen;
                active_options_status = "FULLSCREEN APPLY FAILED";
                return false;
            }
        }
        active_options_status = "APPLIED";
        return true;
    };
    CameraView camera;
    // The projection and the camera clamp read their extent from this map,
    // and the first camera placement happens before the first frame is
    // rendered, so adopt it now rather than in render_world.
    active_render_map = &simulation.map();
    center_camera_on(
        camera,
        initial_camera_tile(
            simulation.buildings(),
            simulation.units(),
            active_view_player,
            active_map_tiles_x(),
            active_map_tiles_y()
        )
    );
    // Modern choice: no original equivalent. Headless captures need to aim
    // at a named tile, because on a full-size map the start view covers a
    // small fraction of the world.
    if (const char* requested = SDL_getenv("AOE_CAMERA_TILE");
        requested != nullptr && requested[0] != '\0') {
        int tile_x{};
        int tile_y{};
        if (std::sscanf(requested, "%d,%d", &tile_x, &tile_y) == 2) {
            const TilePosition tile{
                std::clamp(tile_x, 0, simulation.map().width() - 1),
                std::clamp(tile_y, 0, simulation.map().height() - 1),
            };
            center_camera_on(camera, tile);
        } else {
            SDL_Log("Ignoring malformed AOE_CAMERA_TILE: %s", requested);
        }
    }
    const char* benchmark_path =
        SDL_getenv("AOE_GAMEPLAY_BENCHMARK_PATH");
    const bool gameplay_benchmark =
        benchmark_path != nullptr && benchmark_path[0] != '\0';
    constexpr std::size_t benchmark_warmup_frames = 10;
    constexpr std::size_t benchmark_sample_frames = 120;
    std::size_t benchmark_frame{};
    std::size_t benchmark_max_rendered_tiles{};
    std::size_t benchmark_max_moving_units{};
    std::vector<double> benchmark_frame_times_ms;
    std::vector<double> benchmark_render_times_ms;
    benchmark_frame_times_ms.reserve(benchmark_sample_frames);
    benchmark_render_times_ms.reserve(benchmark_sample_frames);
    SDL_FPoint mouse_position{
        static_cast<float>(view_pixel_width) * 0.5F,
        static_cast<float>(view_pixel_height) * 0.5F,
    };
    std::optional<BuildingKind> pending_building;
    bool pending_attack_move = false;
    bool pending_attack_ground = false;
    bool pending_patrol = false;
    bool pending_guard = false;
    bool pending_garrison = false;
    std::optional<EntityId> pending_rally_building;
    bool pending_conversion = false;
    bool pending_trade_route = false;
    bool pending_repair = false;
    bool pending_heal = false;
    bool pending_relic_action = false;
    bool pending_embark = false;
    bool pending_disembark = false;
    const auto clear_command_target_modes = [
        &pending_attack_move,
        &pending_attack_ground,
        &pending_patrol,
        &pending_guard,
        &pending_garrison,
        &pending_rally_building,
        &pending_conversion,
        &pending_trade_route,
        &pending_repair,
        &pending_heal,
        &pending_relic_action,
        &pending_embark,
        &pending_disembark
    ] {
        pending_attack_move = false;
        pending_attack_ground = false;
        pending_patrol = false;
        pending_guard = false;
        pending_garrison = false;
        pending_rally_building.reset();
        pending_conversion = false;
        pending_trade_route = false;
        pending_repair = false;
        pending_heal = false;
        pending_relic_action = false;
        pending_embark = false;
        pending_disembark = false;
    };
    if (const char* preview = SDL_getenv("AOE_BUILD_PREVIEW")) {
        const auto builder = std::ranges::find_if(
            simulation.units(),
            [](const Unit& unit) {
                return unit.owner == active_view_player &&
                    unit.kind == UnitKind::villager;
            }
        );
        if (builder != simulation.units().end()) {
            simulation.select_units({builder->id}, active_view_player);
            pending_building = BuildingKind::house;
            active_build_preview_tile = builder->position;
            if (std::string_view{preview} != "invalid") {
                for (int dy = -2; dy <= 2; ++dy) {
                    for (int dx = -2; dx <= 2; ++dx) {
                        const TilePosition candidate{
                            builder->position.x + dx,
                            builder->position.y + dy
                        };
                        if (evaluate_building_placement(
                                simulation,
                                builder->id,
                                BuildingKind::house,
                                candidate
                            ).valid) {
                            active_build_preview_tile = candidate;
                        }
                    }
                }
            }
        }
    }
    std::array<ControlGroup, 10> control_groups;
    std::optional<std::size_t> last_control_group_recall;
    Uint64 last_control_group_recall_ms{};
    std::string control_group_status;
    if (const char* proof = SDL_getenv("AOE_SELECTION_PROOF");
        proof != nullptr && proof[0] != '0') {
        control_groups[1].units = simulation.selected_units();
        control_groups[1].building = simulation.selected_building();
        control_group_status =
            "GROUP 1 ASSIGNED; DOUBLE-TAP 1 CENTERS";
    }
    std::optional<EntityId> last_idle_villager;
    std::optional<EntityId> last_idle_military;
    std::optional<SelectionDrag> selection_drag;
    std::optional<TilePosition> formation_preview_center;
    ScenarioPresentation scenario_presentation;
    if (const char* requested =
            SDL_getenv("AOE_OBJECTIVES_VISIBLE");
        requested != nullptr && requested[0] != '0') {
        scenario_presentation.objectives_visible = true;
    }
    const auto execute = [
        &multiplayer_presentation,
        &multiplayer_runtime
    ](
                             Simulation& target,
                             const GameCommand& command
                         ) {
        if (multiplayer_runtime) {
            if (multiplayer_runtime->paused()) return false;
            multiplayer_runtime->queue_command(command);
            return true;
        }
        if (multiplayer_presentation) return false;
        return aoe::execute(target, command);
    };
    auto last_frame_time = std::chrono::steady_clock::now();
    FixedStepAccumulator simulation_time;
    FrameDuration presentation_time{};
    const auto set_build_mode = [
        &pending_building,
        &pending_attack_move,
        &pending_attack_ground,
        &pending_patrol,
        &pending_guard,
        &pending_garrison,
        &pending_rally_building,
        &pending_conversion,
        &pending_trade_route,
        &pending_repair,
        &pending_heal,
        &pending_relic_action,
        &pending_embark,
        &pending_disembark,
        &simulation
    ](BuildingKind kind) {
        if (!building_available_to_player(
                simulation, active_view_player, kind
            )) {
            return;
        }
        pending_building = kind;
        pending_attack_move = false;
        pending_attack_ground = false;
        pending_patrol = false;
        pending_guard = false;
        pending_garrison = false;
        pending_rally_building.reset();
        pending_conversion = false;
        pending_trade_route = false;
        pending_repair = false;
        pending_heal = false;
        pending_relic_action = false;
        pending_embark = false;
        pending_disembark = false;
    };
    const auto apply_editor_cursor = [&]() {
        if (!scenario_editor ||
            !scenario_editor->scenario().map.contains(
                active_editor_cursor
            )) {
            active_editor_status = "EDITOR CURSOR OUTSIDE MAP";
            return false;
        }
        bool changed{};
        switch (active_editor_tool) {
            case EditorTool::grass:
                changed = scenario_editor->paint_terrain(
                    active_editor_cursor, Terrain::grass
                );
                break;
            case EditorTool::water:
                changed = scenario_editor->paint_terrain(
                    active_editor_cursor, Terrain::water
                );
                break;
            case EditorTool::forest:
                changed = scenario_editor->paint_terrain(
                    active_editor_cursor, Terrain::forest
                );
                break;
            case EditorTool::elevation:
                changed = scenario_editor->paint_elevation(
                    active_editor_cursor,
                    std::min(
                        7,
                        scenario_editor->scenario().map.elevation_at(
                            active_editor_cursor
                        ) + 1
                    )
                );
                break;
            case EditorTool::villager:
                changed = scenario_editor->place_unit({
                    UnitKind::villager, active_editor_player,
                    active_editor_cursor,
                    std::nullopt, std::nullopt, std::nullopt,
                    std::nullopt, false, {},
                    UnitStance::aggressive, std::nullopt,
                });
                break;
            case EditorTool::house:
                changed = scenario_editor->place_building({
                    BuildingKind::house, active_editor_player,
                    active_editor_cursor,
                    std::nullopt, std::nullopt, std::nullopt,
                });
                break;
            case EditorTool::erase:
                changed = scenario_editor->remove_at(active_editor_cursor);
                break;
        }
        if (changed) {
            simulation = create_simulation(scenario_editor->scenario());
            active_editor_status = "KEYBOARD EDIT APPLIED; UNSAVED";
        } else {
            active_editor_status = "EDIT REJECTED BY BOUNDS/VALIDATION";
        }
        return changed;
    };

    std::optional<EntityId> sheep_click_proof_sheep;
    std::optional<EntityId> sheep_click_proof_villager;
    bool sheep_click_proof_gather{};
    bool sheep_click_proof_logged{};
    if (const char* proof = SDL_getenv("AOE_SHEEP_CLICK_PROOF");
        proof != nullptr && proof[0] != '\0') {
        const auto sheep = std::ranges::find_if(
            simulation.units(),
            [](const Unit& unit) {
                return unit.kind == UnitKind::sheep &&
                    unit.garrisoned_in == 0;
            }
        );
        const auto villager = std::ranges::find_if(
            simulation.units(),
            [](const Unit& unit) {
                return unit.kind == UnitKind::villager &&
                    unit.owner == active_view_player &&
                    unit.garrisoned_in == 0;
            }
        );
        if (sheep != simulation.units().end() &&
            villager != simulation.units().end()) {
            sheep_click_proof_sheep = sheep->id;
            sheep_click_proof_villager = villager->id;
            sheep_click_proof_gather =
                std::string_view{proof} == "gather";
            if (sheep_click_proof_gather) {
                simulation.select_units(
                    {villager->id}, active_view_player
                );
            }
            const int elevation =
                simulation.map().elevation_at(sheep->position);
            const float click_x = (
                static_cast<float>(
                    map_origin_x() +
                    (sheep->position.x - sheep->position.y) *
                        half_tile_width
                ) - camera.x
            ) * camera.zoom;
            const float click_y = (
                static_cast<float>(
                    map_origin_y +
                    (sheep->position.x + sheep->position.y) *
                        half_tile_height -
                    elevation * elevation_pixel_step +
                    half_tile_height
                ) - camera.y
            ) * camera.zoom;
            SDL_Event down{};
            down.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
            down.button.button = sheep_click_proof_gather
                ? SDL_BUTTON_RIGHT : SDL_BUTTON_LEFT;
            down.button.clicks = 1;
            down.button.x = click_x;
            down.button.y = click_y;
            SDL_PushEvent(&down);
            if (!sheep_click_proof_gather) {
                SDL_Event up = down;
                up.type = SDL_EVENT_MOUSE_BUTTON_UP;
                SDL_PushEvent(&up);
            }
        }
    }

    while (running) {
        const auto frame_started = std::chrono::steady_clock::now();
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (!SDL_ConvertEventToRenderCoordinates(renderer, &event)) {
                SDL_Log(
                    "Could not convert input coordinates: %s",
                    SDL_GetError()
                );
                continue;
            }
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            } else if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
                if (audio) audio->set_focused(false);
            } else if (event.type == SDL_EVENT_WINDOW_FOCUS_GAINED) {
                if (audio) audio->set_focused(true);
            } else if (
                event.type == SDL_EVENT_MOUSE_WHEEL &&
                active_technology_tree_visible
            ) {
                active_tree_zoom = std::clamp(
                    active_tree_zoom + event.wheel.y * 0.08F,
                    0.35F, 1.35F
                );
            } else if (
                event.type == SDL_EVENT_MOUSE_MOTION &&
                active_technology_tree_visible
            ) {
                if (active_tree_dragging) {
                    active_tree_pan_x = std::max(
                        0.0F,
                        active_tree_pan_x -
                            (event.motion.x - active_tree_drag_origin.x)
                    );
                    active_tree_pan_y = std::max(
                        0.0F,
                        active_tree_pan_y -
                            (event.motion.y - active_tree_drag_origin.y)
                    );
                    active_tree_drag_origin = {
                        event.motion.x, event.motion.y
                    };
                }
                active_tree_hover.clear();
                for (const TechnologyTreeNode& node :
                     active_technology_tree.nodes) {
                    const SDL_FRect box{
                        24.0F + node.x * active_tree_zoom -
                            active_tree_pan_x,
                        72.0F + node.y * active_tree_zoom -
                            active_tree_pan_y,
                        138.0F * active_tree_zoom,
                        42.0F * active_tree_zoom,
                    };
                    const SDL_FPoint pointer{
                        event.motion.x, event.motion.y
                    };
                    if (SDL_PointInRectFloat(&pointer, &box)) {
                        std::ostringstream detail;
                        detail << node.label << "  COST W" << node.wood
                               << " F" << node.food << " G" << node.gold
                               << " S" << node.stone << "  "
                               << node.requirement;
                        active_tree_hover = detail.str();
                        break;
                    }
                }
            } else if (
                event.type == SDL_EVENT_MOUSE_BUTTON_UP &&
                active_technology_tree_visible
            ) {
                active_tree_dragging = false;
            } else if (
                event.type == SDL_EVENT_MOUSE_MOTION &&
                event.motion.y >= static_cast<float>(view_pixel_height)
            ) {
                active_command_hover = hud_layout::command_button_at(
                    view_pixel_height,
                    static_cast<int>(event.motion.x),
                    static_cast<int>(event.motion.y)
                );
            } else if (
                event.type == SDL_EVENT_TEXT_INPUT &&
                active_save_browser_visible &&
                active_save_slot_input
            ) {
                const std::string_view added{event.text.text};
                for (const unsigned char byte : added) {
                    if (active_save_slot.size() < 32 &&
                        (std::isalnum(byte) || byte == '-' || byte == '_')) {
                        active_save_slot.push_back(
                            static_cast<char>(byte)
                        );
                    }
                }
            } else if (
                event.type == SDL_EVENT_TEXT_INPUT &&
                multiplayer_presentation &&
                multiplayer_presentation->chat_input_active
            ) {
                const std::string_view added{event.text.text};
                if (multiplayer_presentation->chat_input.size() +
                        added.size() <= 4096) {
                    multiplayer_presentation->chat_input.append(added);
                    multiplayer_presentation->chat_feedback.clear();
                } else {
                    multiplayer_presentation->chat_feedback =
                        "MESSAGE LIMIT: 4096 UTF-8 BYTES";
                }
            } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                mouse_position = {event.button.x, event.button.y};
                if (event.button.button == SDL_BUTTON_LEFT &&
                    active_command_hover >= 0 &&
                    (simulation.selected_unit() ||
                     simulation.selected_building())) {
                    const SelectionPanelModel panel =
                        build_selection_panel(
                            simulation,
                            active_view_player,
                            active_command_page,
                            active_command_subpage);
                    const auto hovered = std::ranges::find_if(
                        panel.commands,
                        [](const CommandButtonModel& button) {
                            return active_command_hover >= 0 &&
                                button.grid_slot == static_cast<std::size_t>(
                                    active_command_hover);
                        }
                    );
                    if (hovered != panel.commands.end()) {
                        const CommandButtonModel& button = *hovered;
                        if (simulation.observer_perspective(
                                active_view_player)) {
                            control_group_status =
                                "OBSERVER: COMMANDS DISABLED";
                            continue;
                        }
                        if (!button.enabled) {
                            control_group_status =
                                button.label + " UNAVAILABLE";
                            continue;
                        }
                        clear_command_target_modes();
                        if (button.command ==
                            PanelCommand::open_economic_buildings) {
                            active_command_page =
                                PanelPage::economic_buildings;
                            active_command_subpage = 0;
                        } else if (
                            button.command ==
                            PanelCommand::open_military_buildings) {
                            active_command_page =
                                PanelPage::military_buildings;
                            active_command_subpage = 0;
                        } else if (
                            button.command ==
                            PanelCommand::open_defensive_buildings) {
                            active_command_page =
                                PanelPage::defensive_buildings;
                            active_command_subpage = 0;
                        } else if (
                            button.command ==
                                PanelCommand::open_production) {
                            active_command_page = PanelPage::production;
                            active_command_subpage = 0;
                        } else if (
                            button.command ==
                                PanelCommand::open_research) {
                            active_command_page = PanelPage::research;
                            active_command_subpage = 0;
                        } else if (
                            button.command ==
                                PanelCommand::previous_page) {
                            if (active_command_subpage > 0) {
                                --active_command_subpage;
                            }
                        } else if (
                            button.command == PanelCommand::next_page) {
                            if (active_command_subpage + 1 <
                                panel.page_count) {
                                ++active_command_subpage;
                            }
                        } else if (button.command == PanelCommand::back) {
                            active_command_page = PanelPage::root;
                            active_command_subpage = 0;
                        } else if (
                            button.command ==
                                PanelCommand::construct_building &&
                            button.building) {
                            set_build_mode(*button.building);
                            active_command_page = PanelPage::root;
                            active_command_subpage = 0;
                        } else if (button.command == PanelCommand::stop) {
                            for (EntityId id :
                                 simulation.selected_units()) {
                                GameCommand command = StopUnitCommand{id};
                                if (execute(simulation, command)) {
                                    replay.record(
                                        simulation.tick_number(),
                                        std::move(command)
                                    );
                                }
                            }
                        } else if (
                            button.command == PanelCommand::delete_entity) {
                            const std::vector<EntityId> selected =
                                simulation.selected_units();
                            for (EntityId id : selected) {
                                GameCommand command =
                                    DeleteEntityCommand{id, false};
                                if (execute(simulation, command)) {
                                    replay.record(
                                        simulation.tick_number(),
                                        std::move(command)
                                    );
                                }
                            }
                            if (selected.empty() &&
                                simulation.selected_building()) {
                                GameCommand command = DeleteEntityCommand{
                                    *simulation.selected_building(),
                                    true,
                                };
                                if (execute(simulation, command)) {
                                    replay.record(
                                        simulation.tick_number(),
                                        std::move(command)
                                    );
                                }
                            }
                        } else if (
                            button.command == PanelCommand::advance_age &&
                            simulation.selected_building()) {
                            GameCommand command = AdvanceAgeCommand{
                                *simulation.selected_building()};
                            if (execute(simulation, command)) {
                                replay.record(
                                    simulation.tick_number(),
                                    std::move(command)
                                );
                            }
                        } else if (
                            button.command ==
                            PanelCommand::attack_move) {
                            pending_attack_move = true;
                        } else if (
                            button.command ==
                            PanelCommand::attack_ground) {
                            pending_attack_ground = true;
                        } else if (
                            button.command == PanelCommand::patrol) {
                            pending_patrol = true;
                        } else if (
                            button.command == PanelCommand::guard ||
                            button.command == PanelCommand::follow) {
                            pending_guard = true;
                            control_group_status =
                                button.command == PanelCommand::follow
                                    ? "RIGHT CLICK FRIENDLY UNIT TO FOLLOW"
                                    : "RIGHT CLICK FRIENDLY UNIT OR BUILDING";
                        } else if (
                            button.command == PanelCommand::garrison) {
                            pending_garrison = true;
                            pending_rally_building.reset();
                            control_group_status =
                                "RIGHT CLICK FRIENDLY GARRISON BUILDING";
                        } else if (
                            button.command == PanelCommand::convert) {
                            pending_conversion = true;
                            control_group_status =
                                "RIGHT CLICK ENEMY UNIT TO CONVERT";
                        } else if (
                            button.command == PanelCommand::repair) {
                            pending_repair = true;
                            control_group_status =
                                "RIGHT CLICK DAMAGED FRIENDLY BUILDING";
                        } else if (
                            button.command == PanelCommand::heal) {
                            pending_heal = true;
                            control_group_status =
                                "RIGHT CLICK WOUNDED FRIENDLY UNIT";
                        } else if (
                            button.command ==
                                PanelCommand::collect_relic ||
                            button.command ==
                                PanelCommand::deposit_relic) {
                            pending_relic_action = true;
                            control_group_status =
                                button.command ==
                                        PanelCommand::collect_relic
                                    ? "RIGHT CLICK RELIC"
                                    : "RIGHT CLICK FRIENDLY MONASTERY";
                        } else if (
                            button.command == PanelCommand::embark) {
                            pending_embark = true;
                            control_group_status =
                                "RIGHT CLICK FRIENDLY TRANSPORT SHIP";
                        } else if (
                            button.command == PanelCommand::disembark) {
                            pending_disembark = true;
                            control_group_status =
                                "RIGHT CLICK VALID SHORE TILE";
                        } else if (
                            button.command == PanelCommand::trade_route) {
                            pending_trade_route = true;
                            control_group_status =
                                "RIGHT CLICK ALLIED TRADE ENDPOINT";
                        } else if (
                            button.command ==
                                PanelCommand::pack_trebuchet ||
                            button.command ==
                                PanelCommand::unpack_trebuchet) {
                            const bool pack =
                                button.command ==
                                    PanelCommand::pack_trebuchet;
                            for (EntityId id :
                                 simulation.selected_units()) {
                                GameCommand command =
                                    PackTrebuchetCommand{id, pack};
                                if (execute(simulation, command)) {
                                    replay.record(
                                        simulation.tick_number(),
                                        std::move(command)
                                    );
                                }
                            }
                        } else if (
                            button.command >=
                                PanelCommand::stance_aggressive &&
                            button.command <=
                                PanelCommand::stance_no_attack &&
                            simulation.selected_unit()) {
                            const UnitStance stance =
                                button.command ==
                                        PanelCommand::stance_aggressive
                                    ? UnitStance::aggressive
                                : button.command ==
                                        PanelCommand::stance_defensive
                                    ? UnitStance::defensive
                                : button.command ==
                                        PanelCommand::stance_stand_ground
                                    ? UnitStance::stand_ground
                                    : UnitStance::passive;
                            for (EntityId id :
                                 simulation.selected_units()) {
                                GameCommand command =
                                    SetStanceCommand{id, stance};
                                if (execute(simulation, command)) {
                                    replay.record(
                                        simulation.tick_number(),
                                        std::move(command)
                                    );
                                }
                            }
                        } else if (
                            button.command ==
                                PanelCommand::formation_line ||
                            button.command ==
                                PanelCommand::formation_box ||
                            button.command ==
                                PanelCommand::formation_staggered ||
                            button.command ==
                                PanelCommand::formation_flank) {
                            if (multiplayer_runtime ||
                                multiplayer_presentation) {
                                control_group_status =
                                    "FORMATION BUTTON DISABLED IN MULTIPLAYER";
                                continue;
                            }
                            const FormationKind kind =
                                button.command ==
                                        PanelCommand::formation_line
                                    ? FormationKind::line
                                : button.command ==
                                        PanelCommand::formation_box
                                    ? FormationKind::box
                                : button.command ==
                                        PanelCommand::formation_staggered
                                    ? FormationKind::staggered
                                    : FormationKind::flank;
                            simulation.set_formation_kind(
                                active_view_player, kind);
                        } else if (
                            button.command ==
                            PanelCommand::cancel_production &&
                            simulation.selected_building()) {
                            GameCommand command =
                                CancelProductionCommand{
                                    *simulation.selected_building()};
                            if (execute(simulation, command)) {
                                replay.record(
                                    simulation.tick_number(),
                                    std::move(command)
                                );
                                control_group_status =
                                    "LAST ORDER CANCELLED; REFUND APPLIED";
                            }
                        } else if (
                            button.command ==
                                PanelCommand::ungarrison &&
                            simulation.selected_building()) {
                            GameCommand command = UngarrisonCommand{
                                *simulation.selected_building()};
                            if (execute(simulation, command)) {
                                replay.record(
                                    simulation.tick_number(),
                                    std::move(command)
                                );
                            }
                        } else if (
                            button.command == PanelCommand::rally) {
                            pending_rally_building =
                                simulation.selected_building();
                            pending_garrison = false;
                            control_group_status =
                                "RIGHT CLICK TO SET RALLY POINT";
                        } else if (
                            button.command == PanelCommand::train_unit &&
                            button.unit &&
                            simulation.selected_building()) {
                            GameCommand command = QueueUnitCommand{
                                *simulation.selected_building(),
                                *button.unit,
                            };
                            if (execute(simulation, command)) {
                                replay.record(
                                    simulation.tick_number(),
                                    std::move(command)
                                );
                            }
                        } else if (
                            button.command == PanelCommand::research &&
                            button.technology &&
                            simulation.selected_building()) {
                            GameCommand command =
                                ResearchTechnologyCommand{
                                    *simulation.selected_building(),
                                    *button.technology,
                                };
                            if (execute(simulation, command)) {
                                replay.record(
                                    simulation.tick_number(),
                                    std::move(command)
                                );
                                active_command_page = PanelPage::root;
                                active_command_subpage = 0;
                            }
                        }
                    }
                    continue;
                }
                if (active_technology_tree_visible) {
                    if (event.button.button == SDL_BUTTON_MIDDLE ||
                        event.button.button == SDL_BUTTON_RIGHT) {
                        active_tree_dragging = true;
                        active_tree_drag_origin = {
                            event.button.x, event.button.y
                        };
                    }
                    continue;
                }
                if (active_frontend_screen != FrontendScreen::hidden) {
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        if (active_frontend_screen ==
                            FrontendScreen::single_player_setup) {
                            if (!random_map_preview) {
                                refresh_random_map_preview();
                            }
                            if (random_map_preview) {
                                random_map_preview->blue_civilization =
                                    active_setup_civilization;
                                MatchRules& rules =
                                    random_map_preview->match_rules;
                                rules.conquest_enabled =
                                    active_setup_victory == 0;
                                rules.wonder_enabled =
                                    active_setup_victory == 1;
                                rules.relic_enabled =
                                    active_setup_victory == 2;
                                demo_scenario = *random_map_preview;
                                simulation =
                                    create_simulation(demo_scenario);
                                computer = ComputerPlayer(
                                    Player::red,
                                    active_setup_difficulty
                                );
                                active_frontend_screen =
                                    FrontendScreen::hidden;
                            }
                        } else {
                            const float y = event.button.y;
                            if (y >= 190.0F && y < 240.0F) {
                                active_frontend_screen =
                                    FrontendScreen::single_player_setup;
                            } else if (y >= 240.0F && y < 290.0F) {
                                active_frontend_screen =
                                    FrontendScreen::hidden;
                            } else if (y >= 290.0F && y < 340.0F) {
                                if (campaign_presentation) {
                                    campaign_presentation->screen =
                                        CampaignPresentation::Screen::briefing;
                                    campaign_presentation->visible = true;
                                    active_frontend_screen =
                                        FrontendScreen::hidden;
                                } else {
                                    active_frontend_status =
                                        "SET AOE_CAMPAIGN TO LAUNCH";
                                }
                            } else if (y >= 340.0F && y < 390.0F) {
                                if (!scenario_editor) {
                                    scenario_editor.emplace(demo_scenario);
                                }
                                active_editor_overlay = true;
                                active_frontend_screen =
                                    FrontendScreen::hidden;
                            }
                        }
                    }
                    continue;
                }
                if (scenario_editor &&
                    event.button.button == SDL_BUTTON_LEFT) {
                    const TilePosition tile =
                        mouse_tile(event.button, camera);
                    active_editor_cursor = tile;
                    if (!scenario_editor->scenario().map.contains(tile)) {
                        active_editor_status = "TILE OUTSIDE MAP";
                        continue;
                    }
                    bool changed{};
                    switch (active_editor_tool) {
                        case EditorTool::grass:
                            changed = scenario_editor->paint_terrain(
                                tile, Terrain::grass);
                            break;
                        case EditorTool::water:
                            changed = scenario_editor->paint_terrain(
                                tile, Terrain::water);
                            break;
                        case EditorTool::forest:
                            changed = scenario_editor->paint_terrain(
                                tile, Terrain::forest);
                            break;
                        case EditorTool::elevation:
                            changed = scenario_editor->paint_elevation(
                                tile,
                                std::min(
                                    7,
                                    scenario_editor->scenario().map
                                        .elevation_at(tile) + 1
                                ));
                            break;
                        case EditorTool::villager:
                            changed = scenario_editor->place_unit({
                                UnitKind::villager,
                                active_editor_player,
                                tile,
                                std::nullopt,
                                std::nullopt,
                                std::nullopt,
                                std::nullopt,
                                false,
                                {},
                                UnitStance::aggressive,
                                std::nullopt,
                            });
                            break;
                        case EditorTool::house:
                            changed = scenario_editor->place_building({
                                BuildingKind::house,
                                active_editor_player,
                                tile,
                                std::nullopt,
                                std::nullopt,
                                std::nullopt,
                            });
                            break;
                        case EditorTool::erase:
                            changed = scenario_editor->remove_at(tile);
                            break;
                    }
                    if (changed) {
                        simulation = create_simulation(
                            scenario_editor->scenario()
                        );
                        active_editor_status =
                            "EDIT APPLIED; UNSAVED";
                    } else {
                        active_editor_status =
                            "EDIT REJECTED BY BOUNDS/VALIDATION";
                    }
                    continue;
                }
                if (campaign_presentation &&
                    campaign_presentation->visible &&
                    campaign_presentation->screen !=
                        CampaignPresentation::Screen::status) {
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        campaign_presentation->screen =
                            CampaignPresentation::Screen::status;
                        campaign_presentation->visible = false;
                    }
                    continue;
                }
                if (event.button.button == SDL_BUTTON_LEFT &&
                    multiplayer_presentation &&
                    !multiplayer_presentation->signal_log.empty() &&
                    event.button.x >= 490.0F &&
                    event.button.x <= 1056.0F &&
                    event.button.y >=
                        static_cast<float>(view_pixel_height) - 220.0F &&
                    event.button.y <
                        static_cast<float>(view_pixel_height)) {
                    center_camera_on(
                        camera,
                        multiplayer_presentation->signal_log.back().tile
                    );
                    control_group_status = "CAMERA: ALLY SIGNAL";
                    continue;
                }
                if (multiplayer_runtime &&
                    multiplayer_presentation->live_status !=
                        LockstepStatus::running) {
                    const SDL_FRect ready_button{
                        314.0F, 490.0F, 248.0F, 52.0F,
                    };
                    const SDL_FRect start_button{
                        718.0F, 490.0F, 248.0F, 52.0F,
                    };
                    const SDL_FPoint click{
                        event.button.x, event.button.y,
                    };
                    if (event.button.button == SDL_BUTTON_LEFT &&
                        SDL_PointInRectFloat(
                            &click, &ready_button
                        ) &&
                        !multiplayer_presentation->local_ready) {
                        multiplayer_runtime->set_ready();
                        multiplayer_presentation->local_ready = true;
                    } else if (
                        event.button.button == SDL_BUTTON_LEFT &&
                        SDL_PointInRectFloat(
                            &click, &start_button
                        ) &&
                        multiplayer_presentation->hosting &&
                        multiplayer_presentation->blue_ready &&
                        multiplayer_presentation->red_ready
                    ) {
                        multiplayer_runtime->request_start();
                    }
                    continue;
                }
                TilePosition minimap_tile;
                if (event.button.button == SDL_BUTTON_LEFT &&
                    minimap_tile_at(
                        event.button.x,
                        event.button.y,
                        simulation,
                        minimap_tile
                    )) {
                    center_camera_on(camera, minimap_tile);
                    selection_drag.reset();
                    continue;
                }
                const TilePosition tile = mouse_tile(event.button, camera);
                if (event.button.button == SDL_BUTTON_LEFT &&
                    pending_map_signal) {
                    if (!simulation.map().contains(tile)) {
                        control_group_status =
                            "SIGNAL REJECTED: OUTSIDE MAP";
                    } else if (!simulation.is_explored_to_controller(
                                   active_view_player, tile)) {
                        control_group_status =
                            "SIGNAL REJECTED: UNEXPLORED TILE";
                    } else {
                        bool accepted = true;
                        if (multiplayer_runtime) {
                            accepted = multiplayer_runtime->send_signal(
                                tile, ChatAudience::allies
                            );
                        } else {
                            const Uint64 now = SDL_GetTicks();
                            std::erase_if(
                                local_signal_times,
                                [now](Uint64 sent) {
                                    return now - sent >= 2000;
                                }
                            );
                            accepted = local_signal_times.size() < 4;
                            if (accepted) {
                                local_signal_times.push_back(now);
                                active_map_signals.push_back({
                                    {local_signal_sequence++,
                                     active_view_player,
                                     ChatAudience::allies,
                                     tile},
                                    now,
                                });
                            }
                        }
                        control_group_status = accepted
                            ? "ALLY SIGNAL SENT"
                            : "SIGNAL RATE LIMITED";
                    }
                    pending_map_signal = false;
                    selection_drag.reset();
                    continue;
                }
                const bool wall_or_gate =
                    pending_building &&
                    (*pending_building == BuildingKind::palisade_wall ||
                     *pending_building == BuildingKind::stone_wall ||
                     *pending_building == BuildingKind::palisade_gate_x ||
                     *pending_building == BuildingKind::palisade_gate_y ||
                     *pending_building == BuildingKind::stone_gate_x ||
                     *pending_building == BuildingKind::stone_gate_y);
                if (event.button.button == SDL_BUTTON_RIGHT &&
                    wall_or_gate) {
                    active_wall_drag_start = tile;
                    active_build_preview_tile = tile;
                    continue;
                }
                if (event.button.button == SDL_BUTTON_LEFT) {
                    selection_drag = SelectionDrag{
                        {event.button.x, event.button.y},
                        {event.button.x, event.button.y},
                    };
                } else if (event.button.button == SDL_BUTTON_RIGHT) {
                    if (replaying) {
                        continue;
                    }
                    const std::vector<EntityId> command_units =
                        simulation.selected_units();
                    const std::size_t recorded_before =
                        replay.commands().size();
                    if (pending_rally_building) {
                        GameCommand command = SetRallyPointCommand{
                            *pending_rally_building, tile
                        };
                        if (execute(simulation, command)) {
                            replay.record(
                                simulation.tick_number(), std::move(command)
                            );
                            pending_rally_building.reset();
                            control_group_status = "RALLY POINT SET";
                        }
                    } else if (pending_garrison &&
                        !simulation.selected_units().empty()) {
                        const auto building = std::ranges::find_if(
                            simulation.buildings(),
                            [tile](const Building& candidate) {
                                const BuildingRules& rules =
                                    rules_for(candidate.kind);
                                return candidate.owner == active_view_player &&
                                    candidate.completed() &&
                                    tile.x >= candidate.position.x &&
                                    tile.y >= candidate.position.y &&
                                    tile.x < candidate.position.x +
                                        rules.footprint_width &&
                                    tile.y < candidate.position.y +
                                        rules.footprint_height;
                            }
                        );
                        bool assigned = false;
                        if (building != simulation.buildings().end()) {
                            for (EntityId unit :
                                 simulation.selected_units()) {
                                GameCommand command = MoveUnitCommand{
                                    unit, building->position
                                };
                                if (execute(simulation, command)) {
                                    replay.record(
                                        simulation.tick_number(),
                                        std::move(command)
                                    );
                                    assigned = true;
                                }
                            }
                        }
                        if (assigned) {
                            pending_garrison = false;
                            control_group_status = "GARRISON ORDERED";
                        }
                    } else if (pending_trade_route &&
                        !simulation.selected_units().empty()) {
                        const bool naval_trade =
                            std::ranges::any_of(
                                simulation.selected_units(),
                                [&simulation](EntityId id) {
                                    const auto found =
                                        std::ranges::find_if(
                                            simulation.units(),
                                            [id](const Unit& unit) {
                                                return unit.id == id;
                                            }
                                        );
                                    return found !=
                                               simulation.units().end() &&
                                        found->kind ==
                                            UnitKind::trade_cog;
                                }
                            );
                        const auto market = std::ranges::find_if(
                            simulation.buildings(),
                            [&simulation, tile, naval_trade](
                                const Building& building
                            ) {
                                const BuildingRules& rules =
                                    rules_for(building.kind);
                                return building.kind ==
                                           (naval_trade
                                            ? BuildingKind::dock
                                            : BuildingKind::market) &&
                                    building.completed() &&
                                    building.owner != active_view_player &&
                                    simulation.diplomacy(
                                        active_view_player,
                                        building.owner
                                    ) == Diplomacy::ally &&
                                    tile.x >= building.position.x &&
                                    tile.y >= building.position.y &&
                                    tile.x < building.position.x +
                                        rules.footprint_width &&
                                    tile.y < building.position.y +
                                        rules.footprint_height;
                            }
                        );
                        bool assigned = false;
                        if (market != simulation.buildings().end()) {
                            for (EntityId cart :
                                 simulation.selected_units()) {
                                GameCommand command = TradeRouteCommand{
                                    cart, market->id
                                };
                                if (execute(simulation, command)) {
                                    replay.record(
                                        simulation.tick_number(),
                                        std::move(command)
                                    );
                                    assigned = true;
                                }
                            }
                        }
                        if (assigned) {
                            pending_trade_route = false;
                        }
                    } else if (pending_embark &&
                        !simulation.selected_units().empty()) {
                        const auto transport = std::ranges::find_if(
                            simulation.units(),
                            [tile](const Unit& unit) {
                                return unit.kind ==
                                           UnitKind::transport_ship &&
                                    unit.owner == active_view_player &&
                                    unit.garrisoned_in == 0 &&
                                    unit.position == tile;
                            }
                        );
                        bool assigned = false;
                        if (transport != simulation.units().end()) {
                            for (EntityId passenger :
                                 simulation.selected_units()) {
                                GameCommand command =
                                    EmbarkCommand{
                                        passenger, transport->id};
                                if (execute(simulation, command)) {
                                    replay.record(
                                        simulation.tick_number(),
                                        std::move(command)
                                    );
                                    assigned = true;
                                }
                            }
                        }
                        if (assigned) pending_embark = false;
                    } else if (pending_disembark &&
                        !simulation.selected_units().empty()) {
                        bool assigned = false;
                        for (EntityId transport :
                             simulation.selected_units()) {
                            GameCommand command =
                                DisembarkCommand{transport, tile};
                            if (execute(simulation, command)) {
                                replay.record(
                                    simulation.tick_number(),
                                    std::move(command)
                                );
                                assigned = true;
                            }
                        }
                        if (assigned) pending_disembark = false;
                    } else if (pending_repair &&
                        !simulation.selected_units().empty()) {
                        const auto target = std::ranges::find_if(
                            simulation.buildings(),
                            [&simulation, tile](
                                const Building& building) {
                                const BuildingRules& rules =
                                    rules_for(building.kind);
                                return building.owner ==
                                           active_view_player &&
                                    building.completed() &&
                                    building.hit_points <
                                        simulation.maximum_hit_points(
                                            building) &&
                                    tile.x >= building.position.x &&
                                    tile.y >= building.position.y &&
                                    tile.x < building.position.x +
                                        rules.footprint_width &&
                                    tile.y < building.position.y +
                                        rules.footprint_height;
                            }
                        );
                        bool assigned = false;
                        if (target != simulation.buildings().end()) {
                            for (EntityId villager :
                                 simulation.selected_units()) {
                                GameCommand command = MoveUnitCommand{
                                    villager, target->position
                                };
                                if (execute(simulation, command)) {
                                    replay.record(
                                        simulation.tick_number(),
                                        std::move(command)
                                    );
                                    assigned = true;
                                }
                            }
                        }
                        if (assigned) pending_repair = false;
                    } else if (pending_heal &&
                        !simulation.selected_units().empty()) {
                        const auto target = std::ranges::find_if(
                            simulation.units(),
                            [&simulation, tile](const Unit& unit) {
                                return unit.owner == active_view_player &&
                                    unit.garrisoned_in == 0 &&
                                    unit.position == tile &&
                                    unit.hit_points <
                                        simulation.maximum_hit_points(unit);
                            }
                        );
                        bool assigned = false;
                        if (target != simulation.units().end()) {
                            for (EntityId monk :
                                 simulation.selected_units()) {
                                GameCommand command =
                                    HealUnitCommand{monk, target->id};
                                if (execute(simulation, command)) {
                                    replay.record(
                                        simulation.tick_number(),
                                        std::move(command)
                                    );
                                    assigned = true;
                                }
                            }
                        }
                        if (assigned) pending_heal = false;
                    } else if (pending_relic_action &&
                        !simulation.selected_units().empty()) {
                        const auto relic = std::ranges::find_if(
                            simulation.units(),
                            [tile](const Unit& unit) {
                                return unit.kind == UnitKind::relic &&
                                    unit.garrisoned_in == 0 &&
                                    unit.position == tile;
                            }
                        );
                        const auto monastery = std::ranges::find_if(
                            simulation.buildings(),
                            [tile](const Building& building) {
                                const BuildingRules& rules =
                                    rules_for(building.kind);
                                return building.kind ==
                                           BuildingKind::monastery &&
                                    building.owner == active_view_player &&
                                    building.completed() &&
                                    tile.x >= building.position.x &&
                                    tile.y >= building.position.y &&
                                    tile.x < building.position.x +
                                        rules.footprint_width &&
                                    tile.y < building.position.y +
                                        rules.footprint_height;
                            }
                        );
                        bool assigned = false;
                        for (EntityId monk :
                             simulation.selected_units()) {
                            std::optional<GameCommand> command;
                            if (relic != simulation.units().end()) {
                                command =
                                    CollectRelicCommand{monk, relic->id};
                            } else if (
                                monastery != simulation.buildings().end()) {
                                command = DepositRelicCommand{
                                    monk, monastery->id
                                };
                            }
                            if (command &&
                                execute(simulation, *command)) {
                                replay.record(
                                    simulation.tick_number(),
                                    std::move(*command)
                                );
                                assigned = true;
                            }
                        }
                        if (assigned) pending_relic_action = false;
                    } else if (pending_conversion &&
                        !simulation.selected_units().empty()) {
                        const auto target = std::ranges::find_if(
                            simulation.units(),
                            [&simulation, tile](const Unit& unit) {
                                return unit.owner == Player::red &&
                                    unit.garrisoned_in == 0 &&
                                    unit.position == tile &&
                                    simulation.is_visible(
                                        active_view_player,
                                        tile
                                    );
                            }
                        );
                        bool assigned = false;
                        if (target != simulation.units().end()) {
                            for (EntityId monk :
                                 simulation.selected_units()) {
                                GameCommand command = ConvertUnitCommand{
                                    monk,
                                    target->id,
                                };
                                if (execute(simulation, command)) {
                                    replay.record(
                                        simulation.tick_number(),
                                        std::move(command)
                                    );
                                    assigned = true;
                                }
                            }
                        }
                        if (assigned) {
                            pending_conversion = false;
                        }
                    } else if (pending_guard &&
                        !simulation.selected_units().empty()) {
                        EntityId target{};
                        bool target_is_building = false;
                        const auto guarded_unit = std::ranges::find_if(
                            simulation.units(),
                            [tile](const Unit& unit) {
                                return unit.owner == active_view_player &&
                                       unit.garrisoned_in == 0 &&
                                       unit.position == tile;
                            }
                        );
                        if (guarded_unit != simulation.units().end()) {
                            target = guarded_unit->id;
                        } else {
                            const auto guarded_building =
                                std::ranges::find_if(
                                    simulation.buildings(),
                                    [tile](const Building& building) {
                                        const BuildingRules& rules =
                                            rules_for(building.kind);
                                        return building.owner ==
                                                   active_view_player &&
                                               building.completed() &&
                                               tile.x >=
                                                   building.position.x &&
                                               tile.y >=
                                                   building.position.y &&
                                               tile.x <
                                                   building.position.x +
                                                   rules.footprint_width &&
                                               tile.y <
                                                   building.position.y +
                                                   rules.footprint_height;
                                    }
                                );
                            if (guarded_building !=
                                simulation.buildings().end()) {
                                target = guarded_building->id;
                                target_is_building = true;
                            }
                        }
                        bool assigned = false;
                        if (target != 0) {
                            const std::vector<EntityId> selected =
                                simulation.selected_units();
                            if (selected.size() > 1) {
                                GameCommand command =
                                    MoveFormationCommand{
                                        selected,
                                        tile,
                                        simulation.formation_kind(
                                            active_view_player
                                        ),
                                        FormationOrderKind::guard,
                                        target,
                                        target_is_building,
                                    };
                                if (execute(simulation, command)) {
                                    replay.record(
                                        simulation.tick_number(),
                                        std::move(command)
                                    );
                                    assigned = true;
                                }
                            } else for (EntityId unit : selected) {
                                GameCommand command = GuardCommand{
                                    unit,
                                    target,
                                    target_is_building,
                                };
                                if (execute(simulation, command)) {
                                    replay.record(
                                        simulation.tick_number(),
                                        std::move(command)
                                    );
                                    assigned = true;
                                }
                            }
                        }
                        if (assigned) {
                            pending_guard = false;
                        }
                    } else if (
                        pending_attack_ground &&
                        !simulation.selected_units().empty()) {
                        for (EntityId unit :
                             simulation.selected_units()) {
                            GameCommand command = AttackGroundCommand{
                                unit,
                                tile,
                            };
                            if (execute(simulation, command)) {
                                replay.record(
                                    simulation.tick_number(),
                                    std::move(command)
                                );
                            }
                        }
                        pending_attack_ground = false;
                    } else if (
                        pending_patrol &&
                        !simulation.selected_units().empty()) {
                        const std::vector<EntityId> selected =
                            simulation.selected_units();
                        if (selected.size() > 1) {
                            GameCommand command = MoveFormationCommand{
                                selected,
                                tile,
                                simulation.formation_kind(active_view_player),
                                FormationOrderKind::patrol,
                            };
                            if (execute(simulation, command)) {
                                replay.record(
                                    simulation.tick_number(),
                                    std::move(command)
                                );
                            }
                        } else {
                            GameCommand command = PatrolCommand{
                                selected.front(),
                                tile,
                            };
                            if (execute(simulation, command)) {
                                replay.record(
                                    simulation.tick_number(),
                                    std::move(command)
                                );
                            }
                        }
                        pending_patrol = false;
                    } else if (
                        pending_attack_move &&
                        !simulation.selected_units().empty()) {
                        const std::vector<EntityId> selected =
                            simulation.selected_units();
                        if (selected.size() > 1) {
                            GameCommand command = MoveFormationCommand{
                                selected,
                                tile,
                                simulation.formation_kind(active_view_player),
                                FormationOrderKind::attack_move,
                            };
                            if (execute(simulation, command)) {
                                replay.record(
                                    simulation.tick_number(),
                                    std::move(command)
                                );
                            }
                        } else {
                            GameCommand command = AttackMoveCommand{
                                selected.front(),
                                tile,
                            };
                            if (execute(simulation, command)) {
                                replay.record(
                                    simulation.tick_number(),
                                    std::move(command)
                                );
                            }
                        }
                        pending_attack_move = false;
                    } else if (
                        pending_building && simulation.selected_unit()
                    ) {
                        const PlacementPreview preview =
                            evaluate_building_placement(
                                simulation,
                                *simulation.selected_unit(),
                                *pending_building,
                                tile
                            );
                        if (!preview.valid) {
                            control_group_status =
                                "CANNOT PLACE: " + preview.reason;
                            continue;
                        }
                        GameCommand command = ConstructBuildingCommand{
                            *simulation.selected_unit(),
                            *pending_building,
                            tile,
                        };
                        if (execute(simulation, command)) {
                            replay.record(
                                simulation.tick_number(),
                                std::move(command)
                            );
                            const std::vector<EntityId> selected =
                                simulation.selected_units();
                            for (EntityId unit_id : selected) {
                                if (unit_id ==
                                    *simulation.selected_unit()) {
                                    continue;
                                }
                                GameCommand assist =
                                    MoveUnitCommand{unit_id, tile};
                                if (execute(simulation, assist)) {
                                    replay.record(
                                        simulation.tick_number(),
                                        std::move(assist)
                                    );
                                }
                            }
                            if ((SDL_GetModState() &
                                 SDL_KMOD_SHIFT) == 0) {
                                pending_building.reset();
                                active_build_preview_tile.reset();
                            }
                        }
                    } else if (
                        !simulation.selected_units().empty() &&
                        (SDL_GetModState() & SDL_KMOD_ALT) != 0
                    ) {
                        const auto target_unit = std::ranges::find_if(
                            simulation.units(),
                            [tile](const Unit& unit) {
                                return unit.garrisoned_in == 0 &&
                                    unit.position == tile;
                            }
                        );
                        const auto target_monastery =
                            std::ranges::find_if(
                                simulation.buildings(),
                                [tile](const Building& building) {
                                    const BuildingRules& rules =
                                        rules_for(building.kind);
                                    return building.kind ==
                                               BuildingKind::monastery &&
                                        building.owner == active_view_player &&
                                        building.completed() &&
                                        tile.x >= building.position.x &&
                                        tile.y >= building.position.y &&
                                        tile.x < building.position.x +
                                            rules.footprint_width &&
                                        tile.y < building.position.y +
                                            rules.footprint_height;
                                }
                            );
                        for (EntityId monk :
                             simulation.selected_units()) {
                            std::optional<GameCommand> command;
                            if (target_unit != simulation.units().end() &&
                                target_unit->kind == UnitKind::relic) {
                                command = CollectRelicCommand{
                                    monk, target_unit->id
                                };
                            } else if (
                                target_unit != simulation.units().end() &&
                                target_unit->owner == active_view_player
                            ) {
                                command = HealUnitCommand{
                                    monk, target_unit->id
                                };
                            } else if (
                                target_monastery !=
                                simulation.buildings().end()
                            ) {
                                command = DepositRelicCommand{
                                    monk, target_monastery->id
                                };
                            }
                            if (command && execute(simulation, *command)) {
                                replay.record(
                                    simulation.tick_number(),
                                    std::move(*command)
                                );
                            }
                        }
                    } else if (!simulation.selected_units().empty()) {
                        const bool shared_target =
                            contextual_group_target(simulation, tile);
                        const bool append_waypoint =
                            !shared_target &&
                            (SDL_GetModState() & SDL_KMOD_SHIFT) != 0;
                        const std::vector<EntityId> selected =
                            simulation.selected_units();
                        const std::vector<TilePosition> destinations =
                            shared_target
                                ? std::vector<TilePosition>{}
                                : simulation.formation_destinations(
                                      selected,
                                      tile
                                  );
                        if (selected.size() > 1 &&
                            !shared_target && !append_waypoint) {
                            GameCommand command = MoveFormationCommand{
                                selected,
                                tile,
                                simulation.formation_kind(active_view_player),
                            };
                            if (execute(simulation, command)) {
                                replay.record(
                                    simulation.tick_number(),
                                    std::move(command)
                                );
                            }
                        } else if (
                            selected.size() > 1 &&
                            !shared_target && append_waypoint
                        ) {
                            GameCommand command = MoveFormationCommand{
                                selected,
                                tile,
                                simulation.formation_kind(active_view_player),
                                FormationOrderKind::queued_waypoint,
                            };
                            if (execute(simulation, command)) {
                                replay.record(
                                    simulation.tick_number(),
                                    std::move(command)
                                );
                            }
                        } else for (std::size_t index = 0;
                                    index < selected.size();
                                    ++index) {
                            const TilePosition destination =
                                shared_target
                                    ? tile
                                    : destinations[index];
                            GameCommand command = append_waypoint
                                ? GameCommand{QueueWaypointCommand{
                                      selected[index],
                                      destination,
                                  }}
                                : GameCommand{MoveUnitCommand{
                                      selected[index],
                                      destination,
                                  }};
                            if (execute(simulation, command)) {
                                replay.record(
                                    simulation.tick_number(),
                                    std::move(command)
                                );
                            }
                        }
                    } else if (simulation.selected_building()) {
                        GameCommand command = SetRallyPointCommand{
                            *simulation.selected_building(),
                            tile,
                        };
                        if (execute(simulation, command)) {
                            replay.record(
                                simulation.tick_number(),
                                std::move(command)
                            );
                        }
                    }
                    if (audio != nullptr &&
                        replay.commands().size() > recorded_before) {
                        const auto acknowledged =
                            std::ranges::find_if(
                                simulation.units(),
                                [&command_units](const Unit& unit) {
                                    return std::ranges::find(
                                               command_units,
                                               unit.id
                                           ) != command_units.end() &&
                                        accepted_command_sound(unit.kind) >= 0;
                                }
                            );
                        if (acknowledged != simulation.units().end()) {
                            audio->set_listener_civilization(
                                simulation.civilization(active_view_player)
                            );
                            audio->play_effect(
                                accepted_command_sound(acknowledged->kind)
                            );
                        }
                    }
                }
            } else if (event.type == SDL_EVENT_MOUSE_MOTION &&
                       selection_drag) {
                mouse_position = {event.motion.x, event.motion.y};
                selection_drag->current = {event.motion.x, event.motion.y};
            } else if (event.type == SDL_EVENT_MOUSE_MOTION) {
                mouse_position = {event.motion.x, event.motion.y};
                if (pending_building &&
                    event.motion.y < view_pixel_height) {
                    active_build_preview_tile = mouse_tile(
                        event.motion.x, event.motion.y, camera
                    );
                }
                formation_preview_center =
                    simulation.selected_units().size() > 1 &&
                    event.motion.y < view_pixel_height
                    ? std::optional<TilePosition>{mouse_tile(
                          event.motion.x,
                          event.motion.y,
                          camera
                      )}
                    : std::nullopt;
            } else if (event.type == SDL_EVENT_MOUSE_WHEEL) {
                const float old_zoom = camera.zoom;
                camera.zoom *= event.wheel.y > 0.0F ? 1.1F : 1.0F / 1.1F;
                camera.zoom = std::clamp(
                    camera.zoom,
                    minimum_camera_zoom,
                    maximum_camera_zoom
                );
                const float focus_x =
                    mouse_position.x / old_zoom + camera.x;
                const float focus_y =
                    mouse_position.y / old_zoom + camera.y;
                camera.x = focus_x - mouse_position.x / camera.zoom;
                camera.y = focus_y - mouse_position.y / camera.zoom;
                clamp_camera(camera);
            } else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP &&
                       event.button.button == SDL_BUTTON_RIGHT &&
                       active_wall_drag_start && pending_building &&
                       simulation.selected_unit()) {
                const TilePosition end =
                    mouse_tile(event.button, camera);
                const auto segment = deterministic_wall_segment(
                    *active_wall_drag_start, end
                );
                std::size_t placed{};
                for (TilePosition position : segment) {
                    const PlacementPreview preview =
                        evaluate_building_placement(
                            simulation,
                            *simulation.selected_unit(),
                            *pending_building,
                            position
                        );
                    if (!preview.valid) break;
                    GameCommand command = ConstructBuildingCommand{
                        *simulation.selected_unit(),
                        *pending_building,
                        position,
                    };
                    if (!execute(simulation, command)) break;
                    replay.record(
                        simulation.tick_number(), std::move(command)
                    );
                    ++placed;
                }
                control_group_status =
                    "WALL SEGMENT " + std::to_string(placed) + "/" +
                    std::to_string(segment.size()) +
                    " PLACED; STOPPED AT FIRST FAILURE";
                active_wall_drag_start.reset();
                if ((SDL_GetModState() & SDL_KMOD_SHIFT) == 0) {
                    pending_building.reset();
                    active_build_preview_tile.reset();
                }
            } else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP &&
                       event.button.button == SDL_BUTTON_LEFT &&
                       selection_drag) {
                selection_drag->current = {event.button.x, event.button.y};
                const float drag_x =
                    selection_drag->current.x - selection_drag->start.x;
                const float drag_y =
                    selection_drag->current.y - selection_drag->start.y;
                if (std::abs(drag_x) >= 6.0F ||
                    std::abs(drag_y) >= 6.0F) {
                    const float left = std::min(
                        selection_drag->start.x,
                        selection_drag->current.x
                    );
                    const float right = std::max(
                        selection_drag->start.x,
                        selection_drag->current.x
                    );
                    const float top = std::min(
                        selection_drag->start.y,
                        selection_drag->current.y
                    );
                    const float bottom = std::max(
                        selection_drag->start.y,
                        selection_drag->current.y
                    );
                    const bool additive =
                        (SDL_GetModState() & SDL_KMOD_SHIFT) != 0;
                    std::vector<EntityId> selected = additive
                        ? simulation.selected_units()
                        : std::vector<EntityId>{};
                    for (const Unit& unit : simulation.units()) {
                        if (unit.owner != active_view_player ||
                            unit.garrisoned_in != 0) {
                            continue;
                        }
                        const SDL_FPoint position =
                            tile_screen_top(unit.position);
                        const float center_y =
                            position.y +
                            half_tile_height * active_camera.zoom;
                        if (position.x >= left && position.x <= right &&
                            center_y >= top && center_y <= bottom) {
                            selected.push_back(unit.id);
                        }
                    }
                    if (!selected.empty() || !additive) {
                        simulation.select_units(selected, active_view_player);
                    }
                } else {
                    const TilePosition tile =
                        mouse_tile(event.button, camera);
                    const auto clicked = std::ranges::find_if(
                        simulation.units(),
                        [tile](const Unit& unit) {
                            return unit.owner == active_view_player &&
                                unit.garrisoned_in == 0 &&
                                unit.position == tile;
                        }
                    );
                    const bool additive =
                        (SDL_GetModState() & SDL_KMOD_SHIFT) != 0;
                    if (event.button.clicks >= 2 &&
                        clicked != simulation.units().end()) {
                        std::vector<EntityId> selected;
                        for (const Unit& unit : simulation.units()) {
                            if (unit.owner == active_view_player &&
                                unit.garrisoned_in == 0 &&
                                unit.kind == clicked->kind &&
                                simulation.is_visible(
                                    active_view_player,
                                    unit.position
                                )) {
                                selected.push_back(unit.id);
                            }
                        }
                        simulation.select_units(
                            selected,
                            active_view_player
                        );
                    } else if (
                        additive &&
                        clicked != simulation.units().end()
                    ) {
                        std::vector<EntityId> selected =
                            simulation.selected_units();
                        toggle_selected_id(selected, clicked->id);
                        simulation.select_units(
                            selected,
                            active_view_player
                        );
                    } else if (!additive) {
                        if (!simulation.select_unit_at(
                                tile,
                                active_view_player
                            )) {
                            simulation.select_building_at(
                                tile,
                                active_view_player
                            );
                        }
                    }
                }
                selection_drag.reset();
            } else if (event.type == SDL_EVENT_KEY_DOWN) {
                if (!event.key.repeat &&
                    event.key.key == SDLK_ESCAPE &&
                    pending_building) {
                    pending_building.reset();
                    active_build_preview_tile.reset();
                    control_group_status = "BUILD PLACEMENT CANCELLED";
                    continue;
                }
                if (!event.key.repeat && event.key.key == SDLK_F4 &&
                    !active_save_browser_visible) {
                    active_browser_entries =
                        browse_user_data_files(active_browser_root);
                    active_browser_selection = 0;
                    active_save_browser_visible = true;
                    active_save_browser_status =
                        "PROJECT-NATIVE SAVE / REPLAY FILES";
                    continue;
                }
                if (!event.key.repeat && active_save_browser_visible) {
                    if (active_save_slot_input) {
                        if (event.key.key == SDLK_ESCAPE) {
                            active_save_slot_input = false;
                            active_save_overwrite_armed = false;
                            SDL_StopTextInput(window);
                        } else if (event.key.key == SDLK_BACKSPACE &&
                                   !active_save_slot.empty()) {
                            active_save_slot.pop_back();
                        } else if (
                            event.key.key == SDLK_RETURN ||
                            event.key.key == SDLK_KP_ENTER) {
                            SDL_StopTextInput(window);
                            active_save_slot_input = false;
                            std::string error;
                            if (save_slot_atomic(
                                    simulation, active_browser_root,
                                    active_save_slot,
                                    active_save_overwrite_armed, error)) {
                                active_save_browser_status =
                                    "SAVE WRITTEN ATOMICALLY";
                                active_save_overwrite_armed = false;
                                active_browser_entries =
                                    browse_user_data_files(
                                        active_browser_root);
                            } else if (
                                error ==
                                "overwrite confirmation required") {
                                active_save_overwrite_armed = true;
                                active_save_slot_input = true;
                                SDL_StartTextInput(window);
                                active_save_browser_status =
                                    "OVERWRITE? PRESS ENTER AGAIN; ESC CANCEL";
                            } else {
                                active_save_browser_status =
                                    "SAVE FAILED: " + error;
                            }
                        }
                        continue;
                    }
                    if (event.key.key == SDLK_ESCAPE ||
                        event.key.key == SDLK_F4) {
                        active_save_browser_visible = false;
                    } else if (event.key.key == SDLK_UP &&
                               active_browser_selection > 0) {
                        --active_browser_selection;
                    } else if (
                        event.key.key == SDLK_DOWN &&
                        active_browser_selection + 1 <
                            active_browser_entries.size()) {
                        ++active_browser_selection;
                    } else if (event.key.key == SDLK_N) {
                        active_save_slot_input = true;
                        active_save_overwrite_armed = false;
                        active_save_slot.clear();
                        active_save_browser_status =
                            "TYPE SLOT NAME; ENTER SAVES";
                        SDL_StartTextInput(window);
                    } else if (
                        (event.key.key == SDLK_RETURN ||
                         event.key.key == SDLK_KP_ENTER) &&
                        !active_browser_entries.empty()) {
                        const BrowserEntry& entry =
                            active_browser_entries[
                                active_browser_selection];
                        if (entry.status !=
                                BrowserFileStatus::compatible) {
                            active_save_browser_status =
                                entry.diagnostic;
                        } else {
                            const auto path = bounded_browser_path(
                                active_browser_root, entry
                            );
                            try {
                                if (entry.kind ==
                                    BrowserFileKind::save) {
                                    simulation =
                                        load_presentable_game(path);
                                    computer =
                                        ComputerPlayer(Player::red);
                                    replaying = false;
                                    control_groups = {};
                                    last_control_group_recall.reset();
                                    active_save_browser_status =
                                        "SAVE LOADED";
                                } else if (
                                    entry.kind ==
                                    BrowserFileKind::replay) {
                                    replay = load_replay(path);
                                    simulation = new_game();
                                    replay.reset_playback();
                                    replaying = true;
                                    active_save_browser_status =
                                        "REPLAY STARTED";
                                }
                                active_save_browser_visible = false;
                            } catch (const std::exception& error) {
                                active_save_browser_status =
                                    std::string{"OPEN FAILED: "} +
                                    error.what();
                            }
                        }
                    }
                    continue;
                }
                if (!event.key.repeat && event.key.key == SDLK_F12 &&
                    !active_statistics_visible) {
                    active_statistics_visible = true;
                    active_statistics_postgame =
                        simulation.outcome() != MatchOutcome::ongoing;
                    continue;
                }
                if (!event.key.repeat && active_statistics_visible) {
                    if (event.key.key >= SDLK_1 &&
                        event.key.key <= SDLK_5) {
                        active_statistics_tab = static_cast<StatisticsTab>(
                            event.key.key - SDLK_1
                        );
                    } else if (
                        event.key.key == SDLK_ESCAPE ||
                        event.key.key == SDLK_F12 ||
                        (active_statistics_postgame &&
                         event.key.key == SDLK_C)) {
                        active_statistics_visible = false;
                    } else if (active_statistics_postgame &&
                               event.key.key == SDLK_R) {
                        simulation = new_game();
                        computer = ComputerPlayer(Player::red);
                        center_camera_on(
                            camera, {active_map_tiles_x() / 2, active_map_tiles_y() / 2}
                        );
                        active_statistics_visible = false;
                        active_statistics_postgame = false;
                        outcome_statistics_seen = false;
                    } else if (active_statistics_postgame &&
                               event.key.key == SDLK_B) {
                        active_statistics_visible = false;
                        active_statistics_postgame = false;
                        active_frontend_screen =
                            FrontendScreen::main_menu;
                    }
                    continue;
                }
                if (!event.key.repeat && active_options_visible) {
                    if (active_options_hotkeys) {
                        if (event.key.key == SDLK_H ||
                            event.key.key == SDLK_ESCAPE) {
                            active_options_hotkeys = false;
                        }
                        continue;
                    }
                    auto step_volume = [](int& value) {
                        value = value >= 100 ? 0 : value + 10;
                    };
                    if (event.key.key == SDLK_ESCAPE) {
                        draft_settings = active_settings;
                        active_options_visible = false;
                        active_options_status = "CHANGES CANCELLED";
                    } else if (event.key.key == SDLK_G) {
                        draft_settings.game_speed =
                            draft_settings.game_speed ==
                                    SinglePlayerSpeed::slow
                                ? SinglePlayerSpeed::normal
                            : draft_settings.game_speed ==
                                    SinglePlayerSpeed::normal
                                ? SinglePlayerSpeed::fast
                                : SinglePlayerSpeed::slow;
                    } else if (event.key.key == SDLK_M) {
                        step_volume(draft_settings.music_volume);
                    } else if (event.key.key == SDLK_E) {
                        step_volume(draft_settings.effects_volume);
                    } else if (event.key.key == SDLK_C) {
                        step_volume(draft_settings.combat_volume);
                    } else if (event.key.key == SDLK_I) {
                        step_volume(draft_settings.interface_volume);
                    } else if (event.key.key == SDLK_B) {
                        step_volume(draft_settings.ambient_volume);
                    } else if (event.key.key == SDLK_F) {
                        draft_settings.fullscreen =
                            !draft_settings.fullscreen;
                    } else if (event.key.key == SDLK_R) {
                        draft_settings.scroll_speed =
                            draft_settings.scroll_speed >= 200
                                ? 50
                                : draft_settings.scroll_speed + 25;
                    } else if (event.key.key == SDLK_X) {
                        draft_settings.edge_scroll =
                            !draft_settings.edge_scroll;
                    } else if (event.key.key == SDLK_V) {
                        draft_settings.fog = !draft_settings.fog;
                    } else if (event.key.key == SDLK_P) {
                        draft_settings.minimap =
                            !draft_settings.minimap;
                    } else if (event.key.key == SDLK_H) {
                        active_options_hotkeys = true;
                    } else if (event.key.key == SDLK_A) {
                        apply_options();
                    } else if (event.key.key == SDLK_S) {
                        if (apply_options()) {
                            std::string error;
                            active_options_status = save_settings_atomic(
                                active_settings,
                                active_settings_path,
                                error
                            ) ? "SAVED ATOMICALLY: " +
                                    active_settings_path.string()
                              : "SAVE FAILED: " + error;
                        }
                    }
                    continue;
                }
                if (!event.key.repeat &&
                    event.key.key == SDLK_F10) {
                    active_diplomacy_panel_visible =
                        !active_diplomacy_panel_visible;
                    continue;
                }
                if (active_diplomacy_panel_visible &&
                    !event.key.repeat) {
                    if (event.key.key == SDLK_ESCAPE) {
                        active_diplomacy_panel_visible = false;
                    } else if (
                        event.key.key == SDLK_A ||
                        event.key.key == SDLK_N ||
                        event.key.key == SDLK_E) {
                        const Diplomacy relation =
                            event.key.key == SDLK_A
                                ? Diplomacy::ally
                            : event.key.key == SDLK_N
                                ? Diplomacy::neutral
                                : Diplomacy::enemy;
                        GameCommand command = SetDiplomacyCommand{
                            active_view_player,
                            opposing_player(active_view_player),
                            relation,
                        };
                        if (execute(simulation, command)) {
                            replay.record(
                                simulation.tick_number(),
                                std::move(command)
                            );
                            active_diplomacy_status =
                                "DIPLOMACY COMMAND QUEUED";
                        } else {
                            active_diplomacy_status =
                                "DIPLOMACY ACTION UNAVAILABLE";
                        }
                    } else if (event.key.key == SDLK_1) {
                        active_tribute_resource = ResourceKind::food;
                    } else if (event.key.key == SDLK_2) {
                        active_tribute_resource = ResourceKind::wood;
                    } else if (event.key.key == SDLK_3) {
                        active_tribute_resource = ResourceKind::gold;
                    } else if (event.key.key == SDLK_4) {
                        active_tribute_resource = ResourceKind::stone;
                    } else if (
                        event.key.key == SDLK_PLUS ||
                        event.key.key == SDLK_EQUALS ||
                        event.key.key == SDLK_KP_PLUS) {
                        active_tribute_amount =
                            std::min(10000, active_tribute_amount + 100);
                    } else if (
                        event.key.key == SDLK_MINUS ||
                        event.key.key == SDLK_KP_MINUS) {
                        active_tribute_amount =
                            std::max(100, active_tribute_amount - 100);
                    } else if (
                        event.key.key == SDLK_RETURN ||
                        event.key.key == SDLK_KP_ENTER) {
                        if (simulation.diplomacy(
                                active_view_player,
                                opposing_player(active_view_player)) !=
                            Diplomacy::ally) {
                            active_diplomacy_status =
                                "TRIBUTE DISABLED: TARGET IS NOT ALLY";
                        } else {
                            GameCommand command = TributeResourceCommand{
                                active_view_player,
                                opposing_player(active_view_player),
                                active_tribute_resource,
                                active_tribute_amount,
                            };
                            if (execute(simulation, command)) {
                                replay.record(
                                    simulation.tick_number(),
                                    std::move(command)
                                );
                                active_diplomacy_status =
                                    "TRIBUTE COMMAND QUEUED";
                            } else {
                                active_diplomacy_status =
                                    "TRIBUTE UNAVAILABLE";
                            }
                        }
                    } else if (
                        event.key.key == SDLK_T ||
                        event.key.key == SDLK_Y) {
                        if (multiplayer_runtime &&
                            multiplayer_presentation) {
                            multiplayer_presentation->chat_audience =
                                event.key.key == SDLK_Y
                                ? ChatAudience::allies
                                : ChatAudience::all;
                            multiplayer_presentation->chat_input_active =
                                true;
                            multiplayer_presentation->visible = true;
                            active_diplomacy_panel_visible = false;
                            SDL_StartTextInput(window);
                        } else {
                            active_diplomacy_status =
                                "CHAT REQUIRES MULTIPLAYER SESSION";
                        }
                    }
                    continue;
                }
                if (!event.key.repeat &&
                    event.key.key == SDLK_F9) {
                    active_technology_tree_visible =
                        !active_technology_tree_visible;
                    if (active_technology_tree_visible) {
                        std::vector<Technology> researched;
                        for (std::size_t index = 0;
                             index < technology_count; ++index) {
                            const Technology technology =
                                static_cast<Technology>(index);
                            if (simulation.has_technology(
                                    active_view_player, technology)) {
                                researched.push_back(technology);
                            }
                        }
                        active_technology_tree =
                            build_technology_tree(
                                simulation.civilization(
                                    active_view_player),
                                researched
                            );
                        active_tree_focus = 0;
                    }
                    continue;
                }
                if (active_technology_tree_visible) {
                    if (event.key.key == SDLK_ESCAPE) {
                        active_technology_tree_visible = false;
                    } else if (event.key.key == SDLK_Q ||
                               event.key.key == SDLK_E) {
                        int civilization = static_cast<int>(
                            active_technology_tree.civilization
                        );
                        civilization += event.key.key == SDLK_E ? 1 : -1;
                        if (civilization < 1) civilization = 18;
                        if (civilization > 18) civilization = 1;
                        active_technology_tree =
                            build_technology_tree(
                                static_cast<Civilization>(civilization)
                            );
                        active_tree_focus = 0;
                    } else if (
                        event.key.key == SDLK_TAB ||
                        event.key.key == SDLK_LEFT ||
                        event.key.key == SDLK_RIGHT ||
                        event.key.key == SDLK_UP ||
                        event.key.key == SDLK_DOWN ||
                        event.key.key == SDLK_HOME ||
                        event.key.key == SDLK_END
                    ) {
                        if (event.key.key == SDLK_HOME) {
                            active_tree_focus = 0;
                        } else if (event.key.key == SDLK_END) {
                            active_tree_focus =
                                active_technology_tree.nodes.empty()
                                ? 0
                                : active_technology_tree.nodes.size() - 1;
                        } else {
                            TechnologyTreeDirection direction =
                                TechnologyTreeDirection::next;
                            if (event.key.key == SDLK_TAB &&
                                (event.key.mod & SDL_KMOD_SHIFT) != 0) {
                                direction =
                                    TechnologyTreeDirection::previous;
                            } else if (event.key.key == SDLK_LEFT) {
                                direction = TechnologyTreeDirection::left;
                            } else if (event.key.key == SDLK_RIGHT) {
                                direction = TechnologyTreeDirection::right;
                            } else if (event.key.key == SDLK_UP) {
                                direction = TechnologyTreeDirection::up;
                            } else if (event.key.key == SDLK_DOWN) {
                                direction = TechnologyTreeDirection::down;
                            }
                            active_tree_focus = navigate_technology_tree(
                                active_technology_tree,
                                active_tree_focus,
                                direction
                            );
                        }
                        if (!active_technology_tree.nodes.empty()) {
                            const TechnologyTreeNode& focused =
                                active_technology_tree.nodes[
                                    active_tree_focus
                                ];
                            active_tree_pan_x = std::max(
                                0.0F,
                                focused.x * active_tree_zoom -
                                    view_pixel_width * 0.5F
                            );
                            active_tree_pan_y = std::max(
                                0.0F,
                                focused.y * active_tree_zoom -
                                    view_pixel_height * 0.5F
                            );
                        }
                    } else if (event.key.key == SDLK_A) {
                        active_tree_pan_x =
                            std::max(0.0F, active_tree_pan_x - 60.0F);
                    } else if (event.key.key == SDLK_D) {
                        active_tree_pan_x += 60.0F;
                    } else if (event.key.key == SDLK_W) {
                        active_tree_pan_y =
                            std::max(0.0F, active_tree_pan_y - 60.0F);
                    } else if (event.key.key == SDLK_S) {
                        active_tree_pan_y += 60.0F;
                    } else if (event.key.key == SDLK_PLUS ||
                               event.key.key == SDLK_EQUALS ||
                               event.key.key == SDLK_KP_PLUS) {
                        active_tree_zoom =
                            std::min(1.35F, active_tree_zoom + 0.1F);
                    } else if (event.key.key == SDLK_MINUS ||
                               event.key.key == SDLK_KP_MINUS) {
                        active_tree_zoom =
                            std::max(0.35F, active_tree_zoom - 0.1F);
                    }
                    continue;
                }
                if (active_frontend_screen != FrontendScreen::hidden &&
                    !event.key.repeat) {
                    if (active_frontend_screen ==
                        FrontendScreen::single_player_setup) {
                        if (event.key.key == SDLK_ESCAPE) {
                            active_frontend_screen =
                                FrontendScreen::main_menu;
                        } else if (event.key.key == SDLK_M) {
                            active_random_settings.kind =
                                active_random_settings.kind ==
                                        RandomMapKind::arabia
                                    ? RandomMapKind::black_forest
                                : active_random_settings.kind ==
                                        RandomMapKind::black_forest
                                    ? RandomMapKind::islands
                                : active_random_settings.kind ==
                                        RandomMapKind::islands
                                    ? RandomMapKind::rivers
                                    : RandomMapKind::arabia;
                            refresh_random_map_preview();
                        } else if (event.key.key == SDLK_Z) {
                            active_random_settings.size =
                                next_random_map_size(
                                    active_random_settings.size
                                );
                            refresh_random_map_preview();
                        } else if (
                            event.key.key == SDLK_PLUS ||
                            event.key.key == SDLK_KP_PLUS ||
                            event.key.key == SDLK_EQUALS) {
                            ++active_random_settings.seed;
                            refresh_random_map_preview();
                        } else if (
                            event.key.key == SDLK_MINUS ||
                            event.key.key == SDLK_KP_MINUS) {
                            if (active_random_settings.seed > 0) {
                                --active_random_settings.seed;
                            }
                            refresh_random_map_preview();
                        } else if (event.key.key == SDLK_C) {
                            active_setup_civilization =
                                active_setup_civilization ==
                                        Civilization::britons
                                    ? Civilization::franks
                                : active_setup_civilization ==
                                        Civilization::franks
                                    ? Civilization::mongols
                                    : Civilization::britons;
                            refresh_random_map_preview();
                        } else if (event.key.key == SDLK_D) {
                            active_setup_difficulty =
                                active_setup_difficulty ==
                                        ComputerDifficulty::easiest
                                    ? ComputerDifficulty::easy
                                : active_setup_difficulty ==
                                        ComputerDifficulty::easy
                                    ? ComputerDifficulty::moderate
                                : active_setup_difficulty ==
                                        ComputerDifficulty::moderate
                                    ? ComputerDifficulty::hard
                                : active_setup_difficulty ==
                                        ComputerDifficulty::hard
                                    ? ComputerDifficulty::hardest
                                    : ComputerDifficulty::easiest;
                        } else if (event.key.key == SDLK_V) {
                            active_setup_victory =
                                (active_setup_victory + 1) % 3;
                        } else if (
                            event.key.key == SDLK_RETURN ||
                            event.key.key == SDLK_KP_ENTER) {
                            if (!random_map_preview) {
                                refresh_random_map_preview();
                            }
                            if (random_map_preview) {
                                random_map_preview->blue_civilization =
                                    active_setup_civilization;
                                MatchRules& rules =
                                    random_map_preview->match_rules;
                                rules.conquest_enabled =
                                    active_setup_victory == 0;
                                rules.wonder_enabled =
                                    active_setup_victory == 1;
                                rules.relic_enabled =
                                    active_setup_victory == 2;
                                demo_scenario = *random_map_preview;
                                simulation =
                                    create_simulation(demo_scenario);
                                computer = ComputerPlayer(
                                    Player::red,
                                    active_setup_difficulty
                                );
                                active_frontend_screen =
                                    FrontendScreen::hidden;
                            }
                        }
                    } else if (event.key.key == SDLK_1) {
                        active_frontend_screen =
                            FrontendScreen::single_player_setup;
                        refresh_random_map_preview();
                    } else if (event.key.key == SDLK_2) {
                        active_frontend_screen = FrontendScreen::hidden;
                    } else if (event.key.key == SDLK_3) {
                        if (campaign_presentation) {
                            campaign_presentation->screen =
                                CampaignPresentation::Screen::briefing;
                            campaign_presentation->visible = true;
                            active_frontend_screen =
                                FrontendScreen::hidden;
                        } else {
                            active_frontend_status =
                                "SET AOE_CAMPAIGN TO LAUNCH";
                        }
                    } else if (event.key.key == SDLK_4) {
                        if (!scenario_editor) {
                            scenario_editor.emplace(demo_scenario);
                        }
                        active_editor_overlay = true;
                        active_frontend_screen = FrontendScreen::hidden;
                    } else if (
                        event.key.key == SDLK_H ||
                        event.key.key == SDLK_J) {
                        if (multiplayer_presentation) {
                            multiplayer_presentation->visible = true;
                            active_frontend_screen =
                                FrontendScreen::hidden;
                        } else {
                            active_frontend_status =
                                "SET AOE_MULTIPLAYER=host OR join";
                        }
                    } else if (event.key.key == SDLK_O) {
                        draft_settings = active_settings;
                        active_options_visible = true;
                    } else if (event.key.key == SDLK_L) {
                        active_browser_entries =
                            browse_user_data_files(active_browser_root);
                        active_browser_selection = 0;
                        active_save_browser_visible = true;
                        active_frontend_screen =
                            FrontendScreen::hidden;
                    } else if (event.key.key == SDLK_ESCAPE) {
                        running = false;
                    }
                    continue;
                }
                if (scenario_editor && !event.key.repeat) {
                    const SDL_Keymod modifiers = SDL_GetModState();
                    const bool command =
                        (modifiers &
                         (SDL_KMOD_CTRL | SDL_KMOD_GUI)) != 0;
                    bool handled = true;
                    if (event.key.key == SDLK_ESCAPE) {
                        active_editor_overlay = false;
                        scenario_editor.reset();
                        active_frontend_screen = FrontendScreen::main_menu;
                        active_editor_status = "EDITOR CLOSED";
                    } else if (event.key.key == SDLK_TAB) {
                        const int step =
                            (modifiers & SDL_KMOD_SHIFT) != 0 ? -1 : 1;
                        active_editor_focus = static_cast<std::size_t>(
                            (static_cast<int>(active_editor_focus) +
                             step + 5) % 5
                        );
                    } else if (
                        event.key.key == SDLK_LEFT ||
                        event.key.key == SDLK_RIGHT ||
                        event.key.key == SDLK_UP ||
                        event.key.key == SDLK_DOWN
                    ) {
                        const auto& map = scenario_editor->scenario().map;
                        active_editor_cursor.x = std::clamp(
                            active_editor_cursor.x +
                                (event.key.key == SDLK_RIGHT ? 1 :
                                 event.key.key == SDLK_LEFT ? -1 : 0),
                            0, map.width() - 1
                        );
                        active_editor_cursor.y = std::clamp(
                            active_editor_cursor.y +
                                (event.key.key == SDLK_DOWN ? 1 :
                                 event.key.key == SDLK_UP ? -1 : 0),
                            0, map.height() - 1
                        );
                    } else if (
                        event.key.key == SDLK_RETURN ||
                        event.key.key == SDLK_KP_ENTER
                    ) {
                        static_cast<void>(apply_editor_cursor());
                    } else if (command && event.key.key == SDLK_Z) {
                        active_editor_status = scenario_editor->undo()
                            ? "UNDO" : "NOTHING TO UNDO";
                        simulation = create_simulation(
                            scenario_editor->scenario());
                    } else if (command && event.key.key == SDLK_Y) {
                        active_editor_status = scenario_editor->redo()
                            ? "REDO" : "NOTHING TO REDO";
                        simulation = create_simulation(
                            scenario_editor->scenario());
                    } else if (command && event.key.key == SDLK_S) {
                        std::string error;
                        active_editor_status = scenario_editor->save(
                            scenario_editor_path, error
                        ) ? "SAVED: " + scenario_editor_path.string()
                          : "SAVE FAILED: " + error;
                    } else if (command && event.key.key == SDLK_O) {
                        try {
                            *scenario_editor =
                                ScenarioEditor{
                                    load_presentable_scenario(
                                        scenario_editor_path
                                    )
                                };
                            simulation = create_simulation(
                                scenario_editor->scenario()
                            );
                            active_editor_status =
                                "LOADED: " + scenario_editor_path.string();
                        } catch (const std::exception& error) {
                            active_editor_status =
                                "LOAD FAILED: " + std::string{error.what()};
                        }
                    } else if (event.key.key == SDLK_P) {
                        active_editor_player =
                            active_editor_player == Player::blue
                            ? Player::red : Player::blue;
                        active_editor_focus = 1;
                    } else if (event.key.key == SDLK_A) {
                        const Player selected = active_editor_player;
                        const Age current =
                            selected == Player::blue
                            ? scenario_editor->scenario().blue_age
                            : scenario_editor->scenario().red_age;
                        scenario_editor->set_age(
                            selected,
                            static_cast<Age>(
                                (static_cast<int>(current) + 1) % 4
                            )
                        );
                        simulation = create_simulation(
                            scenario_editor->scenario()
                        );
                        active_editor_focus = 1;
                        active_editor_status = "PLAYER AGE CHANGED";
                    } else if (event.key.key == SDLK_C) {
                        const Player selected = active_editor_player;
                        const Civilization current =
                            selected == Player::blue
                            ? scenario_editor->scenario().blue_civilization
                            : scenario_editor->scenario().red_civilization;
                        int civilization = static_cast<int>(current) + 1;
                        if (civilization > 18 || civilization < 1) {
                            civilization = 1;
                        }
                        scenario_editor->set_civilization(
                            selected,
                            static_cast<Civilization>(civilization)
                        );
                        simulation = create_simulation(
                            scenario_editor->scenario()
                        );
                        active_editor_focus = 1;
                        active_editor_status =
                            "PLAYER CIVILIZATION CHANGED";
                    } else if (
                        event.key.key == SDLK_LEFTBRACKET ||
                        event.key.key == SDLK_RIGHTBRACKET
                    ) {
                        Economy economy =
                            active_editor_player == Player::blue
                            ? scenario_editor->scenario().blue_economy
                            : scenario_editor->scenario().red_economy;
                        const int delta =
                            event.key.key == SDLK_RIGHTBRACKET ? 100 : -100;
                        economy.wood = std::max(0, economy.wood + delta);
                        economy.food = std::max(0, economy.food + delta);
                        economy.gold = std::max(0, economy.gold + delta);
                        economy.stone = std::max(0, economy.stone + delta);
                        scenario_editor->set_economy(
                            active_editor_player, economy
                        );
                        simulation = create_simulation(
                            scenario_editor->scenario()
                        );
                        active_editor_focus = 1;
                        active_editor_status = "PLAYER RESOURCES CHANGED";
                    } else if (event.key.key == SDLK_R) {
                        MatchRules rules =
                            scenario_editor->scenario().match_rules;
                        rules.wonder_enabled = !rules.wonder_enabled;
                        scenario_editor->set_match_rules(rules);
                        simulation = create_simulation(
                            scenario_editor->scenario()
                        );
                        active_editor_focus = 1;
                        active_editor_status = "MATCH RULES CHANGED";
                    } else if (event.key.key == SDLK_D) {
                        const Diplomacy current =
                            scenario_editor->scenario()
                                .blue_red_diplomacy;
                        const Diplomacy next =
                            current == Diplomacy::enemy
                            ? Diplomacy::neutral
                            : current == Diplomacy::neutral
                                ? Diplomacy::ally
                                : Diplomacy::enemy;
                        scenario_editor->set_diplomacy(next);
                        simulation = create_simulation(
                            scenario_editor->scenario()
                        );
                        active_editor_focus = 1;
                        active_editor_status = "DIPLOMACY CHANGED";
                    } else if (event.key.key == SDLK_O) {
                        int id = 1;
                        for (const auto& objective :
                             scenario_editor->scenario().objectives) {
                            id = std::max(id, objective.id + 1);
                        }
                        const bool added = scenario_editor->add_objective({
                            id, active_editor_player, true, false,
                            "Editor objective " + std::to_string(id),
                        });
                        active_editor_focus = 2;
                        active_editor_status =
                            added ? "OBJECTIVE ADDED" : "OBJECTIVE REJECTED";
                    } else if (event.key.key == SDLK_T) {
                        int id = 1;
                        for (const auto& trigger :
                             scenario_editor->scenario().triggers) {
                            id = std::max(id, trigger.id + 1);
                        }
                        const bool added = scenario_editor->add_trigger({
                            id, id * 10, true, false,
                            "elapsed_ticks >= " + std::to_string(id * 10),
                            std::string{"victory "} +
                                (active_editor_player == Player::blue
                                 ? "blue" : "red"),
                        });
                        active_editor_focus = 3;
                        active_editor_status =
                            added ? "TRIGGER ADDED" : "TRIGGER REJECTED";
                    } else if (event.key.key == SDLK_V) {
                        const ScenarioEditorValidation validation =
                            scenario_editor->validate();
                        active_editor_focus = 4;
                        active_editor_status = validation.valid
                            ? "VALIDATION PASSED"
                            : "VALIDATION FAILED: " +
                                validation.errors.front();
                    } else if (event.key.key == SDLK_1) {
                        active_editor_tool = EditorTool::grass;
                    } else if (event.key.key == SDLK_2) {
                        active_editor_tool = EditorTool::water;
                    } else if (event.key.key == SDLK_3) {
                        active_editor_tool = EditorTool::forest;
                    } else if (event.key.key == SDLK_E) {
                        active_editor_tool = EditorTool::elevation;
                    } else if (event.key.key == SDLK_U) {
                        active_editor_tool = EditorTool::villager;
                    } else if (event.key.key == SDLK_B) {
                        active_editor_tool = EditorTool::house;
                    } else if (event.key.key == SDLK_X ||
                               event.key.key == SDLK_DELETE) {
                        active_editor_tool = EditorTool::erase;
                    } else {
                        handled = false;
                    }
                    if (handled) continue;
                }
                if (campaign_presentation &&
                    campaign_presentation->visible &&
                    campaign_presentation->screen !=
                        CampaignPresentation::Screen::status &&
                    !event.key.repeat) {
                    if (event.key.key == SDLK_RETURN ||
                        event.key.key == SDLK_KP_ENTER) {
                        campaign_presentation->screen =
                            CampaignPresentation::Screen::status;
                        campaign_presentation->visible = false;
                    } else if (event.key.key == SDLK_ESCAPE) {
                        if (campaign_presentation->screen ==
                            CampaignPresentation::Screen::briefing) {
                            running = false;
                        } else {
                            campaign_presentation->screen =
                                CampaignPresentation::Screen::status;
                            campaign_presentation->visible = false;
                        }
                    }
                    continue;
                }
                if (multiplayer_runtime &&
                    multiplayer_presentation &&
                    !event.key.repeat) {
                    MultiplayerPresentation& multiplayer =
                        *multiplayer_presentation;
                    if (event.key.key == SDLK_F &&
                        (SDL_GetModState() & SDL_KMOD_ALT) != 0) {
                        pending_map_signal = true;
                        multiplayer.chat_feedback =
                            "SIGNAL: CLICK AN EXPLORED WORLD TILE";
                        continue;
                    }
                    if (simulation.observer_perspective(
                            active_view_player) &&
                        (event.key.key == SDLK_RETURN ||
                         event.key.key == SDLK_KP_ENTER)) {
                        multiplayer.chat_input_active = false;
                        multiplayer.chat_input.clear();
                        multiplayer.chat_feedback =
                            "OBSERVER: CHAT DISABLED";
                        SDL_StopTextInput(window);
                        continue;
                    }
                    if (multiplayer.chat_input_active) {
                        if (event.key.key == SDLK_ESCAPE) {
                            multiplayer.chat_input_active = false;
                            multiplayer.chat_input.clear();
                            multiplayer.chat_feedback = "MESSAGE CANCELLED";
                            SDL_StopTextInput(window);
                        } else if (event.key.key == SDLK_TAB) {
                            multiplayer.chat_audience =
                                multiplayer.chat_audience ==
                                    ChatAudience::all
                                ? ChatAudience::allies
                                : ChatAudience::all;
                        } else if (
                            event.key.key == SDLK_BACKSPACE &&
                            !multiplayer.chat_input.empty()) {
                            std::size_t codepoint =
                                multiplayer.chat_input.size() - 1;
                            while (
                                codepoint > 0 &&
                                (static_cast<unsigned char>(
                                     multiplayer.chat_input[codepoint]
                                 ) & 0xC0U) == 0x80U) {
                                --codepoint;
                            }
                            multiplayer.chat_input.resize(codepoint);
                        } else if (
                            event.key.key == SDLK_RETURN ||
                            event.key.key == SDLK_KP_ENTER) {
                            if (multiplayer.chat_input.empty()) {
                                multiplayer.chat_feedback =
                                    "MESSAGE CANNOT BE EMPTY";
                            } else if (multiplayer_runtime->send_chat(
                                           multiplayer.chat_input,
                                           multiplayer.chat_audience)) {
                                multiplayer.chat_input.clear();
                                multiplayer.chat_input_active = false;
                                multiplayer.chat_feedback = "MESSAGE SENT";
                                SDL_StopTextInput(window);
                            } else {
                                multiplayer.chat_feedback =
                                    "MESSAGE REJECTED: CHECK CONNECTION/UTF-8";
                            }
                        }
                        continue;
                    }
                    if (event.key.key == SDLK_RETURN ||
                        event.key.key == SDLK_KP_ENTER) {
                        const SDL_Keymod modifiers = SDL_GetModState();
                        const bool request_start =
                            (modifiers &
                             (SDL_KMOD_CTRL | SDL_KMOD_GUI)) != 0 &&
                            multiplayer.hosting &&
                            multiplayer.blue_ready &&
                            multiplayer.red_ready &&
                            multiplayer.live_status !=
                                LockstepStatus::running;
                        if (request_start) {
                            multiplayer_runtime->request_start();
                        } else {
                            multiplayer.chat_input_active = true;
                            multiplayer.chat_feedback.clear();
                            SDL_StartTextInput(window);
                        }
                        continue;
                    }
                    if (event.key.key == SDLK_F6) {
                        if (!multiplayer.hosting) {
                            multiplayer.checkpoint_feedback =
                                "ONLY HOST MAY REQUEST CHECKPOINT";
                        } else if (
                            multiplayer_runtime->request_save_barrier(
                                multiplayer_runtime->current_tick() + 2
                            )) {
                            multiplayer.checkpoint_feedback =
                                "CHECKPOINT BARRIER REQUESTED";
                        } else {
                            multiplayer.checkpoint_feedback =
                                "CHECKPOINT REQUEST REJECTED";
                        }
                        continue;
                    }
                    if (event.key.key == SDLK_F7) {
                        if (!multiplayer.hosting) {
                            multiplayer.control_feedback =
                                "HOST CONTROLS PAUSE";
                        } else if (multiplayer_runtime->propose_pause(
                                       !multiplayer_runtime->paused(),
                                       multiplayer_runtime->current_tick() +
                                           2)) {
                            multiplayer.control_feedback =
                                "PAUSE PROPOSAL SENT; WAITING PEER ACK";
                        } else {
                            multiplayer.control_feedback =
                                "PAUSE PROPOSAL REJECTED";
                        }
                        continue;
                    }
                    if (event.key.key == SDLK_F8) {
                        const GameSpeed next =
                            multiplayer_runtime->game_speed() ==
                                    GameSpeed::normal
                                ? GameSpeed::fast
                            : multiplayer_runtime->game_speed() ==
                                    GameSpeed::fast
                                ? GameSpeed::slow
                                : GameSpeed::normal;
                        if (!multiplayer.hosting) {
                            multiplayer.control_feedback =
                                "HOST CONTROLS SPEED";
                        } else if (multiplayer_runtime->propose_speed(
                                       next,
                                       multiplayer_runtime->current_tick() +
                                           2)) {
                            multiplayer.control_feedback =
                                "SPEED PROPOSAL SENT; WAITING PEER ACK";
                        } else {
                            multiplayer.control_feedback =
                                "SPEED PROPOSAL REJECTED";
                        }
                        continue;
                    }
                }
                if (multiplayer_runtime &&
                    multiplayer_presentation->live_status !=
                        LockstepStatus::running &&
                    !event.key.repeat) {
                    if (event.key.key == SDLK_R &&
                        !multiplayer_presentation->local_ready) {
                        multiplayer_runtime->set_ready();
                        multiplayer_presentation->local_ready = true;
                        continue;
                    }
                    if ((event.key.key == SDLK_RETURN ||
                         event.key.key == SDLK_KP_ENTER) &&
                        (SDL_GetModState() &
                         (SDL_KMOD_CTRL | SDL_KMOD_GUI)) != 0 &&
                        multiplayer_presentation->hosting &&
                        multiplayer_presentation->blue_ready &&
                        multiplayer_presentation->red_ready) {
                        multiplayer_runtime->request_start();
                        continue;
                    }
                    continue;
                }
                if (multiplayer_presentation &&
                    event.key.key == SDLK_F4 &&
                    !event.key.repeat &&
                    (SDL_GetModState() &
                     (SDL_KMOD_CTRL | SDL_KMOD_ALT | SDL_KMOD_GUI)) == 0) {
                    multiplayer_presentation->visible =
                        !multiplayer_presentation->visible;
                    continue;
                }
                if (campaign_presentation &&
                    event.key.key == SDLK_F5 &&
                    !event.key.repeat &&
                    (SDL_GetModState() &
                     (SDL_KMOD_CTRL | SDL_KMOD_ALT | SDL_KMOD_GUI)) == 0) {
                    campaign_presentation->visible =
                        !campaign_presentation->visible;
                    continue;
                }
                if (event.key.key == SDLK_TAB &&
                    !event.key.repeat &&
                    (SDL_GetModState() &
                     (SDL_KMOD_CTRL | SDL_KMOD_ALT | SDL_KMOD_GUI)) == 0) {
                    scenario_presentation.objectives_visible =
                        !scenario_presentation.objectives_visible;
                    continue;
                }
                if (!event.key.repeat &&
                    !replaying &&
                    (simulation.selected_unit() ||
                     simulation.selected_building()) &&
                    (SDL_GetModState() &
                     (SDL_KMOD_CTRL | SDL_KMOD_ALT | SDL_KMOD_GUI)) == 0) {
                    const SelectionPanelModel panel =
                        build_selection_panel(
                            simulation,
                            active_view_player,
                            active_command_page,
                            active_command_subpage
                        );
                    const auto match = std::ranges::find_if(
                        panel.commands,
                        [&event](const CommandButtonModel& button) {
                            return button.enabled &&
                                panel_hotkey_matches(
                                    button.hotkey, event.key.key
                                );
                        }
                    );
                    if (match != panel.commands.end()) {
                        active_command_hover =
                            static_cast<int>(match->grid_slot);
                        SDL_Event activation{};
                        activation.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
                        activation.button.button = SDL_BUTTON_LEFT;
                        activation.button.x = mouse_position.x;
                        activation.button.y = mouse_position.y;
                        SDL_PushEvent(&activation);
                        continue;
                    }
                }
                const SDL_Keymod debug_modifiers = SDL_GetModState();
                if (!event.key.repeat &&
                    event.key.key == SDLK_R &&
                    (debug_modifiers & SDL_KMOD_CTRL) != 0 &&
                    (debug_modifiers & SDL_KMOD_SHIFT) != 0) {
                    GameCommand command =
                        ResignCommand{active_view_player};
                    if (execute(simulation, command)) {
                        replay.record(
                            simulation.tick_number(),
                            std::move(command)
                        );
                        control_group_status =
                            "RESIGNATION SUBMITTED";
                    }
                    continue;
                }
                if (event.key.key == SDLK_F12 &&
                    (debug_modifiers & SDL_KMOD_CTRL) != 0 &&
                    (debug_modifiers & SDL_KMOD_SHIFT) != 0) {
                    computer_debug = !computer_debug;
                    control_group_status = computer_debug
                        ? "AI debug visible"
                        : "AI debug hidden";
                    continue;
                }
                if (const auto group_index =
                        control_group_index(event.key.key)) {
                    ControlGroup& group =
                        control_groups[*group_index];
                    const SDL_Keymod modifiers = SDL_GetModState();
                    const bool assigning =
                        (modifiers &
                         (SDL_KMOD_CTRL | SDL_KMOD_GUI)) != 0;
                    const bool adding =
                        (modifiers & SDL_KMOD_SHIFT) != 0;
                    const bool has_selection =
                        !simulation.selected_units().empty() ||
                        simulation.selected_building().has_value();
                    if (assigning || (adding && has_selection)) {
                        if (!adding) {
                            group.units.clear();
                            group.building.reset();
                        }
                        if (!simulation.selected_units().empty()) {
                            group.building.reset();
                            for (EntityId id :
                                 simulation.selected_units()) {
                                if (std::ranges::find(
                                        group.units,
                                        id
                                    ) == group.units.end()) {
                                    group.units.push_back(id);
                                }
                            }
                        } else if (simulation.selected_building()) {
                            group.units.clear();
                            group.building =
                                simulation.selected_building();
                        }
                        SDL_Log(
                            "Control group %zu assigned",
                            *group_index
                        );
                        std::ostringstream status;
                        status << "Group " << *group_index << ": ";
                        if (!group.units.empty()) {
                            status << group.units.size() << " units";
                        } else if (group.building) {
                            status << "building #" << *group.building;
                        } else {
                            status << "cleared";
                        }
                        control_group_status = status.str();
                        continue;
                    }

                    prune_control_group(
                        group,
                        simulation.units(),
                        simulation.buildings(),
                        active_view_player
                    );
                    const bool group_defined =
                        !group.units.empty() ||
                        group.building.has_value();
                    bool recalled = false;
                    if (!group.units.empty()) {
                        recalled = simulation.select_units(
                            group.units,
                            active_view_player
                        );
                    } else if (group.building) {
                        const auto found = std::ranges::find_if(
                            simulation.buildings(),
                            [&group](const Building& building) {
                                return building.id == *group.building &&
                                    building.owner == active_view_player;
                            }
                        );
                        if (found != simulation.buildings().end()) {
                            recalled = simulation.select_building_at(
                                found->position,
                                active_view_player
                            );
                        } else {
                            group.building.reset();
                        }
                    }
                    if (recalled) {
                        const Uint64 now_ms = SDL_GetTicks();
                        const bool center =
                            last_control_group_recall == *group_index &&
                            now_ms - last_control_group_recall_ms <= 400;
                        last_control_group_recall = *group_index;
                        last_control_group_recall_ms = now_ms;
                        if (center) {
                            if (!group.units.empty()) {
                                const auto found = std::ranges::find(
                                    simulation.units(), group.units.front(),
                                    &Unit::id
                                );
                                if (found != simulation.units().end()) {
                                    center_camera_on(
                                        camera, found->position
                                    );
                                }
                            } else if (group.building) {
                                const auto found = std::ranges::find(
                                    simulation.buildings(),
                                    *group.building, &Building::id
                                );
                                if (found != simulation.buildings().end()) {
                                    center_camera_on(
                                        camera, found->position
                                    );
                                }
                            }
                        }
                        std::ostringstream status;
                        status << "Group " << *group_index
                               << (center ? " centered" : " recalled");
                        control_group_status = status.str();
                        pending_building.reset();
                        pending_attack_move = false;
                        pending_attack_ground = false;
                        pending_patrol = false;
                        pending_guard = false;
                        pending_conversion = false;
                        continue;
                    }
                    if (group_defined) {
                        continue;
                    }
                }
                const SDL_Keymod command_modifiers = SDL_GetModState();
                if (!replaying &&
                    (command_modifiers & SDL_KMOD_CTRL) != 0 &&
                    (command_modifiers &
                     (SDL_KMOD_ALT | SDL_KMOD_GUI)) == 0 &&
                    (event.key.key == SDLK_F1 ||
                     event.key.key == SDLK_F2 ||
                     event.key.key == SDLK_F3 ||
                     event.key.key == SDLK_F4 ||
                     event.key.key == SDLK_F5)) {
                    const FormationKind kind =
                        event.key.key == SDLK_F1
                        ? FormationKind::compact
                        : event.key.key == SDLK_F2
                            ? FormationKind::line
                            : event.key.key == SDLK_F3
                                ? FormationKind::box
                                : event.key.key == SDLK_F4
                                    ? FormationKind::staggered
                                    : FormationKind::flank;
                    GameCommand command =
                        SetFormationKindCommand{active_view_player, kind};
                    if (execute(simulation, command)) {
                        replay.record(
                            simulation.tick_number(),
                            std::move(command)
                        );
                        control_group_status =
                            "Formation " +
                            std::string{formation_name(kind)};
                    }
                    continue;
                }
                if (!replaying &&
                    (command_modifiers & SDL_KMOD_ALT) != 0 &&
                    (command_modifiers & SDL_KMOD_CTRL) != 0 &&
                    (event.key.key == SDLK_A ||
                     event.key.key == SDLK_N ||
                     event.key.key == SDLK_E)) {
                    const Diplomacy relation =
                        event.key.key == SDLK_A
                        ? Diplomacy::ally
                        : event.key.key == SDLK_N
                            ? Diplomacy::neutral
                            : Diplomacy::enemy;
                    GameCommand command = SetDiplomacyCommand{
                        active_view_player,
                        opposing_player(active_view_player),
                        relation
                    };
                    if (execute(simulation, command)) {
                        replay.record(
                            simulation.tick_number(),
                            std::move(command)
                        );
                    }
                    continue;
                }
                if (!replaying &&
                    event.key.key == SDLK_T &&
                    (command_modifiers & SDL_KMOD_ALT) != 0) {
                    const bool selected_market =
                        simulation.selected_building() &&
                        std::ranges::any_of(
                            simulation.buildings(),
                            [&simulation](const Building& building) {
                                return building.id ==
                                           *simulation.selected_building() &&
                                    building.kind ==
                                        BuildingKind::market &&
                                    building.completed();
                            }
                        );
                    if (selected_market) {
                        GameCommand command = QueueUnitCommand{
                            *simulation.selected_building(),
                            UnitKind::trade_cart,
                        };
                        if (execute(simulation, command)) {
                            replay.record(
                                simulation.tick_number(),
                                std::move(command)
                            );
                        }
                    } else {
                        const bool selected_cart =
                            std::ranges::any_of(
                                simulation.selected_units(),
                                [&simulation](EntityId id) {
                                    const auto found =
                                        std::ranges::find_if(
                                            simulation.units(),
                                            [id](const Unit& unit) {
                                                return unit.id == id;
                                            }
                                        );
                                    return found !=
                                               simulation.units().end() &&
                                        (found->kind ==
                                             UnitKind::trade_cart ||
                                         found->kind ==
                                             UnitKind::trade_cog);
                                }
                            );
                        if (selected_cart) {
                            pending_trade_route = true;
                            pending_building.reset();
                            pending_attack_move = false;
                            pending_attack_ground = false;
                            pending_patrol = false;
                            pending_guard = false;
                            pending_conversion = false;
                        }
                    }
                    continue;
                }
                if (!replaying &&
                    event.key.key == SDLK_P &&
                    (command_modifiers &
                     (SDL_KMOD_ALT | SDL_KMOD_CTRL | SDL_KMOD_GUI)) == 0 &&
                    std::ranges::any_of(
                        simulation.selected_units(),
                        [&simulation](EntityId id) {
                            const auto found = std::ranges::find_if(
                                simulation.units(),
                                [id](const Unit& unit) {
                                    return unit.id == id;
                                }
                            );
                            return found != simulation.units().end() &&
                                found->kind == UnitKind::fishing_ship;
                        }
                    )) {
                    set_build_mode(BuildingKind::fish_trap);
                    continue;
                }
                if (!replaying &&
                    event.key.key == SDLK_M &&
                    (command_modifiers & SDL_KMOD_SHIFT) != 0 &&
                    (command_modifiers & SDL_KMOD_ALT) == 0) {
                    set_build_mode(BuildingKind::market);
                    continue;
                }
                const bool selected_market =
                    simulation.selected_building() &&
                    std::ranges::any_of(
                        simulation.buildings(),
                        [&simulation](const Building& building) {
                            return building.id ==
                                       *simulation.selected_building() &&
                                building.kind == BuildingKind::market &&
                                building.completed();
                            }
                    );
                if (!replaying && selected_market &&
                    (command_modifiers & SDL_KMOD_SHIFT) != 0 &&
                    (command_modifiers &
                     (SDL_KMOD_ALT | SDL_KMOD_CTRL | SDL_KMOD_GUI)) == 0 &&
                    (event.key.key == SDLK_1 ||
                     event.key.key == SDLK_2 ||
                     event.key.key == SDLK_3 ||
                     event.key.key == SDLK_4)) {
                    const ResourceKind resource =
                        event.key.key == SDLK_1
                        ? ResourceKind::wood
                        : event.key.key == SDLK_2
                            ? ResourceKind::food
                            : event.key.key == SDLK_3
                                ? ResourceKind::gold
                                : ResourceKind::stone;
                    GameCommand command = TributeResourceCommand{
                        active_view_player,
                        opposing_player(active_view_player),
                        resource,
                        100,
                    };
                    if (execute(simulation, command)) {
                        replay.record(
                            simulation.tick_number(),
                            std::move(command)
                        );
                    }
                    continue;
                }
                if (!replaying && selected_market) {
                    std::optional<Technology> market_technology;
                    if (event.key.key == SDLK_C) {
                        market_technology =
                            (command_modifiers & SDL_KMOD_ALT) != 0
                            ? Technology::cartography
                            : Technology::coinage;
                    } else if (
                        (command_modifiers & SDL_KMOD_ALT) == 0 &&
                        event.key.key == SDLK_B
                    ) {
                        market_technology = Technology::banking;
                    } else if (
                        (command_modifiers & SDL_KMOD_ALT) == 0 &&
                        event.key.key == SDLK_V
                    ) {
                        market_technology = Technology::caravan;
                    } else if (
                        (command_modifiers & SDL_KMOD_ALT) == 0 &&
                        event.key.key == SDLK_G
                    ) {
                        market_technology = Technology::guilds;
                    }
                    if (market_technology &&
                        technology_available_to_player(
                            simulation,
                            active_view_player,
                            *market_technology
                        )) {
                        GameCommand command = ResearchTechnologyCommand{
                            *simulation.selected_building(),
                            *market_technology,
                        };
                        if (execute(simulation, command)) {
                            replay.record(
                                simulation.tick_number(),
                                std::move(command)
                            );
                        }
                        continue;
                    }
                }
                if (!replaying &&
                    selected_market &&
                    (command_modifiers & SDL_KMOD_ALT) != 0 &&
                    (event.key.key == SDLK_F ||
                     event.key.key == SDLK_W ||
                     event.key.key == SDLK_S)) {
                    const MarketResource resource =
                        event.key.key == SDLK_F
                        ? MarketResource::food
                        : event.key.key == SDLK_W
                            ? MarketResource::wood
                            : MarketResource::stone;
                    GameCommand command =
                        (command_modifiers & SDL_KMOD_SHIFT) != 0
                        ? GameCommand{
                            SellResourceCommand{active_view_player, resource}
                        }
                        : GameCommand{
                            BuyResourceCommand{active_view_player, resource}
                        };
                    if (execute(simulation, command)) {
                        replay.record(
                            simulation.tick_number(),
                            std::move(command)
                        );
                    }
                    continue;
                }
                if (!replaying && simulation.selected_building() &&
                    (command_modifiers & SDL_KMOD_ALT) != 0) {
                    const auto selected_defense =
                        std::ranges::find_if(
                            simulation.buildings(),
                            [&simulation](const Building& building) {
                                return building.id ==
                                    *simulation.selected_building();
                            }
                        );
                    std::optional<Technology> defensive_technology;
                    if (selected_defense != simulation.buildings().end()) {
                        if (selected_defense->kind ==
                            BuildingKind::university) {
                            if (event.key.key == SDLK_M) {
                                defensive_technology =
                                    (command_modifiers &
                                     SDL_KMOD_SHIFT) != 0
                                    ? Technology::architecture
                                    : Technology::masonry;
                            } else if (event.key.key == SDLK_B) {
                                defensive_technology =
                                    Technology::ballistics;
                            } else if (event.key.key == SDLK_H) {
                                defensive_technology =
                                    Technology::heated_shot;
                            }
                        } else if (
                            selected_defense->kind ==
                            BuildingKind::town_center &&
                            event.key.key == SDLK_W
                        ) {
                            defensive_technology =
                                (command_modifiers & SDL_KMOD_SHIFT) != 0
                                ? Technology::town_patrol
                                : Technology::town_watch;
                        } else if (
                            selected_defense->kind ==
                            BuildingKind::castle
                        ) {
                            if (event.key.key == SDLK_O) {
                                defensive_technology =
                                    Technology::hoardings;
                            } else if (event.key.key == SDLK_P) {
                                defensive_technology =
                                    Technology::sappers;
                            }
                        }
                    }
                    if (defensive_technology &&
                        technology_available_to_player(
                            simulation,
                            active_view_player,
                            *defensive_technology
                        )) {
                        GameCommand command = ResearchTechnologyCommand{
                            *simulation.selected_building(),
                            *defensive_technology,
                        };
                        if (execute(simulation, command)) {
                            replay.record(
                                simulation.tick_number(),
                                std::move(command)
                            );
                        }
                        continue;
                    }
                }
                const bool selected_dock =
                    simulation.selected_building() &&
                    std::ranges::any_of(
                        simulation.buildings(),
                        [&simulation](const Building& building) {
                            return building.id ==
                                       *simulation.selected_building() &&
                                building.kind == BuildingKind::dock &&
                                building.completed();
                        }
                    );
                if (!replaying && selected_dock &&
                    (command_modifiers &
                     (SDL_KMOD_CTRL | SDL_KMOD_ALT | SDL_KMOD_GUI)) == 0) {
                    std::optional<UnitKind> naval_kind;
                    if (event.key.key == SDLK_F) {
                        naval_kind = UnitKind::fishing_ship;
                    } else if (event.key.key == SDLK_Q) {
                        naval_kind = UnitKind::trade_cog;
                    } else if (event.key.key == SDLK_G) {
                        naval_kind = UnitKind::galley;
                    } else if (event.key.key == SDLK_W) {
                        naval_kind = UnitKind::war_galley;
                    } else if (event.key.key == SDLK_E) {
                        naval_kind = UnitKind::galleon;
                    } else if (event.key.key == SDLK_T) {
                        naval_kind = UnitKind::transport_ship;
                    } else if (event.key.key == SDLK_R) {
                        naval_kind = UnitKind::fire_ship;
                    } else if (event.key.key == SDLK_D) {
                        naval_kind = UnitKind::demolition_ship;
                    } else if (event.key.key == SDLK_C &&
                               (command_modifiers & SDL_KMOD_SHIFT) == 0 &&
                               simulation.has_technology(
                                   active_view_player,
                                   Technology::cannon_galleon
                               )) {
                        naval_kind = UnitKind::cannon_galleon;
                    } else if (
                        event.key.key == SDLK_L &&
                        (command_modifiers & SDL_KMOD_SHIFT) == 0 &&
                        simulation.has_technology(
                            active_view_player, Technology::longboat
                        )
                    ) {
                        naval_kind = UnitKind::longboat;
                    } else if (
                        event.key.key == SDLK_K &&
                        (command_modifiers & SDL_KMOD_SHIFT) == 0 &&
                        simulation.has_technology(
                            active_view_player, Technology::turtle_ship
                        )
                    ) {
                        naval_kind = UnitKind::turtle_ship;
                    }
                    if (naval_kind &&
                        unit_available_to_player(
                            simulation, active_view_player, *naval_kind
                        )) {
                        GameCommand command = QueueUnitCommand{
                            *simulation.selected_building(),
                            *naval_kind,
                        };
                        if (execute(simulation, command)) {
                            replay.record(
                                simulation.tick_number(),
                                std::move(command)
                            );
                        }
                        continue;
                    }
                    std::optional<Technology> dock_technology;
                    if (event.key.key == SDLK_C &&
                        (command_modifiers & SDL_KMOD_SHIFT) != 0) {
                        dock_technology =
                            Technology::elite_cannon_galleon;
                    } else if (event.key.key == SDLK_C &&
                               !simulation.has_technology(
                                   active_view_player,
                                   Technology::cannon_galleon
                               )) {
                        dock_technology = Technology::cannon_galleon;
                    } else if (event.key.key == SDLK_Y) {
                        dock_technology = Technology::careening;
                    } else if (event.key.key == SDLK_U) {
                        dock_technology = Technology::dry_dock;
                    } else if (event.key.key == SDLK_I) {
                        dock_technology = Technology::shipwright;
                    } else if (
                        event.key.key == SDLK_L
                    ) {
                        dock_technology =
                            (command_modifiers & SDL_KMOD_SHIFT) != 0
                            ? Technology::elite_longboat
                            : Technology::longboat;
                    } else if (
                        event.key.key == SDLK_K
                    ) {
                        dock_technology =
                            (command_modifiers & SDL_KMOD_SHIFT) != 0
                            ? Technology::elite_turtle_ship
                            : Technology::turtle_ship;
                    }
                    if (dock_technology &&
                        technology_available_to_player(
                            simulation,
                            active_view_player,
                            *dock_technology
                        )) {
                        GameCommand command = ResearchTechnologyCommand{
                            *simulation.selected_building(),
                            *dock_technology,
                        };
                        if (execute(simulation, command)) {
                            replay.record(
                                simulation.tick_number(),
                                std::move(command)
                            );
                        }
                        continue;
                    }
                }
                const auto economy_building = simulation.selected_building()
                    ? std::ranges::find_if(
                        simulation.buildings(),
                        [&simulation](const Building& building) {
                            return building.id ==
                                *simulation.selected_building();
                        }
                    )
                    : simulation.buildings().end();
                if (!replaying &&
                    economy_building != simulation.buildings().end() &&
                    economy_building->completed() &&
                    (command_modifiers &
                     (SDL_KMOD_CTRL | SDL_KMOD_GUI)) == 0) {
                    std::optional<Technology> economy_technology;
                    const bool alt =
                        (command_modifiers & SDL_KMOD_ALT) != 0;
                    if (economy_building->kind == BuildingKind::mill) {
                        if (alt && event.key.key == SDLK_1)
                            economy_technology = Technology::heavy_plow;
                        if (alt && event.key.key == SDLK_2)
                            economy_technology = Technology::crop_rotation;
                    } else if (
                        economy_building->kind ==
                            BuildingKind::lumber_camp
                    ) {
                        if (alt && event.key.key == SDLK_1)
                            economy_technology = Technology::bow_saw;
                        if (alt && event.key.key == SDLK_2)
                            economy_technology = Technology::two_man_saw;
                    } else if (
                        economy_building->kind ==
                            BuildingKind::mining_camp
                    ) {
                        if (event.key.key == SDLK_G)
                            economy_technology = alt
                                ? Technology::gold_shaft_mining
                                : Technology::gold_mining;
                        if (event.key.key == SDLK_S)
                            economy_technology = alt
                                ? Technology::stone_shaft_mining
                                : Technology::stone_mining;
                    } else if (
                        economy_building->kind ==
                            BuildingKind::town_center &&
                        alt && event.key.key == SDLK_C
                    ) {
                        economy_technology = Technology::hand_cart;
                    }
                    if (economy_technology) {
                        if (technology_available_to_player(
                                simulation, active_view_player,
                                *economy_technology
                            ) &&
                            !simulation.has_technology(
                                active_view_player, *economy_technology
                            )) {
                            GameCommand command =
                                ResearchTechnologyCommand{
                                    *simulation.selected_building(),
                                    *economy_technology,
                                };
                            if (execute(simulation, command)) {
                                replay.record(
                                    simulation.tick_number(),
                                    std::move(command)
                                );
                            }
                        }
                        continue;
                    }
                }
                const bool selected_monastery =
                    simulation.selected_building() &&
                    std::ranges::any_of(
                        simulation.buildings(),
                        [&simulation](const Building& building) {
                            return building.id ==
                                       *simulation.selected_building() &&
                                building.kind ==
                                    BuildingKind::monastery &&
                                building.completed();
                        }
                    );
                if (!replaying && selected_monastery &&
                    (command_modifiers &
                     (SDL_KMOD_CTRL | SDL_KMOD_ALT | SDL_KMOD_GUI)) == 0) {
                    std::optional<UnitKind> train_kind;
                    if (event.key.key == SDLK_M) {
                        train_kind = UnitKind::monk;
                    } else if (event.key.key == SDLK_R) {
                        train_kind = UnitKind::missionary;
                    }
                    if (train_kind) {
                        if (unit_available_to_player(
                                simulation, active_view_player, *train_kind
                            )) {
                            GameCommand command = QueueUnitCommand{
                                *simulation.selected_building(),
                                *train_kind,
                            };
                            if (execute(simulation, command)) {
                                replay.record(
                                    simulation.tick_number(),
                                    std::move(command)
                                );
                            }
                        }
                        continue;
                    }
                    std::optional<Technology> technology;
                    switch (event.key.key) {
                        case SDLK_F1: technology = Technology::sanctity; break;
                        case SDLK_F2: technology = Technology::fervor; break;
                        case SDLK_F3: technology = Technology::redemption; break;
                        case SDLK_F4: technology = Technology::atonement; break;
                        case SDLK_F5: technology = Technology::illumination; break;
                        case SDLK_F6: technology = Technology::block_printing; break;
                        case SDLK_F7: technology = Technology::faith; break;
                        case SDLK_F8: technology = Technology::theocracy; break;
                        case SDLK_F9: technology = Technology::heresy; break;
                        default: break;
                    }
                    if (technology) {
                        if (technology_available_to_player(
                                simulation, active_view_player, *technology
                            ) &&
                            !simulation.has_technology(
                                active_view_player, *technology
                            )) {
                            GameCommand command =
                                ResearchTechnologyCommand{
                                    *simulation.selected_building(),
                                    *technology,
                                };
                            if (execute(simulation, command)) {
                                replay.record(
                                    simulation.tick_number(),
                                    std::move(command)
                                );
                            }
                        }
                        continue;
                    }
                }
                const bool selected_castle =
                    simulation.selected_building() &&
                    std::ranges::any_of(
                        simulation.buildings(),
                        [&simulation](const Building& building) {
                            return building.id ==
                                       *simulation.selected_building() &&
                                building.kind == BuildingKind::castle &&
                                building.completed();
                        }
                    );
                if (!replaying && selected_castle &&
                    (command_modifiers &
                     (SDL_KMOD_CTRL | SDL_KMOD_ALT | SDL_KMOD_GUI)) == 0) {
                    std::optional<Technology> unique_technology;
                    if (event.key.key == SDLK_8) {
                        unique_technology = Technology::yeomen;
                    } else if (event.key.key == SDLK_9) {
                        unique_technology = Technology::bearded_axe;
                    } else if (event.key.key == SDLK_0) {
                        unique_technology = Technology::anarchy;
                    } else if (event.key.key == SDLK_F7) {
                        unique_technology = Technology::crenellations;
                    } else if (event.key.key == SDLK_F6) {
                        unique_technology = Technology::kataparuto;
                    } else if (event.key.key == SDLK_F4) {
                        unique_technology = Technology::rocketry;
                    } else if (event.key.key == SDLK_F3) {
                        unique_technology = Technology::logistica;
                    } else if (event.key.key == SDLK_F2) {
                        unique_technology = Technology::mahouts;
                    } else if (event.key.key == SDLK_F1) {
                        unique_technology = Technology::zealotry;
                    } else if (event.key.key == SDLK_F5) {
                        unique_technology = Technology::artillery;
                    } else if (event.key.key == SDLK_F12) {
                        unique_technology = Technology::drill;
                    } else if (event.key.key == SDLK_EQUALS) {
                        unique_technology = Technology::berserkergang;
                    } else if (event.key.key == SDLK_F10) {
                        unique_technology = Technology::supremacy;
                    } else if (event.key.key == SDLK_F9) {
                        unique_technology = Technology::atheism;
                    } else if (event.key.key == SDLK_F8) {
                        unique_technology = Technology::shinkichon;
                    } else if (event.key.key == SDLK_F11) {
                        unique_technology = Technology::el_dorado;
                    }
                    if (unique_technology) {
                        if (technology_available_to_player(
                                simulation,
                                active_view_player,
                                *unique_technology
                            ) &&
                            !simulation.has_technology(
                                active_view_player,
                                *unique_technology
                            )) {
                            GameCommand command =
                                ResearchTechnologyCommand{
                                    *simulation.selected_building(),
                                    *unique_technology,
                                };
                            if (execute(simulation, command)) {
                                replay.record(
                                    simulation.tick_number(),
                                    std::move(command)
                                );
                            }
                        }
                        continue;
                    }
                    std::optional<UnitKind> unique_unit;
                    std::optional<Technology> base_technology;
                    std::optional<Technology> elite_technology;
                    if (event.key.key == SDLK_R &&
                        unit_available_to_player(
                            simulation, active_view_player, UnitKind::petard
                        )) {
                        GameCommand command = QueueUnitCommand{
                            *simulation.selected_building(),
                            UnitKind::petard,
                        };
                        if (execute(simulation, command)) {
                            replay.record(
                                simulation.tick_number(),
                                std::move(command)
                            );
                        }
                        continue;
                    } else if (event.key.key == SDLK_B) {
                        unique_unit = UnitKind::longbowman;
                        base_technology = Technology::longbowman;
                        elite_technology = Technology::elite_longbowman;
                    } else if (event.key.key == SDLK_X) {
                        unique_unit = UnitKind::throwing_axeman;
                        base_technology = Technology::throwing_axeman;
                        elite_technology =
                            Technology::elite_throwing_axeman;
                    } else if (event.key.key == SDLK_H) {
                        unique_unit = UnitKind::huskarl;
                        base_technology = Technology::huskarl;
                        elite_technology = Technology::elite_huskarl;
                    } else if (event.key.key == SDLK_Q) {
                        unique_unit = UnitKind::teutonic_knight;
                        base_technology = Technology::teutonic_knight;
                        elite_technology =
                            Technology::elite_teutonic_knight;
                    } else if (event.key.key == SDLK_J) {
                        unique_unit = UnitKind::samurai;
                        base_technology = Technology::samurai;
                        elite_technology = Technology::elite_samurai;
                    } else if (event.key.key == SDLK_N) {
                        unique_unit = UnitKind::chu_ko_nu;
                        base_technology = Technology::chu_ko_nu;
                        elite_technology = Technology::elite_chu_ko_nu;
                    } else if (event.key.key == SDLK_C) {
                        unique_unit = UnitKind::cataphract;
                        base_technology = Technology::cataphract;
                        elite_technology = Technology::elite_cataphract;
                    } else if (event.key.key == SDLK_P) {
                        unique_unit = UnitKind::war_elephant;
                        base_technology = Technology::war_elephant;
                        elite_technology = Technology::elite_war_elephant;
                    } else if (event.key.key == SDLK_M) {
                        unique_unit = UnitKind::mameluke;
                        base_technology = Technology::mameluke;
                        elite_technology = Technology::elite_mameluke;
                    } else if (event.key.key == SDLK_Y) {
                        unique_unit = UnitKind::janissary;
                        base_technology = Technology::janissary;
                        elite_technology = Technology::elite_janissary;
                    } else if (event.key.key == SDLK_Z) {
                        unique_unit = UnitKind::berserk;
                        base_technology = Technology::berserk;
                        elite_technology = Technology::elite_berserk;
                    } else if (event.key.key == SDLK_G) {
                        unique_unit = UnitKind::mangudai;
                        base_technology = Technology::mangudai;
                        elite_technology = Technology::elite_mangudai;
                    } else if (event.key.key == SDLK_A) {
                        unique_unit = UnitKind::jaguar_warrior;
                        base_technology = Technology::jaguar_warrior;
                        elite_technology =
                            Technology::elite_jaguar_warrior;
                    } else if (event.key.key == SDLK_U) {
                        unique_unit = UnitKind::plumed_archer;
                        base_technology = Technology::plumed_archer;
                        elite_technology =
                            Technology::elite_plumed_archer;
                    } else if (event.key.key == SDLK_O) {
                        unique_unit = UnitKind::conquistador;
                        base_technology = Technology::conquistador;
                        elite_technology =
                            Technology::elite_conquistador;
                    } else if (event.key.key == SDLK_T) {
                        unique_unit = UnitKind::tarkan;
                        base_technology = Technology::tarkan;
                        elite_technology = Technology::elite_tarkan;
                    } else if (event.key.key == SDLK_W) {
                        unique_unit = UnitKind::woad_raider;
                        base_technology = Technology::woad_raider;
                        elite_technology = Technology::elite_woad_raider;
                    }
                    if (unique_unit && base_technology &&
                        elite_technology &&
                        unit_available_to_player(
                            simulation, active_view_player, *unique_unit
                        ) &&
                        technology_available_to_player(
                            simulation, active_view_player, *base_technology
                        )) {
                        if ((command_modifiers & SDL_KMOD_SHIFT) != 0) {
                            if (!technology_available_to_player(
                                    simulation,
                                    active_view_player,
                                    *elite_technology
                                )) {
                                continue;
                            }
                            GameCommand command =
                                ResearchTechnologyCommand{
                                    *simulation.selected_building(),
                                    *elite_technology,
                                };
                            if (execute(simulation, command)) {
                                replay.record(
                                    simulation.tick_number(),
                                    std::move(command)
                                );
                            }
                        } else if (!simulation.has_technology(
                                       active_view_player,
                                       *base_technology
                                   )) {
                            GameCommand command =
                                ResearchTechnologyCommand{
                                    *simulation.selected_building(),
                                    *base_technology,
                                };
                            if (execute(simulation, command)) {
                                replay.record(
                                    simulation.tick_number(),
                                    std::move(command)
                                );
                            }
                        } else {
                            GameCommand command = QueueUnitCommand{
                                *simulation.selected_building(),
                                *unique_unit,
                            };
                            if (execute(simulation, command)) {
                                replay.record(
                                    simulation.tick_number(),
                                    std::move(command)
                                );
                            }
                        }
                        continue;
                    }
                }
                if (!replaying &&
                    event.key.key == SDLK_BACKSLASH &&
                    (command_modifiers &
                     (SDL_KMOD_CTRL | SDL_KMOD_ALT | SDL_KMOD_GUI)) == 0) {
                    bool handled_trebuchet = false;
                    for (EntityId id : simulation.selected_units()) {
                        const auto found = std::ranges::find_if(
                            simulation.units(),
                            [id](const Unit& unit) {
                                return unit.id == id;
                            }
                        );
                        if (found == simulation.units().end() ||
                            (found->kind != UnitKind::packed_trebuchet &&
                             found->kind != UnitKind::trebuchet)) {
                            continue;
                        }
                        GameCommand command = PackTrebuchetCommand{
                            id,
                            found->kind == UnitKind::trebuchet,
                        };
                        if (execute(simulation, command)) {
                            replay.record(
                                simulation.tick_number(),
                                std::move(command)
                            );
                        }
                        handled_trebuchet = true;
                    }
                    if (handled_trebuchet) {
                        continue;
                    }
                }
                const auto selected_completed_building =
                    [&simulation](BuildingKind kind) {
                        return simulation.selected_building() &&
                            std::ranges::any_of(
                                simulation.buildings(),
                                [&simulation, kind](
                                    const Building& building
                                ) {
                                    return building.id ==
                                               *simulation
                                                    .selected_building() &&
                                        building.kind == kind &&
                                        building.completed();
                                }
                            );
                    };
                if (!replaying &&
                    (command_modifiers &
                     (SDL_KMOD_CTRL | SDL_KMOD_GUI)) == 0) {
                    std::optional<UnitKind> train_kind;
                    std::optional<Technology> upgrade;
                    if (selected_completed_building(
                            BuildingKind::barracks
                        ) &&
                        event.key.key == SDLK_E) {
                        if ((command_modifiers & SDL_KMOD_ALT) != 0) {
                            upgrade = Technology::elite_eagle_warrior;
                        } else {
                            train_kind = UnitKind::eagle_warrior;
                        }
                    } else if (selected_completed_building(
                                   BuildingKind::siege_workshop
                               ) &&
                               event.key.key == SDLK_X) {
                        if ((command_modifiers & SDL_KMOD_ALT) != 0) {
                            upgrade = Technology::heavy_scorpion;
                        } else {
                            train_kind = UnitKind::scorpion;
                        }
                    } else if (selected_completed_building(
                                   BuildingKind::siege_workshop
                               ) &&
                               event.key.key == SDLK_O) {
                        if ((command_modifiers & SDL_KMOD_ALT) != 0) {
                            upgrade =
                                (command_modifiers & SDL_KMOD_SHIFT) != 0
                                ? Technology::siege_onager
                                : Technology::onager;
                        } else {
                            train_kind = UnitKind::mangonel;
                        }
                    } else if (selected_completed_building(
                                   BuildingKind::castle
                               ) &&
                               event.key.key == SDLK_LEFTBRACKET) {
                        train_kind = UnitKind::trebuchet;
                    }
                    if (train_kind &&
                        unit_available_to_player(
                            simulation, active_view_player, *train_kind
                        )) {
                        GameCommand command = QueueUnitCommand{
                            *simulation.selected_building(),
                            *train_kind,
                        };
                        if (execute(simulation, command)) {
                            replay.record(
                                simulation.tick_number(),
                                std::move(command)
                            );
                        }
                        continue;
                    }
                    if (upgrade &&
                        technology_available_to_player(
                            simulation, active_view_player, *upgrade
                        ) &&
                        !simulation.has_technology(
                            active_view_player, *upgrade
                        )) {
                        GameCommand command = ResearchTechnologyCommand{
                            *simulation.selected_building(),
                            *upgrade,
                        };
                        if (execute(simulation, command)) {
                            replay.record(
                                simulation.tick_number(),
                                std::move(command)
                            );
                        }
                        continue;
                    }
                }
                if (!event.key.repeat &&
                    event.key.key == SDLK_F &&
                    (SDL_GetModState() & SDL_KMOD_ALT) != 0) {
                    pending_map_signal = true;
                    control_group_status =
                        "SIGNAL: CLICK AN EXPLORED WORLD TILE";
                    continue;
                }
                switch (event.key.key) {
                    case SDLK_ESCAPE:
                        draft_settings = active_settings;
                        active_options_visible = true;
                        break;
                    case SDLK_F11:
                        if (SDL_SetWindowFullscreen(
                                window,
                                !fullscreen
                            )) {
                            fullscreen = !fullscreen;
                        } else {
                            SDL_Log(
                                "Could not toggle fullscreen: %s",
                                SDL_GetError()
                            );
                        }
                        break;
                    case SDLK_F5:
                        save_game(simulation, save_path);
                        break;
                    case SDLK_PERIOD: {
                        const std::vector<EntityId> idle =
                            simulation.idle_villagers(active_view_player);
                        if (idle.empty()) {
                            control_group_status =
                                "No idle villagers";
                            last_idle_villager.reset();
                            break;
                        }
                        if ((SDL_GetModState() &
                             SDL_KMOD_SHIFT) != 0) {
                            simulation.select_units(
                                idle,
                                active_view_player
                            );
                            control_group_status =
                                "All idle villagers selected";
                        } else {
                            std::size_t index{};
                            if (last_idle_villager) {
                                const auto previous =
                                    std::ranges::find(
                                        idle,
                                        *last_idle_villager
                                    );
                                if (previous != idle.end()) {
                                    index = (
                                        static_cast<std::size_t>(
                                            previous - idle.begin()
                                        ) + 1
                                    ) % idle.size();
                                }
                            }
                            simulation.select_units(
                                {idle[index]},
                                active_view_player
                            );
                            last_idle_villager = idle[index];
                            std::ostringstream status;
                            status << "Idle villager "
                                   << index + 1 << '/'
                                   << idle.size();
                            control_group_status = status.str();
                        }
                        pending_building.reset();
                        pending_attack_move = false;
                        pending_attack_ground = false;
                        pending_patrol = false;
                        pending_guard = false;
                        pending_conversion = false;
                        break;
                    }
                    case SDLK_COMMA: {
                        const std::vector<EntityId> idle =
                            simulation.idle_military(active_view_player);
                        if (idle.empty()) {
                            control_group_status =
                                "No idle military";
                            last_idle_military.reset();
                            break;
                        }
                        if ((SDL_GetModState() &
                             SDL_KMOD_SHIFT) != 0) {
                            simulation.select_units(
                                idle,
                                active_view_player
                            );
                            control_group_status =
                                "All idle military selected";
                        } else {
                            std::size_t index{};
                            if (last_idle_military) {
                                const auto previous =
                                    std::ranges::find(
                                        idle,
                                        *last_idle_military
                                    );
                                if (previous != idle.end()) {
                                    index = (
                                        static_cast<std::size_t>(
                                            previous - idle.begin()
                                        ) + 1
                                    ) % idle.size();
                                }
                            }
                            simulation.select_units(
                                {idle[index]},
                                active_view_player
                            );
                            last_idle_military = idle[index];
                            std::ostringstream status;
                            status << "Idle military "
                                   << index + 1 << '/'
                                   << idle.size();
                            control_group_status = status.str();
                        }
                        pending_building.reset();
                        pending_attack_move = false;
                        pending_attack_ground = false;
                        pending_patrol = false;
                        pending_guard = false;
                        pending_conversion = false;
                        break;
                    }
                    case SDLK_S:
                        if (!replaying) {
                            if (
                                simulation.selected_building() &&
                                (SDL_GetModState() & SDL_KMOD_ALT) != 0
                            ) {
                                GameCommand command =
                                    ResearchTechnologyCommand{
                                        *simulation.selected_building(),
                                        Technology::siege_engineers,
                                    };
                                if (execute(simulation, command)) {
                                    replay.record(
                                        simulation.tick_number(),
                                        std::move(command)
                                    );
                                }
                            } else {
                                const std::vector<EntityId> selected =
                                    simulation.selected_units();
                                for (EntityId unit : selected) {
                                    GameCommand command =
                                        StopUnitCommand{unit};
                                    if (execute(simulation, command)) {
                                        replay.record(
                                            simulation.tick_number(),
                                            std::move(command)
                                        );
                                    }
                                }
                            }
                        }
                        break;
                    case SDLK_L:
                        if (std::filesystem::exists(save_path)) {
                            simulation =
                                load_presentable_game(save_path);
                        }
                        break;
                    case SDLK_BACKSPACE:
                    case SDLK_DELETE:
                        if (!replaying) {
                            const bool delete_selected =
                                event.key.key == SDLK_DELETE ||
                                (SDL_GetModState() & SDL_KMOD_GUI) != 0;
                            if (delete_selected) {
                                const std::vector<EntityId> selected =
                                    simulation.selected_units();
                                for (EntityId unit : selected) {
                                    GameCommand command =
                                        DeleteEntityCommand{unit, false};
                                    if (execute(simulation, command)) {
                                        replay.record(
                                            simulation.tick_number(),
                                            std::move(command)
                                        );
                                    }
                                }
                                if (selected.empty() &&
                                    simulation.selected_building()) {
                                    GameCommand command =
                                        DeleteEntityCommand{
                                            *simulation.selected_building(),
                                            true,
                                        };
                                    if (execute(simulation, command)) {
                                        replay.record(
                                            simulation.tick_number(),
                                            std::move(command)
                                        );
                                    }
                                }
                            } else if (simulation.selected_building()) {
                                GameCommand command =
                                    CancelProductionCommand{
                                        *simulation.selected_building(),
                                    };
                                if (execute(simulation, command)) {
                                    replay.record(
                                        simulation.tick_number(),
                                        std::move(command)
                                    );
                                }
                            }
                        }
                        break;
                    case SDLK_V:
                        if (!replaying && simulation.selected_building()) {
                            GameCommand command =
                                (SDL_GetModState() & SDL_KMOD_ALT) != 0
                                ? GameCommand{
                                    ResearchTechnologyCommand{
                                        *simulation.selected_building(),
                                        Technology::conscription,
                                    }
                                }
                                : GameCommand{
                                    QueueUnitCommand{
                                        *simulation.selected_building(),
                                        UnitKind::villager,
                                    }
                                };
                            if (execute(simulation, command)) {
                                replay.record(
                                    simulation.tick_number(),
                                    std::move(command)
                                );
                            }
                        }
                        break;
                    case SDLK_M:
                        if (!replaying && simulation.selected_building()) {
                            const UnitKind kind =
                                (SDL_GetModState() & SDL_KMOD_ALT) != 0
                                ? UnitKind::monk
                                : UnitKind::militia;
                            GameCommand command = QueueUnitCommand{
                                *simulation.selected_building(),
                                kind,
                            };
                            if (execute(simulation, command)) {
                                replay.record(
                                    simulation.tick_number(),
                                    std::move(command)
                                );
                            }
                        }
                        break;
                    case SDLK_Z:
                        if (!replaying && simulation.selected_building()) {
                            const SDL_Keymod modifiers = SDL_GetModState();
                            GameCommand command =
                                (modifiers & SDL_KMOD_ALT) != 0
                                ? GameCommand{
                                    ResearchTechnologyCommand{
                                        *simulation.selected_building(),
                                        (modifiers & SDL_KMOD_SHIFT) != 0
                                            ? Technology::halberdier
                                            : Technology::pikeman,
                                    }
                                }
                                : GameCommand{
                                    QueueUnitCommand{
                                        *simulation.selected_building(),
                                        UnitKind::spearman,
                                    }
                                };
                            if (execute(simulation, command)) {
                                replay.record(
                                    simulation.tick_number(),
                                    std::move(command)
                                );
                            }
                        }
                        break;
                    case SDLK_K:
                        if (!replaying && simulation.selected_building()) {
                            GameCommand command =
                                (SDL_GetModState() & SDL_KMOD_ALT) != 0
                                    ? GameCommand{
                                        ResearchTechnologyCommand{
                                            *simulation.selected_building(),
                                            (SDL_GetModState() &
                                             SDL_KMOD_SHIFT) != 0
                                                ? Technology::paladin
                                                : Technology::cavalier,
                                        }
                                    }
                                    : GameCommand{
                                        QueueUnitCommand{
                                            *simulation.selected_building(),
                                            UnitKind::knight,
                                        }
                                    };
                            if (execute(simulation, command)) {
                                replay.record(
                                    simulation.tick_number(),
                                    std::move(command)
                                );
                            }
                        }
                        break;
                    case SDLK_B:
                        if (
                            !replaying &&
                            simulation.selected_building() &&
                            (SDL_GetModState() & SDL_KMOD_ALT) != 0
                        ) {
                            const Technology technology =
                                (SDL_GetModState() & SDL_KMOD_SHIFT) != 0
                                ? Technology::husbandry
                                : Technology::bloodlines;
                            GameCommand command = ResearchTechnologyCommand{
                                *simulation.selected_building(),
                                technology,
                            };
                            if (execute(simulation, command)) {
                                replay.record(
                                    simulation.tick_number(),
                                    std::move(command)
                                );
                            }
                        } else {
                            set_build_mode(BuildingKind::barracks);
                        }
                        break;
                    case SDLK_A:
                        if (!simulation.selected_units().empty() &&
                            std::ranges::none_of(
                                simulation.selected_units(),
                                [&simulation](EntityId id) {
                                    const auto found =
                                        std::ranges::find_if(
                                            simulation.units(),
                                            [id](const Unit& unit) {
                                                return unit.id == id;
                                            }
                                        );
                                    return found != simulation.units().end() &&
                                           found->kind ==
                                               UnitKind::villager;
                                }
                            )) {
                            pending_attack_move = true;
                            pending_conversion = false;
                            pending_attack_ground = false;
                            pending_patrol = false;
                            pending_guard = false;
                            pending_conversion = false;
                            pending_building.reset();
                        } else {
                            set_build_mode(BuildingKind::archery_range);
                        }
                        break;
                    case SDLK_D:
                        set_build_mode(BuildingKind::stable);
                        break;
                    case SDLK_J:
                        set_build_mode(BuildingKind::blacksmith);
                        break;
                    case SDLK_U:
                        set_build_mode(BuildingKind::castle);
                        break;
                    case SDLK_I:
                        set_build_mode(BuildingKind::university);
                        break;
                    case SDLK_2:
                        set_build_mode(BuildingKind::siege_workshop);
                        break;
                    case SDLK_1:
                        set_build_mode(BuildingKind::town_center);
                        break;
                    case SDLK_4:
                        set_build_mode(BuildingKind::palisade_wall);
                        break;
                    case SDLK_F9:
                        set_build_mode(BuildingKind::palisade_gate_x);
                        break;
                    case SDLK_F10:
                        set_build_mode(BuildingKind::palisade_gate_y);
                        break;
                    case SDLK_HOME:
                        set_build_mode(BuildingKind::stone_gate_x);
                        break;
                    case SDLK_END:
                        set_build_mode(BuildingKind::stone_gate_y);
                        break;
                    case SDLK_5:
                        set_build_mode(BuildingKind::watch_tower);
                        break;
                    case SDLK_6:
                        set_build_mode(BuildingKind::stone_wall);
                        break;
                    case SDLK_H:
                        if (
                            !replaying &&
                            simulation.selected_building() &&
                            (SDL_GetModState() & SDL_KMOD_ALT) != 0
                        ) {
                            GameCommand command = QueueUnitCommand{
                                *simulation.selected_building(),
                                UnitKind::hand_cannoneer,
                            };
                            if (execute(simulation, command)) {
                                replay.record(
                                    simulation.tick_number(),
                                    std::move(command)
                                );
                            }
                        } else {
                            set_build_mode(BuildingKind::house);
                        }
                        break;
                    case SDLK_F:
                        set_build_mode(BuildingKind::mill);
                        break;
                    case SDLK_T:
                        set_build_mode(BuildingKind::lumber_camp);
                        break;
                    case SDLK_G:
                        if (!simulation.selected_units().empty() &&
                            std::ranges::none_of(
                                simulation.selected_units(),
                                [&simulation](EntityId id) {
                                    const auto found =
                                        std::ranges::find_if(
                                            simulation.units(),
                                            [id](const Unit& unit) {
                                                return unit.id == id;
                                            }
                                        );
                                    return found != simulation.units().end() &&
                                           found->kind ==
                                               UnitKind::villager;
                                }
                            )) {
                            pending_guard = true;
                            pending_conversion = false;
                            pending_attack_move = false;
                            pending_attack_ground = false;
                            pending_patrol = false;
                            pending_building.reset();
                        } else {
                            set_build_mode(BuildingKind::mining_camp);
                        }
                        break;
                    case SDLK_Y:
                        set_build_mode(BuildingKind::farm);
                        break;
                    case SDLK_E:
                        if (!replaying && simulation.selected_building()) {
                            const EntityId building =
                                *simulation.selected_building();
                            GameCommand command =
                                simulation.garrison_count(building) > 0
                                    ? GameCommand{UngarrisonCommand{building}}
                                    : GameCommand{ReseedFarmCommand{building}};
                            if (execute(simulation, command)) {
                                replay.record(
                                    simulation.tick_number(),
                                    std::move(command)
                                );
                            }
                        }
                        break;
                    case SDLK_N:
                        if (!replaying && simulation.selected_building()) {
                            GameCommand command = AdvanceAgeCommand{
                                *simulation.selected_building(),
                            };
                            if (execute(simulation, command)) {
                                replay.record(
                                    simulation.tick_number(),
                                    std::move(command)
                                );
                            }
                        }
                        break;
                    case SDLK_W:
                    case SDLK_X:
                    case SDLK_O:
                    case SDLK_0:
                    case SDLK_9:
                    case SDLK_EQUALS:
                    case SDLK_MINUS:
                    case SDLK_LEFTBRACKET:
                    case SDLK_RIGHTBRACKET:
                    case SDLK_BACKSLASH:
                    case SDLK_SEMICOLON:
                    case SDLK_APOSTROPHE:
                    case SDLK_SLASH:
                    case SDLK_GRAVE:
                    case SDLK_F1:
                    case SDLK_F2:
                    case SDLK_F3:
                    case SDLK_F4:
                    case SDLK_F12:
                        if (event.key.key == SDLK_F12 &&
                            !simulation.selected_building()) {
                            set_build_mode(BuildingKind::wonder);
                            break;
                        }
                        if (
                            !replaying &&
                            event.key.key == SDLK_X &&
                            std::ranges::any_of(
                                simulation.selected_units(),
                                [&simulation](EntityId id) {
                                    const auto found =
                                        std::ranges::find_if(
                                            simulation.units(),
                                            [id](const Unit& unit) {
                                                return unit.id == id;
                                            }
                                        );
                                    return found !=
                                               simulation.units().end() &&
                                        found->kind ==
                                            UnitKind::mangonel;
                                }
                            )
                        ) {
                            pending_attack_ground = true;
                            pending_conversion = false;
                            pending_attack_move = false;
                            pending_patrol = false;
                            pending_guard = false;
                            pending_building.reset();
                            break;
                        }
                        if (!replaying && simulation.selected_building()) {
                            Technology technology = Technology::wheelbarrow;
                            if (event.key.key == SDLK_X) {
                                technology = Technology::fletching;
                            } else if (
                                event.key.key == SDLK_F4 &&
                                (SDL_GetModState() & SDL_KMOD_ALT) != 0
                            ) {
                                technology =
                                    Technology::padded_archer_armor;
                            } else if (
                                event.key.key == SDLK_F3 &&
                                (SDL_GetModState() & SDL_KMOD_ALT) != 0
                            ) {
                                technology =
                                    Technology::plate_barding_armor;
                            } else if (
                                event.key.key == SDLK_F2 &&
                                (SDL_GetModState() & SDL_KMOD_ALT) != 0
                            ) {
                                technology =
                                    Technology::chain_barding_armor;
                            } else if (
                                event.key.key == SDLK_F1 &&
                                (SDL_GetModState() & SDL_KMOD_ALT) != 0
                            ) {
                                technology =
                                    Technology::scale_barding_armor;
                            } else if (event.key.key == SDLK_O) {
                                technology = Technology::forging;
                            } else if (
                                event.key.key == SDLK_7 &&
                                (SDL_GetModState() & SDL_KMOD_ALT) != 0
                            ) {
                                technology = Technology::bombard_tower;
                            } else if (event.key.key == SDLK_0) {
                                technology =
                                    (SDL_GetModState() & SDL_KMOD_ALT) != 0
                                    ? Technology::chemistry
                                    : Technology::murder_holes;
                            } else if (event.key.key == SDLK_9) {
                                technology = Technology::man_at_arms;
                            } else if (event.key.key == SDLK_EQUALS) {
                                technology =
                                    (SDL_GetModState() &
                                     SDL_KMOD_SHIFT) != 0
                                        ? Technology::arbalester
                                        : Technology::crossbowman;
                            } else if (event.key.key == SDLK_MINUS) {
                                technology = Technology::pikeman;
                            } else if (event.key.key == SDLK_LEFTBRACKET) {
                                const SDL_Keymod modifiers =
                                    SDL_GetModState();
                                technology =
                                    (modifiers & SDL_KMOD_ALT) != 0 &&
                                    (modifiers & SDL_KMOD_SHIFT) != 0
                                        ? Technology::champion
                                        : (modifiers & SDL_KMOD_SHIFT) != 0
                                            ? Technology::
                                                two_handed_swordsman
                                            : Technology::long_swordsman;
                            } else if (
                                event.key.key == SDLK_RIGHTBRACKET
                            ) {
                                technology = Technology::loom;
                            } else if (
                                event.key.key == SDLK_BACKSLASH
                            ) {
                                technology = Technology::double_bit_axe;
                            } else if (
                                event.key.key == SDLK_SEMICOLON
                            ) {
                                technology = Technology::horse_collar;
                            } else if (
                                event.key.key == SDLK_APOSTROPHE
                            ) {
                                technology = Technology::fortified_wall;
                            } else if (
                                event.key.key == SDLK_SLASH
                            ) {
                                technology = Technology::guard_tower;
                            } else if (
                                event.key.key == SDLK_GRAVE
                            ) {
                                technology = Technology::keep;
                            } else if (
                                event.key.key == SDLK_F1
                            ) {
                                technology = Technology::bodkin_arrow;
                            } else if (
                                event.key.key == SDLK_F2
                            ) {
                                technology = Technology::bracer;
                            } else if (
                                event.key.key == SDLK_F3
                            ) {
                                technology = Technology::iron_casting;
                            } else if (
                                event.key.key == SDLK_F4
                            ) {
                                technology = Technology::blast_furnace;
                            } else if (
                                event.key.key == SDLK_F12
                            ) {
                                const SDL_Keymod modifiers =
                                    SDL_GetModState();
                                if ((modifiers & SDL_KMOD_ALT) != 0 &&
                                    (modifiers & SDL_KMOD_SHIFT) != 0) {
                                    technology =
                                        Technology::ring_archer_armor;
                                } else if (
                                    (modifiers & SDL_KMOD_ALT) != 0
                                ) {
                                    technology =
                                        Technology::leather_archer_armor;
                                } else if ((modifiers &
                                     (SDL_KMOD_CTRL | SDL_KMOD_GUI)) != 0) {
                                    technology =
                                        Technology::plate_mail_armor;
                                } else if (
                                    (modifiers & SDL_KMOD_SHIFT) != 0
                                ) {
                                    technology =
                                        Technology::chain_mail_armor;
                                } else {
                                    technology =
                                        Technology::scale_mail_armor;
                                }
                            }
                            GameCommand command = ResearchTechnologyCommand{
                                *simulation.selected_building(),
                                technology,
                            };
                            if (execute(simulation, command)) {
                                replay.record(
                                    simulation.tick_number(),
                                    std::move(command)
                                );
                            }
                        }
                        break;
                    case SDLK_C:
                        if (!replaying &&
                            (SDL_GetModState() & SDL_KMOD_ALT) != 0 &&
                            std::ranges::any_of(
                                simulation.selected_units(),
                                [&simulation](EntityId id) {
                                    const auto found =
                                        std::ranges::find_if(
                                            simulation.units(),
                                            [id](const Unit& unit) {
                                                return unit.id == id;
                                            }
                                        );
                                    return found !=
                                               simulation.units().end() &&
                                        found->kind == UnitKind::monk;
                                }
                            )) {
                            pending_conversion = true;
                            pending_attack_move = false;
                            pending_attack_ground = false;
                            pending_patrol = false;
                            pending_guard = false;
                            pending_building.reset();
                        } else if (!replaying &&
                            !simulation.selected_units().empty()) {
                            for (EntityId id :
                                 simulation.selected_units()) {
                                const auto found =
                                    std::ranges::find_if(
                                        simulation.units(),
                                        [id](const Unit& unit) {
                                            return unit.id == id;
                                        }
                                    );
                                if (found == simulation.units().end() ||
                                    found->kind == UnitKind::villager) {
                                    continue;
                                }
                                const UnitStance next =
                                    found->stance ==
                                            UnitStance::aggressive
                                        ? UnitStance::defensive
                                        : found->stance ==
                                              UnitStance::defensive
                                        ? UnitStance::stand_ground
                                        : found->stance ==
                                              UnitStance::stand_ground
                                        ? UnitStance::passive
                                        : UnitStance::aggressive;
                                GameCommand command = SetStanceCommand{
                                    id,
                                    next,
                                };
                                if (execute(simulation, command)) {
                                    replay.record(
                                        simulation.tick_number(),
                                        std::move(command)
                                    );
                                }
                            }
                        } else if (
                            !replaying &&
                            simulation.selected_building()
                        ) {
                            const bool stable = std::ranges::any_of(
                                simulation.buildings(),
                                [&simulation](const Building& building) {
                                    return building.id ==
                                               *simulation.selected_building() &&
                                        building.kind ==
                                               BuildingKind::stable;
                                }
                            );
                            GameCommand command =
                                stable &&
                                (SDL_GetModState() & SDL_KMOD_ALT) != 0
                                ? GameCommand{
                                    ResearchTechnologyCommand{
                                        *simulation.selected_building(),
                                        Technology::heavy_camel,
                                    }
                                }
                                : GameCommand{
                                    QueueUnitCommand{
                                        *simulation.selected_building(),
                                        stable
                                            ? UnitKind::camel_rider
                                            : UnitKind::archer,
                                    }
                                };
                            if (execute(simulation, command)) {
                                replay.record(
                                    simulation.tick_number(),
                                    std::move(command)
                                );
                            }
                        }
                        break;
                    case SDLK_7:
                        if ((SDL_GetModState() & SDL_KMOD_ALT) == 0 &&
                            !simulation.selected_building()) {
                            set_build_mode(BuildingKind::bombard_tower);
                        } else if (!replaying &&
                                   simulation.selected_building()) {
                            GameCommand command =
                                (SDL_GetModState() & SDL_KMOD_ALT) != 0
                                ? GameCommand{
                                    ResearchTechnologyCommand{
                                        *simulation.selected_building(),
                                        Technology::elite_skirmisher,
                                    }
                                }
                                : GameCommand{
                                    QueueUnitCommand{
                                        *simulation.selected_building(),
                                        UnitKind::skirmisher,
                                    }
                                };
                            if (execute(simulation, command)) {
                                replay.record(
                                    simulation.tick_number(),
                                    std::move(command)
                                );
                            }
                        }
                        break;
                    case SDLK_8:
                        if (!replaying && simulation.selected_building()) {
                            GameCommand command = QueueUnitCommand{
                                *simulation.selected_building(),
                                (SDL_GetModState() & SDL_KMOD_ALT) != 0
                                    ? UnitKind::bombard_cannon
                                    : UnitKind::mangonel,
                            };
                            if (execute(simulation, command)) {
                                replay.record(
                                    simulation.tick_number(),
                                    std::move(command)
                                );
                            }
                        }
                        break;
                    case SDLK_Q:
                        if (!replaying && simulation.selected_building()) {
                            GameCommand command =
                                (SDL_GetModState() & SDL_KMOD_ALT) != 0
                                ? GameCommand{
                                    ResearchTechnologyCommand{
                                        *simulation.selected_building(),
                                        (SDL_GetModState() &
                                         SDL_KMOD_SHIFT) != 0
                                            ? Technology::hussar
                                            : Technology::light_cavalry,
                                    }
                                }
                                : GameCommand{
                                    QueueUnitCommand{
                                        *simulation.selected_building(),
                                        UnitKind::scout_cavalry,
                                    }
                                };
                            if (execute(simulation, command)) {
                                replay.record(
                                    simulation.tick_number(),
                                    std::move(command)
                                );
                            }
                        }
                        break;
                    case SDLK_3:
                        if (!replaying &&
                            !simulation.selected_building()) {
                            set_build_mode(BuildingKind::outpost);
                        } else if (
                            !replaying && simulation.selected_building()
                        ) {
                            const SDL_Keymod modifiers = SDL_GetModState();
                            GameCommand command =
                                (modifiers & SDL_KMOD_ALT) != 0
                                ? GameCommand{
                                    ResearchTechnologyCommand{
                                        *simulation.selected_building(),
                                        (modifiers & SDL_KMOD_SHIFT) != 0
                                            ? Technology::siege_ram
                                            : Technology::capped_ram,
                                    }
                                }
                                : GameCommand{
                                    QueueUnitCommand{
                                        *simulation.selected_building(),
                                        UnitKind::battering_ram,
                                    }
                                };
                            if (execute(simulation, command)) {
                                replay.record(
                                    simulation.tick_number(),
                                    std::move(command)
                                );
                            }
                        }
                        break;
                    case SDLK_SPACE:
                        paused = !paused;
                        break;
                    case SDLK_R:
                        simulation = new_game();
                        center_camera_on(
                            camera,
                            {active_map_tiles_x() / 2, active_map_tiles_y() / 2}
                        );
                        computer = ComputerPlayer(Player::red);
                        replay = Replay{};
                        replaying = false;
                        control_groups = {};
                        control_group_status.clear();
                        last_idle_villager.reset();
                        last_idle_military.reset();
                        pending_building.reset();
                        pending_attack_move = false;
                        pending_attack_ground = false;
                        pending_patrol = false;
                        pending_guard = false;
                        pending_conversion = false;
                        paused = false;
                        break;
                    case SDLK_P:
                        if (!replaying &&
                            (SDL_GetModState() & SDL_KMOD_SHIFT) != 0 &&
                            std::ranges::any_of(
                                simulation.selected_units(),
                                [&simulation](EntityId id) {
                                    const auto found =
                                        std::ranges::find_if(
                                            simulation.units(),
                                            [id](const Unit& unit) {
                                                return unit.id == id;
                                            }
                                        );
                                    return found !=
                                               simulation.units().end() &&
                                        found->kind == UnitKind::villager;
                                }
                            )) {
                            set_build_mode(BuildingKind::monastery);
                        } else if (!replaying &&
                            !simulation.selected_units().empty()) {
                            pending_patrol = true;
                            pending_attack_move = false;
                            pending_attack_ground = false;
                            pending_guard = false;
                            pending_conversion = false;
                            pending_building.reset();
                        }
                        break;
                    case SDLK_F8:
                        simulation = new_game();
                        center_camera_on(
                            camera,
                            {active_map_tiles_x() / 2, active_map_tiles_y() / 2}
                        );
                        computer = ComputerPlayer(Player::red);
                        replay.reset_playback();
                        replaying = true;
                        control_groups = {};
                        control_group_status.clear();
                        last_idle_villager.reset();
                        last_idle_military.reset();
                        pending_building.reset();
                        pending_attack_move = false;
                        pending_attack_ground = false;
                        pending_patrol = false;
                        pending_guard = false;
                        pending_conversion = false;
                        paused = false;
                        break;
                    case SDLK_F6:
                        save_replay(replay, replay_path);
                        break;
                    case SDLK_F7:
                        replay = load_replay(replay_path);
                        simulation = new_game();
                        center_camera_on(
                            camera,
                            {active_map_tiles_x() / 2, active_map_tiles_y() / 2}
                        );
                        computer = ComputerPlayer(Player::red);
                        replay.reset_playback();
                        replaying = true;
                        control_groups = {};
                        control_group_status.clear();
                        last_idle_villager.reset();
                        last_idle_military.reset();
                        pending_building.reset();
                        pending_attack_move = false;
                        pending_attack_ground = false;
                        pending_patrol = false;
                        pending_guard = false;
                        pending_conversion = false;
                        paused = false;
                        break;
                    default:
                        break;
                }
            }
        }
        if (!sheep_click_proof_logged && sheep_click_proof_sheep) {
            bool proved = simulation.selected_unit() ==
                sheep_click_proof_sheep;
            if (sheep_click_proof_gather &&
                sheep_click_proof_villager) {
                const auto villager = std::ranges::find(
                    simulation.units(),
                    *sheep_click_proof_villager,
                    &Unit::id
                );
                const auto sheep = std::ranges::find(
                    simulation.units(),
                    *sheep_click_proof_sheep,
                    &Unit::id
                );
                proved =
                    villager != simulation.units().end() &&
                    sheep != simulation.units().end() &&
                    villager->resource_unit_id ==
                        *sheep_click_proof_sheep &&
                    sheep->owner == active_view_player;
            }
            if (proved) {
                SDL_Log(
                    "sheep click proof passed: %s",
                    sheep_click_proof_gather ? "gather" : "select"
                );
                sheep_click_proof_logged = true;
            }
        }

        if (gameplay_benchmark) {
            const int pan_span =
                std::max(1, simulation.map().width() - 40);
            const int pan_x =
                20 + static_cast<int>(benchmark_frame % pan_span);
            center_camera_on(
                camera,
                {pan_x, simulation.map().height() / 2}
            );
        }
        const auto now = std::chrono::steady_clock::now();
        const float camera_step =
            8.0F * static_cast<float>(active_settings.scroll_speed) /
            100.0F / camera.zoom;
        const bool* keys = SDL_GetKeyboardState(nullptr);
        const bool pointer_in_world =
            mouse_position.y >= 0.0F &&
            mouse_position.y < static_cast<float>(view_pixel_height);
        if (keys[SDL_SCANCODE_LEFT] ||
            (active_settings.edge_scroll &&
             pointer_in_world && mouse_position.x <= 5.0F)) {
            camera.x -= camera_step;
        }
        if (keys[SDL_SCANCODE_RIGHT] ||
            (active_settings.edge_scroll && pointer_in_world &&
             mouse_position.x >=
                 static_cast<float>(view_pixel_width) - 5.0F)) {
            camera.x += camera_step;
        }
        if (keys[SDL_SCANCODE_UP] ||
            (active_settings.edge_scroll &&
             pointer_in_world && mouse_position.y <= 5.0F)) {
            camera.y -= camera_step;
        }
        if (keys[SDL_SCANCODE_DOWN] ||
            (active_settings.edge_scroll && pointer_in_world &&
             mouse_position.y >=
                 static_cast<float>(view_pixel_height) - 5.0F)) {
            camera.y += camera_step;
        }
        clamp_camera(camera);
        if (multiplayer_runtime) {
            multiplayer_runtime->poll_transport(simulation);
        }
        const bool campaign_modal =
            campaign_presentation &&
            campaign_presentation->visible &&
            campaign_presentation->screen !=
                CampaignPresentation::Screen::status;
        const bool simulation_active =
            !paused && !campaign_modal && !scenario_editor &&
            active_frontend_screen == FrontendScreen::hidden &&
            (!multiplayer_runtime || !multiplayer_runtime->paused());
        const FrameDuration frame_elapsed = now - last_frame_time;
        last_frame_time = now;
        const std::optional<int> multiplayer_cadence =
            multiplayer_runtime
            ? std::optional<int>{
                  multiplayer_runtime->effective_tick_cadence_ms()
              }
            : multiplayer_presentation
                ? std::optional<int>{200}
                : std::nullopt;
        const std::chrono::milliseconds tick_duration =
            authoritative_tick_duration(
                active_settings.game_speed,
                multiplayer_cadence
            );
        if (simulation_active) {
            simulation_time.add(frame_elapsed);
            presentation_time += frame_elapsed;
        } else {
            simulation_time.reset();
        }
        while (
            simulation_active &&
            simulation_time.step_due(tick_duration)
        ) {
            if (multiplayer_runtime) {
                multiplayer_runtime->pump(simulation);
            } else if (multiplayer_presentation) {
                multiplayer_presentation->waiting_for_turn =
                    !multiplayer_presentation->session.advance(simulation);
                if (multiplayer_presentation->waiting_for_turn) {
                    multiplayer_presentation->session.elapse();
                }
            } else if (replaying) {
                replay.apply_current_tick(simulation);
                if (replay.playback_finished()) {
                    replaying = false;
                }
            } else {
                simulation.update();
                computer.update(simulation);
            }
            simulation_time.consume_step(tick_duration);
        }
        if (multiplayer_runtime && !multiplayer_state_written &&
            multiplayer_exit_tick > 0 &&
            multiplayer_runtime->current_tick() >= multiplayer_exit_tick &&
            (SDL_getenv("AOE_MULTIPLAYER_SCRIPT_CHECKPOINT") == nullptr ||
             (multiplayer_runtime->save_barrier() != nullptr &&
              (multiplayer_runtime->save_barrier()->status() ==
                   SaveBarrierStatus::matched ||
               multiplayer_runtime->save_barrier()->status() ==
                   SaveBarrierStatus::hash_mismatch)))) {
            if (!multiplayer_state_path.empty()) {
                std::ofstream state(multiplayer_state_path);
                state << "tick "
                      << multiplayer_runtime->current_tick() << '\n'
                      << "hash "
                      << std::quoted(deterministic_state_hash(simulation))
                      << '\n'
                      << "status "
                      << static_cast<int>(
                             multiplayer_runtime->status()
                         ) << '\n'
                      << "input_delay "
                      << multiplayer_runtime->session_config()
                             .input_delay_ticks << '\n'
                      << "latency_band "
                      << latency_band_name(
                             multiplayer_runtime->network_metrics()
                                 .latency_band) << '\n'
                      << "paused "
                      << (multiplayer_runtime->paused() ? 1 : 0) << '\n'
                      << "speed "
                      << game_speed_name(
                             multiplayer_runtime->game_speed()) << '\n'
                      << "cadence "
                      << multiplayer_runtime
                             ->effective_tick_cadence_ms() << '\n'
                      << "pause_frozen "
                      << (multiplayer_pause_tick_frozen ? 1 : 0) << '\n';
                if (const LockstepSaveBarrier* barrier =
                        multiplayer_runtime->save_barrier()) {
                    state << "checkpoint "
                          << checkpoint_status_name(barrier->status())
                          << ' ' << barrier->target_tick() << '\n';
                }
                for (const LockstepChatMessage& message :
                     multiplayer_runtime->chat_log()) {
                    state << "chat " << message.sequence << ' '
                          << name(message.sender) << ' '
                          << (message.audience == ChatAudience::all
                                  ? "all" : "allies")
                          << ' ' << std::quoted(message.text) << '\n';
                }
                for (const LockstepMapSignal& signal :
                     multiplayer_runtime->signal_log()) {
                    state << "signal " << signal.sequence << ' '
                          << name(signal.sender) << ' '
                          << (signal.audience == ChatAudience::all
                                  ? "all" : "allies")
                          << ' ' << signal.tile.x << ' '
                          << signal.tile.y << '\n';
                }
                if (!state) {
                    throw std::runtime_error(
                        "could not write multiplayer state proof"
                    );
                }
            }
            multiplayer_state_written = true;
            SDL_Event quit{};
            quit.type = SDL_EVENT_QUIT;
            SDL_PushEvent(&quit);
        }
        if (multiplayer_runtime) {
            multiplayer_presentation->live_status =
                multiplayer_runtime->status();
            const LockstepStatus status =
                multiplayer_presentation->live_status;
            multiplayer_presentation->transport_connected =
                multiplayer_runtime->connected() &&
                status != LockstepStatus::disconnected &&
                status != LockstepStatus::timed_out &&
                status != LockstepStatus::protocol_mismatch &&
                status != LockstepStatus::build_mismatch &&
                status != LockstepStatus::schema_mismatch &&
                status != LockstepStatus::scenario_mismatch &&
                status != LockstepStatus::content_mismatch &&
                status != LockstepStatus::settings_mismatch &&
                status != LockstepStatus::roster_mismatch;
            multiplayer_presentation->config =
                multiplayer_runtime->session_config();
            multiplayer_presentation->blue_ready =
                multiplayer_runtime->peer_ready(Player::blue);
            multiplayer_presentation->red_ready =
                multiplayer_runtime->peer_ready(Player::red);
            multiplayer_presentation->live_tick =
                multiplayer_runtime->current_tick();
            multiplayer_presentation->waiting_for_turn =
                multiplayer_runtime->waiting_for_turn();
            multiplayer_presentation->chat_log =
                multiplayer_runtime->chat_log();
            for (const LockstepChatMessage& message :
                 multiplayer_runtime->chat_log()) {
                if (message.sequence <= active_last_taunt_sequence) {
                    continue;
                }
                if (audio) {
                    if (const auto number = taunt_number(message.text)) {
                        audio->play_taunt(*number);
                    }
                }
                active_last_taunt_sequence = message.sequence;
            }
            multiplayer_presentation->signal_log =
                multiplayer_runtime->signal_log();
            for (const LockstepMapSignal& signal :
                 multiplayer_runtime->signal_log()) {
                if (signal.sequence > active_last_signal_sequence) {
                    active_map_signals.push_back({
                        signal, SDL_GetTicks()
                    });
                    active_last_signal_sequence = signal.sequence;
                }
            }
            multiplayer_presentation->network_metrics =
                multiplayer_runtime->network_metrics();
            multiplayer_presentation->network_paused =
                multiplayer_runtime->paused();
            multiplayer_presentation->game_speed =
                multiplayer_runtime->game_speed();
            multiplayer_presentation->effective_cadence_ms =
                multiplayer_runtime->effective_tick_cadence_ms();
            if (!multiplayer_presentation->hosting) {
                multiplayer_presentation->control_feedback =
                    multiplayer_runtime->paused()
                    ? std::string{
                          "HOST PROPOSAL ACKNOWLEDGED: PAUSED"}
                    : "COMMITTED " +
                          std::string{game_speed_name(
                              multiplayer_runtime->game_speed())};
            }
            if (const LockstepSaveBarrier* barrier =
                    multiplayer_runtime->save_barrier()) {
                multiplayer_presentation->checkpoint_status =
                    barrier->status();
                multiplayer_presentation->checkpoint_tick =
                    barrier->target_tick();
            }
            if (!multiplayer_script_chat_sent &&
                !multiplayer_script_chat.empty() &&
                (status == LockstepStatus::ready ||
                 status == LockstepStatus::running)) {
                bool accepted = true;
                std::size_t begin = 0;
                while (begin <= multiplayer_script_chat.size()) {
                    const std::size_t end =
                        multiplayer_script_chat.find('|', begin);
                    std::string item = multiplayer_script_chat.substr(
                        begin,
                        end == std::string::npos
                            ? std::string::npos : end - begin
                    );
                    ChatAudience audience = ChatAudience::all;
                    if (item.starts_with("allies:")) {
                        audience = ChatAudience::allies;
                        item.erase(0, 7);
                    } else if (item.starts_with("all:")) {
                        item.erase(0, 4);
                    }
                    accepted =
                        multiplayer_runtime->send_chat(
                            std::move(item), audience
                        ) && accepted;
                    if (end == std::string::npos) break;
                    begin = end + 1;
                }
                multiplayer_script_chat_sent = accepted;
            }
            if (!multiplayer_script_signal_sent &&
                !multiplayer_script_signal.empty() &&
                (status == LockstepStatus::ready ||
                 status == LockstepStatus::running)) {
                bool accepted = true;
                std::istringstream input{multiplayer_script_signal};
                std::string item;
                while (std::getline(input, item, '|')) {
                    const std::size_t comma = item.find(',');
                    if (comma == std::string::npos) {
                        accepted = false;
                        break;
                    }
                    const TilePosition tile{
                        std::stoi(item.substr(0, comma)),
                        std::stoi(item.substr(comma + 1)),
                    };
                    accepted =
                        multiplayer_runtime->send_signal(
                            tile, ChatAudience::allies
                        ) && accepted;
                }
                multiplayer_script_signal_sent = accepted;
            }
            if (multiplayer_script_control &&
                multiplayer_presentation->hosting &&
                status == LockstepStatus::running) {
                if (multiplayer_control_stage == 0 &&
                    multiplayer_runtime->propose_speed(
                        GameSpeed::fast,
                        multiplayer_runtime->current_tick() + 2)) {
                    multiplayer_control_stage = 1;
                    multiplayer_presentation->control_feedback =
                        "FAST PROPOSED; WAITING PEER ACK";
                } else if (
                    multiplayer_control_stage == 1 &&
                    multiplayer_runtime->game_speed() ==
                        GameSpeed::fast &&
                    multiplayer_runtime->propose_pause(
                        true,
                        multiplayer_runtime->current_tick() + 2)) {
                    multiplayer_control_stage = 2;
                    multiplayer_presentation->control_feedback =
                        "PAUSE PROPOSED; WAITING PEER ACK";
                } else if (
                    multiplayer_control_stage == 2 &&
                    multiplayer_runtime->paused()) {
                    if (multiplayer_paused_frames == 0) {
                        multiplayer_pause_tick =
                            multiplayer_runtime->current_tick();
                    }
                    multiplayer_pause_tick_frozen =
                        multiplayer_pause_tick_frozen ||
                        (multiplayer_paused_frames >= 20 &&
                         multiplayer_runtime->current_tick() ==
                             multiplayer_pause_tick);
                    ++multiplayer_paused_frames;
                    if (multiplayer_paused_frames >= 24 &&
                        multiplayer_runtime->propose_pause(
                            false,
                            multiplayer_runtime->current_tick())) {
                        multiplayer_control_stage = 3;
                        multiplayer_presentation->control_feedback =
                            "RESUME PROPOSED; WAITING PEER ACK";
                    }
                } else if (
                    multiplayer_control_stage == 3 &&
                    !multiplayer_runtime->paused()) {
                    multiplayer_control_stage = 4;
                    multiplayer_presentation->control_feedback =
                        "PEER ACK COMMITTED: RUNNING FAST";
                }
            }
            if (multiplayer_presentation->hosting &&
                multiplayer_script_checkpoint_tick > 0 &&
                multiplayer_presentation->checkpoint_status ==
                    SaveBarrierStatus::idle &&
                status == LockstepStatus::running &&
                (!multiplayer_script_control ||
                 multiplayer_control_stage == 4)) {
                const std::uint64_t target = std::max(
                    multiplayer_script_checkpoint_tick,
                    multiplayer_runtime->current_tick() + 2
                );
                if (multiplayer_runtime->request_save_barrier(target)) {
                    multiplayer_presentation->checkpoint_feedback =
                        "SCRIPTED BARRIER REQUESTED";
                }
                multiplayer_script_checkpoint_tick = 0;
            }
            if (multiplayer_presentation->checkpoint_status ==
                    SaveBarrierStatus::hash_mismatch) {
                multiplayer_presentation->checkpoint_feedback =
                    "CHECKPOINT FAILED: STATE HASH MISMATCH";
            } else if (
                multiplayer_presentation->checkpoint_status ==
                    SaveBarrierStatus::matched &&
                multiplayer_presentation->hosting &&
                !multiplayer_checkpoint_written) {
                try {
                    const LockstepSaveBarrier* barrier =
                        multiplayer_runtime->save_barrier();
                    write_multiplayer_checkpoint_atomic(
                        simulation,
                        multiplayer_runtime->session_config(),
                        *barrier,
                        multiplayer_checkpoint_save,
                        multiplayer_checkpoint_envelope
                    );
                    const ResumedMultiplayerCheckpoint verified =
                        load_multiplayer_checkpoint(
                            multiplayer_checkpoint_save,
                            multiplayer_checkpoint_envelope,
                            multiplayer_runtime->session_config()
                        );
                    multiplayer_presentation->checkpoint_feedback =
                        "SAVE" +
                        std::to_string(reconstruction_save_version) +
                        " VERIFIED: " +
                        multiplayer_checkpoint_save.string();
                    multiplayer_checkpoint_written =
                        verified.envelope.barrier_tick ==
                        barrier->target_tick();
                } catch (const std::exception& error) {
                    multiplayer_presentation->checkpoint_feedback =
                        std::string{"CHECKPOINT WRITE FAILED: "} +
                        error.what();
                }
            } else if (
                multiplayer_presentation->checkpoint_status ==
                    SaveBarrierStatus::matched &&
                !multiplayer_presentation->hosting) {
                multiplayer_presentation->checkpoint_feedback =
                    "CHECKPOINT MATCHED; HOST SAVED";
            }
        }
        if (audio) {
            audio->set_paused(
                paused ||
                (multiplayer_runtime && multiplayer_runtime->paused())
            );
            AudioMusicContext music_context =
                AudioMusicContext::gameplay;
            if (active_frontend_screen != FrontendScreen::hidden) {
                music_context = AudioMusicContext::menu;
            } else if (
                simulation.outcome() != MatchOutcome::ongoing) {
                const bool local_victory =
                    simulation.outcome() == MatchOutcome::allied_victory ||
                    (active_view_player == Player::blue &&
                     simulation.outcome() == MatchOutcome::blue_victory) ||
                    (active_view_player == Player::red &&
                     simulation.outcome() == MatchOutcome::red_victory);
                music_context = local_victory
                    ? AudioMusicContext::victory
                    : AudioMusicContext::defeat;
            } else if (campaign_modal) {
                music_context = AudioMusicContext::menu;
            } else if (
                simulation.victory_countdown(Player::blue) > 0 ||
                simulation.victory_countdown(Player::red) > 0) {
                music_context = AudioMusicContext::countdown;
            }
            audio->set_music_context(music_context, true);
            if (campaign_presentation &&
                campaign_presentation->visible &&
                campaign_presentation->screen ==
                    CampaignPresentation::Screen::briefing &&
                !campaign_presentation->narration_started &&
                (!campaign_presentation->optional_narration_path.empty() ||
                 !campaign_presentation->scenario
                      .briefing_audio.empty())) {
                const std::filesystem::path narration =
                    !campaign_presentation->optional_narration_path.empty()
                    ? campaign_presentation->optional_narration_path
                    : campaign_presentation->scenario.briefing_audio;
                audio->play_narration(narration);
                campaign_presentation->narration_started = true;
            }
            if (campaign_presentation &&
                campaign_presentation->visible &&
                campaign_presentation->screen ==
                    CampaignPresentation::Screen::debrief &&
                !campaign_presentation->debrief_narration_started &&
                !campaign_presentation->scenario.debrief_audio.empty()) {
                audio->play_narration(
                    campaign_presentation->scenario.debrief_audio
                );
                campaign_presentation->debrief_narration_started = true;
            }

            const float projected_x =
                (static_cast<float>(view_pixel_width) * 0.5F /
                     camera.zoom +
                 camera.x - static_cast<float>(map_origin_x())) /
                half_tile_width;
            const float projected_y =
                (static_cast<float>(view_pixel_height) * 0.5F /
                     camera.zoom +
                 camera.y - static_cast<float>(map_origin_y)) /
                half_tile_height;
            const TilePosition listener_tile{
                std::clamp(
                    static_cast<int>(std::floor(
                        (projected_y + projected_x) / 2.0F
                    )),
                    0,
                    simulation.map().width() - 1
                ),
                std::clamp(
                    static_cast<int>(std::floor(
                        (projected_y - projected_x) / 2.0F
                    )),
                    0,
                    simulation.map().height() - 1
                ),
            };
            const std::uint64_t ambience_variation =
                static_cast<std::uint64_t>(listener_tile.x) *
                    0x9E3779B185EBCA87ULL ^
                static_cast<std::uint64_t>(listener_tile.y);
            active_audio_listener_tile = listener_tile;
            audio->set_terrain_ambience(
                simulation.map().terrain_at(listener_tile),
                ambience_variation
            );
        }
        audio_events.update(simulation, audio.get());
        if (campaign_presentation &&
            !campaign_presentation->outcome_processed &&
            simulation.outcome() != MatchOutcome::ongoing) {
            commit_campaign_outcome(
                campaign_presentation->campaign,
                campaign_presentation->scenario.id,
                simulation.outcome(),
                campaign_presentation->progress,
                campaign_presentation->progress_path
            );
            campaign_presentation->outcome_processed = true;
            campaign_presentation->screen =
                CampaignPresentation::Screen::debrief;
            campaign_presentation->visible = true;
        }
        if (!outcome_statistics_seen &&
            simulation.outcome() != MatchOutcome::ongoing) {
            outcome_statistics_seen = true;
            active_statistics_visible = true;
            active_statistics_postgame = true;
            active_statistics_tab = StatisticsTab::economy;
        }
        const auto render_started = std::chrono::steady_clock::now();
        const std::size_t rendered_tiles = render(
            renderer,
            simulation,
            computer,
            computer_debug,
            pending_building,
            pending_attack_move,
            pending_attack_ground,
            pending_patrol,
            pending_guard,
            pending_conversion,
            pending_trade_route,
            paused,
            control_group_status,
            selection_drag,
            formation_preview_center,
            scenario_presentation,
            campaign_presentation
                ? &*campaign_presentation
                : nullptr,
            multiplayer_presentation
                ? &*multiplayer_presentation
                : nullptr,
            simulation_active
                ? simulation_time.interpolation_alpha(tick_duration)
                : 1.0F,
            static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    presentation_time
                ).count()
            ),
            camera
        );
        const auto render_finished = std::chrono::steady_clock::now();
        if (gameplay_benchmark) {
            if (benchmark_frame >= benchmark_warmup_frames) {
                benchmark_frame_times_ms.push_back(
                    std::chrono::duration<double, std::milli>(
                        render_finished - frame_started
                    ).count()
                );
                benchmark_render_times_ms.push_back(
                    std::chrono::duration<double, std::milli>(
                        render_finished - render_started
                    ).count()
                );
                benchmark_max_rendered_tiles = std::max(
                    benchmark_max_rendered_tiles,
                    rendered_tiles
                );
                benchmark_max_moving_units = std::max(
                    benchmark_max_moving_units,
                    static_cast<std::size_t>(std::ranges::count_if(
                        simulation.units(),
                        [](const Unit& unit) {
                            return unit.moving;
                        }
                    ))
                );
            }
            ++benchmark_frame;
            if (benchmark_frame_times_ms.size() ==
                benchmark_sample_frames) {
                std::ranges::sort(benchmark_frame_times_ms);
                std::ranges::sort(benchmark_render_times_ms);
                const std::size_t p95_index =
                    (benchmark_frame_times_ms.size() * 95 + 99) /
                        100 - 1;
                std::ofstream report(benchmark_path);
                report << std::fixed << std::setprecision(3)
                       << "{\"frames\":"
                       << benchmark_frame_times_ms.size()
                       << ",\"frame_median_ms\":"
                       << benchmark_frame_times_ms[
                              benchmark_frame_times_ms.size() / 2]
                       << ",\"frame_p95_ms\":"
                       << benchmark_frame_times_ms[p95_index]
                       << ",\"frame_max_ms\":"
                       << benchmark_frame_times_ms.back()
                       << ",\"render_p95_ms\":"
                       << benchmark_render_times_ms[p95_index]
                       << ",\"command_ms\":"
                       << gameplay_benchmark_command_ms
                       << ",\"commanded_units\":"
                       << gameplay_benchmark_commanded_units
                       << ",\"max_moving_units\":"
                       << benchmark_max_moving_units
                       << ",\"max_tiles\":"
                       << benchmark_max_rendered_tiles
                       << "}\n";
                if (!report) {
                    throw std::runtime_error(
                        "could not write gameplay benchmark report"
                    );
                }
                running = false;
            }
        } else {
            SDL_Delay(8);
        }
    }

    if (multiplayer_runtime) {
        multiplayer_runtime->disconnect();
    }
    active_legacy_sprites.destroy();
    active_terrain_textures.destroy();
    if (archive_cursor != nullptr) {
        SDL_DestroyCursor(archive_cursor);
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    active_string_table = nullptr;
    return 0;
}

}  // namespace aoe
