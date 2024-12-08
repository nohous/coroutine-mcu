#include "corocore/scheduler.h"
#include <iostream>
#include <thread>

using namespace adva;
namespace cc = corocore;

struct app_scheduler_config {
    static constexpr size_t couroutine_alloc_granularity = 32;
    static constexpr size_t timer_count = 32;
};


using app_scheduler = cc::scheduler<app_scheduler_config>;
using yield_awaitable = cc::yield_awaitable<app_scheduler>;
using async_task = cc::async_task<app_scheduler>;

async_task task1(int a)
{
    int b[128];
    for (int i = 0; ; i+=a) {
        std::cout << "task 1: " << b[i % 128] << std::endl;

        std::this_thread::sleep_for(std::chrono::duration<double>(0.15));
        std::cout << "yield" << std::endl;
        co_await yield_awaitable();
    }
}
async_task task2(int a)
{
    
    for (int i = 0; ; i+=a) {
        std::cout << "task 2: " << i << std::endl;
        std::this_thread::sleep_for(std::chrono::duration<double>(0.1));
        co_await yield_awaitable();
    }
   co_return;
}


int main()
{

    auto& s = app_scheduler::get_instance();


    async_task t1{task1(1)};
    //async_task t2 = task2(-1);

    std::cout << sizeof(async_task) << " " << std::endl;

    std::cout << "t1 " << &t1 << std::endl;

    s.register_task(t1);
    //s.move_task(std::move(t2));
    s.run();

    return 0;
}