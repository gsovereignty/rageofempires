#pragma once

#include <coroutine>
#include <exception>

namespace aoe {

class ApplicationLoop {
public:
    struct promise_type;
    using Handle = std::coroutine_handle<promise_type>;

    explicit ApplicationLoop(Handle handle) noexcept;
    ApplicationLoop(ApplicationLoop&& other) noexcept;
    ApplicationLoop& operator=(ApplicationLoop&& other) noexcept;
    ApplicationLoop(const ApplicationLoop&) = delete;
    ApplicationLoop& operator=(const ApplicationLoop&) = delete;
    ~ApplicationLoop();

    [[nodiscard]] bool resume();
    [[nodiscard]] bool delay_requested() const noexcept;

    struct promise_type {
        bool delay{};
        std::exception_ptr exception;

        [[nodiscard]] ApplicationLoop get_return_object() noexcept;
        [[nodiscard]] std::suspend_always initial_suspend() const noexcept;
        [[nodiscard]] std::suspend_always final_suspend() const noexcept;
        [[nodiscard]] std::suspend_always yield_value(
            bool should_delay
        ) noexcept;
        void return_void() const noexcept;
        void unhandled_exception() noexcept;
    };

private:
    Handle handle_{};
};

}  // namespace aoe
