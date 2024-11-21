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
    char const* name = "unnamed";

    Task(std::coroutine_handle<promise_type> h) : handle(h) {}
    Task(Task&& other) noexcept : handle(other.handle) {
        other.handle = nullptr;
    }
    ~Task() { 

        std::cout << "[TASK]  " << name << " finished." << std::endl;
        if (handle) handle.destroy(); 
    }

    constexpr Task& operator=(Task&& other) = default;

/*bool operator==(Task const& other) {
    return handle == other.handle;
}*/

    bool operator==(std::coroutine_handle<> const& h) {
        return handle == h;
    }

    bool resume() {
        if (!handle.done()) handle.resume();
        return !handle.done();
    }
};

struct TimerService {

};

class Scheduler {
    std::vector<Task> suspended_vector;
    std::deque<Task> ready_queue;
    std::optional<Task> active_task;
    std::deque<int> int_queue;

public:
    void suspend(Task task) {
        suspended_vector.push_back(std::move(task));
    }

    void suspend_current() {
        if (active_task) {
            suspended_vector.push_back(std::move(*active_task));
        }
    }
    /*void suspend(std::coroutine_handle<> const& h) {
        auto it = std::find(ready_queue.begin(), ready_queue.end(), h);
        if (it != ready_queue.end()) {
            suspended_queue.push_back(std::move(*it));
            ready_queue.erase(it);
            std::cout << "removed from ready_queue" << std::endl;
        }

    }*/

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


    /*void schedule(std::coroutine_handle<> const& h) {
        it = std::find(suspended_queue.begin(), suspended_queue.end(), h);
        if (it != suspended_queue.end()) {
            ready_queue.push_back(std::move(*it));
            suspended_queue.erase();
        }

    }*/
    
    void run() {
        while (true) {
            if (!ready_queue.empty()) {
                active_task.emplace(std::move(ready_queue.front()));
                auto active_task_name = active_task->name;
                ready_queue.pop_front();
                
                if (!active_task->resume()) {
                    std::cout << "[SCHED] Task " << active_task_name << " done." << std::endl;
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

Scheduler scheduler;
/*
struct Yield {
    bool await_ready() { return false; }
    void await_suspend(std::coroutine_handle<>) {}
    void await_resume() {}
};*/

struct TimedWait {

    using seconds_t = std::chrono::duration<double>;

    seconds_t time_;

    TimedWait(seconds_t time) : time_(time) { }
    bool await_ready() { return false; }
    void await_suspend(std::coroutine_handle<> h) { 
        std::cout << h.address() << " Starting timer... " << std::endl;
        scheduler.suspend_current();
        std::thread t([=, this] () {
            std::this_thread::sleep_for(time_);
            std::cout << h.address() << " Timer expired " << std::endl;
            scheduler.schedule_suspended(h);
        });
        t.detach();
    }
    void await_resume() {}

};

struct Yield {
    bool await_ready() { return false; }
    void await_suspend(std::coroutine_handle<> h) { 
        scheduler.schedule_current();
        std::cout << h.address() << " rescheduling... " << std::endl;
    }
    void await_resume() {}
};


Task task1() {
    for (int i = 0; ; i++) {
        std::cout << "task1(): " << i << std::endl;
        if (i ==100) {
            co_await TimedWait(TimedWait::seconds_t(1));
        } else {
            co_await Yield{};
        }
    }
    //scheduler.schedule(task1());
}

Task task2() {
    for (int i = 0; ; i++) {
        std::cout << "task2(): " << i << std::endl;
        co_await Yield{};
    }

    //scheduler.schedule(task2());
    
}

int main() {
    auto t1 = task1();
    auto t2 = task2();
    t1.name = "TASK1";
    t2.name = "TASK2";
    scheduler.schedule(std::move(t1));
    scheduler.schedule(std::move(t2));
    scheduler.run();
    return 0;
}