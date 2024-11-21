#include "sched.h"


Task task1() {
    for (int i = 0; ; i++) {
        std::cout << "task1(): " << i << std::endl;
        if (i ==100) {
            co_await TimedWait(TimedWait::seconds_t(1));
        } else {
            co_await Yield{};
        }
        if (i == 10000) co_return;
    }
    //scheduler.schedule(task1());
}

Task task2() {
    for (int i = 0; ; i++) {
        std::cout << "task2(): " << i << std::endl;
        co_await Yield{};
        if (i == 100000) co_return;
    }

    //scheduler.schedule(task2());
    
}

int main() {
    auto t1 = task1();
    auto t2 = task2();
    auto& scheduler = Scheduler::get_instance();
    scheduler.schedule(std::move(t1));
    scheduler.schedule(std::move(t2));
    scheduler.run();
    return 0;
}