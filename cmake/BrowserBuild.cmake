set(AOE_WEB_DIST_DIR "${CMAKE_BINARY_DIR}/dist")
set(AOE_WEB_ASSET_DIR "${CMAKE_BINARY_DIR}/web-assets")

set(AOE_WEB_CORE_SOURCES ${AOE_CORE_SOURCES})
list(REMOVE_ITEM AOE_WEB_CORE_SOURCES
    src/commercial_multiplayer_service.cpp
)
add_library(aoe_web_core STATIC ${AOE_WEB_CORE_SOURCES})
target_include_directories(
    aoe_web_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/generated"
)
target_link_libraries(aoe_web_core PUBLIC ZLIB::ZLIB)
target_compile_definitions(aoe_web_core PRIVATE AOE_NO_NATIVE_TCP=1)
target_compile_options(aoe_web_core PRIVATE -Wall -Wextra -Wpedantic)

add_custom_target(web_asset_pack
    COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/build_web_asset_pack.py"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --output-root "${AOE_WEB_ASSET_DIR}"
    BYPRODUCTS
        "${AOE_WEB_ASSET_DIR}/web_asset_manifest.json"
    VERBATIM
)

add_executable(aoe_web
    "${CMAKE_CURRENT_SOURCE_DIR}/src/web_main.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/sdl_app.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/application_loop.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/runtime_paths_web.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/audio_system_web.cpp"
)
target_include_directories(
    aoe_web PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(aoe_web PRIVATE aoe_web_core SDL3::SDL3)
target_compile_definitions(aoe_web PRIVATE
    AOE_HAVE_NATIVE_MP3=0
    AOE_HAVE_MPG123=0
)
add_dependencies(aoe_web web_asset_pack)
target_compile_options(aoe_web PRIVATE -Wall -Wextra -Wpedantic)
target_link_options(aoe_web PRIVATE
    "SHELL:-lidbfs.js"
    "SHELL:-s ALLOW_MEMORY_GROWTH=1"
    "SHELL:-s FORCE_FILESYSTEM=1"
    "SHELL:-s MIN_WEBGL_VERSION=2"
    "SHELL:-s MAX_WEBGL_VERSION=2"
    "SHELL:-s EXIT_RUNTIME=0"
    "SHELL:-s ENVIRONMENT=web"
    "SHELL:-s INVOKE_RUN=0"
    "SHELL:-s EXPORTED_RUNTIME_METHODS=['callMain']"
    "SHELL:--shell-file ${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html"
    "SHELL:--pre-js ${CMAKE_CURRENT_SOURCE_DIR}/web/browser_runtime.js"
    "SHELL:--preload-file ${AOE_WEB_ASSET_DIR}/resources@/resources"
    "SHELL:--preload-file ${AOE_WEB_ASSET_DIR}/game_data/Bin@/game_data/Bin"
    "SHELL:--preload-file ${AOE_WEB_ASSET_DIR}/game_data/Data@/game_data/Data"
    "SHELL:--preload-file ${AOE_WEB_ASSET_DIR}/game_data/Terrain@/game_data/Terrain"
)
set_target_properties(aoe_web PROPERTIES
    OUTPUT_NAME aoe_web
    SUFFIX ".html"
    RUNTIME_OUTPUT_DIRECTORY "${AOE_WEB_DIST_DIR}"
    LINK_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html;${CMAKE_CURRENT_SOURCE_DIR}/web/browser_runtime.js"
)
add_custom_command(TARGET aoe_web POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory
        "${AOE_WEB_DIST_DIR}/game_data/Sound/music"
    COMMAND "${CMAKE_COMMAND}" -E make_directory
        "${AOE_WEB_DIST_DIR}/game_data/Taunt/en"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${AOE_WEB_ASSET_DIR}/game_data/Sound/music/xmusic1.mp3"
        "${AOE_WEB_DIST_DIR}/game_data/Sound/music/xmusic1.mp3"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${AOE_WEB_ASSET_DIR}/game_data/Taunt/en/03 Food, please.mp3"
        "${AOE_WEB_DIST_DIR}/game_data/Taunt/en/03 Food, please.mp3"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/web/styles.css"
        "${AOE_WEB_DIST_DIR}/styles.css"
    VERBATIM
)

add_custom_target(web_risk_spike
    COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/test_build_web_asset_pack.py"
    DEPENDS aoe_web
    VERBATIM
)
