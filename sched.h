#ifndef SCHED_H_
#define SHCED_H_

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


class Scheduler {
    std::vector<Task> suspended_vector;
    std::deque<Task> ready_queue;
    std::optional<Task> active_task;
    std::deque<int> int_queue;
private:
    static Scheduler *instance;
    Scheduler() { }
public:
    static Scheduler& get_instance() {
        if (Scheduler::instance == nullptr) {
            Scheduler::instance = new Scheduler();
        }
        return *Scheduler::instance;
    }

    void suspend(Task task) {
        suspended_vector.push_back(std::move(task));
    }

    void suspend_current() {
        if (active_task) {
            suspended_vector.push_back(std::move(*active_task));
        }
    }
 
    void schedule(Task task) {
        ready_queue.push_back(std::move(task));
    }

    void schedule_current() {
        if (active_task) {
            ready_queue.push_back(std::move(*active_task));
        }
    }

    void schedule_suspended(std::coroutine_handle<> const& h) {
        auto it = std::find(suspended_vector.begin(), suspended_vector.end(), h);
        if (it != suspended_vector.end()) {
            ready_queue.push_back(std::move(*it));
            suspended_vector.erase(it);
        }
    }

    void run() {
        while (true) {
            if (!ready_queue.empty()) {
                active_task.emplace(std::move(ready_queue.front()));
                auto active_task_id = active_task->id_;
                ready_queue.pop_front();
                active_task->resume();
                std::cout << "[SCHED] Task " << active_task_id << " resumed for single step" << std::endl;
                if (ready_queue.empty() && suspended_vector.empty()) {
                    std::cout << "[SCHED] No more work. Quitting" << std::endl;
                    return;
                }
            } else {
                // idle task
            }
            {
                // handle postponed events 
            }
        }
    }

};

struct TimedWait {

    using seconds_t = std::chrono::duration<double>;

    seconds_t time_;

    TimedWait(seconds_t time) : time_(time) { }
    bool await_ready() { return false; }
    void await_suspend(std::coroutine_handle<> h) { 
        std::cout << h.address() << " Starting timer... " << std::endl;
        Scheduler::get_instance().suspend_current();
        std::thread t([=, this] () {
            std::this_thread::sleep_for(time_);
            std::cout << h.address() << " Timer expired " << std::endl;
            Scheduler::get_instance().schedule_suspended(h);
        });
        t.detach();
    }
    void await_resume() {}

};

struct Yield {
    bool await_ready() { return false; }
    void await_suspend(std::coroutine_handle<> h) { 
        Scheduler::get_instance().schedule_current();
        std::cout << h.address() << " rescheduling... " << std::endl;
    }
    void await_resume() {}
};

Scheduler* Scheduler::instance = nullptr;

#endif