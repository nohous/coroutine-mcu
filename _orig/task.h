#ifndef TASK_H_
#define TASK_H_ 

#include <coroutine>
#include <deque>
#include <vector>
#include <iostream>
#include <thread>
#include <algorithm>
#include <optional>

class Task {
    struct promise_type {
        Task get_return_object() { return Task(std::coroutine_handle<promise_type>::from_promise(*this)); }
        std::suspend_always initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() { std::terminate(); }
    };

public:
    std::coroutine_handle<promise_type> handle;
    using promise_type = promise_type;
    static int idseq;
    int id_;

    Task() : id_(idseq++) {}
    Task(std::coroutine_handle<promise_type> h) : handle(h) {}
    Task(Task&& other) noexcept : handle(other.handle) {
        other.handle = nullptr;
    }
    ~Task() { 
        // False if moved
        if (handle) {
             handle.destroy(); 
             std::cout << "[TASK]  " << id_ << " finished." << std::endl;
        }
    }

    constexpr Task& operator=(Task&& other) = default;

    bool operator==(std::coroutine_handle<> const& h) {
        return handle == h;
    }

    void resume() {
        if (handle && !handle.done()) handle.resume();
    }
};

int Task::idseq = 0;


#endif