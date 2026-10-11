// ds/dynamic_array.h
// -----------------------------------------------------------------------------
// DynamicArray<T> : a resizable array built on a raw heap buffer (no std::vector)
//
//   operation            complexity
//   ------------------   ---------------------
//   operator[]           O(1)
//   push_back            O(1) amortised (capacity doubles when full)
//   insertAt / removeAt  O(n) (elements are shifted)
//   clear                O(1)
//
// `resizeCount()` is exposed so the DSA Lab page can show how often it grew.
// -----------------------------------------------------------------------------
#pragma once
#include <cstddef>
#include <stdexcept>
#include <utility>

namespace ds {

template <typename T>
class DynamicArray {
public:
    DynamicArray() = default;
    explicit DynamicArray(size_t initialCapacity) { reserve(initialCapacity); }

    DynamicArray(const DynamicArray& o) { copyFrom(o); }
    DynamicArray(DynamicArray&& o) noexcept { moveFrom(o); }
    DynamicArray& operator=(const DynamicArray& o) {
        if (this != &o) { release(); copyFrom(o); }
        return *this;
    }
    DynamicArray& operator=(DynamicArray&& o) noexcept {
        if (this != &o) { release(); moveFrom(o); }
        return *this;
    }
    ~DynamicArray() { release(); }

    // ---- element access -----------------------------------------------------
    T& operator[](size_t i) { return data_[i]; }
    const T& operator[](size_t i) const { return data_[i]; }
    T& at(size_t i) {
        if (i >= size_) throw std::out_of_range("DynamicArray::at");
        return data_[i];
    }
    const T& at(size_t i) const {
        if (i >= size_) throw std::out_of_range("DynamicArray::at");
        return data_[i];
    }
    T& back() { return data_[size_ - 1]; }
    const T& back() const { return data_[size_ - 1]; }

    // ---- capacity -----------------------------------------------------------
    size_t size() const { return size_; }
    size_t capacity() const { return cap_; }
    bool empty() const { return size_ == 0; }
    size_t resizeCount() const { return resizes_; }

    void reserve(size_t n) {
        if (n <= cap_) return;
        T* fresh = new T[n];
        for (size_t i = 0; i < size_; ++i) fresh[i] = std::move(data_[i]);
        delete[] data_;
        data_ = fresh;
        cap_ = n;
        ++resizes_;
    }

    // ---- modifiers ----------------------------------------------------------
    void push_back(const T& v) {
        if (size_ == cap_) reserve(cap_ == 0 ? 4 : cap_ * 2);
        data_[size_++] = v;
    }
    void push_back(T&& v) {
        if (size_ == cap_) reserve(cap_ == 0 ? 4 : cap_ * 2);
        data_[size_++] = std::move(v);
    }
    void pop_back() { if (size_ > 0) --size_; }

    void insertAt(size_t index, const T& v) {
        if (index > size_) throw std::out_of_range("DynamicArray::insertAt");
        if (size_ == cap_) reserve(cap_ == 0 ? 4 : cap_ * 2);
        for (size_t i = size_; i > index; --i) data_[i] = std::move(data_[i - 1]);
        data_[index] = v;
        ++size_;
    }
    void removeAt(size_t index) {
        if (index >= size_) throw std::out_of_range("DynamicArray::removeAt");
        for (size_t i = index; i + 1 < size_; ++i) data_[i] = std::move(data_[i + 1]);
        --size_;
    }
    // Remove first element equal to v. Returns true if something was removed.
    bool removeValue(const T& v) {
        for (size_t i = 0; i < size_; ++i)
            if (data_[i] == v) { removeAt(i); return true; }
        return false;
    }
    void clear() { size_ = 0; }

    // ---- iteration (so range-for works) ---------------------------------------
    T* begin() { return data_; }
    T* end() { return data_ + size_; }
    const T* begin() const { return data_; }
    const T* end() const { return data_ + size_; }

private:
    T* data_ = nullptr;
    size_t size_ = 0;
    size_t cap_ = 0;
    size_t resizes_ = 0;

    void release() { delete[] data_; data_ = nullptr; size_ = cap_ = 0; }
    void copyFrom(const DynamicArray& o) {
        cap_ = o.size_;
        size_ = o.size_;
        resizes_ = 0;
        data_ = cap_ ? new T[cap_] : nullptr;
        for (size_t i = 0; i < size_; ++i) data_[i] = o.data_[i];
    }
    void moveFrom(DynamicArray& o) {
        data_ = o.data_; size_ = o.size_; cap_ = o.cap_; resizes_ = o.resizes_;
        o.data_ = nullptr; o.size_ = o.cap_ = o.resizes_ = 0;
    }
};

}  // namespace ds
