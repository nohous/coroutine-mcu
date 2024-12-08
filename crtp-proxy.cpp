#include <iostream>
#include <concepts>
#include <coroutine>

class scheduler;


template <typename Derived, typename S = scheduler>
class awaitable_base;

template <typename T, typename S = scheduler>
concept Awaitable = requires(T a, std::coroutine_handle<> h) 
{
    { a.await_ready() } -> std::convertible_to<bool>;
    requires ( 1 + 1 == 2 );
    a.await_suspend(h);
    a.await_resume();
    a.scheduler_callable();
    requires std::derived_from<T, awaitable_base<T, S> >;
};


class scheduler {
    //template <Awaitable Derived>
    //friend class awaitable;

    template <typename, typename>
    friend struct awaitable_base; // Non-templated base class as friend

private:
    void priv_func() { std::cout << "private" << std::endl; }
public:
    template <Awaitable T>
    void test(T a) { 
        std::cout << "awaitable called in" << std::endl; 
        a.scheduler_callable();
    }
    scheduler() { }
};


template <typename Derived, typename S>
struct awaitable_base {
    awaitable_base(S& s) : s_(s) { }

protected:
    void priv_proxy() { s_.priv_func(); }

    S& s_;
};


/*
struct yield_tag {};

template <>
struct awaitable_base<yield_tag> {
    scheduler s_;
    awaitable_base(scheduler& s) : s_(s) {}
public:
    void call_priv() {  access_private(s_, *this); } //s_.priv_func(); }
};
using yield = awaitable_base<yield_tag>;
*/

struct yield : public awaitable_base<yield> {
    bool await_ready() { std::cout <<"ready" << std:: endl; return false; }
    void await_suspend(std::coroutine_handle<> h) { }
    void await_resume() { s_.test(*this); }
    void call_priv() { priv_proxy(); }
private:
    friend class scheduler;
    void scheduler_callable() { }

};

//template <typename 
//class awaitable_base 

int main()
{

    scheduler s;
    yield y(s);
    y.call_priv();
    //s.test(y);

 
    return 0;
}