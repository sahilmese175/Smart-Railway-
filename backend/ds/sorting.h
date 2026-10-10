// Stable merge sort on DynArray<T>, O(n log n). cmp(a,b) returns true if a must come before b.
#pragma once
#include "dynarray.h"

template <typename T, typename Cmp>
void mergeSortRange(DynArray<T>& a, DynArray<T>& tmp, size_t lo, size_t hi, Cmp cmp) {
    if (hi - lo < 2) return;
    size_t mid = lo + (hi - lo) / 2;
    mergeSortRange(a, tmp, lo, mid, cmp);
    mergeSortRange(a, tmp, mid, hi, cmp);
    size_t i = lo, j = mid, k = lo;
    while (i < mid && j < hi) tmp[k++] = cmp(a[j], a[i]) ? a[j++] : a[i++];   // takes left on ties => stable
    while (i < mid) tmp[k++] = a[i++];
    while (j < hi) tmp[k++] = a[j++];
    for (size_t x = lo; x < hi; x++) a[x] = tmp[x];
}

template <typename T, typename Cmp>
void mergeSort(DynArray<T>& a, Cmp cmp) {
    DynArray<T> tmp;
    for (size_t i = 0; i < a.size(); i++) tmp.push(a[i]);
    mergeSortRange(a, tmp, 0, a.size(), cmp);
}
