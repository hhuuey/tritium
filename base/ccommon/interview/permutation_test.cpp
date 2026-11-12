//
// Created by huang on 2023/2/6.
//

#include "permutation.h"

#include <iostream>
#include <string>

int main(int argc, char *argv[])
{
    std::string test = "1342";
    tritium::next_permutation(test.begin(), test.end());
    std::cout << test << std::endl;
    test = "1423";
    tritium::prev_permutation(test.begin(), test.end());
    std::cout << test << std::endl;
    return 0;
}