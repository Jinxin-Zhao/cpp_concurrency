#ifndef T_SERIES_MUTEX_H
#define T_SERIES_MUTEX_H

#include <iostream>
#include <mutex>
#include <limits.h>

class TA{
public:
    TA(){
        std::cout<<"TA constructed"<<std::endl;
    }
    void print(){
        std::cout<<"print..."<<std::endl;
    }
};

thread_local TA a;

class hierarchical_mutex{
public:
    explicit hierarchical_mutex(unsigned long value)
    : hierarchy_value(value),previous_hierarchy_value(0){
    }

    void lock(){
        check_for_hierarchy_violation();
        internal_mutex.lock();
        update_hierarchy_value();
    }

    void unlock(){
        this_thread_hierarchy_value = previous_hierarchy_value;
        internal_mutex.unlock();
    }

    bool try_lock(){
        check_for_hierarchy_violation();
        if(!internal_mutex.try_lock())
    return false;
    update_hierarchy_value();
    return true;
}
private:
    std::mutex internal_mutex;
    const unsigned long hierarchy_value;
    unsigned previous_hierarchy_value;
    static thread_local unsigned long this_thread_hierarchy_value;

    void check_for_hierarchy_violation(){
        if(this_thread_hierarchy_value <= hierarchy_value){
            throw std::logic_error("mutex hierarchy violated");
        }
    }
    void update_hierarchy_value(){
        previous_hierarchy_value = this_thread_hierarchy_value;
        this_thread_hierarchy_value = hierarchy_value;
    }

};

thread_local unsigned long hierarchical_mutex::this_thread_hierarchy_value(ULONG_MAX);

#endif