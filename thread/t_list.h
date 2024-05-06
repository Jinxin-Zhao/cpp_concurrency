#ifndef __T_LIST_H
#define __T_LIST_H

#include "common.h"

template <typename T>
class concurrent_list{
public:
    concurrent_list(){}
    concurrent_list(concurrent_list const & other) = delete;
    concurrent_list &operator=(concurrent_list const & other) = delete;

    ~concurrent_list(){
        remove_if([](T const &){return true;});
    }

    template <typename Predicate>
    void remove_if(Predicate p){
        node * current = &head;
        std::unique_lock<std::mutex> lk(head.m);
        while(node * const next = current->next){
            std::unique_lock<std::mutex> next_lk(next->m);
            if(p(*next->data)){
                current->next = next->next;
                next_lk.unlock();
                delete next;
            } else {
                lk.unlock();
                current = next;
                lk = std::move(next_lk);
            }
        }
    }

    void push_front(T const & value){
        std::unique_ptr<node> new_node(new node(value));
        std::lock_guard<std::mutex> lk(head.m);
        new_node->next = head.next;
        head.next = new_node.release();
    }

    template <typename Function>
    void for_each(Function f){
        node * current = &head;
        std::unique_lock<std::mutex> lk(head.m);
        while(node * const next = current->next){
            std::unique_lock<std::mutex> next_lk(next->m);
            lk.unlock();
            f(*next->data);
            current = next;
            lk = std::move(next_lk);
        }
    }

    template <typename Predicate>
    std::shared_ptr<T> find_first_if(Predicate p){
        node * current = &head;
        std::unique_lock<std::mutex> lk(head.m);
        while(node * const next = current->next){
            std::unique_lock<std::mutex> next_lk(next->m);
            lk.unlock();
            if(p(*next->data)){
                return next->data;
            }
            current = next;
            lk = std::move(next_lk);
        }
    }
private:
    struct node{
        node() : next(nullptr){}
        node(T const & value) : data(new T(value)){}

        std::mutex m;
        std::shared_ptr<T> data;
        node * next;
    };

    node head;
};



#endif