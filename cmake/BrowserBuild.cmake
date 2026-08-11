set(AOE_WEB_DIST_DIR "${CMAKE_BINARY_DIR}/dist")
set(AOE_WEB_ASSET_DIR "${CMAKE_BINARY_DIR}/web-assets")
set(AOE_BROWSER_TEST_PYTHON "python3" CACHE STRING
    "Host Python command with Selenium for browser acceptance")

find_program(AOE_NPM_EXECUTABLE npm REQUIRED)
set(AOE_NOSTR_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/web/nostr")
set(AOE_NOSTR_BUNDLE "${CMAKE_BINARY_DIR}/nostr/aoe_nostr.js")
set(AOE_NOSTR_INPUTS
    "${AOE_NOSTR_SOURCE_DIR}/package.json"
    "${AOE_NOSTR_SOURCE_DIR}/package-lock.json"
    "${AOE_NOSTR_SOURCE_DIR}/tsconfig.json"
    "${AOE_NOSTR_SOURCE_DIR}/scripts/build.mjs"
    "${AOE_NOSTR_SOURCE_DIR}/src/bridge.ts"
    "${AOE_NOSTR_SOURCE_DIR}/src/protocol.ts"
    "${AOE_NOSTR_SOURCE_DIR}/src/runtime.ts"
)
add_custom_command(
    OUTPUT "${AOE_NOSTR_BUNDLE}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory
        "${CMAKE_BINARY_DIR}/nostr"
    COMMAND "${AOE_NPM_EXECUTABLE}" ci --ignore-scripts
    COMMAND "${AOE_NPM_EXECUTABLE}" run typecheck
    COMMAND "${CMAKE_COMMAND}" -E env
        "AOE_NOSTR_BUNDLE=${AOE_NOSTR_BUNDLE}"
        "${AOE_NPM_EXECUTABLE}" run build
    WORKING_DIRECTORY "${AOE_NOSTR_SOURCE_DIR}"
    DEPENDS ${AOE_NOSTR_INPUTS}
    COMMENT "Building pinned Applesauce browser runtime"
    VERBATIM
)
add_custom_target(nostr_browser_bundle DEPENDS "${AOE_NOSTR_BUNDLE}")

set(AOE_WEB_CORE_SOURCES ${AOE_CORE_SOURCES})
list(REMOVE_ITEM AOE_WEB_CORE_SOURCES
    src/commercial_multiplayer_service.cpp
    src/multiplayer_transport.cpp
)
list(APPEND AOE_WEB_CORE_SOURCES
    src/nostr_browser_bridge.cpp
    src/nostr_multiplayer_runtime.cpp
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
    "${CMAKE_CURRENT_SOURCE_DIR}/src/browser_telemetry_web.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/audio_system_web.cpp"
)
target_include_directories(
    aoe_web PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(aoe_web PRIVATE aoe_web_core SDL3::SDL3)
target_compile_definitions(aoe_web PRIVATE
    AOE_BROWSER_FIXED_ASSET_SCOPE=1
    AOE_HAVE_NATIVE_MP3=0
    AOE_HAVE_MPG123=0
)
add_dependencies(aoe_web web_asset_pack nostr_browser_bundle)
target_compile_options(aoe_web PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -fexceptions
)
target_link_options(aoe_web PRIVATE
    -fexceptions
    "SHELL:-lidbfs.js"
    "SHELL:-s ALLOW_MEMORY_GROWTH=1"
    "SHELL:-s FORCE_FILESYSTEM=1"
    "SHELL:-s MIN_WEBGL_VERSION=2"
    "SHELL:-s MAX_WEBGL_VERSION=2"
    "SHELL:-s EXIT_RUNTIME=0"
    "SHELL:-s ENVIRONMENT=web"
    "SHELL:-s INVOKE_RUN=0"
    "SHELL:-s EXPORTED_RUNTIME_METHODS=['callMain','HEAPU8']"
    "SHELL:-s EXPORTED_FUNCTIONS=['_main','_malloc','_free','_aoe_nostr_enqueue_event','_aoe_nostr_enqueue_status','_aoe_nostr_publish_result']"
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
        "${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html;${CMAKE_CURRENT_SOURCE_DIR}/web/browser_runtime.js;${CMAKE_CURRENT_SOURCE_DIR}/web/styles.css;${AOE_NOSTR_BUNDLE}"
)
add_custom_command(TARGET aoe_web POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory
        "${AOE_WEB_DIST_DIR}/game_data/Sound/music"
    COMMAND "${CMAKE_COMMAND}" -E make_directory
        "${AOE_WEB_DIST_DIR}/game_data/Sound/effects"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${AOE_WEB_ASSET_DIR}/game_data/Sound/music/xmusic1.mp3"
        "${AOE_WEB_DIST_DIR}/game_data/Sound/music/xmusic1.mp3"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/game_data/Sound/effects"
        "${AOE_WEB_DIST_DIR}/game_data/Sound/effects"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/web/styles.css"
        "${AOE_WEB_DIST_DIR}/styles.css"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${AOE_NOSTR_BUNDLE}"
        "${AOE_WEB_DIST_DIR}/aoe_nostr.js"
    VERBATIM
)

add_custom_target(web_risk_spike
    COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/test_build_web_asset_pack.py"
    COMMAND "${AOE_BROWSER_TEST_PYTHON}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/web/browser_risk_spike_test.py"
        --browser chrome
        --evidence "${CMAKE_CURRENT_SOURCE_DIR}/artifacts/browser-risk-spike/evidence-chrome.json"
    COMMAND "${AOE_BROWSER_TEST_PYTHON}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/web/browser_risk_spike_test.py"
        --browser chrome
        --display-matrix
    COMMAND "${AOE_BROWSER_TEST_PYTHON}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/web/browser_risk_spike_test.py"
        --browser chrome
        --persistence-checks
    DEPENDS aoe_web
    USES_TERMINAL
    VERBATIM
)

add_custom_target(web_nostr_multiplayer_smoke
    COMMAND "${AOE_BROWSER_TEST_PYTHON}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/web/nostr_multiplayer_smoke_test.py"
        --evidence
        "${CMAKE_CURRENT_SOURCE_DIR}/artifacts/nostr-multiplayer/production-smoke.json"
    DEPENDS aoe_web
    USES_TERMINAL
    VERBATIM
)
