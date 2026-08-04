cmake_minimum_required(VERSION 3.24)

option(
    AOE_BUILD_SDL3
    "Build locked SDL3 source instead of using a host-built package"
    ${APPLE}
)

if(
    APPLE
    AND AOE_BUILD_SDL3
    AND NOT DEFINED CMAKE_OSX_ARCHITECTURES
)
    set(
        CMAKE_OSX_ARCHITECTURES "arm64;x86_64"
        CACHE STRING "macOS target architectures"
    )
endif()
if(
    APPLE
    AND AOE_BUILD_SDL3
    AND NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET
)
    set(
        CMAKE_OSX_DEPLOYMENT_TARGET 11.0
        CACHE STRING "Oldest supported macOS version"
    )
endif()

project(AoeArchaeology VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
find_package(ZLIB REQUIRED)
find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)

# FindZLIB can publish the macOS SDK's usr/include as an explicit system
# include.  That path is already supplied by -isysroot, and spelling it out
# makes libc++'s wrapper headers fail to find their C counterparts via
# include_next with recent Apple Clang releases.
if(APPLE AND TARGET ZLIB::ZLIB)
    get_target_property(
        AOE_ZLIB_INCLUDE_DIRECTORIES
        ZLIB::ZLIB
        INTERFACE_INCLUDE_DIRECTORIES
    )
    if(AOE_ZLIB_INCLUDE_DIRECTORIES)
        list(
            FILTER AOE_ZLIB_INCLUDE_DIRECTORIES
            EXCLUDE REGEX "/MacOSX[^/]*/usr/include/?$"
        )
        set_target_properties(
            ZLIB::ZLIB
            PROPERTIES
                INTERFACE_INCLUDE_DIRECTORIES
                    "${AOE_ZLIB_INCLUDE_DIRECTORIES}"
        )
    endif()
endif()

if(AOE_BUILD_SDL3)
    include(FetchContent)
    set(FETCHCONTENT_UPDATES_DISCONNECTED ON)
    set(SDL_SHARED ON CACHE BOOL "" FORCE)
    set(SDL_STATIC OFF CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SDL_REVISION "SDL-3.4.12-release-3.4.12" CACHE STRING "" FORCE)
    FetchContent_Declare(
        SDL3
        URL
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party/SDL-3.4.12-f87239e.tar.gz"
        URL_HASH
            SHA256=4b16b11925b65e9a967b0bfa71319716a052e0f9e49aa3be01cb2777305b8597
        DOWNLOAD_EXTRACT_TIMESTAMP FALSE
    )
    FetchContent_MakeAvailable(SDL3)
else()
    find_package(SDL3 3.4 CONFIG REQUIRED)
endif()

option(
    AOE_ENABLE_MPG123
    "Use a host mpg123 library for MP3 playback on non-macOS platforms"
    ON
)
set(
    AOE_ASSET_AUDIT_DATA_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/game_data/Data"
    CACHE PATH
    "Packaged Data directory used by renderer asset coverage targets"
)
if(AOE_ENABLE_MPG123)
    find_path(
        AOE_MPG123_INCLUDE_DIR
        mpg123.h
        HINTS
            /opt/homebrew/include
            /usr/local/include
    )
    if(NOT AOE_MPG123_INCLUDE_DIR)
        message(WARNING
            "mpg123 headers not found: gameplay MP3 music disabled. Install "
            "mpg123 or set AOE_MPG123_INCLUDE_DIR."
        )
    endif()
endif()

add_library(aoe_core
    src/building_damage.cpp
    src/campaign.cpp
    src/localization.cpp
    src/multiplayer.cpp
    src/multiplayer_checkpoint.cpp
    src/ui_assets.cpp
    src/ui_icon_contract.cpp
    src/ui_perspective.cpp
    src/multiplayer_transport.cpp
    src/legacy_dat.cpp
    src/legacy_campaign.cpp
    src/legacy_recorded_game.cpp
    src/legacy_ai_script.cpp
    src/legacy_scenario.cpp
    src/legacy_assets.cpp
    src/scenario.cpp
    src/random_map.cpp
    src/rms_import.cpp
    src/classic_save_import.cpp
    src/scenario_editor.cpp
    src/technology_tree.cpp
    src/settings.cpp
    src/frame_timing.cpp
    src/frontend_menu.cpp
    src/frontend_game_modes.cpp
    src/statistics_view.cpp
    src/save_browser.cpp
    src/command_panel.cpp
    src/selection_controls.cpp
    src/building_placement.cpp
    src/terrain_transition.cpp
    src/window_mode.cpp
    src/shadow_catalog.cpp
    src/projectile_catalog.cpp
    src/render_asset_coverage.cpp
    src/minimap_contract.cpp
    src/fog_rendering_contract.cpp
    src/cursor_contract.cpp
    src/animation_contract.cpp
    src/selection_visual_contract.cpp
    src/hud_layout_contract.cpp
    src/game_command.cpp
    src/gameplay_test_api.cpp
    src/frontend_audio.cpp
    src/computer_player.cpp
    src/game_rules.cpp
    src/game_map.cpp
    src/pathfinding.cpp
    src/simulation.cpp
    src/save_game.cpp
)
target_include_directories(aoe_core PUBLIC include)
target_link_libraries(aoe_core PUBLIC ZLIB::ZLIB)
target_compile_options(aoe_core PRIVATE -Wall -Wextra -Wpedantic)

add_executable(gameplay_test_api_tests tests/gameplay_test_api_tests.cpp)
target_link_libraries(gameplay_test_api_tests PRIVATE aoe_core)
target_compile_options(
    gameplay_test_api_tests PRIVATE -Wall -Wextra -Wpedantic
)
add_test(NAME gameplay_test_api_tests COMMAND gameplay_test_api_tests)

add_executable(settings_tests tests/settings_tests.cpp)
target_link_libraries(settings_tests PRIVATE aoe_core)
target_compile_options(settings_tests PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME settings_tests COMMAND settings_tests)

add_executable(window_mode_tests tests/window_mode_tests.cpp)
target_link_libraries(window_mode_tests PRIVATE aoe_core)
target_compile_options(window_mode_tests PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME window_mode_tests COMMAND window_mode_tests)

add_executable(frame_timing_tests tests/frame_timing_tests.cpp)
target_link_libraries(frame_timing_tests PRIVATE aoe_core)
target_compile_options(frame_timing_tests PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME frame_timing_tests COMMAND frame_timing_tests)

add_executable(audio_mix_tests tests/audio_mix_tests.cpp)
target_link_libraries(audio_mix_tests PRIVATE aoe_core)
target_compile_options(audio_mix_tests PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME audio_mix_tests COMMAND audio_mix_tests)

add_executable(statistics_view_tests tests/statistics_view_tests.cpp)
target_link_libraries(statistics_view_tests PRIVATE aoe_core)
target_compile_options(
    statistics_view_tests PRIVATE -Wall -Wextra -Wpedantic
)
add_test(NAME statistics_view_tests COMMAND statistics_view_tests)

add_executable(save_browser_tests tests/save_browser_tests.cpp)
target_link_libraries(save_browser_tests PRIVATE aoe_core)
target_compile_options(save_browser_tests PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME save_browser_tests COMMAND save_browser_tests)

add_executable(command_panel_tests tests/command_panel_tests.cpp)
target_link_libraries(command_panel_tests PRIVATE aoe_core)
target_compile_options(command_panel_tests PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME command_panel_tests COMMAND command_panel_tests)

add_executable(selection_controls_tests tests/selection_controls_tests.cpp)
target_link_libraries(selection_controls_tests PRIVATE aoe_core)
target_compile_options(
    selection_controls_tests PRIVATE -Wall -Wextra -Wpedantic
)
add_test(NAME selection_controls_tests COMMAND selection_controls_tests)

add_executable(building_placement_tests tests/building_placement_tests.cpp)
target_link_libraries(building_placement_tests PRIVATE aoe_core)
target_compile_options(
    building_placement_tests PRIVATE -Wall -Wextra -Wpedantic
)
add_test(NAME building_placement_tests COMMAND building_placement_tests)

add_executable(terrain_transition_tests tests/terrain_transition_tests.cpp)
target_link_libraries(terrain_transition_tests PRIVATE aoe_core)
target_compile_options(
    terrain_transition_tests PRIVATE -Wall -Wextra -Wpedantic
)
add_test(NAME terrain_transition_tests COMMAND terrain_transition_tests)

add_executable(initial_camera_tests tests/initial_camera_tests.cpp)
target_link_libraries(initial_camera_tests PRIVATE aoe_core)
target_compile_options(
    initial_camera_tests PRIVATE -Wall -Wextra -Wpedantic
)
add_test(NAME initial_camera_tests COMMAND initial_camera_tests)

add_executable(frontend_menu_tests tests/frontend_menu_tests.cpp)
target_link_libraries(frontend_menu_tests PRIVATE aoe_core)
target_compile_options(
    frontend_menu_tests PRIVATE -Wall -Wextra -Wpedantic
)
add_test(NAME frontend_menu_tests COMMAND frontend_menu_tests)

add_executable(
    frontend_game_modes_tests tests/frontend_game_modes_tests.cpp
)
target_link_libraries(frontend_game_modes_tests PRIVATE aoe_core)
target_compile_options(
    frontend_game_modes_tests PRIVATE -Wall -Wextra -Wpedantic
)
add_test(NAME frontend_game_modes_tests COMMAND frontend_game_modes_tests)

add_executable(world_tile_picker_tests tests/world_tile_picker_tests.cpp)
target_link_libraries(world_tile_picker_tests PRIVATE aoe_core)
target_compile_options(
    world_tile_picker_tests PRIVATE -Wall -Wextra -Wpedantic
)
add_test(NAME world_tile_picker_tests COMMAND world_tile_picker_tests)
add_test(
    NAME terrain_edge_sdl_smoke
    COMMAND bash
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/terrain_edge_sdl_smoke.sh
        $<TARGET_FILE:aoe_reconstruction>
)
set_tests_properties(terrain_edge_sdl_smoke PROPERTIES TIMEOUT 30)

add_executable(shadow_catalog_tests tests/shadow_catalog_tests.cpp)
target_link_libraries(shadow_catalog_tests PRIVATE aoe_core)
target_compile_options(
    shadow_catalog_tests PRIVATE -Wall -Wextra -Wpedantic
)
add_test(NAME shadow_catalog_tests COMMAND shadow_catalog_tests)

add_executable(
    render_asset_coverage_tests
    tests/render_asset_coverage_tests.cpp
)
target_link_libraries(render_asset_coverage_tests PRIVATE aoe_core)
target_compile_options(
    render_asset_coverage_tests PRIVATE -Wall -Wextra -Wpedantic
)
add_test(
    NAME render_asset_coverage_tests
    COMMAND render_asset_coverage_tests
)
add_test(
    NAME villager_death_sdl_smoke
    COMMAND bash
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/villager_death_sdl_smoke.sh
        $<TARGET_FILE:aoe_reconstruction>
)
set_tests_properties(villager_death_sdl_smoke PROPERTIES TIMEOUT 30)

add_executable(
    renderer_asset_coverage_audit
    tools/renderer_asset_coverage_audit.cpp
)
target_link_libraries(renderer_asset_coverage_audit PRIVATE aoe_core)
target_compile_options(
    renderer_asset_coverage_audit PRIVATE -Wall -Wextra -Wpedantic
)
add_executable(slp_contact_sheet tools/slp_contact_sheet.cpp)
target_link_libraries(slp_contact_sheet PRIVATE aoe_core)
target_compile_options(
    slp_contact_sheet PRIVATE -Wall -Wextra -Wpedantic
)
if(AOE_ASSET_AUDIT_DATA_ROOT)
    add_custom_target(
        refresh_renderer_asset_coverage
        COMMAND
            $<TARGET_FILE:renderer_asset_coverage_audit>
            --data-root "${AOE_ASSET_AUDIT_DATA_ROOT}"
            --output
            "${CMAKE_CURRENT_SOURCE_DIR}/generated/renderer_asset_coverage_v2.json"
        DEPENDS renderer_asset_coverage_audit
        VERBATIM
    )
    add_custom_target(
        check_renderer_asset_coverage
        COMMAND
            $<TARGET_FILE:renderer_asset_coverage_audit>
            --data-root "${AOE_ASSET_AUDIT_DATA_ROOT}"
            --output
            "${CMAKE_CURRENT_BINARY_DIR}/renderer_asset_coverage_current.json"
        COMMAND
            "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/compare_renderer_asset_coverage.py"
            --baseline
            "${CMAKE_CURRENT_SOURCE_DIR}/generated/renderer_asset_coverage_v2.json"
            --current
            "${CMAKE_CURRENT_BINARY_DIR}/renderer_asset_coverage_current.json"
        DEPENDS renderer_asset_coverage_audit
        VERBATIM
    )
endif()

add_custom_target(
    run_renderer_runtime_coverage
    COMMAND
        "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/run_renderer_runtime_coverage.py"
        "$<TARGET_FILE:aoe_reconstruction>"
        "${CMAKE_CURRENT_SOURCE_DIR}/resources"
        "${CMAKE_CURRENT_BINARY_DIR}/renderer_runtime_coverage.json"
    COMMAND
        "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/compare_runtime_static_coverage.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/generated/renderer_asset_coverage_v2.json"
        "${CMAKE_CURRENT_BINARY_DIR}/renderer_runtime_coverage.json"
    DEPENDS aoe_deploy_runtime_resources
    COMMENT "Checking runtime renderer asset selections against static audit"
    VERBATIM
)
# Campaign scenarios are generator output, so a hand edit has to fail.
add_test(
    NAME campaign_scenario_generator_tests
    COMMAND
        "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_campaign_scenarios.py"
        --resources "${CMAKE_CURRENT_SOURCE_DIR}/resources"
        --check
)

add_executable(
    player_color_palette_tests
    tests/player_color_palette_tests.cpp
)
target_link_libraries(player_color_palette_tests PRIVATE aoe_core)
target_compile_options(
    player_color_palette_tests PRIVATE -Wall -Wextra -Wpedantic
)
add_test(
    NAME player_color_palette_tests
    COMMAND player_color_palette_tests
)

add_executable(building_damage_tests tests/building_damage_tests.cpp)
target_link_libraries(building_damage_tests PRIVATE aoe_core)
target_compile_options(
    building_damage_tests PRIVATE -Wall -Wextra -Wpedantic
)
add_test(NAME building_damage_tests COMMAND building_damage_tests)

add_executable(minimap_contract_tests tests/minimap_contract_tests.cpp)
target_link_libraries(minimap_contract_tests PRIVATE aoe_core)
target_compile_options(
    minimap_contract_tests PRIVATE -Wall -Wextra -Wpedantic
)
add_test(NAME minimap_contract_tests COMMAND minimap_contract_tests)

add_executable(
    projectile_catalog_tests tests/projectile_catalog_tests.cpp
)
target_link_libraries(projectile_catalog_tests PRIVATE aoe_core)
target_compile_options(
    projectile_catalog_tests PRIVATE -Wall -Wextra -Wpedantic
)
add_test(
    NAME projectile_catalog_tests COMMAND projectile_catalog_tests
)

add_executable(player_codec_tests tests/player_codec_tests.cpp)
target_link_libraries(player_codec_tests PRIVATE aoe_core)
target_compile_options(
    player_codec_tests PRIVATE -Wall -Wextra -Wpedantic
)
add_test(NAME player_codec_tests COMMAND player_codec_tests)

add_executable(
    player_replay_codec_tests
    tests/player_replay_codec_tests.cpp
)
target_link_libraries(player_replay_codec_tests PRIVATE aoe_core)
target_compile_options(
    player_replay_codec_tests PRIVATE -Wall -Wextra -Wpedantic
)
add_test(
    NAME player_replay_codec_tests
    COMMAND player_replay_codec_tests
)

add_executable(
    native_persistence_tests
    tests/native_persistence_tests.cpp
)
target_link_libraries(native_persistence_tests PRIVATE aoe_core)
target_compile_options(
    native_persistence_tests PRIVATE -Wall -Wextra -Wpedantic
)
add_test(
    NAME native_persistence_tests
    COMMAND native_persistence_tests
)

add_executable(
    multiplayer_roster_tests
    tests/multiplayer_roster_tests.cpp
)
target_link_libraries(multiplayer_roster_tests PRIVATE aoe_core)
target_compile_options(
    multiplayer_roster_tests PRIVATE -Wall -Wextra -Wpedantic
)
add_test(
    NAME multiplayer_roster_tests
    COMMAND multiplayer_roster_tests
)

add_executable(
    multiplayer_transport_roster_tests
    tests/multiplayer_transport_roster_tests.cpp
)
target_link_libraries(multiplayer_transport_roster_tests PRIVATE aoe_core)
target_compile_options(
    multiplayer_transport_roster_tests PRIVATE -Wall -Wextra -Wpedantic
)
add_test(
    NAME multiplayer_transport_roster_tests
    COMMAND multiplayer_transport_roster_tests
)
set_tests_properties(
    multiplayer_transport_roster_tests
    PROPERTIES
        TIMEOUT 20
        RESOURCE_LOCK localhost_multiplayer_port
)

add_executable(player_roster_tests tests/player_roster_tests.cpp)
target_link_libraries(player_roster_tests PRIVATE aoe_core)
target_compile_options(
    player_roster_tests PRIVATE -Wall -Wextra -Wpedantic
)
add_test(NAME player_roster_tests COMMAND player_roster_tests)

add_executable(
    roster_diplomacy_tests
    tests/roster_diplomacy_tests.cpp
)
target_link_libraries(roster_diplomacy_tests PRIVATE aoe_core)
target_compile_options(
    roster_diplomacy_tests PRIVATE -Wall -Wextra -Wpedantic
)
add_test(
    NAME roster_diplomacy_tests
    COMMAND roster_diplomacy_tests
)

add_executable(multiplayer_signal_tests tests/multiplayer_signal_tests.cpp)
target_link_libraries(multiplayer_signal_tests PRIVATE aoe_core)
target_compile_options(
    multiplayer_signal_tests PRIVATE -Wall -Wextra -Wpedantic
)
add_test(NAME multiplayer_signal_tests COMMAND multiplayer_signal_tests)
add_test(
    NAME multiplayer_signal_sdl_smoke
    COMMAND bash
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/multiplayer_signal_sdl_smoke.sh
        $<TARGET_FILE:aoe_reconstruction>
)
set_tests_properties(multiplayer_signal_sdl_smoke PROPERTIES TIMEOUT 20)

add_test(
    NAME building_placement_sdl_smoke
    COMMAND bash
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/building_placement_sdl_smoke.sh
        $<TARGET_FILE:aoe_reconstruction>
)
set_tests_properties(building_placement_sdl_smoke PROPERTIES TIMEOUT 20)

add_test(
    NAME selection_controls_sdl_smoke
    COMMAND bash
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/selection_controls_sdl_smoke.sh
        $<TARGET_FILE:aoe_reconstruction>
)
set_tests_properties(
    selection_controls_sdl_smoke
    PROPERTIES TIMEOUT 20
)
add_test(
    NAME building_artifact_sdl_smoke
    COMMAND bash
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/building_artifact_sdl_smoke.sh
        $<TARGET_FILE:aoe_reconstruction>
)
set_tests_properties(building_artifact_sdl_smoke PROPERTIES TIMEOUT 20)
add_test(
    NAME building_age_graphics_sdl_smoke
    COMMAND bash
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/building_age_graphics_sdl_smoke.sh
        $<TARGET_FILE:aoe_reconstruction>
)
set_tests_properties(
    building_age_graphics_sdl_smoke
    PROPERTIES TIMEOUT 30
)
add_test(
    NAME market_standing_sdl_smoke
    COMMAND bash
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/market_standing_sdl_smoke.sh
        $<TARGET_FILE:aoe_reconstruction>
)
set_tests_properties(market_standing_sdl_smoke PROPERTIES TIMEOUT 30)
add_test(
    NAME battering_ram_composite_sdl_smoke
    COMMAND bash
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/battering_ram_composite_sdl_smoke.sh
        $<TARGET_FILE:aoe_reconstruction>
)
set_tests_properties(battering_ram_composite_sdl_smoke PROPERTIES TIMEOUT 30)
add_test(
    NAME building_contact_layering_sdl_smoke
    COMMAND bash
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/building_contact_layering_sdl_smoke.sh
        $<TARGET_FILE:aoe_reconstruction>
)
set_tests_properties(building_contact_layering_sdl_smoke PROPERTIES TIMEOUT 30)
add_test(
    NAME frontend_menu_sdl_smoke
    COMMAND bash
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/frontend_menu_sdl_smoke.sh
        $<TARGET_FILE:aoe_reconstruction>
)
set_tests_properties(frontend_menu_sdl_smoke PROPERTIES TIMEOUT 30)
add_test(
    NAME statistics_view_sdl_smoke
    COMMAND bash
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/statistics_view_sdl_smoke.sh
        $<TARGET_FILE:aoe_reconstruction>
)
set_tests_properties(statistics_view_sdl_smoke PROPERTIES TIMEOUT 30)
add_test(
    NAME minimap_interaction_sdl_smoke
    COMMAND bash
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/minimap_interaction_sdl_smoke.sh
        $<TARGET_FILE:aoe_reconstruction>
)
set_tests_properties(minimap_interaction_sdl_smoke PROPERTIES TIMEOUT 20)
add_test(
    NAME sheep_interaction_sdl_smoke
    COMMAND bash
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/sheep_interaction_sdl_smoke.sh
        $<TARGET_FILE:aoe_reconstruction>
)
set_tests_properties(sheep_interaction_sdl_smoke PROPERTIES TIMEOUT 20)

add_test(
    NAME observer_ui_sdl_smoke
    COMMAND bash
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/observer_ui_sdl_smoke.sh
        $<TARGET_FILE:aoe_reconstruction>
)
set_tests_properties(observer_ui_sdl_smoke PROPERTIES TIMEOUT 20)

add_executable(aoe_reconstruction
    src/main.cpp
    src/sdl_app.cpp
    src/audio_system.cpp
)
add_executable(
    aoe_multiplayer_roster_headless
    src/multiplayer_roster_headless.cpp
)
target_link_libraries(aoe_multiplayer_roster_headless PRIVATE aoe_core)
target_compile_options(
    aoe_multiplayer_roster_headless PRIVATE -Wall -Wextra -Wpedantic
)
add_executable(
    aoe_multiplayer_roster_sdl
    src/multiplayer_roster_headless.cpp
)
target_compile_definitions(
    aoe_multiplayer_roster_sdl PRIVATE AOE_ROSTER_SDL=1
)
target_link_libraries(
    aoe_multiplayer_roster_sdl PRIVATE aoe_core SDL3::SDL3
)
target_compile_options(
    aoe_multiplayer_roster_sdl PRIVATE -Wall -Wextra -Wpedantic
)
target_link_libraries(aoe_reconstruction PRIVATE aoe_core SDL3::SDL3)
if(APPLE)
    target_link_libraries(
        aoe_reconstruction PRIVATE
        "-framework AudioToolbox"
        "-framework CoreFoundation"
        "-framework CoreGraphics"
        "-framework CoreText"
    )
    target_compile_definitions(
        aoe_reconstruction PRIVATE AOE_HAVE_NATIVE_MP3=1
    )
else()
    target_compile_definitions(
        aoe_reconstruction PRIVATE AOE_HAVE_NATIVE_MP3=0
    )
endif()

if(AOE_MPG123_INCLUDE_DIR AND NOT APPLE)
    target_include_directories(
        aoe_reconstruction PRIVATE "${AOE_MPG123_INCLUDE_DIR}"
    )
    target_link_libraries(aoe_reconstruction PRIVATE ${CMAKE_DL_LIBS})
    target_compile_definitions(aoe_reconstruction PRIVATE AOE_HAVE_MPG123=1)
else()
    target_compile_definitions(aoe_reconstruction PRIVATE AOE_HAVE_MPG123=0)
endif()
target_compile_options(aoe_reconstruction PRIVATE -Wall -Wextra -Wpedantic)
file(
    GLOB_RECURSE AOE_RUNTIME_RESOURCE_FILES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/*"
)
file(
    GLOB_RECURSE AOE_GAME_DATA_FILES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/game_data/*"
)
# Track every runtime resource, not just executable relinks.
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/deployed-runtime-resources.stamp"
    COMMAND ${CMAKE_COMMAND} -E rm -rf
        "$<TARGET_FILE_DIR:aoe_reconstruction>/resources"
        "$<TARGET_FILE_DIR:aoe_reconstruction>/game_data"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:aoe_reconstruction>/resources"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/resources"
        "$<TARGET_FILE_DIR:aoe_reconstruction>/resources"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/game_data"
        "$<TARGET_FILE_DIR:aoe_reconstruction>/game_data"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/generated/resource_manifest.json"
        "$<TARGET_FILE_DIR:aoe_reconstruction>/resource-manifest.json"
    COMMAND ${CMAKE_COMMAND} -E touch
        "${CMAKE_CURRENT_BINARY_DIR}/deployed-runtime-resources.stamp"
    DEPENDS
        ${AOE_RUNTIME_RESOURCE_FILES}
        ${AOE_GAME_DATA_FILES}
        "${CMAKE_CURRENT_SOURCE_DIR}/generated/resource_manifest.json"
    COMMENT "Deploying tracked runtime resources"
    VERBATIM
)
add_custom_target(aoe_deploy_runtime_resources ALL
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/deployed-runtime-resources.stamp"
)
add_dependencies(aoe_deploy_runtime_resources aoe_reconstruction)

if(APPLE)
    add_executable(aoe_reconstruction_app MACOSX_BUNDLE
        src/main.cpp
        src/sdl_app.cpp
        src/audio_system.cpp
    )
    add_custom_command(
        TARGET aoe_reconstruction_app POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E rm -rf
            "$<TARGET_BUNDLE_CONTENT_DIR:aoe_reconstruction_app>/Resources/resources"
            "$<TARGET_BUNDLE_CONTENT_DIR:aoe_reconstruction_app>/Resources/game_data"
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_BUNDLE_CONTENT_DIR:aoe_reconstruction_app>/Resources/resources"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/resources"
            "$<TARGET_BUNDLE_CONTENT_DIR:aoe_reconstruction_app>/Resources/resources"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/game_data"
            "$<TARGET_BUNDLE_CONTENT_DIR:aoe_reconstruction_app>/Resources/game_data"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/generated/resource_manifest.json"
            "$<TARGET_BUNDLE_CONTENT_DIR:aoe_reconstruction_app>/Resources/resource-manifest.json"
        VERBATIM
    )
    target_link_libraries(
        aoe_reconstruction_app PRIVATE aoe_core SDL3::SDL3
    )
    target_link_libraries(
        aoe_reconstruction_app PRIVATE
        "-framework AudioToolbox"
        "-framework CoreFoundation"
        "-framework CoreGraphics"
        "-framework CoreText"
    )
    target_compile_definitions(
        aoe_reconstruction_app PRIVATE AOE_HAVE_NATIVE_MP3=1
    )
    target_compile_options(
        aoe_reconstruction_app PRIVATE -Wall -Wextra -Wpedantic
    )
    if(AOE_MPG123_INCLUDE_DIR AND NOT APPLE)
        target_include_directories(
            aoe_reconstruction_app PRIVATE "${AOE_MPG123_INCLUDE_DIR}"
        )
        target_link_libraries(
            aoe_reconstruction_app PRIVATE ${CMAKE_DL_LIBS}
        )
        target_compile_definitions(
            aoe_reconstruction_app PRIVATE AOE_HAVE_MPG123=1
        )
    else()
        target_compile_definitions(
            aoe_reconstruction_app PRIVATE AOE_HAVE_MPG123=0
        )
    endif()
    target_link_options(
        aoe_reconstruction_app PRIVATE
        "-Wl,-rpath,@executable_path/../Frameworks"
    )
    set_target_properties(aoe_reconstruction_app PROPERTIES
        OUTPUT_NAME "AoE Archaeology"
        MACOSX_BUNDLE_BUNDLE_NAME "AoE Archaeology"
        MACOSX_BUNDLE_GUI_IDENTIFIER "org.software-archaeology.aoe"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_COPYRIGHT "Research reconstruction"
        MACOSX_BUNDLE_INFO_PLIST
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Info.plist.in"
    )
    add_custom_command(TARGET aoe_reconstruction_app POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_BUNDLE_DIR:aoe_reconstruction_app>/Contents/Frameworks"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:SDL3::SDL3>"
            "$<TARGET_BUNDLE_DIR:aoe_reconstruction_app>/Contents/Frameworks/$<TARGET_FILE_NAME:SDL3::SDL3>"
        COMMAND /usr/bin/install_name_tool
            -change "$<TARGET_FILE:SDL3::SDL3>"
            "@rpath/$<TARGET_FILE_NAME:SDL3::SDL3>"
            "$<TARGET_FILE:aoe_reconstruction_app>"
        COMMAND /usr/bin/codesign --force --sign -
            "$<TARGET_BUNDLE_DIR:aoe_reconstruction_app>/Contents/Frameworks/$<TARGET_FILE_NAME:SDL3::SDL3>"
        COMMAND /usr/bin/codesign --force --deep --sign -
            "$<TARGET_BUNDLE_DIR:aoe_reconstruction_app>"
        VERBATIM
    )
endif()


add_executable(aoe_core_tests tests/simulation_tests.cpp)
target_link_libraries(aoe_core_tests PRIVATE aoe_core)

add_executable(aoe_random_map_tests tests/random_map_tests.cpp)
target_link_libraries(aoe_random_map_tests PRIVATE aoe_core)

add_executable(aoe_rms_import_tests tests/rms_import_tests.cpp)
target_link_libraries(aoe_rms_import_tests PRIVATE aoe_core)

add_executable(
    aoe_classic_save_import_tests
    tests/classic_save_import_tests.cpp
)
target_link_libraries(aoe_classic_save_import_tests PRIVATE aoe_core)

add_executable(
    aoe_match_statistics_tests
    tests/match_statistics_tests.cpp
)
target_link_libraries(aoe_match_statistics_tests PRIVATE aoe_core)

add_executable(aoe_legacy_assets_tests tests/legacy_assets_tests.cpp)
target_link_libraries(aoe_legacy_assets_tests PRIVATE aoe_core)
target_compile_options(
    aoe_legacy_assets_tests PRIVATE -Wall -Wextra -Wpedantic
)

add_executable(aoe_legacy_dat_tests tests/legacy_dat_tests.cpp)
target_link_libraries(aoe_legacy_dat_tests PRIVATE aoe_core)
target_compile_options(aoe_legacy_dat_tests PRIVATE -Wall -Wextra -Wpedantic)

add_executable(aoe_legacy_scenario_tests tests/legacy_scenario_tests.cpp)
target_link_libraries(aoe_legacy_scenario_tests PRIVATE aoe_core)
target_compile_options(
    aoe_legacy_scenario_tests PRIVATE -Wall -Wextra -Wpedantic
)

add_executable(
    aoe_legacy_recorded_game_tests
    tests/legacy_recorded_game_tests.cpp
)
target_link_libraries(aoe_legacy_recorded_game_tests PRIVATE aoe_core)
target_compile_options(
    aoe_legacy_recorded_game_tests PRIVATE -Wall -Wextra -Wpedantic
)

add_executable(aoe_legacy_ai_script_tests tests/legacy_ai_script_tests.cpp)
target_link_libraries(aoe_legacy_ai_script_tests PRIVATE aoe_core)
target_compile_options(
    aoe_legacy_ai_script_tests PRIVATE -Wall -Wextra -Wpedantic
)

add_executable(
    aoe_resignation_observer_tests
    tests/resignation_observer_tests.cpp
)
target_link_libraries(aoe_resignation_observer_tests PRIVATE aoe_core)
target_compile_options(
    aoe_resignation_observer_tests PRIVATE -Wall -Wextra -Wpedantic
)

add_executable(aoe_localization_tests tests/localization_tests.cpp)
target_link_libraries(aoe_localization_tests PRIVATE aoe_core)
target_compile_options(
    aoe_localization_tests PRIVATE -Wall -Wextra -Wpedantic
)

add_executable(aoe_ui_assets_tests tests/ui_assets_tests.cpp)
target_link_libraries(aoe_ui_assets_tests PRIVATE aoe_core)
target_compile_options(
    aoe_ui_assets_tests PRIVATE -Wall -Wextra -Wpedantic
)

add_executable(aoe_ui_perspective_tests tests/ui_perspective_tests.cpp)
target_link_libraries(aoe_ui_perspective_tests PRIVATE aoe_core)
target_compile_options(
    aoe_ui_perspective_tests PRIVATE -Wall -Wextra -Wpedantic
)

add_executable(aoe_elevation_tests tests/elevation_tests.cpp)
target_link_libraries(aoe_elevation_tests PRIVATE aoe_core)
target_compile_options(
    aoe_elevation_tests PRIVATE -Wall -Wextra -Wpedantic
)
add_executable(aoe_scenario_editor_tests tests/scenario_editor_tests.cpp)
target_link_libraries(aoe_scenario_editor_tests PRIVATE aoe_core)
target_compile_options(
    aoe_scenario_editor_tests PRIVATE -Wall -Wextra -Wpedantic
)
add_executable(aoe_technology_tree_tests tests/technology_tree_tests.cpp)
target_link_libraries(aoe_technology_tree_tests PRIVATE aoe_core)
target_compile_options(
    aoe_technology_tree_tests PRIVATE -Wall -Wextra -Wpedantic
)
add_executable(
    aoe_fog_rendering_contract_tests
    tests/fog_rendering_contract_tests.cpp
)
target_link_libraries(aoe_fog_rendering_contract_tests PRIVATE aoe_core)
target_compile_options(
    aoe_fog_rendering_contract_tests PRIVATE -Wall -Wextra -Wpedantic
)
add_executable(
    aoe_cursor_contract_tests
    tests/cursor_contract_tests.cpp
)
target_link_libraries(aoe_cursor_contract_tests PRIVATE aoe_core)
target_compile_options(
    aoe_cursor_contract_tests PRIVATE -Wall -Wextra -Wpedantic
)
add_executable(
    aoe_ui_icon_contract_tests
    tests/ui_icon_contract_tests.cpp
)
target_link_libraries(aoe_ui_icon_contract_tests PRIVATE aoe_core)
target_compile_options(
    aoe_ui_icon_contract_tests PRIVATE -Wall -Wextra -Wpedantic
)
add_executable(
    aoe_animation_contract_tests
    tests/animation_contract_tests.cpp
)
target_link_libraries(aoe_animation_contract_tests PRIVATE aoe_core)
target_compile_options(
    aoe_animation_contract_tests PRIVATE -Wall -Wextra -Wpedantic
)
add_executable(
    aoe_selection_visual_contract_tests
    tests/selection_visual_contract_tests.cpp
)
add_executable(
    aoe_hud_layout_contract_tests
    tests/hud_layout_contract_tests.cpp
)
target_link_libraries(aoe_hud_layout_contract_tests PRIVATE aoe_core)
target_compile_options(
    aoe_hud_layout_contract_tests PRIVATE -Wall -Wextra -Wpedantic
)
target_link_libraries(aoe_selection_visual_contract_tests PRIVATE aoe_core)
target_compile_options(
    aoe_selection_visual_contract_tests PRIVATE -Wall -Wextra -Wpedantic
)

enable_testing()

file(
    GLOB_RECURSE AOE_PYTHON_TOOL_TESTS
    CONFIGURE_DEPENDS
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/test_*.py"
)
foreach(AOE_PYTHON_TOOL_TEST IN LISTS AOE_PYTHON_TOOL_TESTS)
    string(
        REGEX REPLACE "[^A-Za-z0-9]+" "_"
        AOE_PYTHON_TOOL_TEST_SLUG
        "${AOE_PYTHON_TOOL_TEST}"
    )
    string(SHA1 AOE_PYTHON_TOOL_TEST_HASH "${AOE_PYTHON_TOOL_TEST}")
    string(SUBSTRING "${AOE_PYTHON_TOOL_TEST_HASH}" 0 10
        AOE_PYTHON_TOOL_TEST_HASH
    )
    set(
        AOE_PYTHON_TOOL_TEST_NAME
        "python_${AOE_PYTHON_TOOL_TEST_SLUG}_${AOE_PYTHON_TOOL_TEST_HASH}"
    )
    add_test(
        NAME "${AOE_PYTHON_TOOL_TEST_NAME}"
        COMMAND
            "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/${AOE_PYTHON_TOOL_TEST}"
    )
    set_tests_properties(
        "${AOE_PYTHON_TOOL_TEST_NAME}"
        PROPERTIES
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            LABELS "python;tooling"
            TIMEOUT 60
    )
endforeach()

add_test(NAME aoe_core_tests COMMAND aoe_core_tests)
add_test(NAME aoe_random_map_tests COMMAND aoe_random_map_tests)
add_test(NAME aoe_rms_import_tests COMMAND aoe_rms_import_tests)
add_test(
    NAME aoe_classic_save_import_tests
    COMMAND aoe_classic_save_import_tests
)
add_test(
    NAME aoe_match_statistics_tests
    COMMAND aoe_match_statistics_tests
)
add_test(NAME aoe_legacy_assets_tests COMMAND aoe_legacy_assets_tests)
add_test(NAME aoe_legacy_dat_tests COMMAND aoe_legacy_dat_tests)
add_test(NAME aoe_legacy_scenario_tests COMMAND aoe_legacy_scenario_tests)
add_test(
    NAME classic_campaign_sdl_smoke
    COMMAND
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/classic_campaign_sdl_smoke.sh
        $<TARGET_FILE:aoe_reconstruction>
        $<TARGET_FILE:aoe_legacy_scenario_tests>
)
set_tests_properties(
    classic_campaign_sdl_smoke
    PROPERTIES TIMEOUT 30 ENVIRONMENT "AOE_DISABLE_LEGACY_ASSETS=0"
)
add_test(
    NAME aoe_legacy_recorded_game_tests
    COMMAND aoe_legacy_recorded_game_tests
)
add_test(NAME aoe_legacy_ai_script_tests COMMAND aoe_legacy_ai_script_tests)
add_test(
    NAME aoe_resignation_observer_tests
    COMMAND aoe_resignation_observer_tests
)
add_test(NAME aoe_localization_tests COMMAND aoe_localization_tests)
add_test(NAME aoe_ui_assets_tests COMMAND aoe_ui_assets_tests)
add_test(NAME aoe_ui_perspective_tests COMMAND aoe_ui_perspective_tests)
add_test(NAME aoe_elevation_tests COMMAND aoe_elevation_tests)
add_test(NAME aoe_scenario_editor_tests COMMAND aoe_scenario_editor_tests)
add_test(NAME aoe_technology_tree_tests COMMAND aoe_technology_tree_tests)
add_test(
    NAME aoe_fog_rendering_contract_tests
    COMMAND aoe_fog_rendering_contract_tests
)
add_test(
    NAME aoe_cursor_contract_tests
    COMMAND aoe_cursor_contract_tests
)
add_test(
    NAME aoe_ui_icon_contract_tests
    COMMAND aoe_ui_icon_contract_tests
)
add_test(
    NAME aoe_animation_contract_tests
    COMMAND aoe_animation_contract_tests
)
add_test(
    NAME aoe_selection_visual_contract_tests
    COMMAND aoe_selection_visual_contract_tests
)
add_test(
    NAME aoe_hud_layout_contract_tests
    COMMAND aoe_hud_layout_contract_tests
)
add_test(
    NAME aoe_hud_layout_sdl_smoke
    COMMAND
        bash
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/hud_layout_sdl_smoke.sh
        $<TARGET_FILE:aoe_reconstruction>
)
set_tests_properties(aoe_hud_layout_sdl_smoke PROPERTIES TIMEOUT 60)
add_test(
    NAME window_mode_sdl_smoke
    COMMAND
        bash
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/window_mode_sdl_smoke.sh
        $<TARGET_FILE:aoe_reconstruction>
)
set_tests_properties(window_mode_sdl_smoke PROPERTIES TIMEOUT 60)
add_test(
    NAME aoe_self_containment_guard
    COMMAND
        "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/scripts/check_self_containment.py"
        "${CMAKE_CURRENT_SOURCE_DIR}"
)
add_test(
    NAME aoe_packaged_sprites_sdl_smoke
    COMMAND
        bash
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/packaged_sprites_sdl_smoke.sh
        $<TARGET_FILE:aoe_reconstruction>
)
set_tests_properties(
    aoe_packaged_sprites_sdl_smoke PROPERTIES TIMEOUT 120
)
add_test(
    NAME aoe_packaged_audio_sdl_smoke
    COMMAND
        bash
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/audio_playback_sdl_smoke.sh
        $<TARGET_FILE:aoe_reconstruction>
)
set_tests_properties(
    aoe_packaged_audio_sdl_smoke PROPERTIES TIMEOUT 120
)
if(APPLE)
    set(AOE_PACKAGED_STARTUP_EXECUTABLE
        $<TARGET_FILE:aoe_reconstruction_app>
    )
else()
    set(AOE_PACKAGED_STARTUP_EXECUTABLE
        $<TARGET_FILE:aoe_reconstruction>
    )
endif()
add_test(
    NAME aoe_packaged_startup_sdl_smoke
    COMMAND
        bash
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/packaged_startup_sdl_smoke.sh
        ${AOE_PACKAGED_STARTUP_EXECUTABLE}
)
set_tests_properties(
    aoe_packaged_startup_sdl_smoke PROPERTIES TIMEOUT 120
)
add_test(
    NAME aoe_markdown_link_guard
    COMMAND
        "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/scripts/check_markdown_links.py"
        "${CMAKE_CURRENT_SOURCE_DIR}"
)
add_test(
    NAME aoe_visual_overlap_tools_tests
    COMMAND
        "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/visual_overlap_tools_tests.py"
)
add_test(
    NAME aoe_command_icon_manifest
    COMMAND
        "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_command_icon_manifest.py"
        "${CMAKE_CURRENT_SOURCE_DIR}"
)
add_test(
    NAME aoe_technology_icon_bindings
    COMMAND
        "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_technology_icon_bindings.py"
        "${CMAKE_CURRENT_SOURCE_DIR}"
        --check
)
add_test(
    NAME aoe_runtime_resource_manifest
    COMMAND
        "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_resource_manifest.py"
        "${CMAKE_CURRENT_SOURCE_DIR}"
        --check
)
add_test(
    NAME aoe_deployed_resource_manifest
    COMMAND
        "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/scripts/verify_resource_manifest.py"
        "$<TARGET_FILE_DIR:aoe_reconstruction>/resource-manifest.json"
        "$<TARGET_FILE_DIR:aoe_reconstruction>"
)
add_test(
    NAME aoe_selection_visual_sdl_smoke
    COMMAND
        bash
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/selection_visual_sdl_smoke.sh
        $<TARGET_FILE:aoe_reconstruction>
)
add_test(
    NAME aoe_animation_fidelity_sdl_smoke
    COMMAND
        bash
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/animation_fidelity_sdl_smoke.sh
        $<TARGET_FILE:aoe_reconstruction>
)
add_test(
    NAME aoe_ui_icon_sdl_smoke
    COMMAND
        bash
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/ui_icon_sdl_smoke.sh
        $<TARGET_FILE:aoe_reconstruction>
)
add_test(
    NAME aoe_technology_tree_sdl_smoke
    COMMAND
        bash
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/technology_tree_sdl_smoke.sh
        $<TARGET_FILE:aoe_reconstruction>
)
set_tests_properties(
    aoe_technology_tree_sdl_smoke
    PROPERTIES TIMEOUT 20
)
add_test(
    NAME aoe_scenario_editor_sdl_smoke
    COMMAND
        bash
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/scenario_editor_sdl_smoke.sh
        $<TARGET_FILE:aoe_reconstruction>
        ${CMAKE_CURRENT_SOURCE_DIR}/resources/editor-roundtrip.scenario
)
set_tests_properties(
    aoe_scenario_editor_sdl_smoke
    PROPERTIES TIMEOUT 20
)

add_test(
    NAME aoe_localhost_multiplayer_sdl_smoke
    COMMAND
        bash
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/localhost_multiplayer_sdl_smoke.sh
        $<TARGET_FILE:aoe_reconstruction>
)
set_tests_properties(
    aoe_localhost_multiplayer_sdl_smoke
    PROPERTIES
        RESOURCE_LOCK localhost_multiplayer_port
        TIMEOUT 30
)
add_test(
    NAME aoe_localhost_multiplayer_roster_headless_smoke
    COMMAND
        bash
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/localhost_multiplayer_roster_headless_smoke.sh
        $<TARGET_FILE:aoe_multiplayer_roster_headless>
)
set_tests_properties(
    aoe_localhost_multiplayer_roster_headless_smoke
    PROPERTIES
        RESOURCE_LOCK localhost_multiplayer_port
        TIMEOUT 30
)
add_test(
    NAME aoe_localhost_multiplayer_roster_sdl_smoke
    COMMAND
        bash
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/localhost_multiplayer_roster_sdl_smoke.sh
        $<TARGET_FILE:aoe_multiplayer_roster_sdl>
)
set_tests_properties(
    aoe_localhost_multiplayer_roster_sdl_smoke
    PROPERTIES
        RESOURCE_LOCK localhost_multiplayer_port
        TIMEOUT 90
)
add_test(
    NAME aoe_random_map_sdl_smoke
    COMMAND
        bash
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/random_map_sdl_smoke.sh
        $<TARGET_FILE:aoe_reconstruction>
)
set_tests_properties(
    aoe_random_map_sdl_smoke
    PROPERTIES TIMEOUT 20
)
add_test(
    NAME aoe_gameplay_performance_sdl
    COMMAND
        bash
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/gameplay_performance_sdl_test.sh
        $<TARGET_FILE:aoe_reconstruction>
)
set_tests_properties(
    aoe_gameplay_performance_sdl
    PROPERTIES
        TIMEOUT 30
        LABELS "performance;sdl"
)
add_test(
    NAME aoe_frontend_performance_sdl
    COMMAND
        bash
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/frontend_performance_sdl_test.sh
        $<TARGET_FILE:aoe_reconstruction>
)
set_tests_properties(
    aoe_frontend_performance_sdl
    PROPERTIES
        TIMEOUT 30
        LABELS "performance;sdl"
)
add_test(
    NAME aoe_diplomacy_panel_sdl_smoke
    COMMAND
        bash
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/diplomacy_panel_sdl_smoke.sh
        $<TARGET_FILE:aoe_reconstruction>
)
set_tests_properties(
    aoe_diplomacy_panel_sdl_smoke
    PROPERTIES TIMEOUT 20
)

if(APPLE)
    find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
    add_test(
        NAME aoe_visual_smoke
        COMMAND
            "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/visual_smoke_test.py"
            "$<TARGET_FILE:aoe_reconstruction>"
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/visual-audit.scenario"
    )
    set_tests_properties(
        aoe_visual_smoke PROPERTIES
        TIMEOUT 100
        LABELS "visual;smoke"
    )
endif()

# Keep ordinary tests hermetic and fast even after a full packaging build has
# deployed large optional legacy archives beside the executable. Dedicated
# packaged-asset smokes explicitly re-enable those archives.
get_property(AOE_ALL_TESTS DIRECTORY PROPERTY TESTS)
set_tests_properties(
    ${AOE_ALL_TESTS}
    PROPERTIES ENVIRONMENT
        "AOE_DISABLE_LEGACY_ASSETS=1;AOE_MAIN_MENU=0"
)
set_tests_properties(
    aoe_packaged_sprites_sdl_smoke
    aoe_packaged_audio_sdl_smoke
    battering_ram_composite_sdl_smoke
    building_contact_layering_sdl_smoke
    classic_campaign_sdl_smoke
    PROPERTIES ENVIRONMENT
        "AOE_DISABLE_LEGACY_ASSETS=0;AOE_MAIN_MENU=0"
)
set_tests_properties(
    aoe_packaged_startup_sdl_smoke
    PROPERTIES ENVIRONMENT "AOE_DISABLE_LEGACY_ASSETS=0"
)
