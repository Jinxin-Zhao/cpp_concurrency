//
// Created by Xin A on 2020-04-05.
//
#include "t_thread.h"

void producer(){
    for (auto i = 0; i < 10 ;++i){
        std::unique_lock<std::mutex> lock(mtx);
        while(!vec.empty()){
            cv.wait(lock);
        }
        vec.push_back(i+1);
        std::cout<<"produce:  "<<i+1<<std::endl;
        cv.notify_all();

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void consumer(){
    for (auto i = 0; i < 10 ;++i){
        std::unique_lock<std::mutex> lock(mtx);
        while(vec.empty()){
            cv.wait(lock);
        }
        int data = vec.back();
        vec.pop_back();
        std::cout<<"consume:  "<<data<<std::endl;
        cv.notify_all();

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}


//2.7 spawn some threads and wait for them to finish
void do_work(unsigned id){
}
static void spf(){
    std::vector<std::thread> threads;
    for(unsigned i = 0; i < 20; ++i){
        threads.push_back(std::thread(do_work,i));
    }
    std::for_each(threads.begin(),threads.end(),std::mem_fn(&std::thread::join));
}

//2.2 passing arguments to a thread function
void func(int i, const std::string & s){
    std::cout<<s.c_str()<<std::endl;
}

//2.2 the string literal is passed as "const char *",and only converted to a std::string in
//the context of the new thread. This is particularly important
void oops(int some_param){
    std::thread t(func,3,"hello");
    //
    char buffer[1024];
    sprintf(buffer,"%i",some_param);
    std::thread(func,3,buffer);  //don't pass buffer like this!!!
    //comment: it's the pointer to the local variable buffer that is passed through to the new thread
    //and there's a significant chance that the function "oops" will exit before the buffer has been
    //converted to a std::string on the new thread,thus leading to a undeined behaviour
    //solution: cast to std::string before passing the buffer to the std::thread constructor
    std::thread(func,3,std::string(buffer));
}

void update_data(int id,int & data){
    data = 888;
    std::cout<<"id: "<<id<<" data: "<<data<<std::endl;
}
void process_data(int & data){
    std::cout<<"start to process data: "<<data<<std::endl;
}
void oops_again(int id){
    int data = 0;
    //the std::thread constructor just blindly copies the supplied values. When it calls update_data,it will
    //end up passing a reference to the internel copy of "data",and not a reference to data itself.
    //std::thread t(update_data,id,data);  //compile error [the third param should be std::ref(data)]
    std::thread t(update_data,id,std::ref(data));
    t.join();
    //process_data(data);  //data is 0
    //solution will be readily apparent
    process_data(std::ref(data)); //data is 888

    X my_x;
    int first_param = 0;
    std::thread ct(&X::do_lengthy_work,&my_x,first_param);
    //this code will invoke my_x.do_lengthy_work() on the new thread
    ct.join();
}

void X::do_lengthy_work(int i){
    std::cout<<"do lengthy work"<<std::endl;
}