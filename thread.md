# # chapter 3 sharing data betweening threads
### 3 ways to deal with problematic race conditions:
    - the simplest option is to wrap your data structure with a protection mechanism,to ensure that only the thread actually performing a modification can see the intermediate states where the invariants are broken;
    - another option is to modify the design of your data structure and its invariants so that modifications are done as series if indivisible changes,each of which preserves the invariants.This is generally referred to as lock-free programming;
    - another way of dealing with race conditions is to handle the updates to the data structure as a "transaction",just as updates to a database are done within a transaction.The required series of data modifications and reads is stored in a transaction log,and then committed in a single step.
      If the commit cannot proceed because the data structure has been modified by another thread,then the transaction is restarted. This is termed Software Transactional Memory. There's no direct support for STM in C++.

### further guidelines for avoiding deadlock
    + avoid nested locks;
    + avoid calling user-supplied code whilst holding a lock;
    + acquire locks in a fixed order;
    + use a lock hierarchy;

### std::unique_lock & std::lock_guard
    + std::unique_lock is more flexible than std::lock_guard,but there is a slight performance penalty when using std::unique_lock;
    + std::unique_lock instances do not have to own their associated mutexes, the ownership of a mutex can be transfered between instances by moving the instances around;
        - one possible use is to allow a function to lock a mutex and transfer ownership of that lock to the caller,

### don't do any really time consuming activities like file I/O whilst holding a lock;

### condition_variable & condition_variable_any
+ In both cases,they need to work with a mutex in order to provide appropriate synchronization:
    - the former is limited to working with the std::mutex;
    - whereas the latter can work with anything that meets some minimal criteria for being mutex-like;

###condition variable & future(one-off events)
+ if the waiting thread is only ever going to wait at once, when the condition is true it will never wait on this condition variable again, a condition variable might not be
the best choice of synchronization mechanism.
+ future: std::shared_future<>  && std::unique_future<>
    - std::unique_future should be your first choice wherever it is appropriate:
    std::shared_future only allows you to copy the data associated with the future;
    std::unique_future allows you to move the data out(using std::move). This can be important where the data is potentially expensive to copy, but cheap to move(such as a std::vector with a million elements),
    and is vital where the data cannot be copied,only moved(such as std::thread, or std::ifstream)

###packaged_task<> & std::promise
+ std::packaged_task cannot be copied, it ties a future to the result of a function call: when the function completes,the future is ready, and the associated data is the value returned by the function;