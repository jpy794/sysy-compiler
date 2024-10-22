#pragma once

#include <cassert>
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
        virtual ~node() = 0;

        /* we have to delete copy constructor in the base class
           so that there's no default copy constructor
           in the derived class */
        node(const node &) = delete;
        node &operator=(const node &) = delete;
    };

    template <typename elem, bool reverse = false> class raw_iterator {
        friend class ilist<T>;

      public:
        using iterator_category = std::bidirectional_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = elem;
        using pointer = value_type *;
        using reference = value_type &;

        raw_iterator(pointer ptr) : _ptr(ptr) {}

        reference operator*() const { return *_ptr; }
        pointer operator->() const { return _ptr; }

        bool operator==(const raw_iterator &rhs) const {
            return _ptr == rhs._ptr;
        }
        bool operator!=(const raw_iterator &rhs) const {
            return _ptr != rhs._ptr;
        }

        // --it & ++it
        raw_iterator &operator--() {
            _ptr = (reverse ? _ptr->_next : _ptr->_prev);
            return *this;
        }
        raw_iterator &operator++() {
            _ptr = (reverse ? _ptr->_prev : _ptr->_next);
            return *this;
        }

      private:
        pointer _ptr{nullptr};
    };

    using iterator = raw_iterator<T>;
    using reverse_iterator = raw_iterator<T, true>;
    using const_iterator = raw_iterator<const T>;
    using const_reverse_iterator = raw_iterator<const T, true>;

  private:
    T *_head{nullptr}, *_tail{nullptr};
    size_t _size{0}, _tag{0};

    void _mark_node(T *p) { p->_tag = _tag; }
    void _unmark_node(T *p) { p->_tag = 0; }

    bool _is_node(T *p) { return p->_tag == _tag; }

    static size_t _alloc_tag() {
        static size_t _next_tag{0};
        _next_tag++;
        return _next_tag;
    }

  public:
    ilist() {
        _head = static_cast<T *>(::operator new(sizeof(T)));
        _tail = static_cast<T *>(::operator new(sizeof(T)));
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
        ::operator delete(_head);
        ::operator delete(_tail);
    }

    iterator begin() { return iterator{_head->_next}; }
    iterator end() { return iterator{_tail}; }
    reverse_iterator rbegin() { return reverse_iterator{_tail->_prev}; }
    reverse_iterator rend() { return reverse_iterator{_head}; }

    size_t size() const { return _size; }

    void push_back(T *p) {
        p->_next = _tail;
        p->_prev = _tail->_prev;
        _tail->_prev->_next = p;
        _tail->_prev = p;
        _size += 1;
        _mark_node(p);
    }

    template <typename... Args> void emplace_back(Args &&...args) {
        push_back(new T(std::forward<Args>(args)...));
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

    T *release(const iterator &it) {
        auto p = it._ptr;
        if (p == _head || p == _tail) {
            throw std::logic_error{"trying to release head or tail"};
        } else if (not _is_node(p)) {
            // TODO: impl stricter check
            throw std::logic_error{"trying to release a node not in the list"};
        }
        p->_prev->_next = p->_next;
        p->_next->_prev = p->_prev;
        _unmark_node(p);
        _size -= 1;
        return dynamic_cast<T *>(p);
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

    T &front() {
        assert(_size);
        return *(_head->_next);
    }
    T &back() {
        assert(_size);
        return *(_tail->_prev);
    }

    // const method
    const_iterator cbegin() const { return const_iterator{_head->_next}; }
    const_iterator cend() const { return const_iterator{_tail}; }

    const_iterator begin() const { return cbegin(); }
    const_iterator end() const { return cend(); }

    const_reverse_iterator rbegin() const {
        return const_reverse_iterator{_tail->_prev};
    }
    const_reverse_iterator rend() const {
        return const_reverse_iterator{_head};
    }

    const T &front() const {
        assert(_size);
        return *(_head->_next);
    }
    const T &back() const {
        assert(_size);
        return *(_tail->_prev);
    }
};

template <typename T> ilist<T>::node::~node() {}
