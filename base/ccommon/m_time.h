#ifndef TRITUM_M_TIME_H
#define TRITUM_M_TIME_H

#include <chrono>
#include <string>
#include <list>

namespace tritium {

class MTime {
public:
    MTime (const std::string &module_name) : module_name_(module_name) {
        start_time_ = std::chrono::steady_clock::now();
    }

    void Start() {
        start_time_ = std::chrono::steady_clock::now();
    }

    void Count(const std::string &submodule) {
        middle_record_.emplace_back(std::make_pair(submodule, std::chrono::steady_clock::now()));
    }

    ~MTime () {
        auto dt = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start_time_).count();
        printf("MODULE:[%s], takes [%.2f] ms \n", module_name_.c_str(), 1.0 * dt / 1e3);
        auto last_tp = start_time_;
        for (const auto &middle : middle_record_) {
            auto dt_sub_begin = std::chrono::duration_cast<std::chrono::microseconds>(middle.second - start_time_).count();
            auto dt_sub_last = std::chrono::duration_cast<std::chrono::microseconds>(middle.second - last_tp).count();
            printf("-- submodule:[%s], takes [%.2f] ms from last count, takes [%.2f] ms from begining \n",
                   middle.first.c_str(), 1.0 * dt_sub_last / 1e3, 1.0 * dt_sub_begin / 1e3);
            last_tp = middle.second;
        }
    }

private:
    std::string module_name_;
    std::chrono::steady_clock::time_point start_time_;
    std::list<std::pair<std::string, std::chrono::steady_clock::time_point>> middle_record_;
};
}
#endif //TRITUM_M_TIME_H
