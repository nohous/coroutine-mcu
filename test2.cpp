#include <concepts>
#include <iostream>
#include <coroutine>

// Mock scheduler class
struct scheduler {
    void priv_func() {
        std::cout << "scheduler::priv_func called\n";
    }
};

// Define the Awaitable concept
template <typename A>
concept Awaitable = requires(A a, std::coroutine_handle<> h) {
    { a.await_ready() } -> std::convertible_to<bool>;
    { a.await_suspend(h) };
    { a.await_resume() };
    { a.scheduler_callable() };
};

// Base class for scheduling friend functionality
template <typename A>
struct scheduler_friend {

    scheduler_friend(scheduler& s) : s_(s) { 
        static_assert(Awaitable<A>, "A must satisfy the Awaitable concept");

    }

    void scheduler_proxy() {
        s_.priv_func();
    }

protected:
    scheduler& s_;
};

// Derived awaitable type
struct yield_awaitable : scheduler_friend<yield_awaitable> {
    yield_awaitable(scheduler& s) : scheduler_friend<yield_awaitable>(s) { }

    bool await_ready() { return true; }
    void await_suspend(std::coroutine_handle<>) { }
    void await_resume() { }
    void scheduler_callable() {
        std::cout << "yield_awaitable::scheduler_callable called\n";
    }
};

// Test usage
int main() {
    scheduler s;
    yield_awaitable y(s);

    y.scheduler_proxy();
    y.scheduler_callable();
}