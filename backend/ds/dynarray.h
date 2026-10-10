// DynArray<T> - dynamic array (ARRAY) with amortised O(1) push.
#pragma once
#include <cstddef>

template <typename T>
class DynArray {
    T* data_ = nullptr;
    size_t size_ = 0, cap_ = 0;
    void grow() {
        size_t nc = cap_ ? cap_ * 2 : 8;
        T* nd = new T[nc];
        for (size_t i = 0; i < size_; i++) nd[i] = data_[i];
        delete[] data_;
        data_ = nd;
        cap_ = nc;
    }
public:
    DynArray() = default;
    DynArray(const DynArray& o) { for (size_t i = 0; i < o.size_; i++) push(o.data_[i]); }
    DynArray& operator=(const DynArray& o) {
        if (this != &o) { clear(); for (size_t i = 0; i < o.size_; i++) push(o.data_[i]); }
        return *this;
    }
    ~DynArray() { delete[] data_; }

    void push(const T& v) { if (size_ == cap_) grow(); data_[size_++] = v; }
    void removeAt(size_t i) { for (size_t j = i; j + 1 < size_; j++) data_[j] = data_[j + 1]; size_--; }  // O(n)
    void clear() { size_ = 0; }
    size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }
    T& operator[](size_t i) { return data_[i]; }
    const T& operator[](size_t i) const { return data_[i]; }
};
