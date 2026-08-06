#include "aoe/application_loop.hpp"

#include <utility>

namespace aoe {

ApplicationLoop::ApplicationLoop(Handle handle) noexcept : handle_(handle) {}

ApplicationLoop::ApplicationLoop(ApplicationLoop&& other) noexcept
    : handle_(std::exchange(other.handle_, {})) {}

ApplicationLoop& ApplicationLoop::operator=(ApplicationLoop&& other) noexcept {
    if (this == &other) return *this;
    if (handle_) handle_.destroy();
    handle_ = std::exchange(other.handle_, {});
    return *this;
}

ApplicationLoop::~ApplicationLoop() {
    if (handle_) handle_.destroy();
}

bool ApplicationLoop::resume() {
    if (!handle_ || handle_.done()) return false;
    handle_.resume();
    if (handle_.promise().exception) {
        std::rethrow_exception(handle_.promise().exception);
    }
    return !handle_.done();
}

bool ApplicationLoop::delay_requested() const noexcept {
    return handle_ && handle_.promise().delay;
}

ApplicationLoop ApplicationLoop::promise_type::get_return_object() noexcept {
    return ApplicationLoop{Handle::from_promise(*this)};
}

std::suspend_always
ApplicationLoop::promise_type::initial_suspend() const noexcept {
    return {};
}

std::suspend_always
ApplicationLoop::promise_type::final_suspend() const noexcept {
    return {};
}

std::suspend_always ApplicationLoop::promise_type::yield_value(
    bool should_delay
) noexcept {
    delay = should_delay;
    return {};
}

void ApplicationLoop::promise_type::return_void() const noexcept {}

void ApplicationLoop::promise_type::unhandled_exception() noexcept {
    exception = std::current_exception();
}

}  // namespace aoe
