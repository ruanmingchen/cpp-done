#ifndef __THREAD_POOL_H__
#define __THREAD_POOL_H__

//
// Created by root on 23-4-3.
//

#include "thread_pool.h"

Work::Work(std::function<void()>&& cb) : cb_func_(std::move(cb)), status_(WorkStatusType::kNotUsed)
{
}

Work::~Work()
{
}

void Work::Doing()
{
    if (nullptr != cb_func_) {
        status_ = WorkStatusType::kStarting;
        cb_func_();
        status_ = WorkStatusType::kInvalid;
    }
}

Work::WorkStatusType Work::work_status() const
{
    return status_;
}

void Work::set_work_status(Work::WorkStatusType status)
{
    status_ = status;
}

WorkThread::~WorkThread()
{
    StopWork();
}

bool WorkThread::StartWork()
{
    try {
        stoped_.store(false);
        worker_ = std::thread([this]() {
            while (true) {
                std::shared_ptr<Work> work;
                {
                    std::unique_lock<std::mutex> lock{works_mtx_};
                    works_cv_.wait(lock, [this] { return stoped_.load() || !works_.empty(); });
                    if (stoped_.load()) {
                        return;
                    }
                    work = std::move(works_.front());
                    works_.pop();
                }

                if (nullptr != work && Work::WorkStatusType::kInvalid != work->work_status()) {
                    work->Doing();
                }
            }
        });
    } catch (const std::exception& e) {
    } catch (...) {
    }
}

void WorkThread::StopWork()
{
    try {
        stoped_.store(true);
        works_cv_.notify_one();
        if (worker_.joinable()) {
            worker_.join();
        }
    } catch (const std::exception& e) {
    } catch (...) {
    }
}

void WorkThread::PushWork(const std::shared_ptr<Work>& ptr)
{
    if (!stoped_.load()) {
        std::lock_guard<std::mutex> lock{works_mtx_};
        works_.push(ptr);
    }
}

ThreadPool::ThreadPool() : offset_(0), threads_size_(0), stop_(true)
{
}

ThreadPool::~ThreadPool()
{
    StopPool();
}

void ThreadPool::StartPool(std::size_t num)
{
    threads_size_ = num;
    MakeWorkThread(num);
}

void ThreadPool::StopPool()
{
    stop_ = true;
    for (const auto& worker : workers_) {
        worker->StopWork();
    }
}

template <typename F, class... Args>
auto ThreadPool::PostWork(F&& f, Args&&... args) -> std::tuple<std::size_t, std::future<decltype(F(args...))>>
{
    if (stop_) {
        return;
    }

    using return_type = typename std::result_of<F(Args...)>::type;
    auto task =
        std::make_shared<std::packaged_task<return_type()>>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));

    auto func_ret = task->get_future();
    std::size_t work_id = Rand();
    auto ret = std::make_pair(work_id, func_ret);

    if (offset_ > threads_size_) {
        offset_ = 0;
    }

    auto work = std::make_shared<Work>([task]() { (*task)(); });

    {
        std::lock_guard<std::mutex> lock{works_mtx_};
        name_works_.emplace(work_id, work);
    }

    workers_[offset_]->PushWork(work);

    offset_++;

    return ret;
}

void ThreadPool::DeleteWork(std::size_t id)
{
    std::lock_guard<std::mutex> lock{works_mtx_};
    auto iter = name_works_.find(id);
    if (iter != name_works_.end()) {
        iter->second->set_work_status(Work::WorkStatusType::kInvalid);
    }
}

void ThreadPool::MakeWorkThread(std::size_t num)
{
    try {
        auto max_threads = std::thread::hardware_concurrency();
        if (threads_size_ >= max_threads) {
            return;
        }

        auto best_threads = max_threads - threads_size_;
        for (size_t i = num; i <= best_threads; i++) {
            auto work_thread = std::make_shared<WorkThread>();
            work_thread->StartWork();
            workers_.emplace_back(std::move(work_thread));
        }
    } catch (const std::exception& e) {
    } catch (...) {
    }
}

std::size_t ThreadPool::Rand(std::size_t min, std::size_t max)
{
    static int s_seed = 0;

    if (s_seed == 0) {
        s_seed = time(NULL);
        srand(s_seed);
    }

    int rand_num = rand();
    rand_num = min + (int)((double)((double)(max) - (min) + 1.0) * ((rand_num) / ((RAND_MAX) + 1.0)));
    return rand_num;
}

#endif  // __THREAD_POOL_H__