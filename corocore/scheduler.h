#ifndef ADVAOS_SCHEDULER_H_
#define ADVAOS_SCHEDULER_H_

#include <iostream>
#include <coroutine>
#include <etl/pool.h>
#include <etl/queue.h>
#include <etl/optional.h>
#include <etl/static_assert.h>


namespace adva::corocore {

    enum class task_priority {
        LOW, MID, HIGH, ISR
    };
    
    enum class task_state {
        INACTIVE,
        MOVED,
        SUSPENDED,
        SCHEDULED,
        ACTIVE,
        ZOMBIE
    };

    class async_task {
    private:
        template <typename C> friend class scheduler;

        struct promise_type {
            async_task* task_ = nullptr;
            async_task get_return_object() { 
                auto task = async_task(std::coroutine_handle<promise_type>::from_promise(*this)); 
                task_ = &task;
                return task;
            }
            std::suspend_always initial_suspend() { return {}; }
            std::suspend_always final_suspend() noexcept { return {}; }
            //void return_void() {}
            void return_value(int v) { }

            void unhandled_exception() { std::terminate(); }
        };

    public:
        using handle_type = std::coroutine_handle<promise_type>;
        using promise_type = promise_type;
    private:
        handle_type handle_;
        task_state state_;

        void resume() { if (handle_ && !handle_.done()) handle_.resume(); }

    public:

        async_task(handle_type handle) : 
            handle_(handle),
            state_(task_state::INACTIVE) {}
        async_task(async_task&& other) noexcept : 
            handle_(other.handle_),
            state_(other.state_)
        {
            other.handle_ = nullptr;
            other.state_ = task_state::MOVED;
            std::cout << "move " << std::endl;
        }
        ~async_task() { if (handle_) handle_.destroy(); }

        static async_task& from_handle(handle_type const& h) { return *h.promise().task_; }

        async_task& operator=(async_task&& other) {
            std::cout << "=" << std::endl;
            handle_ = other.handle_;
            state_ = other.state_;
            other.handle_ = nullptr;
            std::cout << static_cast<int>(state_) << std::endl;
            other.state_ = task_state::MOVED;
            return *this;
        }
        bool operator==(std::coroutine_handle<> const& h) {
            return handle_ == h;
        }
        task_state state() const { return state_; }

        int i;

    };

    template <typename Impl>
    class timer_service {
    private:
        template <typename C> friend class scheduler;
    
    public:
        using impl_type = Impl;

    private: 
        impl_type& impl_;

    private:
        timer_service(impl_type& impl) : impl_(impl) { }
    };


    struct scheduler_config_default {
        static constexpr size_t task_pool_size = 16;
    };

    template <typename C=scheduler_config_default>
    class scheduler {
    public:
        using config_type = C;

    private:
        etl::pool<async_task, config_type::task_pool_size> task_pool; 
        etl::queue<async_task*, config_type::task_pool_size> scheduled_queue;
        async_task* active_task;
    
    private:
        scheduler() : active_task(nullptr) { }

    public:
        static scheduler& get_instance() { static scheduler<C> inst; return inst; }

        etl::optional<async_task::handle_type> move_task(async_task&& task) {
            auto t = task_pool.allocate();
            if (t == nullptr) return etl::nullopt;
            std::cout << "move_task(): " << static_cast<int>(t->state_) << std::endl;
            *t = std::move(task);
            t->state_ = task_state::SUSPENDED;
            std::cout << "move_task() 2: " << static_cast<int>(t->state_) << std::endl;
            schedule_suspended(t->handle_);
            return t->handle_;
        }

        bool schedule_suspended(async_task::handle_type const& h) {
            auto& t = async_task::from_handle(h);
            std::cout << "schedule_suspended(): " << static_cast<int>(t.state_) << std::endl;
            if (t.state_ != task_state::SUSPENDED) return false;
            t.state_ = task_state::SCHEDULED;
            scheduled_queue.push(&t);
            std::cout << scheduled_queue.size() << std::endl;
            return true;
        }

        bool schedule_active(async_task::handle_type const &h) {
            auto& t = async_task::from_handle(h);
            if (t.state_ != task_state::ACTIVE) return false;
            t.state_ = task_state::SCHEDULED;
            scheduled_queue.push(&t);
            return true;
        }

        void run() {
            while (true) {
                if (!scheduled_queue.empty()) {
                    auto t = scheduled_queue.front();
                    scheduled_queue.pop();
                    t->state_ = task_state::ACTIVE;

                    t->resume();
                    
                    if (t->handle_.done()) {
                        t->state_ = task_state::INACTIVE;
                        task_pool.destroy(t);
                    } else if (t->state_ == task_state::ACTIVE) {
                        // Should be either suspended or scheduled, so force zombie
                        t->state_ = task_state::ZOMBIE;
                    }
                }
            }
        }

    };

    template <typename S=scheduler<> >
    struct yield {
        bool await_ready() { return false; }
        void await_suspend(async_task::handle_type h) { 
            S::get_instance().schedule_active(h);
        }
        void await_resume() {}
    };
}

#endif