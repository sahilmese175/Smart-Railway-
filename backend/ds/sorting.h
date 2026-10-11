// ds/sorting.h
// -----------------------------------------------------------------------------
// Four hand-written sorting algorithms that work on ds::DynamicArray<T>.
// Every one takes a `less(a, b)` comparator and returns SortStats so the UI can
// show *how much work* each algorithm did.
//
//   algorithm        best        average     worst       stable
//   insertionSort    O(n)        O(n^2)      O(n^2)      yes
//   mergeSort        O(n log n)  O(n log n)  O(n log n)  yes
//   quickSort        O(n log n)  O(n log n)  O(n^2)      no   (Hoare, median of 3)
//   heapSort         O(n log n)  O(n log n)  O(n log n)  no
//
// Tip: give the comparator a tie-breaker (e.g. train number) so that every
// algorithm produces exactly the same order, stable or not.
// -----------------------------------------------------------------------------
#pragma once
#include <chrono>
#include <cstddef>
#include <string>
#include <utility>

#include "dynamic_array.h"

namespace ds {

struct SortStats {
    size_t comparisons = 0;
    size_t moves = 0;      // element assignments / swaps
    double micros = 0.0;   // filled in by sortWith()
};

// ---- insertion sort -------------------------------------------------------------
template <typename T, typename Less>
SortStats insertionSort(DynamicArray<T>& a, Less less) {
    SortStats st;
    for (size_t i = 1; i < a.size(); ++i) {
        T key = a[i];
        size_t j = i;
        while (j > 0) {
            ++st.comparisons;
            if (!less(key, a[j - 1])) break;
            a[j] = a[j - 1];
            ++st.moves;
            --j;
        }
        a[j] = key;
        ++st.moves;
    }
    return st;
}

// ---- merge sort -----------------------------------------------------------------
namespace detail {
template <typename T, typename Less>
void mergeRange(DynamicArray<T>& a, DynamicArray<T>& buf, size_t lo, size_t hi, Less& less, SortStats& st) {
    if (hi - lo < 2) return;
    size_t mid = lo + (hi - lo) / 2;
    mergeRange(a, buf, lo, mid, less, st);
    mergeRange(a, buf, mid, hi, less, st);
    size_t i = lo, j = mid, k = lo;
    while (i < mid && j < hi) {
        ++st.comparisons;
        if (less(a[j], a[i])) buf[k++] = a[j++]; else buf[k++] = a[i++];  // take left on ties => stable
        ++st.moves;
    }
    while (i < mid) { buf[k++] = a[i++]; ++st.moves; }
    while (j < hi) { buf[k++] = a[j++]; ++st.moves; }
    for (size_t x = lo; x < hi; ++x) { a[x] = buf[x]; ++st.moves; }
}

template <typename T, typename Less>
void quickRange(DynamicArray<T>& a, long lo, long hi, Less& less, SortStats& st) {
    while (lo < hi) {
        long mid = lo + (hi - lo) / 2;
        // median of three -> a[mid] holds the median
        st.comparisons += 3;
        if (less(a[mid], a[lo])) { std::swap(a[mid], a[lo]); ++st.moves; }
        if (less(a[hi], a[lo])) { std::swap(a[hi], a[lo]); ++st.moves; }
        if (less(a[hi], a[mid])) { std::swap(a[hi], a[mid]); ++st.moves; }
        T pivot = a[mid];
        long i = lo - 1, j = hi + 1;
        long split;
        while (true) {
            do { ++i; ++st.comparisons; } while (less(a[i], pivot));
            do { --j; ++st.comparisons; } while (less(pivot, a[j]));
            if (i >= j) { split = j; break; }
            std::swap(a[i], a[j]);
            ++st.moves;
        }
        // recurse into the smaller half, loop on the larger (keeps stack O(log n))
        if (split - lo < hi - split) { quickRange(a, lo, split, less, st); lo = split + 1; }
        else { quickRange(a, split + 1, hi, less, st); hi = split; }
    }
}

template <typename T, typename Less>
void siftDown(DynamicArray<T>& a, size_t start, size_t n, Less& less, SortStats& st) {
    size_t root = start;
    while (true) {
        size_t child = 2 * root + 1;
        if (child >= n) return;
        if (child + 1 < n) { ++st.comparisons; if (less(a[child], a[child + 1])) ++child; }
        ++st.comparisons;
        if (!less(a[root], a[child])) return;
        std::swap(a[root], a[child]);
        ++st.moves;
        root = child;
    }
}
}  // namespace detail

template <typename T, typename Less>
SortStats mergeSort(DynamicArray<T>& a, Less less) {
    SortStats st;
    if (a.size() < 2) return st;
    DynamicArray<T> buf(a);  // scratch space of the same size
    detail::mergeRange(a, buf, 0, a.size(), less, st);
    return st;
}

template <typename T, typename Less>
SortStats quickSort(DynamicArray<T>& a, Less less) {
    SortStats st;
    if (a.size() < 2) return st;
    detail::quickRange(a, 0, static_cast<long>(a.size()) - 1, less, st);
    return st;
}

template <typename T, typename Less>
SortStats heapSort(DynamicArray<T>& a, Less less) {
    SortStats st;
    size_t n = a.size();
    if (n < 2) return st;
    for (size_t i = n / 2; i-- > 0;) detail::siftDown(a, i, n, less, st);   // build max-heap
    for (size_t end = n - 1; end > 0; --end) {
        std::swap(a[0], a[end]);
        ++st.moves;
        detail::siftDown(a, 0, end, less, st);
    }
    return st;
}

// ---- choose an algorithm by name --------------------------------------------------
enum class SortAlgo { Merge, Quick, Insertion, Heap };

inline bool parseSortAlgo(const std::string& s, SortAlgo& out) {
    if (s == "merge") out = SortAlgo::Merge;
    else if (s == "quick") out = SortAlgo::Quick;
    else if (s == "insertion") out = SortAlgo::Insertion;
    else if (s == "heap") out = SortAlgo::Heap;
    else return false;
    return true;
}
inline const char* sortAlgoName(SortAlgo a) {
    switch (a) {
        case SortAlgo::Merge: return "merge";
        case SortAlgo::Quick: return "quick";
        case SortAlgo::Insertion: return "insertion";
        case SortAlgo::Heap: return "heap";
    }
    return "merge";
}

// Sorts `a` in place with the chosen algorithm and measures wall-clock time.
template <typename T, typename Less>
SortStats sortWith(SortAlgo algo, DynamicArray<T>& a, Less less) {
    auto t0 = std::chrono::steady_clock::now();
    SortStats st;
    switch (algo) {
        case SortAlgo::Merge: st = mergeSort(a, less); break;
        case SortAlgo::Quick: st = quickSort(a, less); break;
        case SortAlgo::Insertion: st = insertionSort(a, less); break;
        case SortAlgo::Heap: st = heapSort(a, less); break;
    }
    auto t1 = std::chrono::steady_clock::now();
    st.micros = std::chrono::duration<double, std::micro>(t1 - t0).count();
    return st;
}

}  // namespace ds
