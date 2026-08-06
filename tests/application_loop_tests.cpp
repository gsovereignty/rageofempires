#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

#include "aoe/application_loop.hpp"

namespace {

aoe::ApplicationLoop counted_loop(int& count) {
    ++count;
    co_yield false;
    ++count;
    co_yield true;
    ++count;
}

aoe::ApplicationLoop throwing_loop() {
    co_yield false;
    throw std::runtime_error("frame failed");
}

void require(bool condition, const char* message) {
    if (condition) return;
    std::cerr << message << '\n';
    std::exit(1);
}

}  // namespace

int main() {
    int count{};
    auto loop = counted_loop(count);
    require(count == 0, "coroutine must start suspended");
    require(loop.resume(), "initial resume must yield");
    require(count == 1, "initial resume must perform initialization");
    require(!loop.delay_requested(), "initialization must not request delay");
    require(loop.resume(), "first frame must yield");
    require(count == 2, "first frame must run once");
    require(loop.delay_requested(), "native frame must request delay");
    require(!loop.resume(), "completed loop must stop");
    require(count == 3, "shutdown tail must run");
    require(!loop.resume(), "completed loop must remain stopped");

    auto failed = throwing_loop();
    require(failed.resume(), "throwing loop must initialize");
    try {
        static_cast<void>(failed.resume());
        require(false, "frame exception must escape resume");
    } catch (const std::runtime_error& error) {
        require(
            std::string{error.what()} == "frame failed",
            "frame exception text must survive"
        );
    }
    return 0;
}
