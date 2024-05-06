#ifndef _CHAPTER_9_H
#define _CHAPTER_9_H

#include "chapter_6.h"
#include "chapter_8.h"

// 函数包装器，为实现一个可等待任务的线程池
// 包装一个自定义函数，用来处理只可移动的类型。
// 这就是一个带有函数操作符的类型擦除类。只需要处理那些没有函数和无返回的函数，
// 所以这是一个简单的虚函数调用。
class function_wrapper {
    struct impl_base {
        virtual void call() = 0;
        virtual ~impl_base() {}
    };

    template <typename FunctionType>
    struct impl_type : impl_base {
        FunctionType _f;
        impl_type(FunctionType && f) : _f(move(f)) {}
        void call() { _f();}
    };

    unique_ptr<impl_base> _impl;
public:
    template <typename F>
    function_wrapper(F && f) : _impl(new impl_type<F>(move(f))) {}

    void operator()() { _impl->call(); }

    function_wrapper() = default;

    function_wrapper(function_wrapper&& other) : _impl(move(other._impl)) {}

    function_wrapper & operator=(function_wrapper&& other) {
        _impl = move(other._impl);
        return *this;
    }

    function_wrapper(const function_wrapper& other) = delete;
    function_wrapper(function_wrapper &) = delete;
    function_wrapper & operator=(const function_wrapper&) = delete;
};

// 9.1.5 空闲线程窃取其他线程任务简单加锁实现
class work_stealing_queue {
private:
    using data_type = function_wrapper;
    deque<data_type> the_queue;
    mutable mutex the_mtx;
public:
    work_stealing_queue() {}

    work_stealing_queue(const work_stealing_queue & other) = delete;
    work_stealing_queue & operator=(const work_stealing_queue & other) = delete;
    void push(data_type data) {
        lock_guard<mutex> lock(the_mtx);
        the_queue.push_front(move(data));
    }
    bool empty() const {
        lock_guard<mutex> lock(the_mtx);
        return the_queue.empty();
    }

    bool try_pop(data_type & res) {
        lock_guard<mutex> lock(the_mtx);
        if (the_queue.empty()) {
            return false;
        }
        res = move(the_queue.front());
        the_queue.pop_front();
        return true;
    }

    bool try_steal(data_type & res) {
        lock_guard<mutex> lock(the_mtx);
        if (the_queue.empty()) {
            return false;
        }
        res = move(the_queue.back());
        the_queue.pop_back();
        return true;
    }
};

class simple_thread_pool {
    atomic_bool _done;

    thread_safe_queue<function<void()>> _work_queue;
    thread_safe_queue<function_wrapper> _func_wrapper_queue_;

    // use thread local queue
    using local_queue_type = queue<function_wrapper>;
    static thread_local unique_ptr<local_queue_type> local_work_queue;

    vector<thread> _threads;
    join_threads _joiner;

    void work_thread() {
        while (!_done) {
            function<void()> task;
            if (_work_queue.try_pop(task)) {
                task();
            } else {
                this_thread::yield();
            }
        }
    }

    void worker_thread_func() {
        while (!_done) {
            function_wrapper _task;
            if (_func_wrapper_queue_.try_pop(_task)) {
                _task();
            } else {
                this_thread::yield();
            }
        }
    }

    // for thread local work_thread
    void thread_local_work() {
        // 每个线程自己维护自己的任务队列
        local_work_queue.reset(new local_queue_type);
        while (!_done) {
            run_local_pending_task();
        }
    }
public:
    simple_thread_pool() : _done(false), _joiner(_threads) {
        unsigned const thread_count = thread::hardware_concurrency();
        try {
            for (unsigned i = 0; i < thread_count; i++) {
                _threads.push_back(thread(&simple_thread_pool::worker_thread_func, this));
                // 9.1.4 如果是调用thread_local版本,如下
                //_threads.push_back(thread(&simple_thread_pool::thread_local_work,this));
            }
        } catch (...) {
            _done = true;
            throw;
        }
    }
    ~simple_thread_pool() {
        _done = true;
    }

    // simple version
//    template <typename FunctionType>
//    void submit(FunctionType f) {
//        _work_queue.push(function<void()>(f));
//    }

    // wrapped version
    template <typename FunctionType>
    future<typename result_of<FunctionType()>::type> submit(FunctionType f) {
        typedef typename result_of<FunctionType()>::type result_type;
        packaged_task<result_type ()> _task(move(f));
        future<result_type> res(_task.get_future());
        // std::package_task<>实例是不可拷贝的，仅是可移动的，所以不能再使用function<>来实现任务队列
        // 因为std::function<>需要存储可复制构造的函数对象
        _func_wrapper_queue_.push(move(_task));
        return res;
    }

    // 9.1.3 for concurrency quick sort
    void run_pending_task() {
        function_wrapper task;
        if (_func_wrapper_queue_.try_pop(task)) {
            task();
        } else {
            this_thread::yield();
        }
    }

    // 9.1.4 for thread_local_pending task
    // wrapped version
    template <typename FunctionType>
    future<typename result_of<FunctionType()>::type> local_submit(FunctionType f) {
        typedef typename result_of<FunctionType()>::type result_type;
        packaged_task<result_type ()> _task(move(f));
        future<result_type> res(_task.get_future());
        // std::package_task<>实例是不可拷贝的，仅是可移动的，所以不能再使用function<>来实现任务队列
        // 因为std::function<>需要存储可复制构造的函数对象
        if (local_work_queue) {
            local_work_queue->push(move(_task));
        } else {
            _func_wrapper_queue_.push(move(_task));
        }
        return res;
    }

    void run_local_pending_task() {
        function_wrapper task;
        if (local_work_queue && !local_work_queue->empty()) {
            task = move(local_work_queue->front());
            local_work_queue->pop();
            task();
        } else if (_func_wrapper_queue_.try_pop(task)) {
            task();
        } else {
            this_thread::yield();
        }
    }

};

// use thread_pool, a unit test function
template<typename Iterator, typename T>
T parallel_accumulate(Iterator first, Iterator last, T init) {
    auto len = distance(first, last);
    if (!len) {
        return init;
    }
    unsigned long const block_size = 25;
    unsigned long const num_blocks = (len + block_size - 1) / block_size;
    vector<future<T>> futures(num_blocks - 1);
    simple_thread_pool pool;
    Iterator block_start = first;
    int i_v = 0;
    for (unsigned long i = 0; i < num_blocks - 1; ++i) {
        Iterator block_end = block_start;
        // it 表示某个迭代器，n 为整数。该函数的功能是将 it 迭代器前进或后退 n 个位置。
        advance(block_end, block_size);
        function<int ()> f = [=]() ->int {
            int i = 0;
            //return accumulate_block<Iterator, T>(block_start, block_end, i);
            return accumulate(block_start, block_end, i);
        };
        futures[i] = pool.submit(f);
        block_start = block_end;
    }
    // 每个块25就交给一个线程，剩下的看下一步
    //T last_result = accumulate_block<Iterator, T>(block_start, last, i_v);
    T last_result = accumulate(block_start, last, i_v);
    T result = init;
    for (unsigned long i = 0; i < num_blocks - 1; ++i) {
        result += futures[i].get();
    }
    result += last_result;
    return result;
}

// 9.1.3
template<typename T>
struct td_quick_sorter {
    simple_thread_pool pool;
    list<T> do_sort(list<T> & chunk_data) {
        if (chunk_data.empty()) {
            return chunk_data;
        }
        list<T> result;
        // move chunk_data to result
        result.splice(result.begin(), chunk_data, chunk_data.begin());
        T const & partition_val = *result.begin();
        // 该算法不能保证元素的初始相对位置，如果需要保证初始相对位置，应该使用stable_partition
        // 满足判断条件pred的元素会被放在区间的前段，不满足pred的元素会被放在区间的后段
        typename list<T>::iterator divide_point = partition(chunk_data.begin(), chunk_data.end(), [&](T const & val){
            return val < partition_val;
        });
        //
        list<T> new_lower_chunk;
        new_lower_chunk.splice(new_lower_chunk.end(),chunk_data, chunk_data.begin(),divide_point);

        future<list<T>> new_lower = pool.submit(bind(&td_quick_sorter::do_sort, this, move(new_lower_chunk)));
        // 此时chunk_data里面只有divide_point及后面的元素了
        list<T> new_higher(do_sort(chunk_data));
        // 在result.end()前面插入new_higher
        result.splice(result.end(), new_higher);
        while (!new_lower.wait_for(chrono::seconds(0)) == future_status::timeout) {
            pool.run_pending_task();
        }
        // 在result.begin()前面插入new_lower()
        result.splice(result.begin(), new_lower.get());
        return result;
    }


};

template <typename T>
list<T> td_quick_sort(list<T> input) {
    if (input.empty()) {
        return input;
    }
    td_quick_sorter<T> s;
    return s.do_sort(input);
}

// steal thread pool
class steal_thread_pool {
    using task_type = function_wrapper;
    atomic_bool _done;
    thread_safe_queue<task_type> _pool_work_queue;
    vector<unique_ptr<work_stealing_queue>> _queues;
    vector<thread> _threads;
    join_threads _joiner;

    // 利用一个dequeue实现的简易加锁的queue
    static thread_local work_stealing_queue * local_work_queue;
    static thread_local unsigned _my_index;

    void worker_thread(unsigned my_index) {
        _my_index = my_index;
        local_work_queue = _queues[my_index].get();
        while (!_done) {
            run_pending_task();
        }
    }

    bool pop_task_from_local_queue(task_type & task) {
        return local_work_queue && local_work_queue->try_pop(task);
    }

    bool pop_task_from_pool_queue(task_type & task) {
        return _pool_work_queue.try_pop(task);
    }

    bool pop_task_from_other_thread_queue(task_type & task) {
        for (unsigned i = 0; i < _queues.size(); i++) {
            unsigned const index = (_my_index + i + 1) % _queues.size();
            if (_queues[index]->try_steal(task)) {
                return true;
            }
        }
        return false;
    }

    public:
        steal_thread_pool() : _done(false), _joiner(_threads){
            unsigned const thread_count = thread::hardware_concurrency();
            try {
                for (unsigned i = 0; i < thread_count; ++i) {
                    _queues.push_back(unique_ptr<work_stealing_queue>(new work_stealing_queue));
                    _threads.push_back(thread(&steal_thread_pool::worker_thread, this, i));
                }
            } catch (...) {
                _done = true;
                throw;
            }
        }

        ~steal_thread_pool(){
            _done = true;
        }

        template <typename FunctionType>
        future<typename result_of<FunctionType()>::type> submit (FunctionType f) {
            using result_type = typename result_of<FunctionType()>::type;
            packaged_task<result_type()> _task(f);
            future<result_type> res(_task.get_future());
            if (local_work_queue) {
                local_work_queue->push(move(_task));
            } else {
                _pool_work_queue.push(move(_task));
            }
            return res;
        }

        void run_pending_task() {
           task_type task;
           if (pop_task_from_local_queue(task) ||
                   pop_task_from_pool_queue(task) ||
                   pop_task_from_other_thread_queue(task)) {
               task();
           } else {
               this_thread::yield();
           }
        }
};

//////////////////////////// interruptible thread
class interrupt_flag {
    atomic<bool> _flag;
    condition_variable * _thread_cond;
    mutex set_clear_mutex;
public:
    interrupt_flag() : _thread_cond(0) {}
    void set() {
        _flag.store(true, std::memory_order_relaxed);
        lock_guard<mutex> lk(set_clear_mutex);
        if (_thread_cond) {
            _thread_cond->notify_all();
        }
    }
    bool is_set() const {
        return _flag.load(std::memory_order_relaxed);
    }
    void set_condition_variable(condition_variable & cv) {
        lock_guard<mutex> lk(set_clear_mutex);
        _thread_cond = &cv;
    }

    void clear_condition_variable() {
        lock_guard<mutex> lk(set_clear_mutex);
        _thread_cond = 0;
    }

    struct clear_cv_on_destruct {
        ~clear_cv_on_destruct(){
            //this_thread_interrupt_flag.clear_condition_variable();
        }
    };
};

thread_local interrupt_flag this_thread_interrupt_flag;

class interruptible_thread {
    thread _thread;
    interrupt_flag * _flag;
public:
    template<typename FunctionType>
    interruptible_thread(FunctionType f) {
        promise<interrupt_flag *> p;
        _thread = thread([f, &p] {
            p.set_value(&this_thread_interrupt_flag);
            f();
        });
        _flag = p.get_future().get();
    }
    void interrupt() {
        if (_flag) {
            _flag->set();
        }
    }

    void join();
    void detach();
    bool joinable() const;

    void interruption_point() {
        if (this_thread_interrupt_flag.is_set()) {
            //throw thread_interrupted();
        }
    }

    void interruptible_wait(condition_variable & cv, unique_lock<mutex> & lk) {
        interruption_point();
        this_thread_interrupt_flag.set_condition_variable(cv);
        interrupt_flag::clear_cv_on_destruct guard;
        interruption_point();
        cv.wait_for(lk, chrono::milliseconds(1));
        interruption_point();
    }

//    void foo() {
//        while (!_done) {
//            interruption_point();
//            // process next item
//        }
//    }

};

// bind cpu cores
void * threadFunctions(void * arg) {
    int cpuId = *reinterpret_cast<int*>(arg);
    cpu_set_t cpu_s;
    CPU_ZERO(&cpu_s);
    CPU_SET(cpuId, &cpu_s);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpu_s);
    // do something

    return nullptr;
}

int test_cpu_bind(){
    pthread_t _thread;
    int cpuId = 0;
    int result = pthread_create(&_thread,nullptr,threadFunctions, &cpuId);
    if (result != 0) {
        cerr << "error to create thread " << result << endl;
        return 1;
    }
    pthread_join(_thread, nullptr);
    return 0;
}

#endif