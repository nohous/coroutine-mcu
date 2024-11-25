#include "corocore/scheduler.h"
#include <iostream>
#include <thread>

using namespace adva::corocore;


async_task task1()
{
    for (int i = 0; ; i++) {
        std::cout << "task 1: " << i << std::endl;
        //std::this_thread::sleep_for(std::chrono::duration<double>(0.15));
        if (i % 10 == 0) co_await yield<scheduler<>>();
    }
}
async_task task2()
{
    for (int i = 0; ; i++) {
        std::cout << "task 2: " << i << std::endl;
        //std::this_thread::sleep_for(std::chrono::duration<double>(0.15));
        if (i % 10 == 0) co_await yield<scheduler<>>();
    }
}

int main()
{
    auto& s = scheduler<>::get_instance();

    /*for (int i = 0; i< 158; i++) {
        auto h = s.move_task(task1());
        if (!h) {
            std::cout << "Out of space " << i << std::endl;
            break;
        }
        auto t = async_task::from_handle(*h).i = 125;
        
    }*/

   auto h1 = s.move_task(task1());
   auto h2 = s.move_task(task2());
   s.run();

    return 0;
}