#include "aoe/browser_telemetry.hpp"

namespace aoe {

void publish_browser_telemetry(const BrowserTelemetry&) {}
bool browser_render_telemetry_enabled() { return false; }
void publish_browser_render_telemetry(std::string_view) {}
std::string consume_browser_pixel_capture_request() { return {}; }
void publish_browser_pixel_capture_complete(std::string_view) {}

}  // namespace aoe
