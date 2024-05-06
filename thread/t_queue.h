#ifndef T_THREAD_QUEUE_H
#define T_THREAD_QUEUE_H

#include "common.h"

template <typename  T>
class t_queue{
public:
    t_queue(){}
    t_queue(const t_queue & other){
        std::lock_guard<std::mutex> lk(other.mut);
        data_queue = other.data_queue;
    }
    t_queue &operator=(const t_queue & other);

    void push(T new_value){
        std::lock_guard<std::mutex>  lk(mut);
        data_queue.push(new_value);
        data_cond.notify_one();
    }

    bool try_pop(T & value){
        std::lock_guard<std::mutex> lk(mut);
        if(data_queue.empty())
            return false;
        value = data_queue.front();
        data_queue.pop();
    }
    std::shared_ptr<T> try_pop(){
        std::lock_guard<std::mutex> lk(mut);
        if(data_queue.empty())
            return std::shared_ptr<T>();
        std::shared_ptr<T> res(new T(data_queue.front()));
        data_queue.pop();
        return res;
    }

    void wait_and_pop(T & value){
        std::unique_lock<std::mutex>  lk(mut);
        //the implemention of wait() checks the condition and returns if it is satisfied.
        //if the condition is not satisfied,wait() unlocks the mutex and puts the thread in
        //a "waiting" state;
        //if the condition variable is notified by a call to "notify_one()",the thread wakes
        //from its slumber,re-acquires the lock on the mutex and checks the condition again,
        //returning from wait() with the mutex still locked if the condition has been satisfied.
        //if the condition has not been satisfied, the thread unlocks the mutex and resume waiting.
        data_cond.wait(lk,[this]{return !data_queue.empty();});
        value = data_queue.front();
        data_queue.pop();
    }
    std::shared_ptr<T> wait_and_pop(){
        std::unique_lock<std::mutex>  lk(mut);
        data_cond.wait(lk,[this]{return !data_queue.empty();});
        std::shared_ptr<T> res(new T(data_queue.front()));
        data_queue.pop();
        return res;
    }

    bool empty() const {
        std::lock_guard<std::mutex> lk(mut);
        return data_queue.empty();
    }

private:
    std::mutex mut;
    std::queue<T> data_queue;
    std::condition_variable data_cond;
};

template <class T>
t_queue<T> &t_queue<T>::operator=(const t_queue & other){
    std::lock_guard<std::mutex> lk(other.mut);
    if(this == &other){
        return *this;
    }
    data_queue = other.data_queue;
}

/* a simple queue implementation*/
template <typename T>
class s_queue{
public:
    s_queue() : head (nullptr),tail(nullptr){}
    ~s_queue()
    {
        while (head) {
            node *const old_head = head;
            head = old_head->next;
            delete old_head;
        }
    }
    std::shared_ptr<T> try_pop(){
        if(!head){
            return std::shared_ptr<T>();
        }
        std::shared_ptr<T> const res(new T(head->data));
        node * const old_head = head;
        head = old_head->next;
        delete old_head;
        return res;
    }
    void push(T new_value){
        std::unique_ptr<node> p(new node(new_value));
        if(tail){
            tail->next = p.get();
        } else {
            head = p.get();
        }
        tail = p.release();
    }
private:
    struct node{
        T data;
        node * next;
        node(T data_):data(data_),next(nullptr){}
    };
    node * head;
    node * tail;
};

/*separate data by pre-allocating a dummy node with no data*/
template <typename T>
class separate_queue{
public:
    separate_queue() : head (new node),tail(head){}
    ~separate_queue()
    {
        while (head) {
            node *const old_head = head;
            head = old_head->next;
            delete old_head;
        }
    }
    std::shared_ptr<T> try_pop(){
        if(head == tail){
            return std::shared_ptr<T>();
        }
        std::shared_ptr<T> const res(head->data);
        node * const old_head = head;
        head = old_head->next;
        delete old_head;
        return res;
    }
    void push(T new_value){
        std::shared_ptr<T> new_data(new T(new_value));
        std::auto_ptr<node> p(new node);
        tail->data = new_data;
        tail->next = p.get();
        tail = p.release();
    }
private:
    struct node{
        std::shared_ptr<T> data;
        node * next;
        node(): next(nullptr){}
    };
    node * head;
    node * tail;
};

/*a thread-safe queue with fine-grained locking*/
template <typename T>
class thread_queue{
public:
    thread_queue():head(new node),tail(head){}
    thread_queue(const thread_queue &) = delete;
    thread_queue &operator=(const thread_queue &) = delete;

    ~thread_queue(){
        while(head){
            node * const old = head;
            head = old->next;
            delete old;
        }
    }

    std::shared_ptr<T> try_pop(){
        node * old = pop_head();
        if(!old){
            return std::shared_ptr<T>();
        }
        std::shared_ptr<T> const res(old->data);
        delete old;
        return res;
    }

    void push(T new_value){
        std::shared_ptr<T> new_data(new T(new_value));
        std::unique_ptr<node> p(new node);
        std::lock_guard<std::mutex> tail_lock(tail_mutex);
        tail->data = new_data;
        tail->next = p.get();
        tail = p.release();
    }

private:
    struct node{
        std::shared_ptr<T> data;
        node * next;

        node(): next(nullptr){}
    };

    std::mutex head_mutex;
    node * head;
    std::mutex tail_mutex;
    node * tail;

    node * get_tail(){
        std::lock_guard<std::mutex> tail_lock(tail_mutex);
        return tail;
    }
    node * pop_head(){
        //#1 must be ahead of #2, if it did not, then the call to pop_head() could be stuck in between the call to get_tail() and
        //the lock on the head_mutex
        std::lock_guard<std::mutex> head_lock(head_mutex);      //#1
        if(head == get_tail()){                                 //#2
            return nullptr;
        }
        node * const old = head;
        head = old->next;
        return old;
    }
};

/*waiting for an item to pop*/
template <typename  T>
class final_queue{
public:
    final_queue():head(new node),tail(head){}
    final_queue(const final_queue & other) = delete;
    final_queue &operator=(const final_queue & other) = delete;

    ~final_queue(){
        while(head){
            node * const old = head;
            head = old->next;
            delete old;
        }
    }

    std::shared_ptr<T> try_pop(){
        node * const old_head = try_pop_head();
        if(!old_head){
            return std::shared_ptr<T>();
        }
        std::shared_ptr<T> const res(old_head->data);
        delete old_head;
        return res;
    }

    bool try_pop(T & value){
        node * const old_head = try_pop_head(value);
        if(!old_head){
            return false;
        }
        delete old_head;
        return true;
    }

    std::shared_ptr<T> wait_and_pop(){
        node * const old_head = wait_pop_head();
        std::shared_ptr<T> const res(old_head->data);
        delete old_head;
        return res;
    }

    void wait_and_pop(T & value){
        node * const old_head = wait_pop_head(value);
        delete old_head;
    }

    void push(T new_value){
        std::shared_ptr<T> new_data(new T(new_value));
        std::unique_ptr<node> p(new node);
        {
            std::lock_guard<std::mutex> tail_lock(tail_mutex);
            tail->data = new_data;
            tail->next = p.get();
            tail = p.release();
        }
        data_cond.notify_one();
    }

    void empty(){
        std::lock_guard<std::mutex> head_lock(head_mutex);
        return head == get_tail();
    }
private:
    struct node{
        node() : next(nullptr){}
        std::shared_ptr<T> data;
        node * next;
    };

    node * get_tail(){
        std::lock_guard<std::mutex> tail_lock(tail_mutex);
        return tail;
    }

    node * pop_head(){
        node * const old_head = head;
        head = old_head->next;
        return old_head;
    }

    node * try_pop_head(){
        std::lock_guard<std::mutex> head_lock(head_mutex);
        if(head == get_tail()){
            return nullptr;
        }
        return pop_head();
    }

    node * try_pop_head(T & value){
        std::lock_guard<std::mutex> head_lock(head_mutex);
        if(head == get_tail()){
            return nullptr;
        }
        value = *head->data;
        return pop_head();
    }

    std::unique_lock<std::mutex> wait_for_data(){
        std::unique_lock<std::mutex> head_lock(head_mutex);
        data_cond.wait(head_lock,[&]{return head != get_tail();});
        return head_lock;
    }

    node * wait_pop_head(){
        std::unique_lock<std::mutex> head_lock(wait_for_data());
        return pop_head();
    }

    node * wait_pop_head(T & value){
        std::unique_lock<std::mutex> head_lock(wait_for_data());
        value = *head->data;
        return pop_head();
    }
private:
    std::mutex head_mutex;
    node * head;
    std::mutex tail_mutex;
    node * tail;
    std::condition_variable data_cond;
};

#endif