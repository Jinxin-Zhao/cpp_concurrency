#include "t_thread.h"
#include "series_mutex.h"
#include "future_t.h"
#include "t_hash_table.h"
#include "t_list.h"
#include "t_stack.h"
#include <unistd.h>

hierarchical_mutex high_level_mutex(10000);
hierarchical_mutex low_level_mutex(5000);
hierarchical_mutex other_mutex(100);

int do_low_level_stuff(){
    std::lock_guard<hierarchical_mutex> lk(other_mutex);
    std::cout<<"do low level stuff"<<std::endl;
    return 1;
}
int low_level_func(){
    std::lock_guard<hierarchical_mutex> lk(low_level_mutex);
    return do_low_level_stuff();
}

void high_level_stuff(int some_param){
    std::cout<<__FUNCTION__<<"  solve param"<<std::endl;
}

void thread_a(){
    std::lock_guard<hierarchical_mutex> lk(high_level_mutex);
    high_level_stuff(low_level_func());
}

////
void do_other_stuff(){
    std::cout<<"do other stuff"<<std::endl;
}

void other_stuff(){
    std::lock_guard<hierarchical_mutex> lk(high_level_mutex);
    high_level_stuff(low_level_func());
    do_other_stuff();
}

//std::unique_lock works very well in this situation, as you can call unlock() when
//the code no longer needs access to the shared data, and then call lock() again if
//access is required later in the code:
int process(int s){
}
void write_data(int d){
}
void get_and_process_data(){
    std::mutex some_mutex;
    std::unique_lock<std::mutex> my_lock(some_mutex);
    //some data process
    int data_process = low_level_func();
    my_lock.unlock();
    //we don't need the mutex locked across the call to process()
    int res = process(data_process);
    my_lock.lock();
    write_data(data_process);
}

#include <unordered_map>


class testDemo
{
public:
    testDemo(int num) :num(num) {
        std::cout << "调用构造函数" << std::endl;
    }
    testDemo(const testDemo& other) :num(other.num) {
        std::cout << "调用拷贝构造函数" << std::endl;
    }
    testDemo(testDemo&& other) :num(other.num) {
        std::cout << "调用移动构造函数" << std::endl;
    }
private:
    int num;
};
#include <future>
std::shared_future<int> ss;

struct testand{
    int ee:3;
    int e2:5;
    int e3:7;
};

struct zte{
    int a;
};

struct bzte : zte{
    int b;
};

class test_op{
public:
    test_op(){m_a = 0;}
    test_op(const test_op &) = delete;
    test_op &operator=(const test_op & other) = delete;
    test_op(const test_op &&){}

private:
    int m_a;
};

test_op return_test_op(){
    test_op ob;
    return ob;
}

#include <map>
#include <atomic>
using namespace std;
int main()
{
    string ssszhaojx = "abcdefghijklmn";
    ssszhaojx.reserve(ssszhaojx.size());

    test_op nowtest(return_test_op());
    std::auto_ptr<int> d;
    std::array<int,5> rrrrr;
    std::atomic<int> test_int;
    test_int.store(4);
    int cut = test_int.load();
    int ppre = test_int.fetch_add(3);
    int previous = test_int;
    auto sssssss = std::async(launch::deferred ,process,2);
    sssssss.get();
    bzte bbtee;
    bbtee.a = 1;
    using ii = std::integral_constant<int, 1>;
    std::cout<<"meta func"<<meta_func<1,2>::value<<std::endl;
    testand teste;
    std::cout<<"sizeof testand: "<<sizeof(testand)<<std::endl;
    std::atomic<bool> atb{true};
    bool reab = atb.load();
    atb.store(false);
    //创建空 map 容器
    std::map<std::string, testDemo> mymap;

    cout << "emplace():" << endl;
    for(auto i = 0; i < 3; i++)
        mymap.emplace( "http://c.biancheng.net/stl/", std::move(testDemo(2)));
   /* cout << "emplace_hint():" << endl;
    mymap.emplace_hint(mymap.begin(), "http://c.biancheng.net/stl/", 1);

    cout << "insert():" << endl;
    mymap.insert({ "http://c.biancheng.net/stl/", testDemo(1) });
*/

    std::cout<<"size :"<<mymap.size()<<std::endl;
    auto it = mymap.begin();
    auto v = it->second;
    //std::cout<<"value: "<<it->second<<std::endl;
    return 0;

    std::unordered_map<int,int> mm;
    mm[1] = 22;
    mm[2] = 33;
    mm[3] = 44;
    std::cout<<"mm[3] "<<mm[3]<<std::endl;
    //std::thread t1(producer);
    //std::thread t2(consumer);
    //t1.join();
    //t2.join();

    //background_task f;
    //std::thread(f);

    //int id = 8;
    //oops_again(id);
    //a.print();
    //unsigned long const h = std::thread::hardware_concurrency();
    //std::cout<<"threads nums: "<<h<<std::endl;

    std::thread ta(thread_a);
    std::thread taa(thread_a);
    std::thread taaa(thread_a);
    //std::thread tb(thread_b);

    ta.join();
    taa.join();
    taaa.join();
    //tb.join();

    return 0;
}