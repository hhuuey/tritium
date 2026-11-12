#include <iostream>
#include <chrono>
#include <cstdlib>

using namespace std;

bool compute2time(const std::string &threadid, size_t &begin, const size_t end, const size_t &dur)
{
    auto ts1 = std::chrono::high_resolution_clock::now();
    for (size_t i = begin; i < end; i += 2) {
        // 时间片段到了，记录直行情况后退出
        if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - ts1).count() > dur) {
            begin = i;
            return false;
        }
        std::cout << threadid << "(" << dur << "us) : " << i << std::endl;
    }
    std::cout << threadid << " finished!" << std::endl;
    return true;
}

int main()
{
    bool thread1_stop = false;
    bool thread2_stop = false;

    size_t thread1_index = 0;
    size_t thread2_index = 1;
    size_t end_index = 100;

    size_t epoch = 10; // 每一轮多线程计算的总时间10微秒
    srand((unsigned)time(0));

    while (true) {
        size_t epoch1, epoch2;
        if (thread1_stop && thread2_stop) {
            break;
        }

        if (thread1_stop) {
            thread2_stop = compute2time("thread2", thread2_index, end_index, epoch);
        } else if (thread2_stop) {
            thread1_stop = compute2time("thread1", thread1_index, end_index, epoch);
        } else {
            epoch1 = rand() % epoch;
            epoch2 = epoch - epoch1;
            thread1_stop = compute2time("thread1", thread1_index, end_index, epoch1);
            thread2_stop = compute2time("thread2", thread2_index, end_index, epoch2);
        }
    }
    return 0;
}
