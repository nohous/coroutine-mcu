#ifndef CORONIMO_UTILITY_H_
#define CORONIMO_UTILITY_H_

namespace adva::coronimo {

template<typename T>
class resetable_ref {
    T* ptr;
public:
    constexpr resetable_ref(T& p) : ptr(&p) {}
    resetable_ref() = delete;
    constexpr T* get() const { return ptr; }
    T* operator ->() const { return ptr; }
    void reset() { ptr = nullptr; }
    bool is_valid() const { return ptr != nullptr; }
};

}

#endif // CORONIMO_UTILITY_H_