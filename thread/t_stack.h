#ifndef T_THREAD_STACK_H
#define T_THREAD_STACK_H

#include "common.h"

template<typename T>
class stack{
private:
    struct node{
        std::shared_ptr<T> data;
        node * next;
        node(T const & data_):data(new T(data_)){}
    };
    std::atomic<node*> head;
public:
    void push(T const & data){
        node * const new_node = new node(data);
        new_node->next = head.load();
        //if "head" is not equal to "new_node->next",then set "new_node->next = head" and return false, else set "head = new_node"
        //比较失败就代表head节点已经被其他的线程修改，此时借助while循环和exchange机制，重新设定new_node->next的值为新的head节点，下次loop再来的时候就会比较成功，彼时便会set "head = new_node"
        //当然比较成功的话(head == new_node->next),直接会set "head = new_node",compare_exchange_weak会返回true
        while(!head.compare_exchange_weak(new_node->next,new_node));
    }

    //1. read the current value of head
    //2. read head->next
    //3. set head to head->next
    //4. return the data from the retrieved node
    //5. delete the retrived node

    //if one thread then proceeds all the way through to step 5 before the other gets to step 2, then the second thread will be dereferencing a dangling pointer.

    //two problems: 1 it doesn't work on an empty list(if head is null)
    //            : 2 pesky leaks(the nodes needing to be deleted are still in the linklist)
//    std::shared_ptr<T> pop(){
//        node * old_head = head.load();
//        while(old_head && !head.compare_exchange_weak(old_head,old_head->next));
//        return old_head ? old_head->data : std::shared_ptr<T>();
//    }

    //pop() with no pesky leaks
    std::atomic<unsigned> threads_in_pop;
    std::atomic<node *> to_be_deleted;
    static void delete_nodes(node * nodes){
        while(nodes){
            node * next = nodes->next;
            delete nodes;
            nodes = next;
        }
    }

    std::shared_ptr<T> pop(){
        ++threads_in_pop;
        node * old_head = head.load();
        while(old_head && !head.compare_exchange_weak(old_head,old_head->next));
        std::shared_ptr<T> res;
        if(old_head){
            res.swap(old_head->data);
        }
        node * nodes_to_delete = to_be_deleted.load();
        //in high-load situations, other threads have entered pop() before all the threads initially in pop() have left.
        if(!--threads_in_pop){
            if(to_be_deleted.compare_exchange_strong(nodes_to_delete,nullptr)){
                delete_nodes(nodes_to_delete);
            }
            delete old_head;
        } else {
            //under such a scenario, it causes memory leaking again.
            //if there are many threads in pop(), then the to_be_deleted list would grow without bounds
            old_head->next = nodes_to_delete;
            while(!to_be_deleted.compare_exchange_weak(old_head->next,old_head));
        }
        return res;
    }


    //reclaim the pop() by using hazard pointers
    //the basic idea is that if a thread is going to access an object that another thread might wat to delete
    //then it first sets a hazard pointer to reference the object, thus informing the other thread that deleting the object would indeed be hazardous.

    std::shared_ptr<T> pop(){
        //std::atomic_address & hp = get_hazard_pointer_for_current_thread();
        std::atomic<void *> & hp = get_hazard_pointer_for_current_thread();
        node * old_head = head.load();
        do{
            node * temp;
            do{
                temp = old_head;
                hp.store(old_head);
                //in case other threads update head node
                old_head = head.load();
            }while(old_head != temp);
        }while(old_head && !head.compare_exchange_strong(old_head,old_head->next));
        hp.store(nullptr);
        std::shared_ptr<T> res;
        if(old_head){
            res.swap(old_head->data);
            if(outstanding_hazard_pointers_for(old_head)){
                reclaim_later(old_head);
            } else {
                delete old_head;
            }
            delete_nodes_with_no_hazards();
        }
        return res;
    }
};



#endif