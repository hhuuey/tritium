//
// Created by huang on 2023/2/6.
//

#ifndef TRITUM_PERMUTATION_H
#define TRITUM_PERMUTATION_H

#include <iterator>
#include <algorithm>

namespace tritium {
//// STL实现
//template<class BidirIt>
//bool next_permutation(BidirIt first, BidirIt last)
//{
//    auto r_first = std::make_reverse_iterator(last);
//    auto r_last = std::make_reverse_iterator(first);
//    auto left = std::is_sorted_until(r_first, r_last);
//    if(left != r_last){
//        auto right = std::upper_bound(r_first, left, *left);
//        std::iter_swap(left, right);
//    }
//    std::reverse(left.base(), last);
//    return left != r_last;
//}

template <class T>
bool next_permutation(const T &it_begin, const T &it_end) // const &
{
    if (std::distance(it_begin, it_end) < 2) return false;

    auto it1 = std::prev(std::prev(it_end)); // 左闭右开
    do {
        if (*it1 < *std::next(it1)) {
            auto mid = std::prev(it_end);
            while (true) {
                if (*mid > *it1) break;
                --mid;
            }
            std::iter_swap(it1, mid);
            std::reverse(std::next(it1), it_end);
            return true;
        }
        if (it1 == it_begin) { //当前it1还是最大 且已经指向第一个迭代器
            std::reverse(it_begin, it_end);
            return false;
        }
        --it1;
    } while (true);
}

template<class T>
bool prev_permutation(const T &it_begin, const T &it_end)
{
    if (std::distance(it_begin, it_end) < 2) return false;
    auto it1 = std::prev(std::prev(it_end));
    do {
        if (*it1 > *std::next(it1)) {
            auto mid = std::prev(it_end);
            while (true) {
                if (*mid < *it1) break;
                --mid;
            }
            std::iter_swap(it1, mid);
            std::reverse(std::next(it1), it_end);
            return true;
        }

        if (it1 == it_begin) {
            std::reverse(it_begin, it_end);
            return false;
        }
        --it1;
    } while(true);
}
}


#endif //TRITUM_PERMUTATION_H