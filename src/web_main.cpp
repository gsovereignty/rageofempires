#include <SDL3/SDL.h>
#include <emscripten/emscripten.h>

#include <cstdlib>
#include <exception>
#include <stdexcept>
#include <string>

#include "aoe/sdl_app.hpp"
#include "aoe/runtime_paths.hpp"

namespace {

EM_JS(bool, browser_risk_fixture_requested, (), {
    return new URLSearchParams(window.location.search).get('scenario') ===
        'risk-spike';
});

aoe::SdlApp application;

EM_JS(void, report_browser_failure, (const char* message), {
    const text = UTF8ToString(message);
    if (Module['reportFailure']) Module['reportFailure'](text);
});

EM_JS(void, record_browser_lifecycle, (int event), {
    if (!Module.browserLifecycle) Module.browserLifecycle = {
      initializations: 0,
      shutdowns: 0,
      restarts: 0,
      activeCallbacks: 1
    };
    if (event === 0) Module.browserLifecycle.initializations += 1;
    if (event === 1) Module.browserLifecycle.shutdowns += 1;
    if (event === 2) Module.browserLifecycle.restarts += 1;
});

void initialize_application() {
    application.initialize();
    if (!application.frame()) {
        throw std::runtime_error("browser application stopped during startup");
    }
    record_browser_lifecycle(0);
}

void browser_frame() {
    try {
        if (application.frame()) return;
        application.shutdown();
        record_browser_lifecycle(1);
        if (aoe::consume_application_restart_request()) {
            record_browser_lifecycle(2);
            initialize_application();
            return;
        }
        emscripten_cancel_main_loop();
    } catch (const std::exception& error) {
        emscripten_cancel_main_loop();
        report_browser_failure(error.what());
        application.shutdown();
    }
}

}  // namespace

int main() {
    const bool use_risk_fixture = browser_risk_fixture_requested();
    setenv(
        "AOE_SCENARIO_PATH",
        use_risk_fixture
            ? "/resources/browser-risk-spike.scenario"
            : "/resources/browser-skirmish.scenario",
        true
    );
    setenv("AOE_MAIN_MENU", "0", true);
    if (use_risk_fixture) {
        setenv("AOE_FOG", "0", true);
    }
    setenv("AOE_RENDER_FALLBACK_REPORT", "/user/fallback-report.json", true);
    try {
        initialize_application();
        emscripten_set_main_loop(browser_frame, 0, false);
        emscripten_set_main_loop_timing(EM_TIMING_SETTIMEOUT, 5);
    } catch (const std::exception& error) {
        report_browser_failure(error.what());
        return 1;
    }
    return 0;
}
