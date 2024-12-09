#include "corocore/scheduler.h"
#include <iostream>
#include <thread>

using namespace adva;
namespace cc = corocore;


struct clock_mock {
    using time_type = int;

    time_type now() { return now_; }

    void advance() { 
        std::cout << "tick " << now_ << std::endl;
        now_++; 
    }

private:
    time_type now_ = 0;
};

struct app_scheduler_config {
    static constexpr size_t max_task_count = 16;
    static constexpr size_t timer_count = 32;
};

using app_scheduler = cc::scheduler<app_scheduler_config>;
using yield = cc::yield_awaitable<app_scheduler>;
using event = cc::event_awaitable<app_scheduler>;
using timer_service = cc::timer_service<clock_mock, app_scheduler>;
using async_task = app_scheduler::async_task_type;

async_task task1(int a, event& e)
{
    int b[128];
    for (int i = 0; ; i+=a) {
        std::cout << "task 1: " << i << " " << b[i % 128] << std::endl;
        if (i == 5)  co_await e;

        //std::this_thread::sleep_for(std::chrono::duration<double>(0.01));
        std::cout << "yield" << std::endl;
        co_await yield{};
    }
    co_return;
}

async_task task2(int a, timer_service& ts)
{
    for (int i = 0; ; i+=a) {
        std::cout << "task 2: " << i << std::endl;
        co_await ts.sleep_for(100);
    }
}

int main()
{
    auto& s = app_scheduler::get_instance();

    event e{};

    clock_mock c;

    timer_service ts{c};

    auto t1 = task1(1, e);
    auto t2 = task2(-1, ts);
    
    ts.sleep_until(40);
    ts.sleep_until(20);
    ts.sleep_until(30);

    s.schedule_all_suspended();

    for ( ; ; ) {
        s.run_once();
        ts.run_once();
        c.advance();
        if (c.now() == 1000) e.activate();
        std::this_thread::sleep_for(std::chrono::duration<double>(0.01));
    }

    return 0;
}