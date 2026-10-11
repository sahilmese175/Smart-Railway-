// Unit tests for backend/ds/*.h   (build & run: make test)
#include <cstdio>
#include <string>

#include "ds/benchmark.h"
#include "ds/dynamic_array.h"
#include "ds/hash_table.h"
#include "ds/linked_list.h"
#include "ds/searching.h"
#include "ds/sorting.h"

static int g_checks = 0, g_failed = 0;
#define CHECK(cond)                                                              \
    do {                                                                         \
        ++g_checks;                                                              \
        if (!(cond)) { ++g_failed; std::printf("  FAILED %s:%d  %s\n", __FILE__, __LINE__, #cond); } \
    } while (0)

static void testDynamicArray() {
    ds::DynamicArray<int> a;
    CHECK(a.empty());
    for (int i = 0; i < 100; ++i) a.push_back(i);
    CHECK(a.size() == 100);
    CHECK(a.capacity() >= 100);
    CHECK(a.resizeCount() >= 5);  // 4 -> 8 -> 16 -> 32 -> 64 -> 128
    CHECK(a[0] == 0 && a[99] == 99);
    a.insertAt(0, -1);
    CHECK(a[0] == -1 && a[1] == 0 && a.size() == 101);
    a.removeAt(0);
    CHECK(a[0] == 0 && a.size() == 100);
    CHECK(a.removeValue(50));
    CHECK(!a.removeValue(50));
    CHECK(a.size() == 99 && a[50] == 51);
    ds::DynamicArray<int> b(a);          // copy is deep
    b[0] = 777;
    CHECK(a[0] == 0 && b[0] == 777);
    ds::DynamicArray<int> c(std::move(b));
    CHECK(c.size() == 99 && b.size() == 0);
    bool threw = false;
    try { a.at(1000); } catch (const std::out_of_range&) { threw = true; }
    CHECK(threw);
    int sum = 0;
    for (int v : c) sum += v;
    CHECK(sum > 0);
}

static void testLinkedList() {
    ds::LinkedList<int> l;
    l.push_back(2); l.push_back(3); l.push_front(1);
    CHECK(l.size() == 3 && l.at(0) == 1 && l.at(2) == 3);
    l.insertAt(1, 99);                    // 1 99 2 3
    CHECK(l.at(1) == 99 && l.at(2) == 2 && l.size() == 4);
    l.insertAt(4, 100);                   // append position
    CHECK(l.tailNode()->value == 100);
    l.removeAt(1);                        // 1 2 3 100
    CHECK(l.at(1) == 2);
    l.removeAt(0);
    CHECK(l.headNode()->value == 2 && l.headNode()->prev == nullptr);
    l.removeAt(static_cast<size_t>(l.size() - 1));
    CHECK(l.tailNode()->value == 3 && l.tailNode()->next == nullptr);
    int expected[] = {2, 3}, i = 0;
    for (int v : l) CHECK(v == expected[i++]);
    ds::LinkedList<int> copy(l);
    copy.push_back(4);
    CHECK(l.size() == 2 && copy.size() == 3);
    l.clear();
    CHECK(l.empty() && l.headNode() == nullptr && l.tailNode() == nullptr);
    bool threw = false;
    try { l.removeAt(0); } catch (const std::out_of_range&) { threw = true; }
    CHECK(threw);
}

static void testHashTable() {
    ds::HashTable<int, int> h(4);
    for (int i = 0; i < 1000; ++i) CHECK(h.put(i * 7, i));
    CHECK(h.size() == 1000);
    CHECK(h.rehashCount() > 0);
    CHECK(h.loadFactor() <= 0.75 + 1e-9);
    bool allFound = true;
    for (int i = 0; i < 1000; ++i) { int* v = h.get(i * 7); if (!v || *v != i) allFound = false; }
    CHECK(allFound);
    CHECK(h.get(3) == nullptr);
    CHECK(!h.put(7, 12345));              // overwrite returns false
    CHECK(*h.get(7) == 12345 && h.size() == 1000);
    CHECK(h.remove(7) && !h.remove(7) && h.size() == 999);
    size_t visited = 0;
    h.forEach([&](int, int) { ++visited; });
    CHECK(visited == 999);

    ds::HashTable<std::string, int> s(2);   // tiny table forces long chains and collisions
    s.put("pune", 1); s.put("mumbai", 2); s.put("delhi", 3); s.put("kota", 4);
    CHECK(*s.get("mumbai") == 2);
    CHECK(s.get("goa") == nullptr);
    CHECK(s.lastBucket() < s.bucketCount());
    CHECK(s.collisions() == s.size() - s.usedBuckets());

    ds::HashTable<std::string, int> moved(std::move(s));
    CHECK(moved.size() == 4 && *moved.get("kota") == 4);

    // chains: sum of chain lengths equals size
    size_t total = 0;
    for (size_t b = 0; b < h.bucketCount(); ++b) total += h.chainLength(b);
    CHECK(total == h.size());
}

struct Pair { int key; int order; };

static void checkSort(ds::SortAlgo algo, size_t n, uint64_t seed) {
    ds::DynamicArray<int> a = ds::randomInts(n, seed);
    long sumBefore = 0;
    for (int v : a) sumBefore += v;
    ds::SortStats st = ds::sortWith(algo, a, [](int x, int y) { return x < y; });
    bool sorted = true;
    long sumAfter = 0;
    for (size_t i = 0; i < a.size(); ++i) { sumAfter += a[i]; if (i && a[i - 1] > a[i]) sorted = false; }
    CHECK(sorted);
    CHECK(sumBefore == sumAfter && a.size() == n);
    if (n > 1) CHECK(st.comparisons > 0);
}

static void testSorting() {
    ds::SortAlgo algos[] = {ds::SortAlgo::Insertion, ds::SortAlgo::Merge, ds::SortAlgo::Quick, ds::SortAlgo::Heap};
    size_t sizes[] = {0, 1, 2, 3, 10, 57, 500, 2000};
    for (ds::SortAlgo algo : algos)
        for (size_t n : sizes) checkSort(algo, n, 99 + n);

    // all equal / already sorted / reversed
    for (ds::SortAlgo algo : algos) {
        ds::DynamicArray<int> eq, asc, desc;
        for (int i = 0; i < 300; ++i) { eq.push_back(5); asc.push_back(i); desc.push_back(300 - i); }
        ds::sortWith(algo, eq, [](int x, int y) { return x < y; });
        ds::sortWith(algo, asc, [](int x, int y) { return x < y; });
        ds::sortWith(algo, desc, [](int x, int y) { return x < y; });
        bool ok = eq[0] == 5 && eq[299] == 5 && asc[0] == 0 && asc[299] == 299 && desc[0] == 1 && desc[299] == 300;
        CHECK(ok);
    }

    // stability of merge and insertion sort
    for (ds::SortAlgo algo : {ds::SortAlgo::Merge, ds::SortAlgo::Insertion}) {
        ds::DynamicArray<Pair> p;
        for (int i = 0; i < 200; ++i) p.push_back(Pair{i % 5, i});
        ds::sortWith(algo, p, [](const Pair& a, const Pair& b) { return a.key < b.key; });
        bool stable = true;
        for (size_t i = 1; i < p.size(); ++i)
            if (p[i - 1].key == p[i].key && p[i - 1].order > p[i].order) stable = false;
        CHECK(stable);
    }

    // complexity sanity: insertion sort does far more comparisons than merge sort on 2000 items
    ds::DynamicArray<ds::BenchmarkRow> rows = ds::benchmarkSorts(2000);
    size_t ins = 0, mer = 0;
    for (const auto& r : rows) {
        CHECK(r.sortedOk);
        if (r.algo == ds::SortAlgo::Insertion) ins = r.stats.comparisons;
        if (r.algo == ds::SortAlgo::Merge) mer = r.stats.comparisons;
    }
    CHECK(ins > 10 * mer);

    ds::SortAlgo parsed;
    CHECK(ds::parseSortAlgo("heap", parsed) && parsed == ds::SortAlgo::Heap);
    CHECK(!ds::parseSortAlgo("bubble", parsed));
}

static void testSearching() {
    ds::DynamicArray<int> a;
    for (int i = 0; i < 64; ++i) a.push_back(i * 2);   // sorted evens
    size_t steps = 0;
    CHECK(ds::linearSearch(a, [](int v) { return v == 40; }, &steps) == 20 && steps == 21);
    CHECK(ds::linearSearch(a, [](int v) { return v == 41; }, &steps) == -1 && steps == 64);
    ds::DynamicArray<size_t> trace;
    CHECK(ds::binarySearch(a, 40, [](int v) { return v; }, &trace) == 20);
    CHECK(trace.size() <= 7);
    CHECK(ds::binarySearch(a, 41, [](int v) { return v; }) == -1);
}

int main() {
    testDynamicArray();
    testLinkedList();
    testHashTable();
    testSorting();
    testSearching();
    std::printf("test_ds: %d checks, %d failed\n", g_checks, g_failed);
    return g_failed ? 1 : 0;
}
