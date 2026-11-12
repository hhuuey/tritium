#include "m_sort.h"
#include "m_time.h"

#include <iostream>
#include <iterator>
#include <vector>

int main()
{
    tritium::MTime mt("SORT");
    std::vector<int> data = {3, 1, 4, 5, 7,  6,  -1,  -4, -7, 2, 4, 5, 6, 7, 8, 999, 12, 345};
    mt.Count("m_data_prepare");

    std::vector<int> data0 = data;
    tritium::m_insert_sort(data0.begin(), data0.end());
    std::copy(data0.begin(), data0.end(), std::ostream_iterator<int>(std::cout, " "));
    std::cout << std::endl;
    mt.Count("m_insert_sort");

    std::vector<int> data1 = data;
    tritium::m_bubble_sort(data1.begin(), data1.end());
    std::copy(data1.begin(), data1.end(), std::ostream_iterator<int>(std::cout, " "));
    std::cout << std::endl;
    mt.Count("m_bubble_sort");


    std::vector<int> data2 = data;
    tritium::m_q_sort(data2.begin(), data2.end());
    std::copy(data2.begin(), data2.end(), std::ostream_iterator<int>(std::cout, " "));
    std::cout << std::endl;
    mt.Count("m_q_sort");

    std::vector<int> data3 = data;
    tritium::m_merge_sort(data3.begin(), data3.end());
    std::copy(data3.begin(), data3.end(), std::ostream_iterator<int>(std::cout, " "));
    std::cout << std::endl;
    mt.Count("m_merge_sort");
    return 0;
}