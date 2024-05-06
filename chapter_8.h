#ifndef __CHAPTER_8_H_
#define __CHAPTER_8_H_

#include "common.h"
#include "chapter_3.h"

using namespace std;

// 8.49
template <typename T>
struct sorter {
    struct chunk_to_sort {
        std::list<T> data;
        bool end_of_data{false};
        std::promise<std::list<T> > promise;
        chunk_to_sort(bool done = false): end_of_data(done){}

        chunk_to_sort(const chunk_to_sort & other){
            end_of_data = other.end_of_data;
            data = other.data;
            //promise = std::move(other.promise);
        }

        chunk_to_sort(const chunk_to_sort && other){
            end_of_data = other.end_of_data;
            data = std::move(other.data);
            //promise = std::move(other.promise);
        }
    };

    thread_safe_stack<chunk_to_sort> chunks;
    std::vector<std::thread> threads;
    unsigned const max_thread_count;

    //
    sorter(): max_thread_count(std::thread::hardware_concurrency() - 1) {}

    ~sorter() {
        for (unsigned i = 0; i < threads.size(); ++i) {
            chunks.push(std::move(chunk_to_sort(true)));
        }
        for (unsigned i = 0; i < threads.size(); ++i) {
            threads[i].join();
        }
    }

    bool try_sort_chunk() {
        std::shared_ptr<chunk_to_sort> chunk = chunks.pop();
        if (chunk) {
            if (chunk->end_of_data) {
               return false;
            }
            sort_chunk(chunk);
        }
        return true;
    }

    std::list<T> do_sort(std::list<T> & chunk_data) {
        if (chunk_data.empty()) {
            return chunk_data;
        }
        std::list<T> result;
        result.splice(result.begin(), chunk_data, chunk_data.begin());
        T const & partition_val = *result.begin();
        typename std::list<T>::iterator divide_point = std::partition(chunk_data.begin(), chunk_data.end(),  [partition_val](T const & elem){
            return elem < partition_val;
        });
        chunk_to_sort new_lower_chunk;
        new_lower_chunk.data.splice(new_lower_chunk.data.begin(), chunk_data,chunk_data.begin(),divide_point);
        //std::unique_future<std::list<T>> new_lower = new_lower_chunk.promised.get_future();
        std::future<std::list<T>> new_lower = new_lower_chunk.promise.get_future();
        chunks.push(std::move(new_lower_chunk));
        if (threads.size() < max_thread_count) {
            threads.push_back(std::thread(&sorter::try_sort_chunk, this));
        }
        std::list<T> new_upper(do_sort(chunk_data));
        result.splice(result.end(), new_upper);

        
        new_lower.get();
        try_sort_chunk();
        //
//        std::future_status status;
//        do {
//            status = new_lower.template wait_for(std::chrono::seconds(2));
//            switch (status) {
//                case std::future_status::timeout:
//                    std::cout << "timeout" << std::endl;
//                    break;
//                case std::future_status::deferred:
//                    std::cout << "deferred" << std::endl;
//                    break;
//                case std::future_status::ready:
//                    std::cout << "ready" << std::endl;
//                    try_sort_chunk();
//                    break;
//            }
//        }while (status != std::future_status::ready);

        result.splice(result.begin(), new_lower.get());
        return result;
    }

    void sort_chunk(std::shared_ptr<chunk_to_sort> const & chunk) {
        chunk->promise.set_value(do_sort(chunk->data));
    }
    void sort_thread() {
        while (try_sort_chunk()) {
            std::this_thread::yield();
        }
    }
};



template <typename T>
std::list<T> parallel_quick_sort(std::list<T> input) {
    if (input.empty()) {
        return input;
    }
    sorter<T> s;
    return s.do_sort(input);
}


class join_threads
{
    std::vector<std::thread>& threads;
public:
    explicit join_threads(std::vector<std::thread>& threads_):
            threads(threads_)
    {}
    ~join_threads()
    {
        for(unsigned long i=0; i<threads.size(); ++i)
        {
            if(threads[i].joinable())
                threads[i].join();
        }
    }
};

template<typename Iterator, typename T>
struct accumulate_block {
    T operator()(Iterator first, Iterator last, T & result) {
        result = std::accumulate(first, last, result);
    }
};

#endif