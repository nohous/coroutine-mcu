#ifndef SCHED_H_
#define SCHED_H_

#include <coroutine>
#include <deque>
#include <vector>
#include <iostream>
#include <thread>
#include <algorithm>
#include <optional>
#include <functional>
#include <mutex>
#include "task.h"

class Scheduler {
    std::vector<Task> suspended_vector;
    std::deque<Task> ready_queue;
    std::optional<Task> active_task;
    std::deque<int> int_queue;
    std::mutex m;
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

        std::unique_lock lock(m);
        ready_queue.push_back(std::move(task));
    }

    void schedule_current() {
        if (active_task) {
            std::unique_lock lock(m);
            ready_queue.push_back(std::move(*active_task));
        }
    }

    void schedule_suspended(std::coroutine_handle<> const& h) {
        std::unique_lock lock(m);
        auto it = std::find(suspended_vector.begin(), suspended_vector.end(), h);
        if (it != suspended_vector.end()) {
            ready_queue.push_back(std::move(*it));
            suspended_vector.erase(it);
        }
    }

    void run() {
        while (true) {
            std::unique_lock lock(m);
            if (!ready_queue.empty()) {
                active_task.emplace(std::move(ready_queue.front()));
                auto active_task_id = active_task->id_;
                ready_queue.pop_front();
                lock.unlock();
                active_task->resume();
                lock.lock();
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


Scheduler* Scheduler::instance = nullptr;

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
            Scheduler::get_instance().schedule_suspended(h);
        });
        t.detach();
    }
    int await_resume() { return 42; }

};

struct Yield {
    bool await_ready() { return false; }
    void await_suspend(std::coroutine_handle<> h) { 
        Scheduler::get_instance().schedule_current();
        std::cout << h.address() << " rescheduling... " << std::endl;
    }
    void await_resume() {}
};


#endif