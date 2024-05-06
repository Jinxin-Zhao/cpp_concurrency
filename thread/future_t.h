#ifndef _FUTURE_H
#define _FUTURE_H

#include "common.h"

template<typename F,typename A>
std::future<typename std::result_of<F(A&&)>::type> spawn_task(F&& f,A&& a)
{
    typedef typename std::result_of<F(A&&)>::type result_type;
    std::packaged_task<result_type(A&&)> task(std::move(f));
    std::future<result_type> res(task.get_future());
    std::thread(std::move(task),std::move(a));
    return res;
}



#endif