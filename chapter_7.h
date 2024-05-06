#ifndef _CHAPTER_H_
#define _CHAPTER_H_

#include "common.h"

template <typename T>
class simple_lock_free_stack {
private:
    struct node {
        shared_ptr<T> _data;
        //T _data;
        node * _next;
        //node(T const & data) : _data(data) {}
        node(T const & data_) : _data(make_shared<T>(data_)) {}
    };

    atomic<node *> _head;
public:
    void push(T const & data) {
        node * const new_node = new node(data);
        new_node->_next = _head.load();
        // a_v.compare_exchange_weak(expected, desired): if (a_v != expected) { expected = a_v; return false;}
        // else {a_v = desired; return true}
        // instatnce:
        // if (_head != new_node->_next) { new_node->_next = _head; return false;}
        // else { new_node->_next = new_node; return true;}
        while (!_head.compare_exchange_weak(new_node->_next, new_node));
    }

    // 有memory leak风险，因为只是移动了head指针，并没有delete
    // 同时也要考虑到head指针是一个空指针
    void pop(T & result) {
        node * old_node = _head.load();
        while(!_head.compare_exchange_weak(old_node, old_node->_next));
        result = old_node->_data;
    }
    // shared_ptr version
    shared_ptr<T> pop() {
        node * old_head = _head.load();
        while (old_head && !_head.compare_exchange_weak(old_head,old_head->_next));
        return old_head ? old_head : shared_ptr<T>();
    }
};


template <typename T>
class lock_free_stack {
private:
    struct node {
        shared_ptr<T> _data;
        node *_next;
        node(T const &data_) : _data(make_shared<T>(data_)) {}
    };

    atomic<node *> _head;

    atomic<unsigned> _thread_in_pop;
    atomic<node *> _to_be_deleted;

    static void * delete_node(node * nodes) {
        while (nodes) {
            node * next = nodes->_next;
            delete nodes;
            nodes = next;
        }
    }

    void try_reclaim(node * old_head) {
        if (_thread_in_pop.load() == 1) {
            node * nodes_to_be_del = _to_be_deleted.exchange(nullptr);
            if (!--_thread_in_pop) {
                // 没人pop，直接一个一个删除
                delete_node(nodes_to_be_del);
            } else if (nodes_to_be_del) {
                chain_pending_nodes(nodes_to_be_del);
            }
            delete old_head;
        } else {
            chain_pending_nodes(old_head);
            --_thread_in_pop;
        }
    }

    void chain_pending_nodes(node * nodes) {
        node * last = nodes;
        while (node * const next = last->_next) {
            last = next;
        }
        // [nodes, last]
        chain_pending_nodes(nodes, last);
    }

    void chain_pending_nodes(node * first, node * last) {
        last->_next = _to_be_deleted;
        while (!_to_be_deleted.compare_exchange_weak(last->_next, first));
    }

public:
    shared_ptr<T> pop() {
        ++_thread_in_pop;
        node * old_head = _head.load();
        while (old_head && !_head.compare_exchange_weak(old_head,old_head->_next));
        shared_ptr<T> res;
        // assgin value to _to_be_deleted
        //_to_be_deleted = old_head;
        if (old_head) {
            res.swap(old_head->_data);
        }
        // 当调用try_reclaim()时，计数器减一，当这个函数被节点调用时，说明这个节点已经被删除4
        try_reclaim(old_head);  //4

//        node * nodes_to_del = _to_be_deleted.load();
//        if (!--_thread_in_pop) {
//            if (_to_be_deleted.compare_exchange_strong(nodes_to_del,nullptr)) {
//                delete_node(nodes_to_del);
//            }
//            delete old_head;
//        } else if (old_head) {
//            old_head->_next = nodes_to_del;
//            while (!_to_be_deleted.compare_exchange_weak(old_head->_next, old_head));
//        }
        return res;
    }
};



#endif