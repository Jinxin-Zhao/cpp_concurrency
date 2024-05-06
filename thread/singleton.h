#ifndef T_THREAD_SINGLETON_H
#define T_THREAD_SINGLETON_H

#include "common.h"

inline std::once_flag resource_flag;

class singleton{
};

static singleton * m_instance_ptr;

singleton& get_instance() {
    if (m_instance_ptr == nullptr) {
        std::lock_guard<std::mutex> mu_lock(singleton::m_mutex);
        if (m_instance_ptr == nullptr) {
            m_instance_ptr = new singleton();
        }
    }
    return *m_instance_ptr;
}


singleton& get_instance() {
    std::call_once(resource_flag, [&](){m_instance_ptr = new singleton();});
    return *m_instance_ptr;
}

static std::atomic<singleton*> m_instance;

singleton* get_instance() {
    singleton* tmp = m_instance.load(std::memory_order_acquire);
    if (tmp == nullptr) {
        std::lock_guard<std::mutex> lock(m_mutex);
        tmp = m_instance.load(std::memory_order_relaxed);
        if (tmp == nullptr) {
            tmp = new singleton;
            m_instance.store(tmp, std::memory_order_release);
        }
    }
    return tmp;
}


////from book
//this is an example of the type of race condition defined as a data race by the C++ Standard
void undefined_behaviour_with_double_checked_locking(){
    if(!resource_ptr){  //#1
        std::lock_guard<std::mutex> lk(resource_mutex);
        if(!resource_ptr) {  //#2
            resource_ptr.reset(new some_resource);  //#3
        }
    }
    resource_ptr->do_something();
}

////from book
////thread-safe lazy initialization of a class member using std::call_once
class XSafe{
public:
    XSafe(const connection_info & connection_details):
        m_connection_details(connection_details){
    }

    void send_data(const data_packet & data){
        std::call_once(m_connection_init_flag,&XSafe::open_connection,this);
        m_connection.send_data(data);
    }

    data_packet receive_data(){
        std::call_once(m_connection_init_flag,&XSafe::open_connection,this);
        return m_connection.receive_data();
    }
private:
    void open_connection(){
        m_connection = connection_manager.open(m_connection_details);
    }

private:
    connection_info    m_connection_details
    connection_handle  m_connection;
    std::once_flag     m_connection_init_flag;
};


////an alternative to std::call_once
////safe in multiple threads
singleton & get_singleton_instance(){
    static singleton instance;
    return instance;
}

#endif