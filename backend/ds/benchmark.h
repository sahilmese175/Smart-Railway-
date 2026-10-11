// ds/benchmark.h
// -----------------------------------------------------------------------------
// Runs every sorting algorithm on the same pseudo-random integers so the DSA Lab
// can plot comparisons / time against input size.
// -----------------------------------------------------------------------------
#pragma once
#include <cstdint>

#include "dynamic_array.h"
#include "sorting.h"

namespace ds {

struct BenchmarkRow {
    SortAlgo algo;
    SortStats stats;
    bool sortedOk;
};

inline DynamicArray<int> randomInts(size_t n, uint64_t seed = 12345) {
    DynamicArray<int> a(n);
    uint64_t x = seed;
    for (size_t i = 0; i < n; ++i) {
        x = x * 6364136223846793005ull + 1442695040888963407ull;  // LCG
        a.push_back(static_cast<int>((x >> 33) % 100000));
    }
    return a;
}

inline DynamicArray<BenchmarkRow> benchmarkSorts(size_t n) {
    static const SortAlgo algos[] = {SortAlgo::Insertion, SortAlgo::Merge, SortAlgo::Quick, SortAlgo::Heap};
    DynamicArray<int> source = randomInts(n);
    DynamicArray<BenchmarkRow> rows;
    for (SortAlgo algo : algos) {
        DynamicArray<int> copy(source);
        BenchmarkRow row;
        row.algo = algo;
        row.stats = sortWith(algo, copy, [](int a, int b) { return a < b; });
        row.sortedOk = true;
        for (size_t i = 1; i < copy.size(); ++i) if (copy[i - 1] > copy[i]) { row.sortedOk = false; break; }
        rows.push_back(row);
    }
    return rows;
}

}  // namespace ds
