set(AOE_WEB_DIST_DIR "${CMAKE_BINARY_DIR}/dist")

add_executable(aoe_web
    "${CMAKE_CURRENT_SOURCE_DIR}/src/web_main.cpp"
)
target_link_libraries(aoe_web PRIVATE SDL3::SDL3)
target_compile_options(aoe_web PRIVATE -Wall -Wextra -Wpedantic)
target_link_options(aoe_web PRIVATE
    "SHELL:-s ALLOW_MEMORY_GROWTH=1"
    "SHELL:-s FORCE_FILESYSTEM=1"
    "SHELL:-s MIN_WEBGL_VERSION=2"
    "SHELL:-s MAX_WEBGL_VERSION=2"
    "SHELL:-s EXIT_RUNTIME=0"
    "SHELL:-s ENVIRONMENT=web"
    "SHELL:--shell-file ${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html"
    "SHELL:--pre-js ${CMAKE_CURRENT_SOURCE_DIR}/web/browser_runtime.js"
)
set_target_properties(aoe_web PROPERTIES
    OUTPUT_NAME aoe_web
    SUFFIX ".html"
    RUNTIME_OUTPUT_DIRECTORY "${AOE_WEB_DIST_DIR}"
)
add_custom_command(TARGET aoe_web POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/web/styles.css"
        "${AOE_WEB_DIST_DIR}/styles.css"
    VERBATIM
)

add_custom_target(web_risk_spike DEPENDS aoe_web)
