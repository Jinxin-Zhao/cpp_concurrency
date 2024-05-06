#ifndef _SEMAPHORE_H__
#define _SEMAPHORE_H__

#include "common.h"


class TestSemaphore {
    protected:
        sem_t first_smt;
        sem_t second_smt;
    public:
        TestSemaphore() {
            sem_init(&first_smt,0,0);
            sem_init(&second_smt,0,0);
        }

        void first(std::function<void ()> printFirst) {
            printFirst();
            sem_post(&first_smt);
        }

        void second(std::function<void ()> printSecond) {
            sem_wait(&first_smt);
            printSecond();
            sem_post(&second_smt);
        }

        void third(std::function<void ()> printThird) {
            sem_wait(&second_smt);
            printThird();
        }
};



#endif