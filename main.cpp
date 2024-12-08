#include "corocore/scheduler.h"
#include <iostream>
#include <thread>

using namespace adva;
namespace cc = corocore;

struct app_scheduler_config {
    static constexpr size_t max_task_count = 16;
    static constexpr size_t couroutine_alloc_granularity = 32;
    static constexpr size_t timer_count = 32;
};


using app_scheduler = cc::scheduler<app_scheduler_config>;
using yield_awaitable = cc::yield_awaitable<app_scheduler>;
using async_task = app_scheduler::async_task_type;

async_task task1(int a)
{
    int b[128];
    for (int i = 0; ; i+=a) {
        std::cout << "task 1: " << i << " " << b[i % 128] << std::endl;

        std::this_thread::sleep_for(std::chrono::duration<double>(0.15));
        std::cout << "yield" << std::endl;
        co_await yield_awaitable{};
    }
    co_return;
}
async_task task2(int a)
{
    
    for (int i = 0; ; i+=a) {
        std::cout << "task 2: " << i << std::endl;
        std::this_thread::sleep_for(std::chrono::duration<double>(0.1));
        co_await yield_awaitable();
    }
}


int main()
{

    auto& s = app_scheduler::get_instance();

    auto t1 = task1(1);
    auto t2 = task2(-1);

    s.schedule_all_suspended();
    while (true) {
        s.run_once();
        std::cout << "main" << std::endl;
    }

    return 0;
}