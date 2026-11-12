#ifndef TRITUM_M_SORT_H
#define TRITUM_M_SORT_H

#include <algorithm>
#include <iterator>
#include <vector>

namespace tritium {

// 插入排序
template <class T>
void m_insert_sort(const T &it_begin, const T &it_end) {
    for (auto i = it_begin + 1; i != it_end; ++i) {
        std::rotate(std::upper_bound(it_begin, i, *i), i, i + 1);
    }
}


//冒泡排序
template <class T>
void m_bubble_sort(const T &it_begin, const T &it_end) {
    size_t length = std::distance(it_begin, it_end);
    size_t last_order_index = length; // 此index及之后的元素是有序的
    for (int i = 0; i < length - 1; ++i) {
        size_t last_change_index = 0; // 记录本轮最后一次交换的后者index, 该index及之后的元素是有序的
        for (int j = 0; j < last_order_index - 1; ++j) {
            if (*(it_begin + j) > *(it_begin + j + 1)) {
                std::iter_swap(it_begin + j, it_begin + j + 1);
                last_change_index = j + 1; // 左右交换后，只能保证右边的元素比较大
            }
        }
        if (last_change_index <= 1) { // 0:没有交换 1:只交换了第一第二两个元素
            break;
        }
        last_order_index = last_change_index;
    }
}


//快速排序
template <class T>
void m_q_sort(const T& it_begin, const T& it_end) {
    size_t length = std::distance(it_begin, it_end);
    if (length < 2) return;  // 0个或1个元素
    decltype(*it_begin) pivot = *it_begin; // 比较的基准元素
    T pos = it_begin; // 最后一个小于pivot的位置
    for (T it = it_begin + 1; it != it_end; ++it) {
        if (*(it) < pivot) std::iter_swap(it, ++pos);
    }
    std::iter_swap(it_begin, pos);
    m_q_sort(it_begin, pos);
    m_q_sort(std::next(pos), it_end);
}


// 归并排序
template <class T>
void m_merge_sort(const T& it_begin, const T& it_end) {
    size_t length = std::distance(it_begin, it_end);
    if (length <= 1) return;
    size_t mid_pos = length / 2;
    m_merge_sort(it_begin, it_begin + mid_pos);
    m_merge_sort(it_begin + mid_pos, it_end);
    std::vector<typename std::iterator_traits<T>::value_type> res;

    size_t i = 0;
    size_t j = mid_pos;
    while (i < mid_pos && j < length) {
        if (*(it_begin + i) <  *(it_begin + j))
            res.emplace_back(*(it_begin + i++));
        else
            res.emplace_back(*(it_begin + j++));
    }
    while (i < mid_pos) res.emplace_back(*(it_begin + i++));
    while (j < length) res.emplace_back(*(it_begin + j++));

    std::copy(res.begin(), res.end(), it_begin);
}

}

#endif //TRITUM_M_SORT_H
