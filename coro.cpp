#include "sched.h"
#include "fake_uart.h"

Task task1() {
    for (int i = 0; ; i++) {
        std::cout << "task1(): " << i << std::endl;
        if (i % 100 == 0) {
            co_await TimedWait(TimedWait::seconds_t(1));
        } else {
            co_await Yield{};
        }
        //if (i == 10000) co_return;
    }
    //scheduler.schedule(task1());
}

Task task2() {
    for (int i = 0; ; i++) {
        std::cout << "task2(): " << i << std::endl;
        co_await Yield{};
        if (i == 100000) co_return;
        //co_await TimedWait(TimedWait::seconds_t(2));
    }

    //scheduler.schedule(task2());
}


Task uart_task() {
    while (true) {
        //auto res =  " ";
        co_await TimedWait(TimedWait::seconds_t(0.1));
        //int res = co_await async_read_uart();
        std::cout << " data from uart: " <<  std::endl;
    }
    co_return;
}

int main() {
    auto t1 = task1();
    auto t2 = task2();
  //  auto tuart = uart_task();
    auto& scheduler = Scheduler::get_instance();
    scheduler.schedule(std::move(t1));
    scheduler.schedule(std::move(t2));
 //   scheduler.schedule(std::move(tuart));
    scheduler.run();
    return 0;
}