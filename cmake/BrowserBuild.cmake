set(AOE_WEB_DIST_DIR "${CMAKE_BINARY_DIR}/dist")
set(AOE_WEB_ASSET_DIR "${CMAKE_BINARY_DIR}/web-assets")

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
target_link_libraries(aoe_web PRIVATE aoe_core SDL3::SDL3)
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
    "SHELL:--preload-file ${AOE_WEB_ASSET_DIR}/game_data@/game_data"
)
set_target_properties(aoe_web PROPERTIES
    OUTPUT_NAME aoe_web
    SUFFIX ".html"
    RUNTIME_OUTPUT_DIRECTORY "${AOE_WEB_DIST_DIR}"
    LINK_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html;${CMAKE_CURRENT_SOURCE_DIR}/web/browser_runtime.js"
)
add_custom_command(TARGET aoe_web POST_BUILD
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
