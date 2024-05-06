#ifndef _CHAPTER_H_
#define _CHAPTER_H_

#include "common.h"

// single thread version list
template <typename T>
class single_thread_queue {
public:
    single_thread_queue() {}
    single_thread_queue(const single_thread_queue & other) = delete;
    single_thread_queue & operator=(const single_thread_queue & other) = delete;

    shared_ptr<T> try_pop()  {
        if (!_head) {
            return shared_ptr<T>();
        }
        shared_ptr<T> result(make_shared<T>(move(_head->_data)));
        unique_ptr<node> const old_head = move(_head);
        _head = move(old_head->next);   // 3
        return result;
    }

    // 当多线程场景下：
    // push可以同时修改 _head & _tail
    // 且push会更新_tail->next,而 try_pop会读取(_head->next)当 _head==_tail时。
    // 就会发生两个函数操作同一个对象
    // 此时这个_head->next对象需要保护，肯定就是加锁，然而当这个对象未被_head和_tail同时访问时，
    // push和try_pop锁住的是同一个锁
    void push(T new_value) {
        unique_ptr<node> p(new node(move(new_value)));
        node * const new_tail = p.get();
        if (_tail) {
            _tail->next = move(p); //4
        } else {
            _head = move(p); //5
        }
        _tail = new_tail; //6
    }
private:
    struct node {
        T _data;
        unique_ptr<node> next;

        node(T data) : _data(move(data)) {}
    };

    unique_ptr<node> _head;
    node * _tail;
};

// 通过增加一个预分配虚拟节点，确保这个节点永远在队列的最后，用来分离头尾指针能访问的节点的办法
template <typename T>
class virtual_node_queue {
public:
    virtual_node_queue() : _head(new node), _tail(_head.get()) {}
    virtual_node_queue(const virtual_node_queue & other) = delete;
    virtual_node_queue & operator=(const virtual_node_queue & other) = delete;

    shared_ptr<T> try_pop() {
        if (_head.get() == _tail) {
            return shared_ptr<T>();
        }
        shared_ptr<T> const result(_head->data);
        unique_ptr<node> old_head = move(_head);
        _head = move(old_head->next);
        return result;
    }

    // 现在push只能访问tail，而不能访问head，try_pop只会在一开始访问tail进行比较，时间很短
    // 所以这里不再需要互斥了，只需要一个互斥量来保护head和tail就够了，应该锁哪里呢
    // 目的是最大程度并发化，需要上锁时间尽可能小，互斥量需要对tail的访问进行上锁，锁需要持续到函数结束时才能解开
    void push(T new_value) {
        shared_ptr<T> new_data(make_shared<T>(move(new_value)));
        unique_ptr<node> p(new node);
        _tail->data = new_data;
        node * const new_tail = p.get();
        _tail->next = move(p);
        _tail = new_tail;
    }
private:
    struct node {
        shared_ptr<T> data;
        unique_ptr<node> next;
    };

    unique_ptr<node> _head;
    node * _tail;
};

// 这个队列的实现将作为第7章无锁队列的基础。这是一个无界(unbounded)队列;线程可以持续向队列中添加数据项，即使没有元素被删除。
// 与之相反的就是有界(bounded)队列，在有界队列中，队列在创建的时候最大长度就已经是固定的了。当有界队列满载时，尝试在向其添加元素的操作将会失败或者阻塞，
// 直到有元素从队列中弹出。在任务执行时(详见第8章)，有界队列对于线程间的工作花费是很有帮助的。其会阻止线程对队列进行填充，并且可以避免线程从较远的地方对数据项进行索引。
template <typename T>
class thread_safe_queue {
public:
    thread_safe_queue(): _head(new node), _tail(_head.get()) {}
    thread_safe_queue(const thread_safe_queue & other) = delete;
    thread_safe_queue operator=(const thread_safe_queue & other) = delete;

    shared_ptr<T> try_pop() {
        unique_ptr<node> old_head = pop_head();
        return old_head ? old_head->data : shared_ptr<T>();
    }
    bool try_pop(T & value) {
        unique_ptr<node> const old_head = pop_head(value);
        return old_head != nullptr;
    }

    bool empty() {
        lock_guard<mutex> head_lock(head_mutex);
        return (_head.get() == get_tail());
    }

    // normal version
    void push(T new_value) {
        shared_ptr<T> new_data(make_shared<T>(move(new_value)));
        unique_ptr<node> p(new node);
        node * const new_tail = p.get();
        lock_guard<mutex> tail_lock(tail_mutex);
        _tail->data = new_data;
        _tail->next = move(p);
        _tail = new_tail;
    }

    // condition version
    // 当将互斥量和notify_one()混用的时，如果被通知的线程在互斥量解锁后被唤醒，那么这个线程就不得不等待互斥量上锁。
    // 另一方面，当解锁操作在notify_one()之前调用，那么互斥量可能会等待线程醒来，来获取互斥锁(假设没有其他线程对互斥量上锁)。
    void push_cond(T new_value) {
        shared_ptr<T> new_data(make_shared<T>(move(new_value)));
        unique_ptr<node> p(new node);
        {
            lock_guard<mutex> tail_lock(tail_mutex);
            _tail->data = new_data;
            node * const new_tail = p.get();
            _tail->next = move(p);
            _tail = new_tail;
        }
        data_cond.notify_one();
    }

    // condition version
    shared_ptr<T> wait_and_pop() {
        unique_ptr<node> const old_head = wait_pop_head();
        return old_head->data;
    }
    // condition version
    void wait_and_pop(T & value) {
        unique_ptr<node> const old_head = wait_pop_head(value);
    }

private:
    struct node {
        shared_ptr<T> data;
        unique_ptr<node> next;
    };

    mutex head_mutex;
    unique_ptr<node> _head;

    mutex tail_mutex;
    node * _tail;

    condition_variable data_cond;

    node * get_tail() {
        lock_guard<mutex> tail_lock(tail_mutex);
        return _tail;
    }

    // normal version
    unique_ptr<node> pop_head() {
        lock_guard<mutex> head_lock(head_mutex);
        if (_head.get() = get_tail()) {
            return nullptr;
        }
        return pop_head_cond();
    }
    unique_ptr<node> pop_head(T & value) {
        lock_guard<mutex> head_lock(head_mutex);
        if (_head.get() == get_tail()) {
            return nullptr;
        }
        value = move(*_head->data);
        return pop_head_cond();
    }

    // condition version
    unique_ptr<node> pop_head_cond() {
        unique_ptr<node> old_head = move(_head);
        _head = move(old_head->next);
        return old_head;
    }
    // condition version
    unique_lock<mutex> wait_for_data() {
        unique_lock<mutex> head_lock(head_mutex);
        data_cond.wait(head_lock,[&] {return _head.get() != get_tail();});
        return move(head_lock);
    }
    // condition version
    unique_ptr<node> wait_pop_head() {
        unique_lock<mutex> head_lock(wait_for_data());
        return pop_head_cond();
    }
    // condition version
    unique_ptr<node> wait_pop_head(T & value) {
        unique_lock<mutex> head_lock(wait_for_data());
        value = move(*_head->data);
        return pop_head_cond();
    }
};

// 6.3 基于锁设计更加复杂的数据结构
// 6.3.1 编写一个使用锁的线程安全查询表
// 查询表基本操作有： 添加、修改、删除、通过给定键值获取对应数据
template <typename Key, typename Value, typename Hash = hash<Key>>
class thread_safe_lookup_table {
private:
    class bucket_type {
    public:

        Value value_for(Key const & key, Value const & value) const {
            shared_lock<shared_mutex> lock(sh_mutex);
            bucket_iterator const found_entry = find_entry_for(key);
            return (found_entry == data.end()) ? value : found_entry->second;
        }

        void add_or_update_mapping(Key const & key, Value const & value) {
            unique_lock<shared_mutex> lock(sh_mutex);
            bucket_iterator const found_entry = find_entry_for(key);
            if (found_entry == data.end()) {
                data.push_back(bucket_value(key, value));
            } else {
                found_entry->second = value;
            }
        }

        void remove_mapping(Key const & key) {
            unique_lock<shared_mutex> lock(sh_mutex);
            bucket_iterator const found_entry = find_entry_for(key);
            if (found_entry == data.end()) {
                data.erase(found_entry);
            }
        }

    private:
        using bucket_value = pair<Key, Value>;
        using bucket_data = list<bucket_value>;
        using bucket_iterator = typename bucket_data::iterator;

        bucket_data data;
        mutable shared_mutex sh_mutex;

        bucket_iterator find_entry_for(Key const & key) const {
            // O(n), traverse list
            return find_if(data.begin(), data.end(), [&](bucket_value const & item) {
                return item.first == key;
            });
        }
    };

public:
    using key_type = Key;
    using mapped_type = Value;
    using hash_type = Hash;
    thread_safe_lookup_table(unsigned num_buckets = 19, Hash const & hasher_ = Hash()) : buckets(num_buckets), hasher(hasher_) {
        for (auto i = 0; i < num_buckets; ++i) {
            buckets[i].reset(new bucket_type);
        }
    }
    thread_safe_lookup_table(thread_safe_lookup_table const & other) = delete;
    thread_safe_lookup_table & operator=(thread_safe_lookup_table const & other) = delete;

    Value value_for(Key const & key, Value const & default_value = Value()) const {
        return get_bucket(key).value_for(key,default_value);
    }

    void add_or_update_mapping(Key const & key, Value const & value) {
        get_bucket(key).add_or_update_mapping(key, value);
    }
    void remove_mapping(Key const & key) {
        get_bucket(key).remove_mapping(key);
    }

    // 查询表的一个“可有可无”(nice-to-have)的特性，会将选择当前状态的快照，例如，一个std::map<>
    // 这将要求锁住整个容器，用来保证拷贝副本的状态是可以索引的，这将要求锁住所有的桶。因为对于查询表的“普通”的操作，
    // 需要在同一时间获取一个桶上的一个锁，而这个操作将要求查询表将所有桶都锁住。因此，只要每次以相同的顺序进行上锁(例如，递增桶的索引值)，就不会产生死锁。
    map<Key,Value> get_map() const {
        vector<unique_lock<shared_mutex>> locks;
        for (unsigned int i = 0; i < buckets.size(); i++) {
            locks.push_back(unique_lock<shared_mutex>(buckets[i]->sh_mutex));
        }
        map<Key, Value> res;
        for (unsigned int i = 0; i < buckets.size(); i++) {
            for (auto it = buckets[i].data.begin(); it != buckets[i].data.end();++it) {
                res.insert(*it);
            }
        }
        return res;
    }

private:
    vector<unique_ptr<bucket_type>> buckets;
    Hash hasher;
    bucket_type & get_bucket(Key const & key) const {
        size_t const bucket_index = hasher(key) % buckets.size();
        return *buckets[bucket_index];
    }
};

// 6.3.2 线程安全链表
template <typename T>
class thread_safe_list {
    struct node {
        mutex m;
        shared_ptr<T> _data;
        unique_ptr<node> _next;
        node() : _next() {}
        node(T const & value) : _data(make_shared<T>(value)) {}
    };
    node _head;

public:
    thread_safe_list() {}
    ~thread_safe_list() {
        // 自定义成员remove_if
        remove_if([](node const &) {return true;});
    }
    thread_safe_list(thread_safe_list const & other) = delete;
    thread_safe_list & operator=(thread_safe_list const & other) = delete;

    void push_front(T const & value) {
        unique_ptr<node> new_node(new node(value));
        lock_guard<mutex> lk(_head.m);
        new_node->_next = move(_head._next);
        _head._next = move(new_node);
    }

    template<typename Fuction>
    void for_each(Fuction f) {
        node * current = &_head;
        unique_lock<mutex> lk(_head.m);
        while (node * const next = current->_next.get()) {
            unique_lock<mutex> next_lk(next->m);
            lk.unlock();
            f(*next->_data);
            current = next;
            lk = move(next_lk);
        }
    }

    template<typename Predicate>
    shared_ptr<T> find_first_if(Predicate p) {
        node * current = &_head;
        unique_lock<mutex> lk(_head.m);
        while (node * const next = current->_next.get()) {
            unique_lock<mutex> next_lk(next->m);
            lk.unlock();
            if(p(*next->_data)) {
                return next->_data;
            }
            current = next;
            lk = move(next_lk);
        }
        return shared_ptr<T>();
    }

    template<typename Predicate>
    void remove_if(Predicate p) {
        node * current = &_head;
        unique_lock<mutex> lk(_head.m);
        while (node * const next = current->_next.get()) {
            unique_lock<mutex> next_lk(next->m);
            if (p(*next->_data)) {
                unique_ptr<node> old = move(current->_next);
                current->_next = move(next->_next);
                next_lk.unlock();
            } else {
                lk.unlock();
                current = next;
                lk = move(next_lk);
            }
        }
    }
};




#endif