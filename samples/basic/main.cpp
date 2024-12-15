
#define COROCORE_DEBUG
#include <corocore/scheduler.h>
#include <iostream>
#include <thread>
#include <random>

using namespace adva;
namespace cc = corocore;

struct clock_std_chrono {
    using time_type = std::chrono::high_resolution_clock::time_point;
    using duration_type = std::chrono::high_resolution_clock::duration;

    time_type now() { return std::chrono::high_resolution_clock::now(); }
};

struct clock_tick {
    using time_type = int;
    using duration_type = int;

    time_type now() { return now_; } 

    void advance() { 
        now_++; 
    }
private:
    int now_ = 0;
};

static_assert(cc::Clock<clock_std_chrono>, "This is no clock");
static_assert(cc::Clock<clock_tick>, "This is no clock");

struct app_scheduler_config {
    static constexpr size_t max_task_count = 16;
    static constexpr size_t timer_count = 32;
};

using app_scheduler = cc::scheduler<app_scheduler_config>;
using yield = cc::yield_awaitable<app_scheduler>;
using event = cc::event_awaitable<app_scheduler>;
using timer_service = cc::timer_service<clock_std_chrono, app_scheduler>;
using async_task = app_scheduler::async_task_type;
using coro = app_scheduler::coroutine_type;

using namespace std::chrono_literals;
using namespace std::chrono;

int f1() {
    int x[1000];

    std::random_device rd;  // a seed source for the random number engine
    std::mt19937 gen(rd()); // mersenne_twister_engine seeded with rd()
    std::uniform_int_distribution<> distrib(1, 6);
    for (int i = 1; i < std::size(x); i++) {
        x[i] = distrib(gen) + x[i - 1];
    }

    std::cout << x[999] << std::endl;

    return x[999];
}

void f2() {
    int x[1000];
}

int r = 0, s = 0;

coro coro_sleep(timer_service &ts) {
    co_await ts.sleep_for(2000ms);

    pr_debug("post sleep");
}

coro coro_coro(timer_service& ts) {
    pr_debug("sub-task r " << r);
    co_await coro_sleep(ts);
    pr_debug("post await");
    /*if (s++ < 1) co_await test();
    pr_debug("sub-task s " << r);1*/
    
    co_return;
}

async_task task1(int a, timer_service& ts, event& e, int (*f)())
{
    using namespace std::chrono_literals;
    int b[128];
    for (int i = 0; ; i+=a) {
 //       if (i == 5)  co_await e;
        if (i == 5) co_return; // cc::exception("Who knows what happened");
        pr_debug(i);
        pr_debug("task 1: calling");
        co_await coro_coro(ts);
        pr_debug("task 1: returned");
        f();
        co_await ts.sleep_for(500ms);
    }
    //co_return;
}

async_task task2(int a, timer_service& ts, event& e)
{
    for (int i = 0; ; i+=a) {
        pr_debug("task 2: ");
        auto start = system_clock::now();
        co_await ts.sleep_for(150ms);
        auto end = system_clock::now();

        pr_debug(duration_cast<microseconds>(end - start) .count() / 1000.);
    }
}

int main()
{
    auto& s = app_scheduler::get_instance();

    event e{};

    clock_std_chrono c;

    timer_service ts{c};

    auto t1 = task1(1, ts, e, f1);
    auto t2 = task2(-1, ts, e);

    if (t1.invalid()) {
    }
    
    /*ts.sleep_until(40ms);
    ts.sleep_until(20ms);
    ts.sleep_until(30ms);*/

    s.schedule_all_suspended();

    for ( ; ; ) {
        s.run_once();
        ts.run_once();
        //c.advance();
        //if (c.now() == 1000) e.activate();
        //std::this_thread::sleep_for(std::chrono::duration<double>(0.01));
    }

    return 0;
}