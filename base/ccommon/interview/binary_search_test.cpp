//
// Created by huanghh on 2023/2/14.
//

#include "binary_search.h"
#include <iostream>


int main()
{
    std::vector<int> data = {1,2,2,2,2,2,3};
    std::cout << "lower_bound:" << std::endl;
    std::cout << lower_bound(data, 0) << std::endl;
    std::cout << lower_bound(data, 2) << std::endl;
    std::cout << lower_bound(data, 4) << std::endl;

    std::cout << "upper_bound:" << std::endl;
    std::cout << upper_bound(data, 0) << std::endl;
    std::cout << upper_bound(data, 2) << std::endl;
    std::cout << upper_bound(data, 4) << std::endl;
    return 0;
}
