// ds/hash_table.h
// -----------------------------------------------------------------------------
// HashTable<K, V> : separate chaining (every bucket is a singly linked chain).
//
//   * bucket array doubles when load factor > 0.75 (rehash)
//   * integer keys  -> multiplicative bit-mixing hash
//   * string keys   -> FNV-1a hash
//   * put / get / remove are O(1) on average, O(n) worst case
//
// The inspection helpers (bucketOf, chainLength, forEachInBucket, lastProbes ...)
// exist so the web "DSA Lab" can draw the table and explain every lookup.
// -----------------------------------------------------------------------------
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

namespace ds {

template <typename K> struct Hasher;

template <> struct Hasher<int> {
    size_t operator()(int key) const {
        uint32_t x = static_cast<uint32_t>(key);
        x = ((x >> 16) ^ x) * 0x45d9f3bu;
        x = ((x >> 16) ^ x) * 0x45d9f3bu;
        x = (x >> 16) ^ x;
        return x;
    }
};

template <> struct Hasher<std::string> {
    size_t operator()(const std::string& s) const {
        uint64_t h = 1469598103934665603ull;  // FNV offset basis
        for (unsigned char c : s) { h ^= c; h *= 1099511628211ull; }
        return static_cast<size_t>(h);
    }
};

template <typename K, typename V, typename H = Hasher<K>>
class HashTable {
public:
    struct Node {
        K key;
        V value;
        Node* next;
        Node(const K& k, const V& v, Node* n) : key(k), value(v), next(n) {}
    };

    explicit HashTable(size_t initialBuckets = 16) { allocate(initialBuckets < 4 ? 4 : initialBuckets); }
    HashTable(const HashTable&) = delete;
    HashTable& operator=(const HashTable&) = delete;
    HashTable(HashTable&& o) noexcept { steal(o); }
    HashTable& operator=(HashTable&& o) noexcept {
        if (this != &o) { release(); steal(o); }
        return *this;
    }
    ~HashTable() { release(); }

    // ---- core operations --------------------------------------------------------
    // Insert or overwrite. Returns true if a NEW key was inserted.
    bool put(const K& key, const V& value) {
        size_t b = bucketOf(key);
        for (Node* n = buckets_[b]; n; n = n->next)
            if (n->key == key) { n->value = value; return false; }
        buckets_[b] = new Node(key, value, buckets_[b]);
        ++size_;
        if (loadFactor() > kMaxLoad) rehash(bucketCount_ * 2);
        return true;
    }

    // Returns pointer to the value or nullptr. Records how many nodes were visited.
    V* get(const K& key) {
        size_t b = bucketOf(key);
        lastBucket_ = b;
        lastProbes_ = 0;
        for (Node* n = buckets_[b]; n; n = n->next) {
            ++lastProbes_;
            if (n->key == key) return &n->value;
        }
        return nullptr;
    }
    const V* get(const K& key) const { return const_cast<HashTable*>(this)->get(key); }

    bool contains(const K& key) const { return get(key) != nullptr; }

    bool remove(const K& key) {
        size_t b = bucketOf(key);
        Node* prev = nullptr;
        for (Node* n = buckets_[b]; n; prev = n, n = n->next) {
            if (n->key == key) {
                if (prev) prev->next = n->next; else buckets_[b] = n->next;
                delete n;
                --size_;
                return true;
            }
        }
        return false;
    }

    void clear() {
        for (size_t i = 0; i < bucketCount_; ++i) {
            Node* n = buckets_[i];
            while (n) { Node* nx = n->next; delete n; n = nx; }
            buckets_[i] = nullptr;
        }
        size_ = 0;
    }

    // Visit every (key, value) pair. Order = bucket order, not insertion order.
    template <typename F>
    void forEach(F fn) const {
        for (size_t i = 0; i < bucketCount_; ++i)
            for (Node* n = buckets_[i]; n; n = n->next) fn(n->key, n->value);
    }

    // ---- statistics / inspection -------------------------------------------------
    size_t size() const { return size_; }
    size_t bucketCount() const { return bucketCount_; }
    double loadFactor() const { return bucketCount_ ? static_cast<double>(size_) / bucketCount_ : 0.0; }
    size_t rehashCount() const { return rehashes_; }
    size_t bucketOf(const K& key) const { return hasher_(key) % bucketCount_; }

    size_t chainLength(size_t bucket) const {
        size_t len = 0;
        for (Node* n = buckets_[bucket]; n; n = n->next) ++len;
        return len;
    }
    size_t longestChain() const {
        size_t best = 0;
        for (size_t i = 0; i < bucketCount_; ++i) { size_t l = chainLength(i); if (l > best) best = l; }
        return best;
    }
    size_t usedBuckets() const {
        size_t used = 0;
        for (size_t i = 0; i < bucketCount_; ++i) if (buckets_[i]) ++used;
        return used;
    }
    // number of keys that share a bucket with an earlier key
    size_t collisions() const { return size_ - usedBuckets(); }

    template <typename F>
    void forEachInBucket(size_t bucket, F fn) const {
        for (Node* n = buckets_[bucket]; n; n = n->next) fn(n->key, n->value);
    }

    // info about the most recent get()
    size_t lastBucket() const { return lastBucket_; }
    size_t lastProbes() const { return lastProbes_; }

private:
    static constexpr double kMaxLoad = 0.75;
    Node** buckets_ = nullptr;
    size_t bucketCount_ = 0;
    size_t size_ = 0;
    size_t rehashes_ = 0;
    mutable size_t lastBucket_ = 0;
    mutable size_t lastProbes_ = 0;
    H hasher_;

    void allocate(size_t n) {
        bucketCount_ = n;
        buckets_ = new Node*[n];
        for (size_t i = 0; i < n; ++i) buckets_[i] = nullptr;
    }
    void release() {
        if (!buckets_) return;
        clear();
        delete[] buckets_;
        buckets_ = nullptr;
        bucketCount_ = 0;
    }
    void steal(HashTable& o) {
        buckets_ = o.buckets_; bucketCount_ = o.bucketCount_; size_ = o.size_;
        rehashes_ = o.rehashes_; hasher_ = o.hasher_;
        o.buckets_ = nullptr; o.bucketCount_ = 0; o.size_ = 0;
    }
    void rehash(size_t newCount) {
        Node** old = buckets_;
        size_t oldCount = bucketCount_;
        allocate(newCount);
        for (size_t i = 0; i < oldCount; ++i) {
            Node* n = old[i];
            while (n) {
                Node* nx = n->next;
                size_t b = bucketOf(n->key);
                n->next = buckets_[b];
                buckets_[b] = n;
                n = nx;
            }
        }
        delete[] old;
        ++rehashes_;
    }
};

}  // namespace ds
