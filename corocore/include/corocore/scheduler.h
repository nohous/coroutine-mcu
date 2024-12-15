#ifndef ADVAOS_SCHEDULER_H_
#define ADVAOS_SCHEDULER_H_

#include <coroutine>
#include <functional>
#include <utility>
#include <compare>
#include <etl/variant.h>
#include <etl/flat_set.h>
#include <etl/queue.h>
#include <etl/intrusive_stack.h>
#include <etl/intrusive_links.h>
#include <etl/intrusive_forward_list.h>

#ifdef COROCORE_DEBUG

#include <iostream>
#include <source_location> 
//#define pr_debug(x) { auto loc = std::source_location::current(); std::cout << loc.function_name()  << x << std::endl; }
#define pr_debug(x) { auto loc = std::source_location::current(); std::cout << __func__ << ": " << x << std::endl; }

#else
#define pr_debug(x)
#endif

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

template <typename S>
class async_task;

//template <typename S>
//struct async_task<S>::struct promise_type;

struct exception {
    exception(char const* what) : what_(what) {}

protected:
    char const* what_;
};

template <typename S>
class coroutine {
public:
    struct promise_type;
   
    using scheduler_type = S;
    
    using coroutine_type = coroutine<scheduler_type>;
    using coroutine_handle_type = std::coroutine_handle<coroutine_type::promise_type>;

    using async_task_type = async_task<scheduler_type>;
    using async_task_handle_type = async_task_type::async_task_handle_type;

    struct promise_type : public etl::forward_link<0> {
    private:
        friend coroutine_type;
        friend scheduler_type;

        async_task_handle_type task_handle_;

    public:

        promise_type() : task_handle_(nullptr) {}
        static coroutine_type get_return_object_on_allocation_failure()
        {
            return coroutine(coroutine_type::null_handle);
        }
        void* operator new(std::size_t n) noexcept
        {
            pr_debug("[" << n << "]");
            void *mem = new char[n];
            return mem;
        }
        ~promise_type() {
        }

        async_task_handle_type get_task_handle() {
            return task_handle_;
        }

        // Promise interface
        coroutine_type get_return_object() noexcept {
            auto h = coroutine_handle_type::from_promise(*this);
            return coroutine(h);
        }
        std::suspend_always initial_suspend() noexcept { 
            pr_debug("");
            return {}; 
        }
        std::suspend_always final_suspend() noexcept { 
            pr_debug("");
            return {}; 
        }
        void return_void() noexcept {
            pr_debug("coroutine: RETURN");

            if (task_handle_) {
                task_handle_.promise().await_stack_pop();
                task_handle_.promise().resume();
            }
        }
        //exception return_value(exception a);
        void unhandled_exception() { std::terminate(); }

    };

private:
    explicit coroutine(coroutine_handle_type& h) noexcept : handle_(std::move(h)) { }
    promise_type& promise() const noexcept { return handle_.promise(); }

private:
    coroutine_handle_type handle_;

    static coroutine_handle_type null_handle;
public:

    bool await_ready() { return false; }
    coroutine_handle_type await_suspend(async_task_handle_type awaiter_handle) {
        pr_debug("coroutine: SUSPEND FROM TASK");
        promise().task_handle_ = awaiter_handle;
        promise().task_handle_.promise().await_stack_push(promise());
        return handle_;
    }
    coroutine_handle_type await_suspend(coroutine_handle_type awaiter_handle) {
        pr_debug("coroutine: SUSPEND FROM CORO");
        promise().task_handle_ = awaiter_handle.promise().task_handle_;
        promise().task_handle_.promise().await_stack_push(promise());
        return handle_;
    }
    void await_resume() {
        pr_debug("coroutine: RESUME");
        promise().task_handle_.promise().await_stack_pop();
        promise().task_handle_ = nullptr;
    }

};
template <typename S>
coroutine<S>::coroutine_handle_type coroutine<S>::null_handle{nullptr};

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

    using return_type = etl::variant<void, etl::exception>;
    using scheduler_type = S;
    
    using async_task_type = async_task<scheduler_type>;
    using async_task_handle_type = std::coroutine_handle<async_task_type::promise_type>;

    using coroutine_type = coroutine<scheduler_type>;
    using coroutine_handle_type = coroutine_type::coroutine_handle_type;
    //using coroutine_handle_type = std::coroutine_handle<coroutine_type::promise_type>;

    friend scheduler_type;

    struct promise_type {
    private:
        //using coroutine_stack = etl::intrusive_stack<coroutine_type::promise_type, etl::forward_link<0> >;
        
        using coroutine_promise_type = coroutine_type::promise_type;
        using coroutine_stack = etl::intrusive_stack<coroutine_promise_type, etl::forward_link<0> >;

        friend scheduler_type;
        friend async_task_type;
        friend coroutine_type;

        task_state state_;
        coroutine_stack await_stack_;

    public:

        promise_type() : state_(task_state::INACTIVE) {}
        static async_task_type get_return_object_on_allocation_failure()
        {
            return async_task_type(async_task_type::null_handle);
        }
        void* operator new(std::size_t n) noexcept
        {
            pr_debug("[" << n << "]");
            void *mem = new char[n];
            return mem;
        }
        ~promise_type() {
            scheduler_type::get_instance().erase_task(async_task_handle_type::from_promise(*this));
        }

        async_task_handle_type get_task_handle() {
            return async_task_handle_type::from_promise(*this);
        }
        void await_stack_push(coroutine_promise_type& promise) {
            pr_debug("");
            await_stack_.push(promise);
        }
        void await_stack_pop() {
            pr_debug("");
            if (!await_stack_.empty()) await_stack_.pop();
        }
        void resume() {
            state_ = task_state::ACTIVE;
            if (await_stack_.empty()) {
                async_task_handle_type::from_promise(*this).resume();
            } else {
                coroutine_handle_type::from_promise(await_stack_.top()).resume();
            }
        }

        // Promise interface
        async_task_type get_return_object() noexcept { 
            auto h = async_task_handle_type::from_promise(*this);
            if (!scheduler_type::get_instance().insert_task(h)) {
                h.destroy();
                h = async_task_type::null_handle;
            }
            // Prvalue is materialized on caller's stack
            return async_task_type(h);
        }
        std::suspend_always initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void return_void() noexcept {}
        //exception return_value(exception a);
        void unhandled_exception() { std::terminate(); }

    };

private:
    async_task_handle_type handle_;

    static async_task_handle_type null_handle;

private:
    explicit async_task(async_task_handle_type& h) noexcept : handle_(std::move(h)) { }
    promise_type& promise() const noexcept { return handle_.promise(); }

public:
    async_task() = delete;
    async_task(const async_task&) = delete;
    async_task& operator=(const async_task&) = delete;

    async_task(async_task&& other) noexcept 
        : handle_(std::exchange(other.handle_, nullptr)) 
    {}
    async_task& operator=(async_task&& other) noexcept {
        if (this != &other) {
            if (handle_) handle_.destroy();
            handle_ = std::exchange(other.handle_, nullptr);
        }
        return *this;
    }

    ~async_task() noexcept { 
        if (handle_) {
            handle_.destroy(); 
        }
    }
    task_state state() const noexcept { 
        if (handle_) {
            return promise().state_; 
        } else {
            return task_state::ZOMBIE;
        }
    }
    bool invalid() const noexcept {
        return state() == task_state::ZOMBIE;
    }
};

template <typename S>
async_task<S>::async_task_handle_type async_task<S>::null_handle{nullptr};

template <SchedulerConfig C> 
class scheduler {
public:
    using config_type = C;
    using scheduler_type = scheduler<config_type>;
    using async_task_type = async_task<scheduler_type>;
    using async_task_handle_type = async_task_type::async_task_handle_type;
    using coroutine_type = coroutine<scheduler_type>;
    using coroutine_handle_type = coroutine_type::coroutine_handle_type;

    template <typename A, typename S> friend struct scheduler_friend;
    friend async_task_type;
    friend coroutine_type;

private:
    using handle_set = etl::flat_set<async_task_handle_type, config_type::max_task_count>;
    using scheduled_queue = etl::queue<async_task_handle_type, config_type::max_task_count>;

    handle_set handles_;
    scheduled_queue scheduled_;

private:
    scheduler() { }

    bool insert_task(async_task_handle_type& h) {
        if (handles_.contains(h)) return false;

        h.promise().state_ = task_state::SUSPENDED;
        auto [it, result] = handles_.insert(h);
        return result;
    }
    bool erase_task(async_task_handle_type const& h) {
        return handles_.erase(h) > 0;
    }

    bool schedule(async_task_handle_type& h, auto&& pred) {
        if (!handles_.contains(h)) return false;

        auto& p = h.promise();
        if (!pred(p.state_)) return false;
        
        p.state_ = task_state::SCHEDULED;
        scheduled_.push(h);
        return true;
    }
    bool schedule(coroutine_handle_type& handle, auto&& pred) {
        auto task_handle = handle.promise().task_handle_;
        if (!task_handle) return false;
        return schedule(task_handle, pred);
    }

    bool suspend_active(async_task_handle_type& h) {
        if (!handles_.contains(h)) return false;

        auto& p = h.promise();
        if (p.state_ != task_state::ACTIVE) return false;

        p.state_ = task_state::SUSPENDED;
        return true;
    }
    bool suspend_active(coroutine_handle_type& handle) {
        auto task_handle = handle.promise().task_handle_;
        if (!task_handle) return false;
        return suspend_active(task_handle);
    }

public:
    static scheduler_type& get_instance() { static scheduler_type inst; return inst; }

    void schedule_all_suspended() {
        for (auto& h: handles_) {
            if (h.promise().state_ != task_state::SUSPENDED) continue;
            scheduled_.push(h);
        }
    }
    bool run_once() {
        // TODO: handle events

        if (scheduled_.empty()) {
            // TODO: idle task
            return false;
        }

        auto& task_handle = scheduled_.front();
        scheduled_.pop();
        
        auto& task_promise = task_handle.promise();

        task_promise.resume();
        
        if (task_handle.done()) {
            task_promise.state_ = task_state::DONE;
        } else if (task_promise.state_ == task_state::ACTIVE) {
            // Should be either suspended or scheduled, so force zombie
            task_promise.state_ = task_state::ZOMBIE;
        } 
        return true;
    }

    void run_one() {
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

template <typename A, typename S>
concept Awaitable = requires(A a, typename S::async_task_handle_type& h) {
    { a.await_ready() } -> std::convertible_to<bool>;
    { a.await_suspend(h) };
    { a.await_resume() };
};

template <typename D, typename S>
struct scheduler_friend {
    using async_task_handle_type = S::async_task_handle_type;

    scheduler_friend(scheduler_friend const& other) : scheduler_friend() { }
    scheduler_friend& operator=(scheduler_friend const& other) { 
        return *this; 
    }
    scheduler_friend() : s_(S::get_instance()) { 
        static_assert(Awaitable<D, S> || Service<D, S>, "derived class is not compatible");
    }

private:
    S& s_;

protected:
    template <typename H>
    bool schedule_active(H& h) { 
        return s_.schedule(h, [](task_state state) { return state == task_state::ACTIVE; }); 
    }
    template <typename H>
    bool schedule_suspended(H& h) { 
        return s_.schedule(h, [](task_state state) { return state == task_state::SUSPENDED; }); 
    }
    template <typename H>
    bool suspend_active(H& h) { return s_.suspend_active(h); }

};

template <typename S>
struct yield_awaitable : public scheduler_friend<yield_awaitable<S>, S> {
    using async_task_handle_type = S::async_task_handle_type;
    using base_type = scheduler_friend<yield_awaitable<S>, S>;

    // Awaitable interface
    bool await_ready() { return false; }
    template <typename H>
    bool await_suspend(H& h) { return base_type::schedule_active(h); }
    void await_resume()  {}
};
// "Deduction guide":
// template <typename S> yield_awaitable(S) -> yield_awaitable<S>;

#if 0

template <typename S, typename... A>
struct combined_awaitable : public scheduler_friend<combined_awaitable<S, A...>, S> {
    using async_task_handle_type = typename S::async_task_handle_type;
    using base_type = scheduler_friend<combined_awaitable<S, A...>, S>;

    // Check that all template parameters A are Awaitable
    static_assert((Awaitable<A, S> && ...), "All variadic template parameters must be Awaitable");

    combined_awaitable(A&... awaitables) 
        : base_type()
        , handles_{}
        , refs_{awaitables...}
    {}

    // Awaitable interface
    bool await_ready() {
        return all_ready(std::index_sequence_for<A...>{});
    }

    bool await_suspend(async_task_handle_type& h) {
        return suspend_all(h, std::index_sequence_for<A...>{});
    }

    void await_resume() {
        resume_all(std::index_sequence_for<A...>{});
    }

private:
    template<size_t... Is>
    bool all_ready(std::index_sequence<Is...>) {
        return (std::get<Is>(refs_).await_ready() && ...);
    }

    template<size_t... Is>
    bool suspend_all(async_task_handle_type& h, std::index_sequence<Is...>) {
        bool any_suspended = false;
        ((
            [&]() {
                if (!std::get<Is>(refs_).await_ready()) {
                    if (std::get<Is>(refs_).await_suspend(h)) {
                        handles_.insert(h);
                        any_suspended = true;
                    }
                }
            }()
        ), ...);
        return any_suspended && base_type::suspend_active(h);
    }

    template<size_t... Is>
    void resume_all(std::index_sequence<Is...>) {
        (std::get<Is>(refs_).await_resume(), ...);
    }

    etl::flat_set<async_task_handle_type, sizeof...(A)> handles_;
    std::tuple<A&...> refs_;  // Store references to awaitables
};

// Deduction guide
template <typename S, typename... A>
combined_awaitable(A&...) -> combined_awaitable<S, A...>;

#endif

template <typename S>
struct event_awaitable : public scheduler_friend<event_awaitable<S>, S> {
    using async_task_handle_type = S::async_task_handle_type;
    using base_type = scheduler_friend<event_awaitable<S>, S>;

    event_awaitable() 
        : base_type(),  
          handle_(nullptr)
    {}
    event_awaitable(event_awaitable const& other) = delete;
    event_awaitable& operator=(event_awaitable const& other) = delete;
        
    void notify() { 
        if (handle_) {
            base_type::schedule_suspended(handle_); 
            handle_ = nullptr;
        }
    }

    // Awaitable interface
    bool await_ready() { return !!handle_; }
    template <typename H>
    bool await_suspend(H& h) {
        handle_ = h.promise().get_task_handle();
        return base_type::suspend_active(h);
    }
    void await_resume() {}

private:
    async_task_handle_type handle_;
};

template <typename C>
concept Clock = requires(C c, typename C::time_type t, typename C::duration_type d) {
    { c.now() } -> std::convertible_to<typename C::time_type>;
    { t + d } -> std::convertible_to<typename C::time_type>;
    { t - d } -> std::convertible_to<typename C::time_type>;
    { t <=> t } -> std::convertible_to<std::strong_ordering>;
    { d <=> d } -> std::convertible_to<std::strong_ordering>;
};

template <Clock C, typename S>
struct timer_service : public scheduler_friend<timer_service<C, S>, S> {
    using clock_type = C;
    using time_type = clock_type::time_type;
    using duration_type = clock_type::duration_type;
    using base_type = scheduler_friend<timer_service<C, S>, S>;
    using async_task_handle_type = S::async_task_handle_type;

    struct timer : etl::forward_link<0>{
        time_type time;
        event_awaitable<S> event;

        timer(time_type t) noexcept : time(t) {}

        bool operator<(timer const&  other) const {
            return time < other.time;
        }
    };

    timer_service(clock_type& clock) noexcept : 
        s_(S::get_instance()), 
        clock_(clock) 
    {}

    // Service interface
    bool run_once() {
        auto now = clock_.now();

        if (timers_.empty()) return false;

        auto& timer = timers_.front();
        if (now >= timer.time) {
            timer.event.notify();
            timers_.pop_front();
            timer_pool_.destroy(&timer);
            return true;
        }

        return false;
    }

    event_awaitable<S>& sleep_until(time_type time) {
        auto it = timers_.begin(), it_prev = timers_.begin();
        auto* timer = timer_pool_.create(time);

        if (timer == nullptr) return null_event;

        for ( ; it != timers_.end(); it_prev = it, it++) {
            if (time < it->time) break;
        }

        if (it == timers_.begin()) {
            timers_.push_front(*timer);
        } else {
            timers_.insert_after(it_prev, *timer);
        }
        return timer->event;
    }

    event_awaitable<S>& sleep_for(duration_type dur) {
        return sleep_until(clock_.now() + dur);
    }

private:
    S& s_;
    clock_type& clock_;
    etl::pool<timer, S::config_type::timer_count> timer_pool_;
    etl::intrusive_forward_list<timer, etl::forward_link<0> > timers_;

    static event_awaitable<S> null_event;
};
template <Clock C, typename S>
event_awaitable<S> timer_service<C, S>::null_event{};


}

#endif