#include <iostream>
#include <functional>
#include <tuple>
#include <utility>


template <typename S, typename... T>
struct container {
    using s_type = S;

    template<typename... Args>
    container(Args&&... args) 
    : t_(std::forward<T>(args)...) {
        // Construct tuple elements directly with the forwarded args
        std::cout << "container rvalue" << std::endl;
    }
    std::tuple<T...> t_;
};

template <typename S, typename... T> container(T&&...) -> container<S, T...>;

template <typename... T> using void_container = container<void, T...>;

//-------------------------------------------------------------------
/*
template<typename... Items>
class owning_ref_container {
    std::tuple<Items...> storage;
    std::tuple<std::reference_wrapper<Items>...> refs;
    
public:
    template<typename... Args>
    requires (sizeof...(Args) == sizeof...(Items))
    owning_ref_container(Args&&... args)
        : storage{std::forward<Args>(args)...}
        , refs{std::get<Items>(storage)...} {}
        
    template<size_t I>
    decltype(auto) get() {
        return std::get<I>(refs).get();
    }
};*/
/*
template <typename... Args>
struct tuple;

template <>
struct tuple<> {
};


template <typename T, typename... Rest>
struct tuple<T, Rest...> {
    T first;
    tuple <Rest...> rest;
    
    template<typename FirstArg, typename... RestArgs>
    tuple(FirstArg t, RestArgs... r) 
        : first(t), 
          (rest(r)...)
    {}
};

// For direct types (including prvalues)
template<typename... Types>
tuple(Types...) -> tuple<Types...>;
*/
// For references we still need this
/*template<typename... Types>
tuple(Types&...) -> tuple<Types&...>;*/

/*
template<typename... Types>
tuple(Types&&...) -> tuple<std::remove_reference_t<Types>...>;

// For reference arguments, preserve the reference
template<typename... Types>
tuple(Types&...) -> tuple<Types&...>;
*/

template <typename T>
struct wrapper;

template <typename T>
struct wrapper {
    T value;
};
template <typename T>
struct wrapper<T&> {
    T& value;
};

//wrapper(item i) -> wrapper<item>;
//wrapper(item& i) -> wrapper<item&>;
template<typename T>
wrapper(T&) -> wrapper<T&>;

template<typename T>
wrapper(T&&) -> wrapper<T>;    // For prvalues/values
/*
template <typename... T>
struct tuple;

template <>
struct tuple<> {
};

template <typename T>
struct tuple<T> {
    T value;
};

template <typename T>
struct tuple<T&> {     // base case for references
    T& value;
};

template <typename T, typename... Rest>
struct tuple<T, Rest...> {
    T value;
    tuple<Rest...> rest;
};

template <typename T, typename... Rest>
struct tuple<T&, Rest...> {
    T& value;
    tuple<Rest...> rest;
};

template<typename T, typename... Rest>
tuple(T&, Rest&&...) -> tuple<T&, Rest&&...>;

template<typename T, typename... Rest>
tuple(T&&, Rest&&...) -> tuple<T, Rest&&...>;    // For prvalues/values

template<typename T1, typename T2, typename... Rest>
tuple(T1&, T2&, Rest&&...) -> tuple<T1&, T2&, Rest&&...>;

// For first being prvalue, second being reference
template<typename T1, typename T2, typename... Rest>
tuple(T1&&, T2&, Rest&&...) -> tuple<T1, T2&, Rest&&...>;

// For first being reference, second being prvalue
template<typename T1, typename T2, typename... Rest>
tuple(T1&, T2&&, Rest&&...) -> tuple<T1&, T2, Rest&&...>;

// For both being prvalues
template<typename T1, typename T2, typename... Rest>
tuple(T1&&, T2&&, Rest&&...) -> tuple<T1, T2, Rest&&...>;
*/
template<typename... Types>
struct tuple_impl;

template<typename Head>
struct tuple_impl<Head> {
    Head head;
};

template<typename Head, typename... Tail>
struct tuple_impl<Head, Tail...>
    : public tuple_impl<Tail...>
{
    Head head;
};

template<typename... Types>
using tuple = tuple_impl<Types...>;

// Base case deduction
template<typename Head>
tuple_impl(Head&&) -> tuple_impl<Head>;

// Recursive case deduction
template<typename Head, typename... Tail>
tuple_impl(Head&&, Tail&&...) -> tuple_impl<Head, Tail...>;


struct item {
    item(int i) : value(i) {
        std::cout << "item ctor " << i << std::endl;
    }
    item(item &&i) = delete;
    item(const item &i) = delete;
    ~item() {
        std::cout << "item dtor " << value << std::endl;
    }

    int value;

};


item make_item(int i) {
    return item(i);
}

template <typename... TRest>
struct etuple_impl;

template <typename T>
struct etuple_impl<T> {
    T value;
};

template <typename T>
struct etuple_impl<T&> {
    T& value;
};

template <typename T, typename... TRest>
struct etuple_impl<T, TRest...> : public etuple_impl<TRest...> {
    using base_type = etuple_impl<T, TRest...>;
    T value;
};

template <typename T, typename... TRest>
struct etuple_impl<T&, TRest&&...> : public etuple_impl<TRest&&...> {
    using base_type = etuple_impl<T&, TRest&&...>;
    T& value;
};
template <typename T, typename... TRest>
etuple_impl(T&, TRest&&...) -> etuple_impl<T&, TRest&...>;


template <typename T, typename... TRest>
etuple_impl(T&&, TRest&&...) -> etuple_impl<T, TRest&&...>;

template <typename T>
etuple_impl(T&) -> etuple_impl<T&>;

template <typename T>
etuple_impl(T&&) -> etuple_impl<T>;

int main() {
    float pi = 3.14;
    //tuple<int, float&> x{5, pi};
    //tuple x{5, pi};
    //tuple<item, item> x{1, 2};
    //tuple<item, item> x{1, 2};
    //tuple x{make_item(2), make_item(3)};

    //x.rest.first.value = 5;
    //std::cout << x.rest.first.value << std::endl;
    //wrapper x(make_item(3));
    //item y{4};
    //wrapper w2{y};
  
  //  auto t = tuple(make_item(2), make_item(3));
    //auto t = tuple(wrapper{make_item(1)}, wrapper{make_item(2)});
    //tuple t2{item{3}};
    //tuple<item> t3{{4}};
    //std::cout << t.head.value.value << std::endl;
    item i1{111};
    item i2{222};

    etuple_impl e{i1, i1, i2};

    etuple_impl e2{item{3}, i1, item{4}};

    //etuple_impl e{make_item(1), i1, make_item(13), make_item(42), make_item(69), make_item(88) };
    std::cout << e.value.value << std::endl;
    
    /*
    item i{1};
    {
        void_container a{i, item(2), item{4}};

        //std::apply([](auto&... a) { ((std::cout << a.value << std::endl), ...); }, a.t_);

        std::cout << std::get<0>(a.t_).value << std::endl;
        std::cout << std::get<1>(a.t_).value << std::endl;
    }
    std::cout << "post container" << std::endl;
    */

    return 0;
}