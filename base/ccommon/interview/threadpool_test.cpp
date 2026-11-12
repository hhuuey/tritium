#include "base/m_threadpool.h"

#include <iostream>
#include <chrono>

int main()
{

    tritium::ThreadPool pool(4);
//    std::vector<std::future<int>> results;
    std::vector<std::future<void>> results;

    for(int i = 0; i < 8; ++i) {
        results.emplace_back(
                pool.enqueue([i] {
                    std::cout << "hello " << i << std::endl;
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    std::cout << "world " << i << std::endl;
//                    return i*i;
                })
        );
    }

//    for(auto && result: results) result.get();

    return 0;
}