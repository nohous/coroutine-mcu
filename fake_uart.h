#ifndef FAKE_UART_H_
#define FAKE_UART_H_
#include "sched.h"

struct UartWait : TimedWait {

    int i;
    std::string data;
    UartWait() : TimedWait(TimedWait::seconds_t(1)), i(0) {
    }

    int await_resume() { 
        //data = std::string("UART data ") + std::to_string(i++);
        return 42;
    }
};

TimedWait async_read_uart() {
    return TimedWait(TimedWait::seconds_t(1));
}

#endif
