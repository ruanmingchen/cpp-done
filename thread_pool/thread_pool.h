//
// Created by root on 23-4-3.
//

#ifndef CPP_DONE_THREADPOOL_H
#define CPP_DONE_THREADPOOL_H

#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <vector>

class Work
{
public:
    enum class WorkStatusType { kNotUsed = 0, kStarting, kInvalid };

public:
    Work(std::function<void()>&& cb);
    ~Work();

    void Doing();

    WorkStatusType work_status() const;
    void set_work_status(WorkStatusType status);

private:
    std::function<void()> cb_func_;
    WorkStatusType status_;
};

class WorkThread
{
public:
    WorkThread() = default;
    ~WorkThread();

    bool StartWork();
    void StopWork();
    void PushWork(const std::shared_ptr<Work>& ptr);

private:
    std::atomic_bool stoped_;
    std::thread worker_;

private:
    std::mutex works_mtx_;
    std::condition_variable works_cv_;
    std::queue<std::shared_ptr<Work>> works_;
};

class ThreadPool
{
public:
    ThreadPool();
    ~ThreadPool();

    void StartPool(std::size_t num = std::thread::hardware_concurrency());
    void StopPool();

    template <typename F, class... Args>
    auto PostWork(F&& f, Args&&... args) -> std::tuple<std::size_t, std::future<decltype(F(args...))>>;

    void DeleteWork(std::size_t id);

private:
    void MakeWorkThread(std::size_t num);
    std::size_t Rand(std::size_t min = 1UL, std::size_t max = 999999UL);

private:
    uint32_t offset_;
    uint32_t threads_size_;

    bool stop_;

    std::mutex workers_mtx_;
    std::vector<std::shared_ptr<WorkThread>> workers_;

    std::mutex works_mtx_;
    std::unordered_map<std::size_t, std::shared_ptr<Work>> name_works_;
};

#endif  // CPP_DONE_THREADPOOL_H
