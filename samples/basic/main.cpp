
#define coronimo_DEBUG
#include <coronimo/scheduler.h>
#include <iostream>
#include <thread>
#include <random>

#if 1
using namespace adva;
namespace cc = coronimo;

struct clock_std_chrono {
    using time_type = std::chrono::high_resolution_clock::time_point;
    using duration_type = std::chrono::high_resolution_clock::duration;

    time_type now() { return std::chrono::high_resolution_clock::now(); }
};

struct clock_tick {
    using time_type = int;
    using duration_type = int;

    time_type now() { return now_; } 

    // TODO: show in example how to handle counter overflows
    void advance() { 
        now_++; 
    }
private:
    int now_ = 0;
};

static_assert(cc::Clock<clock_std_chrono>, "This is no clock");
static_assert(cc::Clock<clock_tick>, "This is no clock");


/* App scheduler configuration and type specializations */
struct app_scheduler_config {
    static constexpr size_t max_task_count = 16;
    static constexpr size_t timer_count = 32;
};
using app_scheduler = cc::scheduler<app_scheduler_config>;
using yield = cc::yield_awaitable<app_scheduler>;
using event = cc::event<app_scheduler>;
using timer_service = cc::timer_service<clock_std_chrono, app_scheduler>;
using async_task = app_scheduler::async_task_type;
using async_func = app_scheduler::async_func_type;
// Note: IMPORTANT, DO NOT DELETE
template <typename ...A> struct app_any_of : cc::any_of_awaitable<app_scheduler, A...> {};
template <typename ...A> app_any_of(A&&...) -> app_any_of<A...>;



using event_awaitable = cc::event_awaitable<app_scheduler>;

using namespace std::chrono_literals;
using namespace std::chrono;

int f1() {
    int x[1000];

    std::random_device rd;  // a seed source for the random number engine
    std::mt19937 gen(rd()); // mersenne_twister_engine seeded with rd()
    std::uniform_int_distribution<> distrib(1, 6);
    for (std::size_t i = 1; i < std::size(x); i++) {
        x[i] = distrib(gen) + x[i - 1];
    }

    std::cout << x[999] << std::endl;

    return x[999];
}

void f2() {
    int x[1000];
}

int r = 0, s = 0;

async_func sleepy_func(timer_service &ts) {

    pr_debug("post sleep");
    co_return;
}

async_func drowsy_func(timer_service& ts) {
    auto sleep = sleepy_func(ts);
    pr_debug("sub-task r:" << r << ", sizeof(sleep)= " << sizeof(sleep));
    co_await sleep;
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
        //if (i % 8 == 0) {
            {
                //auto sa = ts.sleep_for(10ms);
                //auto ea = e.create_awaitable("task1 e lvalue");
                //co_await any_of<event_awaitable, event_awaitable>{ ts.sleep_for(100ms), e.create_awaitable() };
                //pr_debug("creating any_of...");
                pr_debug("waiting on any_of...");
                //co_await any_of{  e.create_awaitable("task1 e prvalue"), ts.sleep_for(2000ms, "task1 timer")  };
                pr_debug("post any_of");
                //co_await any;
                
                //co_await any_of<event_awaitable, event_awaitable>( sa, ea );
            }
            pr_debug("mlemlemle");
        //}
        pr_debug(i);
        //pr_debug("calling");
        //co_await drowsy_func(ts);
        //pr_debug("task 1: returned");
        //f();
        //co_await ts.sleep_for(100ms);
    }
    co_return;
}



async_task task2(int a, timer_service& ts, event& e)
{
    for (int i = 0; ; i+=a) {
        auto start = system_clock::now();
        auto t1 = ts.sleep_for(150ms);
        auto t2 = ts.sleep_for(50ms);

        auto a1 = t1.operator co_await();
        auto a2 = t2.operator co_await();
        pr_debug("Pre tuple_embed_test");

        //auto tup = cc::direct_tuple{t1, t2, ts.sleep_for(300ms)};

        //auto any = cc::any_of_awaitable{a1, a2};
        auto any = app_any_of{a1, a2};

        co_await any;



        //auto tuple_embed = tuple_embed_test{ t1, ts.sleep_for(300ms) };

        //pr_debug(tuple_embed.s);
        pr_debug("Post tuple_embed_test");
        //auto any = any_of{ a1, a2 };

        auto end = system_clock::now();

        pr_debug(duration_cast<microseconds>(end - start) .count() / 1000.);
        if (-(i - 1) % 20 == 0) {
            e.activate();
            pr_debug("Event activated");
        }
    }
    co_return;
}

int main()
{
    auto& s = app_scheduler::get_instance();

    event e{};

    clock_std_chrono c;

    timer_service ts{c};

 //   auto t1 = task1(1, ts, e, f1);
    auto t2 = task2(-1, ts, e);

   /* if (t1.invalid()) {
    }*/
    
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
#else
int main(){
    std::cout << "Hello, World!" << std::endl;
    return EXIT_SUCCESS;
}
#endif