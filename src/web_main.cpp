#include <SDL3/SDL.h>
#include <emscripten/emscripten.h>

#include <cstdlib>
#include <exception>
#include <string>

#include "aoe/sdl_app.hpp"

namespace {

aoe::SdlApp application;

EM_JS(void, report_browser_failure, (const char* message), {
    const text = UTF8ToString(message);
    if (Module['reportFailure']) Module['reportFailure'](text);
});

void browser_frame() {
    try {
        if (application.frame()) return;
        emscripten_cancel_main_loop();
        application.shutdown();
    } catch (const std::exception& error) {
        emscripten_cancel_main_loop();
        report_browser_failure(error.what());
        application.shutdown();
    }
}

}  // namespace

int main() {
    setenv(
        "AOE_SCENARIO_PATH",
        "/resources/browser-risk-spike.scenario",
        true
    );
    setenv("AOE_MAIN_MENU", "0", true);
    setenv("AOE_FOG", "0", true);
    setenv("AOE_DISABLE_LEGACY_ASSETS", "1", true);
    try {
        application.initialize();
        emscripten_set_main_loop(browser_frame, 0, false);
    } catch (const std::exception& error) {
        report_browser_failure(error.what());
        return 1;
    }
    return 0;
}
