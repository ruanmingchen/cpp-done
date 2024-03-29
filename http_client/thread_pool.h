#ifndef __THREADPOOL_H__
#define __THREADPOOL_H__

#include <condition_variable>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

#include "thread.h"

namespace net
{

class ThreadPool
{
    using Task = std::function<void()>;
    using ThreadPtr = std::shared_ptr<Thread>;
    using MutexLockGuard = std::unique_lock<std::mutex>;

public:
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    explicit ThreadPool(const std::string& name = std::string("ThreadPool"));
    ~ThreadPool();

    void setMaxQueueSize(int size) { maxQueueSize_ = size; }

    void start(int numThreads);
    void stop();
    void run(const Task& task);
    void run(Task&& task);

private:
    void runInThread();
    bool isFull() const;
    Task take();

    std::string name_;
    bool running_;
    std::vector<ThreadPtr> threads_;
    std::deque<Task> queue_;
    mutable std::mutex mutex_;
    std::condition_variable notFull_;
    std::condition_variable notEmpty_;
    size_t maxQueueSize_;
};

}  // namespace net
#endif  // __THREADPOOL_H__