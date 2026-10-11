// ds/linked_list.h
// -----------------------------------------------------------------------------
// LinkedList<T> : doubly linked list with head and tail pointers.
//
//   operation              complexity
//   --------------------   -----------
//   push_front / push_back O(1)
//   insertAt / removeAt    O(n) to walk to the position, O(1) to relink
//   nodeAt(i)              O(n)
//
// Used for: the ordered list of stops (route) of every train.
// -----------------------------------------------------------------------------
#pragma once
#include <cstddef>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace ds {

template <typename T>
class LinkedList {
public:
    struct Node {
        T value;
        Node* prev = nullptr;
        Node* next = nullptr;
        explicit Node(const T& v) : value(v) {}
    };

    LinkedList() = default;
    LinkedList(const LinkedList& o) { for (const Node* n = o.head_; n; n = n->next) push_back(n->value); }
    LinkedList(LinkedList&& o) noexcept { steal(o); }
    LinkedList& operator=(const LinkedList& o) {
        if (this != &o) { clear(); for (const Node* n = o.head_; n; n = n->next) push_back(n->value); }
        return *this;
    }
    LinkedList& operator=(LinkedList&& o) noexcept {
        if (this != &o) { clear(); steal(o); }
        return *this;
    }
    ~LinkedList() { clear(); }

    size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }
    Node* headNode() { return head_; }
    Node* tailNode() { return tail_; }
    const Node* headNode() const { return head_; }
    const Node* tailNode() const { return tail_; }

    Node* push_back(const T& v) {
        Node* n = new Node(v);
        n->prev = tail_;
        if (tail_) tail_->next = n; else head_ = n;
        tail_ = n;
        ++size_;
        return n;
    }
    Node* push_front(const T& v) {
        Node* n = new Node(v);
        n->next = head_;
        if (head_) head_->prev = n; else tail_ = n;
        head_ = n;
        ++size_;
        return n;
    }
    // Insert so that the new node ends up at position `index` (0 = front).
    Node* insertAt(size_t index, const T& v) {
        if (index > size_) throw std::out_of_range("LinkedList::insertAt");
        if (index == 0) return push_front(v);
        if (index == size_) return push_back(v);
        Node* after = nodeAt(index);
        Node* n = new Node(v);
        n->prev = after->prev;
        n->next = after;
        after->prev->next = n;
        after->prev = n;
        ++size_;
        return n;
    }
    void removeAt(size_t index) {
        if (index >= size_) throw std::out_of_range("LinkedList::removeAt");
        unlink(nodeAt(index));
    }
    void removeNode(Node* n) { unlink(n); }

    Node* nodeAt(size_t index) {
        if (index >= size_) throw std::out_of_range("LinkedList::nodeAt");
        // walk from the nearer end
        if (index < size_ / 2) {
            Node* n = head_;
            for (size_t i = 0; i < index; ++i) n = n->next;
            return n;
        }
        Node* n = tail_;
        for (size_t i = size_ - 1; i > index; --i) n = n->prev;
        return n;
    }
    T& at(size_t index) { return nodeAt(index)->value; }

    void clear() {
        Node* n = head_;
        while (n) { Node* nx = n->next; delete n; n = nx; }
        head_ = tail_ = nullptr;
        size_ = 0;
    }

    // ---- forward iteration ----------------------------------------------------
    template <bool Const>
    class Iter {
        using NodeT = typename std::conditional<Const, const Node, Node>::type;
        NodeT* cur_;
    public:
        explicit Iter(NodeT* n) : cur_(n) {}
        auto& operator*() const { return cur_->value; }
        auto* operator->() const { return &cur_->value; }
        Iter& operator++() { cur_ = cur_->next; return *this; }
        bool operator!=(const Iter& o) const { return cur_ != o.cur_; }
        bool operator==(const Iter& o) const { return cur_ == o.cur_; }
    };
    Iter<false> begin() { return Iter<false>(head_); }
    Iter<false> end() { return Iter<false>(nullptr); }
    Iter<true> begin() const { return Iter<true>(head_); }
    Iter<true> end() const { return Iter<true>(nullptr); }

private:
    Node* head_ = nullptr;
    Node* tail_ = nullptr;
    size_t size_ = 0;

    void steal(LinkedList& o) {
        head_ = o.head_; tail_ = o.tail_; size_ = o.size_;
        o.head_ = o.tail_ = nullptr; o.size_ = 0;
    }
    void unlink(Node* n) {
        if (n->prev) n->prev->next = n->next; else head_ = n->next;
        if (n->next) n->next->prev = n->prev; else tail_ = n->prev;
        delete n;
        --size_;
    }
};

}  // namespace ds
