#ifndef __CHAPTER_5_H_
#define __CHAPTER_5_H_

#include "common.h"

// Store操作，可选如下顺序：memory_order_relaxed, memory_order_release, memory_order_seq_cst。
// Load操作，可选如下顺序：memory_order_relaxed, memory_order_consume, memory_order_acquire, memory_order_seq_cst。
// Read-modify-write(读-改-写)操作，可选如下顺序：memory_order_relaxed, memory_order_consume, memory_order_acquire, memory_order_release, memory_order_acq_rel, memory_order_seq_cst。
// 所有操作的默认顺序都是memory_order_seq_cst。



void test_atomic_flag() {
// atomic_flag
// std::atomic_flag类型的对象必须被ATOMIC_FLAG_INIT初始化。初始化标志位是“清除”状态。这里没得选择；这个标志总是初始化为“清除”：
    atomic_flag f = ATOMIC_FLAG_INIT;

// 在初始化后，你只能做三件事情：销毁、清除or设置。
// 对应clear()成员函数，test_and_set()函数。
// clear()是一个存储操作，所以不能有memory_order_acquire or memory_order_acq_rel语义。
// 所有默认内存顺序都是memory_order_seq_cst

    f.clear(memory_order_release);
    bool x = f.test_and_set();
    atomic_flag df = x;
}

// spinlock using atomic_flag
class spinlock_mutex {
public:
    spinlock_mutex() : flag(ATOMIC_FLAG_INIT){}
    void lock() {
        while (flag.test_and_set(memory_order_acquire));
    }
    void unlock() {
        flag.clear(memory_order_release);
    }
private:
    atomic_flag flag;
};

// 最好使用 atomic<bool>， atomic_flag局限性太强了。因为它没有非修改查询操作，它甚至不能像普通的布尔标志那样使用
void test_atomic_bool() {
    atomic<bool> b(true);
    b = false;
    bool x = b.load(std::memory_order_acquire);
    b.store(true);
    x = b.exchange(false, std::memory_order_acq_rel);
}

// 5.3
vector<int> v_data;
atomic<bool> data_ready(false);

void read_thread() {
    while (!data_ready.load()) {
        this_thread::sleep_for(chrono::milliseconds(1));
    }
    cout << " result is " << v_data[0] << endl;
}

void write_thread() {
    // 对v_data的操作先于data_ready标志的修改
    v_data.push_back(22);
    data_ready.store(true);
}


#endif
