#ifndef M_THREADPOOL_H
#define M_THREADPOOL_H

#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>

namespace tritium {
class ThreadPool {
public:
    ThreadPool(size_t);

    template<class F, class... Args>
    auto enqueue(F &&f, Args &&... args) -> std::future<typename std::result_of<F(Args...)>::type>;

    ~ThreadPool();

private:
    // need to keep track of threads so we can join them
    std::vector<std::thread> workers_;
    // the task queue
    std::queue<std::function<void()>> tasks_;

    // synchronization
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stop_;
};

// the constructor just launches some amount of workers_
inline ThreadPool::ThreadPool(size_t threads) : stop_(false) {
    for (size_t i = 0; i < threads; ++i)
        workers_.emplace_back( [this] {
                    for (;;) {
                        std::function <void()> task;
                        {
                            std::unique_lock<std::mutex> lock(this->mutex_);
                            this->cv_.wait(lock, [this] { return this->stop_ || !this->tasks_.empty(); });
                            if (this->stop_ && this->tasks_.empty()) return;
                            task = std::move(this->tasks_.front());
                            this->tasks_.pop();
                        }
                        task();
                    }
                }
        );
}

// add new work item to the pool
template<class F, class... Args>
auto ThreadPool::enqueue(F &&f, Args &&... args) -> std::future<typename std::result_of<F(Args...)>::type> {
    using return_type = typename std::result_of<F(Args...)>::type;

    auto task = std::make_shared<std::packaged_task<return_type()>>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));

    std::future<return_type> res = task->get_future();
    {
        std::unique_lock<std::mutex> lock(mutex_);

        // don't allow enqueueing after stop_ping the pool
        if (stop_)
            throw std::runtime_error("enqueue on stop_ped ThreadPool");

        tasks_.emplace([task]() { (*task)(); });
    }
    cv_.notify_one();
    return res;
}

// the destructor joins all threads
inline ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(mutex_);
        stop_ = true;
    }
    cv_.notify_all();
    for (std::thread &worker: workers_)
        worker.join();
}
}

#endif //M_THREADPOOL_H
