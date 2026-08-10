#pragma once

#include <optional>

#include "aoe/application_loop.hpp"

namespace aoe {

class SdlApp {
public:
    SdlApp() = default;
    SdlApp(const SdlApp&) = delete;
    SdlApp& operator=(const SdlApp&) = delete;
    ~SdlApp();

    void initialize();
    [[nodiscard]] bool frame();
    void shutdown();
    int run();

private:
    [[nodiscard]] ApplicationLoop loop();

    std::optional<ApplicationLoop> loop_;
    bool stop_requested_{};
    bool delay_requested_{};
};

}  // namespace aoe
