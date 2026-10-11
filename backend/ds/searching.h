// ds/searching.h
// -----------------------------------------------------------------------------
// Array searches that report how many steps they took.
//
//   linearSearch  O(n)       works on any array
//   binarySearch  O(log n)   array must already be sorted by the key
// -----------------------------------------------------------------------------
#pragma once
#include <cstddef>

#include "dynamic_array.h"

namespace ds {

// Returns index of the first element where pred(element) is true, or -1.
// `steps` (optional) receives the number of elements inspected.
template <typename T, typename Pred>
long linearSearch(const DynamicArray<T>& a, Pred pred, size_t* steps = nullptr) {
    size_t s = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        ++s;
        if (pred(a[i])) { if (steps) *steps = s; return static_cast<long>(i); }
    }
    if (steps) *steps = s;
    return -1;
}

// Returns index of the element whose keyOf(element) == key, or -1.
// `trace` (optional) receives every middle index that was probed, in order.
template <typename T, typename Key, typename KeyOf>
long binarySearch(const DynamicArray<T>& a, const Key& key, KeyOf keyOf, DynamicArray<size_t>* trace = nullptr) {
    long lo = 0, hi = static_cast<long>(a.size()) - 1;
    while (lo <= hi) {
        long mid = lo + (hi - lo) / 2;
        if (trace) trace->push_back(static_cast<size_t>(mid));
        const Key k = keyOf(a[mid]);
        if (k == key) return mid;
        if (k < key) lo = mid + 1; else hi = mid - 1;
    }
    return -1;
}

}  // namespace ds
