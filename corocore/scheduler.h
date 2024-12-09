#ifndef ADVAOS_SCHEDULER_H_
#define ADVAOS_SCHEDULER_H_

#include <iostream>
#include <coroutine>
#include <functional>
#include <utility>
#include <tuple>
#include <array>
#include <etl/utility.h>
#include <etl/flat_set.h>
#include <etl/queue.h>
#include <etl/priority_queue.h>
#include <etl/intrusive_forward_list.h>
#include <etl/optional.h>

namespace adva::corocore {

template <typename C>
concept SchedulerConfig = requires {
    { C::max_task_count } -> std::convertible_to<size_t>;
    { C::timer_count } -> std::convertible_to<size_t>;
};

struct scheduler_config_default {
    static constexpr size_t max_task_count = 16;
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
    DONE,
    ZOMBIE,
};

template <typename S>
class async_task {
public:
    struct promise_type;

    using scheduler_type = S;
    using async_task_type = async_task<scheduler_type>;
    using handle_type = std::coroutine_handle<async_task_type::promise_type>;

    struct promise_type {
    private:
        friend scheduler_type;

        task_state state_;

    public:

        promise_type() : state_(task_state::INACTIVE) {}
        #if 0
        static async_task get_return_object_on_allocation_failure()
        {
            std::cerr << __func__ << '\n';
            //throw std::bad_alloc(); // or, return Coroutine(nullptr);
        }
        #endif
        void* operator new(std::size_t n) noexcept
        {
            void *mem = new char[n];
            std::cout << __func__ <<  " " << mem << " " << n << std::endl;
            return mem;
        }
        ~promise_type() {
            scheduler_type::get_instance().erase_task(handle_type::from_promise(*this));
            std::cout << __func__ << std::endl;
        }

        // Promise interface
        async_task_type get_return_object() noexcept { 
            std::cout << __func__ << " gro " << std::endl;
            
            auto h = handle_type::from_promise(*this);
            scheduler_type::get_instance().insert_task(h);
            return async_task_type(h); // Prvalue is materialized on caller's stack
        }
        std::suspend_always initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void return_void() noexcept {}
        void unhandled_exception() { std::terminate(); }

    };

private:
    friend scheduler_type;

    handle_type handle_;

private:
    explicit async_task(handle_type& h) noexcept : handle_(std::move(h)) { 
        std::cout <<  __func__ << " " << this << " move handle " << &h << std::endl;
    }

    promise_type& promise() noexcept { return handle_.promise(); }

public:
    async_task() = delete;
    async_task(const async_task&) = delete;
    async_task& operator=(const async_task&) = delete;

    async_task(async_task&& other) noexcept 
        : handle_(std::exchange(other.handle_, nullptr))
    {
        std::cout <<  __func__ << " move" << std::endl;
    }
    async_task& operator=(async_task&& other) noexcept {
        if (this != &other) {
            if (handle_) handle_.destroy();
            handle_ = std::exchange(other.handle_, nullptr);
        }
        return *this;
    }

    ~async_task() noexcept { std::cout << __func__ << std::endl; 
        if (handle_) handle_.destroy(); }
    task_state state() const noexcept { return promise()._state; }

};    

template <SchedulerConfig C> 
class scheduler {
public:
    using config_type = C;
    using scheduler_type = scheduler<config_type>;
    using async_task_type = async_task<scheduler_type>;
    using handle_type = async_task_type::handle_type;

    template <typename A, typename S> friend struct scheduler_friend;
    friend async_task_type;

private:
    using handle_set = etl::flat_set<handle_type, config_type::max_task_count>;
    using scheduled_queue = etl::queue<handle_type, config_type::max_task_count>;

    handle_set handles_;
    scheduled_queue scheduled_;

private:
    scheduler() { }

    bool insert_task(handle_type& h) {
        if (handles_.contains(h)) return false;

        h.promise().state_ = task_state::SUSPENDED;
        auto [it, result] = handles_.insert(h);
        return result;
    }
    bool erase_task(handle_type const& h) {
        return handles_.erase(h) > 0;
    }

    bool schedule(handle_type& h, auto&& pred) {
        if (!handles_.contains(h)) return false;

        auto& p = h.promise();
        if (!pred(p.state_)) return false;
        
        p.state_ = task_state::SCHEDULED;
        scheduled_.push(h);
        return true;
    }
    bool suspend_active(handle_type& h) {
        if (!handles_.contains(h)) return false;

        auto& p = h.promise();
        if (p.state_ != task_state::ACTIVE) return false;

        p.state_ = task_state::SUSPENDED;
        return true;
    }

public:
    static scheduler_type& get_instance() { static scheduler_type inst; return inst; }

    void schedule_all_suspended() {
        for (auto& h: handles_) {
            if (h.promise().state_ != task_state::SUSPENDED) continue;
            scheduled_.push(h);
        }
    }
    void run_once() {
        // TODO: handle events

        if (scheduled_.empty()) {
            // TODO: idle task
            return;
        }

        auto& h = scheduled_.front();
        scheduled_.pop();
        
        auto& p = h.promise();
        p.state_ = task_state::ACTIVE;

        h.resume();
        
        if (h.done()) {
            p.state_ = task_state::DONE;
        } else if (p.state_ == task_state::ACTIVE) {
            // Should be either suspended or scheduled, so force zombie
            p.state_ = task_state::ZOMBIE;
        } 
    }
    void run() {
        schedule_all_suspended();

        for ( ; ; ) {
            run_once();
        }
    }

};

template <typename T, typename S>
concept Service = requires(T s) {
    { s.run_once() } -> std::convertible_to<bool>;
};

template <typename T, typename S>
concept Awaitable = requires(T a, typename S::handle_type& h) {
    { a.await_ready() } -> std::convertible_to<bool>;
    { a.await_suspend(h) };
    { a.await_resume() };
};

template <typename D, typename S>
struct scheduler_friend {
    using handle_type = S::handle_type;

    scheduler_friend(scheduler_friend const& other) : scheduler_friend() { }
    scheduler_friend& operator=(scheduler_friend const& other) { 
        return *this; 
    }
    scheduler_friend() : s_(S::get_instance()) { 
        static_assert(
            Awaitable<D, S> || Service<D, S>,
            "D is not compatible");
    }

private:
    S& s_;

protected:
    bool schedule_active(handle_type& h) { 
        return s_.schedule(h,[](task_state state) { return state == task_state::ACTIVE; }); 
    }
    bool schedule_suspended(handle_type& h) { 
        return s_.schedule(h,[](task_state state) { return state == task_state::SUSPENDED; }); 
    }
    bool suspend_active(handle_type& h) { return s_.suspend_active(h); }

};

template <typename S>
struct yield_awaitable : public scheduler_friend<yield_awaitable<S>, S> {
    using handle_type = S::handle_type;
    using base_type = scheduler_friend<yield_awaitable<S>, S>;

    // Awaitable interface
    bool await_ready() { return false; }
    bool await_suspend(handle_type& h) { return base_type::schedule_active(h); }
    void await_resume()  {}
};
// template <typename S> yield_awaitable(S) -> yield_awaitable<S>;

template <typename S>
struct event_awaitable : public scheduler_friend<event_awaitable<S>, S> {
    using handle_type = S::handle_type;
    using base_type = scheduler_friend<event_awaitable<S>, S>;

    event_awaitable() : base_type(),  h_(nullptr) {}
    event_awaitable(event_awaitable const& other) noexcept : base_type(), h_(other.h_) {}
    event_awaitable& operator=(event_awaitable const& other) noexcept {
        h_ = other.h_;
        return *this;
    }
        
    void activate() { if (h_) base_type::schedule_suspended(h_); }

    // Awaitable interface
    bool await_ready() { return false; }
    bool await_suspend(handle_type& h) {
        h_ = h;
        return base_type::suspend_active(h);
    }
    void await_resume()  { }

private:
    handle_type h_;
};

template <typename C>
concept Clock = requires(C c, typename C::time_type t) {
    { c.now() } -> std::convertible_to<typename C::time_type>;
    //{ tt <=> tt } -> std::three_way_comparable;
    { t < t } -> std::convertible_to<bool>;
    { t >= t } -> std::convertible_to<bool>;
};

template <Clock C, typename S>
struct timer_service : public scheduler_friend<timer_service<C, S>, S> {
    using clock_type = C;
    using time_type = clock_type::time_type;
    using base_type = scheduler_friend<timer_service<C, S>, S>;
    using handle_type = S::handle_type;

    struct timer : etl::forward_link<0>{
        time_type t;
        event_awaitable<S> e;

        timer(time_type t) noexcept : t(t) {}

        //timer(time_type t, event_awaitable<S>&& e) : t(t), e(std::move(e)) {}
        timer(timer&& other) noexcept 
            : t(std::move(other.t)), 
              e(std::move(other.e)) 
        {}
        
        timer& operator=(timer&& other) noexcept {
            if (this != &other) {
                t = std::move(other.t);
                e = std::move(other.e);
            }
            return *this;
        }

        bool operator<(timer const&  other) const {
            return t < other.t;
        }
    };

    timer_service(clock_type& clock) noexcept : 
        s_(S::get_instance()), 
        clock_(clock) 
    {}

    // Service interface
    bool run_once() {
        auto now = clock_.now();

        std::cout << "timer_service::run_once()" << std::endl;
        #if 1
        if (timers_.empty()) return false;

        auto& timer = timers_.front();
        std::cout << "timer " << timer.t << ", now " << now <<  std::endl;
        if (now >= timer.t) {
            timer.e.activate();
            timers_.pop_front();
            timer_pool_.destroy(&timer);
            std::cout << "pop" << std::endl;
            return true;
        }
        #endif

        return false;
    }

    event_awaitable<S>& sleep_until(time_type t) {
        auto end = timers_.end();
        auto it  = timers_.begin();
        auto it_prev = it;
        auto* tim = timer_pool_.create(t);

        std::cout << "sleep_until" << std::endl;

        if (tim == nullptr) { /* tell your grandma */ }

        for ( ; it != end; it++) {
            if (it->t < t) break;
            it_prev = it;
        }

        // TODO: un-uglify
        if (it_prev == timers_.begin()) {
            timers_.push_front(*tim);
        } else {
            timers_.insert_after(it_prev, *tim);
        }
        return tim->e;
    }

    event_awaitable<S>& sleep_for(time_type dur) {
        return sleep_until(clock_.now() + dur);
    }

private:
    S& s_;
    clock_type& clock_;
    //etl::priority_queue<timer, S::config_type::timer_count> timers_;
    etl::pool<timer, S::config_type::timer_count> timer_pool_;
    etl::intrusive_forward_list<timer, etl::forward_link<0> > timers_;
};

}

#endif