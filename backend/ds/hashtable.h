// HashTable<K,V> - separate chaining, auto-resize at load factor 0.75 (HASH TABLE).
// Average O(1) put / get / remove. Add a hashKey() overload to support new key types.
#pragma once
#include <string>
#include <cstddef>
#include "dynarray.h"

inline size_t hashKey(int k) { size_t h = (size_t)k * 2654435761u; return h ^ (h >> 16); }
inline size_t hashKey(const std::string& s) { size_t h = 5381; for (unsigned char c : s) h = h * 33 + c; return h; }  // djb2

template <typename K, typename V>
class HashTable {
    struct Node { K key; V val; Node* next; };
    Node** b_;
    size_t cap_, n_ = 0;

    void rehash() {
        size_t nc = cap_ * 2;
        Node** nb = new Node*[nc]();
        for (size_t i = 0; i < cap_; i++)
            for (Node* c = b_[i]; c;) {
                Node* nx = c->next;
                size_t j = hashKey(c->key) % nc;
                c->next = nb[j]; nb[j] = c; c = nx;
            }
        delete[] b_; b_ = nb; cap_ = nc;
    }
public:
    explicit HashTable(size_t cap = 16) : b_(new Node*[cap]()), cap_(cap) {}
    HashTable(const HashTable&) = delete;
    HashTable& operator=(const HashTable&) = delete;
    ~HashTable() {
        for (size_t i = 0; i < cap_; i++)
            for (Node* c = b_[i]; c;) { Node* nx = c->next; delete c; c = nx; }
        delete[] b_;
    }

    V* get(const K& k) const {
        for (Node* c = b_[hashKey(k) % cap_]; c; c = c->next) if (c->key == k) return &c->val;
        return nullptr;
    }
    bool put(const K& k, const V& v) {               // true = inserted, false = overwritten
        if (V* e = get(k)) { *e = v; return false; }
        if ((n_ + 1) * 4 > cap_ * 3) rehash();
        size_t i = hashKey(k) % cap_;
        b_[i] = new Node{k, v, b_[i]}; n_++;
        return true;
    }
    bool remove(const K& k) {
        Node** p = &b_[hashKey(k) % cap_];
        while (*p) {
            if ((*p)->key == k) { Node* d = *p; *p = d->next; delete d; n_--; return true; }
            p = &(*p)->next;
        }
        return false;
    }
    size_t size() const { return n_; }
    size_t buckets() const { return cap_; }
    DynArray<int> bucketLengths() const {            // for the DSA Inspector page
        DynArray<int> out;
        for (size_t i = 0; i < cap_; i++) { int l = 0; for (Node* c = b_[i]; c; c = c->next) l++; out.push(l); }
        return out;
    }
    template <typename F> void forEach(F f) const {
        for (size_t i = 0; i < cap_; i++) for (Node* c = b_[i]; c; c = c->next) f(c->key, c->val);
    }
};
