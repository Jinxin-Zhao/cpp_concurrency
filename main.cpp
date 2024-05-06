#include "common.h"
#include "chapter_5.h"
#include "chapter_6.h"
#include "chapter_8.h"
#include "chapter_9.h"

int main() {

    vector<int> vec(100, 1);
    int init = 0;
    auto result = parallel_accumulate(vec.begin(), vec.end(), init);
    cout << "value is " << result << endl;

    list<int> start{1, 2, 3, 4, 5, 6, 7, 8, 9};
    typename list<int>::iterator divide_point = partition(start.begin(), start.end(), [&](int const & val){
        return val < 5;
    });
    list<int> n_s;
    n_s.splice(n_s.begin(),start, start.begin(), divide_point);
//    cout << "n_s size " << n_s.size() << endl;
//    for (auto elem : n_s) {
//        cout << elem << " ";
//    }
    list<int> new_higher{0,4,2,4,9,2};
    n_s.splice(n_s.end(), new_higher);

    list<int> tmp{9,9,9,9};
    n_s.splice(n_s.begin(), tmp, tmp.begin());
    for (auto elem : n_s) {
        cout << elem << " ";
    }
    return 0;
    //
    test_atomic_flag();
    std::list<int> list;
    list.push_back(2);
    list.push_back(22);
    list.push_back(25);
    list.push_back(21);
    list.push_back(8);
    list.push_back(9);
    list.push_back(7);
    parallel_quick_sort<int>(list);
    std::cout << "first elem " << *list.begin() << std::endl;
    return 0;
}
