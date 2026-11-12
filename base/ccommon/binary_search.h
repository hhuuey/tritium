//
// Created by huanghh on 2023/2/14.
//

#ifndef TRITUM_BINARY_SEARCH_H
#define TRITUM_BINARY_SEARCH_H

#include <vector>

//查到一个就停止, 没查到就返回-1
template<class T>
int binary_search_common(const std::vector<T> &data, const T &val)
{
    int i = 0;
    int j = data.size();
    while (i < j) { //左闭右开
        int mid = i + (j - i) / 2;
        if (data[mid] == val) return mid;
        if (data[mid] < val) i = mid + 1;
        else j = mid - 1;
    }
    return -1;
}

template<class T>
int lower_bound(const std::vector<T> &data, const T &val)
{
    int i = 0;
    int j = data.size();
    while (i < j) {
        int mid = i + (j - i) / 2;
        if (data[mid] >= val) j = mid;
        else i = mid + 1;
    }
    return i;
}

template<class T>
int upper_bound(const std::vector<int> &data, const T &val)
{
    int i = 0;
    int j = data.size();
    while (i < j) {
        int mid = i + (j - i) / 2;
        if (data[mid] > val) j = mid;
        else i = mid + 1;
    }
    return i;
}
#endif //TRITUM_BINARY_SEARCH_H
