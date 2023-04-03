//
// Created by root on 23-4-3.
//

#ifndef CPP_DONE_THREADPOOL_H
#define CPP_DONE_THREADPOOL_H

#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>
#include <string>

class ThreadPool {
public:
    explicit ThreadPool(std::size_t num);
    ~ThreadPool();

    template<typename F, class... Args>
    auto enqueue(F && f, Args &&... args)->std::future<typename std::result_of<F(Args...)>::type>;

private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable cond_;
    std::atomic_bool stop_{};
};


#endif //CPP_DONE_THREADPOOL_H
