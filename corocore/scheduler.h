#ifndef ADVAOS_SCHEDULER_H_
#define ADVAOS_SCHEDULER_H_

#include <iostream>
#include <coroutine>
#include <utility>
#include <tuple>
#include <chrono>
#include <etl/flat_set.h>
#include <etl/queue.h>
#include <etl/optional.h>
#include <etl/priority_queue.h>

namespace adva::corocore {

    namespace chr = std::chrono;

    template <typename C>
    concept SchedulerConfig = requires {
        { C::couroutine_alloc_granularity } -> std::convertible_to<size_t>;
        { C::timer_count } -> std::convertible_to<size_t>;
    };

    struct scheduler_config_default {
        static constexpr size_t couroutine_alloc_granularity = 16;
        static constexpr size_t timer_count = 16;
    };

    template <SchedulerConfig C = scheduler_config_default>
    class scheduler;


    enum class task_priority {
        LOW, MID, HIGH, ISR
    };
    
    enum class task_state {
        INACTIVE,
        SUSPENDED,
        SCHEDULED,
        ACTIVE,
        ZOMBIE
    };

    template <typename S>
    struct async_task_promise;

    template <typename S>
    class async_task : public std::coroutine_handle<async_task_promise<S>> {
    public:
        using scheduler_type = S;
        using promise_type = async_task_promise<scheduler_type>;
        using handle_type = std::coroutine_handle<promise_type>;
    private:
        friend scheduler_type;
        friend promise_type;

        task_state state_;

    public:
        async_task() : 
            state_(task_state::INACTIVE) {}
        async_task(handle_type const& h) : handle_type(std::move(h)) { 
            std::cout <<  __func__ << " move handle" << std::endl;
        }
        async_task(async_task&& other) noexcept : 
            handle_type(std::move(other)),
            state_(other.state_)
        {
            std::cout <<  __func__ << " move" << std::endl;
        }
        ~async_task() { }

        async_task& operator=(async_task&& other) {
            std::cout << __func__ << "=" << std::endl;
            state_ = other.state_;
            return *this;
        }
        task_state state() const { return state_; }

    };    
    
    template <typename S>
    struct async_task_promise {
        using scheduler_type = S;
        using async_task = async_task<scheduler_type>;

        async_task get_return_object() { 
            //auto task = async_task(std::coroutine_handle<promise>::from_promise(*this)); 
            return async_task::from_promise(*this); 
        }
        std::suspend_always initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void return_void() {}

        //void return_value(T val) { val = 42; }

            // ensure the use of non-throwing operator-new
        #if 0
        static async_task get_return_object_on_allocation_failure()
        {
            std::cerr << __func__ << '\n';
            //throw std::bad_alloc(); // or, return Coroutine(nullptr);
        }
        #endif
    
        // custom non-throwing overload of new
        void* operator new(std::size_t n) noexcept
        {
            std::cout << __func__ <<  " " << sizeof(async_task) << " " << n << std::endl;
            return new char[n];
        }

        void unhandled_exception() { std::terminate(); }

        ~async_task_promise() {
            std::cout << __func__ << std::endl;
        }
    };



    template <SchedulerConfig C>
    class scheduler {
    public:
        using config_type = C;
        using scheduler_type = scheduler<config_type>;
        using async_task_type = async_task<scheduler_type>;
        template <typename A, typename S> friend struct scheduler_friend;

    private:
        using registered_tasks_set = etl::flat_set<async_task_type*, 16>;
        using scheduled_queue = etl::queue<async_task_type*, 16>;
        using time_point = chr::time_point<chr::high_resolution_clock>;
        using duration_us = chr::duration<chr::microseconds>;

        using timer = std::tuple<time_point, async_task_type*>;
        using timer_queue = etl::priority_queue<timer, config_type::timer_count>;

        registered_tasks_set registered_tasks;
        scheduled_queue scheduled_tasks;
        timer_queue timers;
        async_task_type* active_task;
    
    private:
        scheduler() : active_task(nullptr) { }

        void priv_func() { }

        /*bool schedule_suspended(async_task&& t);
        bool schedule_active(async_task&& t);*/

        bool schedule_suspended(async_task_type& t) {
            if (t.state_ != task_state::SUSPENDED) return false;
            t.state_ = task_state::SCHEDULED;
            scheduled_tasks.push(&t);
            std::cout << scheduled_tasks.size() << std::endl;
            return true;
        }

        bool schedule_active(async_task_type& t) {
            if (t.state_ != task_state::ACTIVE) return false;
            t.state_ = task_state::SCHEDULED;
            scheduled_tasks.push(&t);

            //auto tp = std::chrono::high_resolution_clock::now() + duration_us(1000);
            //timer_queue.emplace(std::make_tuple<timer_type>(tp, nullptr));
            return true;
        }


    public:
        static scheduler_type& get_instance() { static scheduler_type inst; return inst; }
        /*bool move_task(async_task&& task);
        void run();*/

        bool register_task(async_task_type& t) {
            std::cout << __func__ << " register " << &t << std::endl;
            registered_tasks.insert(&t);
            t.state_ = task_state::SUSPENDED;
            schedule_suspended(t);
            return true;
        }

        void run() {
            while (true) {
                if (!scheduled_tasks.empty()) {
                    auto& t = *scheduled_tasks.front();
                    scheduled_tasks.pop();
                    t.state_ = task_state::ACTIVE;

                    std::cout << __func__ << " resume" << std::endl;
                    t.resume();
                    
                    if (t.done()) {
                        registered_tasks.erase(&t);
                        t.destroy();
                        t.state_ = task_state::INACTIVE;
                    } else if (t.state_ == task_state::ACTIVE) {
                        // Should be either suspended or scheduled, so force zombie
                        t.state_ = task_state::ZOMBIE;
                    }
                }
            }
        }

    };



    template <typename A, typename S>
    concept Awaitable = requires(A a, async_task<S>::handle_type& t) {
        { a.await_ready() } -> std::convertible_to<bool>;
        { a.await_suspend(t) };
        { a.await_resume() };
        { a.scheduler_callable() };
    };

    template <typename A, typename S>
    struct scheduler_friend {
        scheduler_friend() : s_(S::get_instance()) { 
            static_assert(Awaitable<A, S>, "A is not awaitable");
        }
    private:
        S& s_;
    protected:
        bool schedule_active(async_task<S>& t) { return s_.schedule_active(t); }
    };

    template <typename S>
    struct yield_awaitable : public scheduler_friend<yield_awaitable<S>, S> {
        using base_type = scheduler_friend<yield_awaitable<S>, S>;
        bool await_ready() { return false; }
        bool await_suspend(async_task<S>::handle_type& h) { 
            std::cout << "suspend" << std::endl;
            if (!base_type::schedule_active(static_cast<async_task<S>&>(h))) { 
                std::cout << "E" << std::endl;
            }
            return true;
        }
        void await_resume()  { }
        void scheduler_callable()  { }

    };
    template <typename S> yield_awaitable(S) -> yield_awaitable<S>;
}

#endif