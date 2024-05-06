//
// Created by Xin A on 2020-04-05.
//

#ifndef T_THREADS_THREAD_H
#define T_THREADS_THREAD_H

#include "common.h"

//
template <int N, int M>
struct meta_func{
    static const int value = N + M;
};


inline std::mutex mtx;
inline std::condition_variable cv;
inline std::vector<int>  vec;

void producer();
void consumer();

class background_task{
public:
    void operator()() const{
        //do_something();
        //do_something_else();
    }
};

class scoped_thread{
    std::thread t;
public:
    explicit scoped_thread(std::thread t_):t(std::move(t_)){
        if(!t.joinable()){
            throw std::logic_error("No thread");
        }
    }
    ~scoped_thread(){
        t.join();
    }
    scoped_thread(scoped_thread const &) = delete;
    scoped_thread &operator=(scoped_thread const &) = delete;
};

class X {//: public std::binary_function<X,X,bool>{
public:
    void do_lengthy_work(int i);
//    bool operator() (X & lhs,X & rhs){
//        return lhs < rhs;
//    }
    bool operator<(const X & rhs) const{
        this->m_a < rhs.m_a;
    }

private:
    int m_a;
};

class TestAdpotLock{
public:
    TestAdpotLock(const X & sd)
    : m_some_detail(sd){}

    friend bool operator<(const TestAdpotLock & lhs,const TestAdpotLock & rhs){
        if(&lhs == &rhs){
            return false;
        }
        std::lock(lhs.m,rhs.m);
        std::lock_guard<std::mutex> lock_a(lhs.m,std::adopt_lock);
        std::lock_guard<std::mutex> lock_b(rhs.m,std::adopt_lock);
        return lhs.m_some_detail < rhs.m_some_detail;
    }
private:
    X m_some_detail;
    mutable std::mutex m;
    //thread_local
};

//2.2 passing arguments to a thread function
void func(int i, const std::string & s);

void oops(int some_param);
void update_data(int id,int & data);
void process_data(int & data);
void oops_again(int id);

template <typename Iterator,typename T>
struct accumulate_block{
    void operator() (Iterator first, Iterator last,T & result){
        result = std::accumulate(first,last,result);
    }
};

template <typename Iterator,typename T>
T parallel_accumulate(Iterator first,Iterator last,T init){
    unsigned long const length = std::distance(first,last);
    //
    unsigned long const num_threads = 3;
    std::vector<T> results(num_threads);
    accumulate_block<Iterator,T>()(first,last,results[num_threads - 1]);
    //std::this_thread::get_id()    //return type is std::thread::id
//std::thread::id
}

#endif //THREADS_THREAD_H
