#ifndef ADVAOS_SCHEDULER_H_
#define ADVAOS_SCHEDULER_H_

#include <coroutine>
#include <functional>
#include <utility>
#include <compare>
#include <tuple>
#include <etl/variant.h>
#include <etl/flat_set.h>
#include <etl/queue.h>
#include <etl/intrusive_stack.h>
#include <etl/intrusive_links.h>
#include <etl/intrusive_forward_list.h>
#include <etl/intrusive_list.h>

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
/**
 * @brief A coroutine function type that can be awaited within tasks or other coroutines
 * 
 * @tparam S The scheduler type that will manage this coroutine
 * 
 * The async_func class represents a coroutine that:
 * - Can be awaited by tasks (async_task) or other async_func coroutines
 * - Maintains a call stack through the scheduler
 * - Suspends initially and finally
 * - Returns void
 * 
 * Key features:
 * - Integrates with a task-based scheduling system
 * - Supports nested coroutine calls by maintaining a call stack
 * - Provides basic exception handling through std::terminate
 * - Custom memory management through operator new
 * 
 * Usage example:
 * @code
 * async_func<MyScheduler> my_coroutine() {
 *     // Coroutine body
 *     co_return;
 * }
 * @endcode
 * 
 * The coroutine can be awaited using co_await:
 * @code
 * co_await my_coroutine();
 * @endcode
 */
class async_func {
public:
    struct promise_type;
   
    using scheduler_type = S;
    
    using async_func_type = async_func<scheduler_type>;
    using async_func_handle_type = std::coroutine_handle<async_func_type::promise_type>;

    using async_task_type = async_task<scheduler_type>;
    using async_task_handle_type = async_task_type::async_task_handle_type;

    struct promise_type : public etl::forward_link<0> {
    private:
        friend async_func_type;
        friend scheduler_type;

        async_task_handle_type task_handle_;

    public:

        promise_type() : task_handle_(nullptr) {}
        static async_func_type get_return_object_on_allocation_failure()
        {
            return async_func_type(async_func_type::null_handle);
        }
        void* operator new(std::size_t n) noexcept
        {
            pr_debug("[" << n << "]");
            void *mem = new char[n];
            return mem;
        }
        ~promise_type() {
        }

        async_task_handle_type task_handle() {
            return task_handle_;
        }

        // Promise interface
        async_func_type get_return_object() noexcept {
            auto h = async_func_handle_type::from_promise(*this);
            return async_func_type(h);
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
            pr_debug("async_func: RETURN");

            if (task_handle_) {
                task_handle_.promise().callstack_pop();
                task_handle_.promise().resume();
            }
        }
        //exception return_value(exception a);
        void unhandled_exception() { std::terminate(); }

    };

private:
    explicit async_func(async_func_handle_type& h) noexcept : handle_(std::move(h)) { }
    promise_type& promise() const noexcept { return handle_.promise(); }

private:
    async_func_handle_type handle_;

    static async_func_handle_type null_handle;

public:

    bool await_ready() { return false; }
    async_func_handle_type await_suspend(async_task_handle_type awaiter_handle) {
        pr_debug("async_func: SUSPEND FROM TASK");
        promise().task_handle_ = awaiter_handle;
        promise().task_handle_.promise().callstack_push(promise());
        return handle_;
    }
    async_func_handle_type await_suspend(async_func_handle_type awaiter_handle) {
        pr_debug("async_func: SUSPEND FROM CORO");
        promise().task_handle_ = awaiter_handle.promise().task_handle_;
        promise().task_handle_.promise().callstack_push(promise());
        return handle_;
    }
    void await_resume() {
        pr_debug("async_func: RESUME");
        promise().task_handle_.promise().callstack_pop();
        promise().task_handle_ = nullptr;
    }

};
template <typename S>
async_func<S>::async_func_handle_type async_func<S>::null_handle{nullptr};

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
/**
 * @brief A class representing an asynchronous task that can be scheduled and executed.
 * 
 * @tparam S The scheduler type that will manage this task
 * 
 * The async_task class implements a coroutine-based asynchronous task that can be scheduled 
 * and executed by a scheduler. It maintains its own execution state and call stack for nested
 * asynchronous function calls.
 * 
 * Key features:
 * - Manages task lifecycle and state
 * - Supports nested asynchronous function calls via call stack
 * - Integrates with a scheduler for execution
 * - Provides move semantics but prohibits copying
 * - Automatically cleans up resources when destroyed
 * 
 * The task can be in one of several states:
 * - INACTIVE: Task is created but not yet running
 * - ACTIVE: Task is currently executing
 * - ZOMBIE: Task is invalid/destroyed
 * 
 * @note This class is not copyable but is movable
 * @note Tasks are automatically registered with the scheduler upon creation
 * @warning Invalid tasks (in ZOMBIE state) cannot be resumed or used
 */
class async_task {
public:
    struct promise_type;

    using return_type = etl::variant<void, etl::exception>;
    using scheduler_type = S;
    
    using async_task_type = async_task<scheduler_type>;
    using async_task_handle_type = std::coroutine_handle<async_task_type::promise_type>;

    using async_func_type = async_func<scheduler_type>;
    using async_func_handle_type = async_func_type::async_func_handle_type;
    //using coroutine_handle_type = std::coroutine_handle<coroutine_type::promise_type>;

    friend scheduler_type;

    struct promise_type : etl::bidirectional_link<0> {
    private:
        //using coroutine_stack = etl::intrusive_stack<coroutine_type::promise_type, etl::forward_link<0> >;
        
        using async_func_promise_type = async_func_type::promise_type;
        using async_func_stack = etl::intrusive_stack<async_func_promise_type, etl::forward_link<0> >;

        friend scheduler_type;
        friend async_task_type;
        friend async_func_type;

        task_state state_;
        task_priority priority_;
        async_func_stack callstack_;

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
            scheduler_type::get_instance().erase_task(*this);
        }

        async_task_handle_type task_handle() {
            return async_task_handle_type::from_promise(*this);
        }
        void callstack_push(async_func_promise_type& promise) {
            pr_debug("");
            callstack_.push(promise);
        }
        void callstack_pop() {
            pr_debug("");
            if (!callstack_.empty()) callstack_.pop();
        }
        void resume() {
            state_ = task_state::ACTIVE;
            if (callstack_.empty()) {
                async_task_handle_type::from_promise(*this).resume();
            } else {
                async_func_handle_type::from_promise(callstack_.top()).resume();
            }
        }

        // Promise interface
        async_task_type get_return_object() noexcept { 
            auto h = async_task_handle_type::from_promise(*this);
            if (!scheduler_type::get_instance().insert_task(*this)) {
                // Warning: "this" no longer valid after coroutine is destroyed via handle_.destroy()
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
/**
 * @brief A cooperative scheduler for managing asynchronous tasks and functions
 * 
 * @tparam C Configuration type that defines scheduler parameters like maximum task count
 * 
 * The scheduler class provides functionality to:
 * - Manage lifecycle of async tasks and functions
 * - Schedule and suspend tasks
 * - Execute scheduled tasks in a cooperative manner
 * 
 * Key features:
 * - Single instance (singleton) design
 * - Task state management (SUSPENDED, SCHEDULED, ACTIVE, DONE, ZOMBIE)
 * - Safe task scheduling with duplicate prevention
 * - Cooperative multitasking through run_once() and run_one() methods
 * 
 * The scheduler maintains:
 * - A set of task handles for tracking valid tasks
 * - A queue of scheduled tasks pending execution
 * 
 * Usage example:
 * @code
 * auto& sched = scheduler<config>::get_instance();
 * sched.run_one(); // Runs scheduler loop
 * @endcode
 * 
 * @note This scheduler implements cooperative multitasking, meaning tasks must
 *       voluntarily yield control back to the scheduler
 */
class scheduler {
public:
    using config_type = C;
    using scheduler_type = scheduler<config_type>;
    using async_task_type = async_task<scheduler_type>;
    using async_task_handle_type = async_task_type::async_task_handle_type;
    using async_func_type = async_func<scheduler_type>;
    using async_func_handle_type = async_func_type::async_func_handle_type;

    template <typename A, typename S> friend struct scheduler_friend;
    friend async_task_type;
    friend async_func_type;

private:
    using async_task_promise_type = async_task_type::promise_type;
    using handle_set = etl::flat_set<async_task_handle_type, config_type::max_task_count>;
    using scheduled_queue = etl::intrusive_list<async_task_promise_type, etl::bidirectional_link<0>>;

    handle_set handles_;
    scheduled_queue scheduled_;

private:
    scheduler() { }

    bool insert_task(async_task_promise_type& p) {
        auto h = p.task_handle();
        if (handles_.contains(h)) return false;

        p.state_ = task_state::SUSPENDED;
        auto [it, result] = handles_.insert(h); 
        return result;
    }
    bool erase_task(async_task_promise_type& p) {
        return handles_.erase(p.task_handle()) ;
    }

    bool schedule(async_task_handle_type& h, auto&& pred) {
        if (!handles_.contains(h)) return false;

        auto& p = h.promise();
        if (!pred(p.state_)) return false;
        
        p.state_ = task_state::SCHEDULED;
        scheduled_.push_back(p);
        return true;
    }
    bool schedule(async_func_handle_type& handle, auto&& pred) {
        auto task_handle = handle.promise().task_handle();
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
    bool suspend_active(async_func_handle_type& handle) {
        auto task_handle = handle.promise().task_handle();
        if (!task_handle) return false;
        return suspend_active(task_handle);
    }

public:
    static scheduler_type& get_instance() { static scheduler_type inst; return inst; }

    void schedule_all_suspended() {
        for (auto& h: handles_) {
            if (h.promise().state_ != task_state::SUSPENDED) continue;
            scheduled_.push_back(h.promise());
        }
    }
    bool run_once() {
        // TODO: handle events

        if (scheduled_.empty()) {
            // TODO: idle task
            return false;
        }

        auto& task_promise = scheduled_.front();
        scheduled_.pop_front();
        
        task_promise.resume();
    
        if (task_promise.task_handle().done()) {
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

template <typename H, typename S>
concept Handle = requires(H h) {
    { h.promise().task_handle() } -> std::convertible_to<typename S::async_task_handle_type>;
};

template <typename A, typename S>
concept Awaitable = requires(A a, S::async_task_handle_type& h) {
    { a.await_ready() } -> std::convertible_to<bool>;
    { a.await_suspend(h) };
    { a.await_resume() };
};


/**
 * @brief A friend class providing scheduling functionality for derived classes.
 * 
 * This class provides a common interface for scheduling and managing tasks within a scheduler system.
 * It ensures that derived classes are either Awaitable or Service compatible with the scheduler.
 * 
 * @tparam D The derived class type
 * @tparam S The scheduler type
 * 
 * @note The derived class must satisfy either Awaitable<D, S> or Service<D, S> concept
 */
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

protected:
    template <Handle<S> H>
    bool schedule_active(H& h) { 
        return s_.schedule(h, [](task_state state) { return state == task_state::ACTIVE; }); 
    }
    template <Handle<S> H>
    bool schedule_suspended(H& h) { 
        return s_.schedule(h, [](task_state state) { return state == task_state::SUSPENDED; }); 
    }
    template <Handle<S> H>
    bool suspend_active(H& h) { return s_.suspend_active(h); }

private:
    S& s_;
};

/**
 * @brief An awaitable that yields control back to the scheduler
 * 
 * This awaitable allows a coroutine to voluntarily give up its execution time
 * to allow other coroutines to run. When awaited, it suspends the current
 * task and reschedules it for later execution.
 * 
 * @tparam S The scheduler type this awaitable works with
 */
template <typename S>
struct yield_awaitable : public scheduler_friend<yield_awaitable<S>, S> {
    using async_task_handle_type = S::async_task_handle_type;
    using base_type = scheduler_friend<yield_awaitable<S>, S>;

    // Awaitable interface
    bool await_ready() { return false; }
    template <Handle<S> H>
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
struct event {
    using scheduler_type = S;
    using event_type = event<S>;
    using event_awaitable_type = event_awaitable<event_type>;
    using awaitable_list = etl::intrusive_forward_list<event_awaitable_type, etl::forward_link<0>>;

    event(bool auto_reset = true) : auto_reset_(auto_reset) {}

    void activate() { 
        active_ = true; 
        for (auto& awaitable : awaitables_) {
            awaitable.notify();
        }
    }
    void reset() { active_ = false; }
    bool is_active() const { return active_; }

private:
    bool auto_reset_;
    bool active_ = false;
    awaitable_list awaitables_;

};

template <typename E>
struct event_awaitable : public scheduler_friend<event_awaitable<E>, typename E::scheduler_type>, public etl::forward_link<0> {
    using scheduler_type = typename E::scheduler_type;
    using async_task_handle_type = scheduler_type::async_task_handle_type;
    using base_type = scheduler_friend<event_awaitable<E>, scheduler_type>;

    event_awaitable(E& e) : event_(e) {}
    ~event_awaitable() { 
        if (handle_) {
            event_.awaitables_.erase(*this);
        }
    }

    void notify() { 
        if (handle_) {
            base_type::schedule_active(handle_);
            handle_ = nullptr;
        }
    }

    // Awaitable interface 
    bool await_ready() { return event_.is_active(); }
    template <Handle<S> H>
    bool await_suspend(H& h) {
        if (!event_.is_active()) {
            handle_ = h.promise().task_handle();
            event_.awaitables_.push_front(*this);
            return base_type::suspend_active(h);
        }
        return false;
    }
    void await_resume() {
        event_.awaitables_.erase(*this);
        handle_ = nullptr;
    }

private:
    E& event_;
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