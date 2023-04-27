#pragma once

#include <iterator>
#include <stdexcept>

template <typename T> class ilist {
  public:
    /* the derived class should inherit node publicly,
       or the public member of node (from view of friend ilist)
       will be inaccessible in the derived class */
    class node {
        friend class ilist<T>;

      private:
        T *_prev{nullptr}, *_next{nullptr};
        // mark if the node is in some ilist
        size_t _tag{0};

      public:
        node() = default;

        /* we have to delete copy constructor in the base class
           so that there's no default copy constructor
           in the derived class */
        node(const node &) = delete;
        node &operator=(const node &) = delete;
    };

    class iterator {
        friend class ilist<T>;

      public:
        using iterator_category = std::bidirectional_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = T;
        using pointer = value_type *;
        using reference = value_type &;

        iterator(pointer ptr) : _ptr(ptr) {}

        reference operator*() const { return *_ptr; }
        pointer operator->() const { return _ptr; }

        bool operator==(const iterator &rhs) const { return _ptr == rhs._ptr; }
        bool operator!=(const iterator &rhs) const { return _ptr != rhs._ptr; }

        // --it & ++it
        iterator &operator--() {
            _ptr = _ptr->_prev;
            return *this;
        }
        iterator &operator++() {
            _ptr = _ptr->_next;
            return *this;
        }

      private:
        pointer _ptr{nullptr};
    };

  private:
    T *_head{nullptr}, *_tail{nullptr};
    size_t _size{0}, _tag{0};

    void _mark_node(T *p) { p->_tag = _tag; }

    bool _is_node(T *p) { return p->_tag == _tag; }

    static size_t _alloc_tag() {
        static size_t _next_tag{0};
        _next_tag++;
        return _next_tag;
    }

  public:
    ilist() {
        _head = new T;
        _tail = new T;
        _head->_next = _tail;
        _tail->_prev = _head;
        _tag = _alloc_tag();
        _mark_node(_head);
        _mark_node(_tail);
    }

    ~ilist() {
        while (_size > 0) {
            pop_back();
        }
        delete _head;
        delete _tail;
    }

    iterator begin() { return iterator{_head->_next}; }
    iterator end() { return iterator{_tail}; }

    size_t size() const { return _size; }

    void push_back(T *p) {
        p->_next = _tail;
        p->_prev = _tail->_prev;
        _tail->_prev->_next = p;
        _tail->_prev = p;
        _size += 1;
        _mark_node(p);
    }

    template <typename... Args> void emplace_back(Args... args) {
        push_back(new T{args...});
    }

    void push_front(T *p) {
        p->_next = _head->_next;
        p->_prev = _head;
        _head->_next->_prev = p;
        _head->_next = p;
        _size += 1;
        _mark_node(p);
    }

    template <typename... Args> void emplace_front(Args... args) {
        push_front(new T{args...});
    }

    void pop_back() {
        if (_size == 0) {
            throw std::logic_error{"trying to pop from an empty list"};
        }
        auto p = _tail->_prev;
        p->_prev->_next = _tail;
        _tail->_prev = p->_prev;
        delete p;
        _size -= 1;
    }

    void pop_front() {
        if (_size == 0) {
            throw std::logic_error{"trying to pop from an empty list"};
        }
        auto p = _head->_next;
        p->_next->_prev = _head;
        _head->_next = p->_next;
        delete p;
        _size -= 1;
    }

    iterator erase(const iterator &it) {
        auto p = it._ptr;
        if (p == _head || p == _tail) {
            throw std::logic_error{"trying to erase head or tail"};
        } else if (not _is_node(p)) {
            // TODO: impl stricter check
            throw std::logic_error{"trying to erase a node not in the list"};
        }
        auto ret = iterator{p->_next};
        p->_prev->_next = p->_next;
        p->_next->_prev = p->_prev;
        delete p;
        _size -= 1;
        return ret;
    }

    // insert p before pos
    iterator insert(const iterator &it, T *p) {
        auto p_it = it._ptr;
        if (p_it == _head) {
            throw std::logic_error{"trying to insert before head"};
        } else if (not _is_node(p_it)) {
            // TODO: impl stricter check
            throw std::logic_error{
                "trying to insert before a node not in the list"};
        }
        p->_prev = p_it->_prev;
        p->_next = p_it;
        p_it->_prev->_next = p;
        p_it->_prev = p;
        _size += 1;
        _mark_node(p);
        return p;
    }

    // emplace p before pos
    template <typename... Args>
    iterator emplace(const iterator &it, Args... args) {
        return insert(it, new T{args...});
    }

    T &front() { return *(_head->_next); }
    T &back() { return *(_tail->_prev); }
};
