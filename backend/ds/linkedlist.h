// LinkedList<T> - singly linked list (LINKED LIST). Used for a train's route:
// stations stay in travel order and a stop can be spliced in without shifting.
#pragma once
#include <cstddef>

template <typename T>
class LinkedList {
public:
    struct Node { T val; Node* next; };
private:
    Node *head_ = nullptr, *tail_ = nullptr;
    size_t n_ = 0;
public:
    LinkedList() = default;
    LinkedList(const LinkedList&) = delete;
    LinkedList& operator=(const LinkedList&) = delete;
    ~LinkedList() { clear(); }

    void push_back(const T& v) {                       // O(1)
        Node* nd = new Node{v, nullptr};
        if (tail_) tail_->next = nd; else head_ = nd;
        tail_ = nd; n_++;
    }
    void clear() {
        while (head_) { Node* t = head_; head_ = head_->next; delete t; }
        tail_ = nullptr; n_ = 0;
    }
    Node* head() const { return head_; }
    size_t size() const { return n_; }
    const T* at(int idx) const {                        // O(n)
        Node* c = head_;
        for (int i = 0; c && i < idx; i++) c = c->next;
        return c ? &c->val : nullptr;
    }
    const T* last() const { return tail_ ? &tail_->val : nullptr; }
};
