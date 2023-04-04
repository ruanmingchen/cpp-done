//
// Created by root on 23-4-3.
//

#include "ThreadPool.h"

ThreadPool::ThreadPool(std::size_t num):stop_(false)
{
    for(size_t i = 0;i<num;++i){
        workers_.emplace_back(
                [this]
                {
                    for(;;)
                    {
                        std::function<void()> task;

                        {
                            std::unique_lock<std::mutex> lock(this->mutex_);
                            this->cond_.wait(lock,
                                                 [this]{ return this->stop_ || !this->tasks_.empty(); });
                            if(this->stop_.load() && this->tasks_.empty())
                                return;
                            task = std::move(this->tasks_.front());
                            this->tasks_.pop();
                        }

                        task();
                    }
                }
        );
    }
}

ThreadPool::~ThreadPool()
{
    stop_.store(true);
    cond_.notify_all();
    for(auto & worker: workers_){
        worker.join();
    }

}

template<typename F, class... Args>
auto ThreadPool::enqueue(F && f, Args &&... args)->std::future<typename std::result_of<F(Args...)>::type>
{
    using return_type = typename std::result_of<F(Args...)>::type;
    auto task = std::make_shared< std::packaged_task<return_type()> >(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );

    std::future<return_type> res = task->get_future();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        tasks_.emplace([task](){ (*task)(); });
    }
    cond_.notify_one();
    return res;
}

