#ifndef CORONIMO_DIRECT_TUPLE_H_
#define CORONIMO_DIRECT_TUPLE_H_

#include <etl/type_traits.h>

namespace adva::coronimo {

/**
 * @file direct_tuple.h
 * @brief A tuple-like container designed for direct initialization of elements.
 * 
 * @details This container specializes in handling prvalues (pure rvalues) and non-copyable types:
 * - Directly initializes elements from prvalues without temporary objects
 * - Preserves value categories during initialization
 * - Works with non-copyable, non-movable types
 * - Allows mixing prvalues with lvalue references 
 * 
 * Example usage:
 * @code
 * struct non_copyable {
 *     non_copyable(non_copyable&&) = delete;
 *     int value;
 * };
 * 
 * // Function returning a prvalue
 * non_copyable make_value(int i) {
 *     return non_copyable{i};
 * }
 * 
 * non_copyable item{123}
 * 
 * // Direct initialization from prvalue and reference
 * direct_tuple t{make_value(42), item};
 * @endcode
 */

template <typename ...E>
struct direct_tuple;

template <>
struct direct_tuple<> {};

template <typename E>
struct direct_tuple<E> {
    E e; ///< Single or last element, stored with original value category
};

template <typename E, typename ...Rest>
struct direct_tuple<E, Rest...> {
    E e;                          ///< First element, stored with original value category
    direct_tuple<Rest...> rest;   ///< Remaining elements in nested tuple
};

template <typename... E>
direct_tuple(E&&...) -> direct_tuple<etl::conditional_t<etl::is_lvalue_reference_v<E>, E&, E>...>;

/**
 * @brief Gets a reference to an element in a direct tuple
 * @tparam I Index of the element to get
 * @param t The direct tuple to get the element from
 * @return Reference to the element of type specified by template parameter at index I
 * @see direct_tuple
 */
template <size_t I, typename... E>
auto& get(direct_tuple<E...>& t) {
    if constexpr (I == 0) {
        return t.e;
    } else {
        return get<I - 1>(t.rest);
    }
}

template <typename... Es, typename F>
void tuple_for_each(direct_tuple<Es...>& dt, F&& f)
{
    if constexpr (sizeof...(Es) == 0) {
        // Base case: no elements => do nothing
    }
    else if constexpr (sizeof...(Es) == 1) {
        // Single-element direct_tuple => dt.e is the only field
        f(dt.e);
    }
    else {
        // Multi-element => dt.e plus dt.rest
        f(dt.e);
        tuple_for_each(dt.rest, std::forward<F>(f));
    }
}

}
#endif // CORONIMO_DIRECT_TUPLE_H_