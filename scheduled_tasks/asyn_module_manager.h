#ifndef ASYN_MODULE_MANAGER_HPP
#define ASYN_MODULE_MANAGER_HPP

#include <sys/syscall.h>
#include <unistd.h>

#include <boost/asio.hpp>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <list>
#include <mutex>
#include <thread>
#include <vector>

#include "cpu_load.h"

using namespace std;
#define THREAD_TIMEOUT_TIME     (60 * 30)  // 30min
#define WATCHDOG_CHECK_INTERVAL (60)
#define SYS_CPULOAD_THRESHOLD   80

class AsynModuleManager
{
    using DelayedEventTab = std::map<std::chrono::steady_clock::time_point, std::list<std::function<void(void)>>>;
    using ThreadSptr = std::shared_ptr<std::thread>;
    using Executor_Type = boost::asio::io_service::executor_type;

    struct thread_info {
        long tid;
        std::chrono::steady_clock::time_point time;
    };

public:
    /**
     * @brief 启动异步模块管理器
     *
     * @param workerThreadCount 工作线程数量
     * @return true 启动成功
     * @return false 启动失败
     */
    bool Start(int workerThreadCount)
    {
        if (!stoped_)
            return true;

        startlock_.lock();

        if (io_context_.stopped())
            io_context_.restart();

        if (workerThreadCount == 0) {
            workerThreadCount = sysconf(_SC_NPROCESSORS_ONLN) * 2 + 2;
        }

        for (int i = 0; i < workerThreadCount; ++i) {
            worker_threads_.emplace_back(std::make_shared<std::thread>([this]() {
                thread_active_tablock_.lock();
                thread_active_timetab_[std::this_thread::get_id()].tid = syscall(__NR_gettid);
                thread_active_timetab_[std::this_thread::get_id()].time = std::chrono::steady_clock::now();
                thread_active_tablock_.unlock();

                while (!io_context_.stopped()) {
                    io_context_.run_for(std::chrono::seconds(WATCHDOG_CHECK_INTERVAL));
                    thread_active_timetab_[std::this_thread::get_id()].time = std::chrono::steady_clock::now();
                }
            }));
        }

        watch_dog_thread_ = std::make_shared<std::thread>([this]() {
            std::vector<long> blocked_threads;
            while (!io_context_.stopped()) {
                std::unique_lock<std::mutex> lock(cv_lock_);
                cv_.wait_for(lock, std::chrono::seconds(WATCHDOG_CHECK_INTERVAL));
                auto now_time = std::chrono::steady_clock::now();
                /* 一个检测周期内，系统cpu利用率大于80时，放弃本次死锁检测 */
                if (SystemCpuLoad() > SYS_CPULOAD_THRESHOLD) {
                    thread_active_tablock_.lock();
                    for (auto it : thread_active_timetab_) {
                        it.second.time = now_time;
                    }
                    thread_active_tablock_.unlock();
                    continue;
                }

                for (auto it : thread_active_timetab_) {
                    auto active_time = it.second.time;
                    if (now_time - active_time > std::chrono::seconds(THREAD_TIMEOUT_TIME)) {
                        blocked_threads.push_back(it.second.tid);
                        std::ostringstream oss;
                        oss << it.first;
                        // log("asyn module work thread block. thread_id:%s. tid:%ld", oss.str().c_str(),
                        // it.second.tid);
                    }
                }

                if (!blocked_threads.empty())
                    handleOverdue(blocked_threads);
                blocked_threads.clear();
            }
        });

        check_delayed_event_timer_.expires_from_now(std::chrono::seconds(1));
        check_delayed_event_timer_.async_wait(
            [this](boost::system::error_code result) { CheckDelayedEventCallback(result); });

        stoped_ = false;
        startlock_.unlock();
        return true;
    }
    /**
     * @brief 停止异步模块管理器
     *
     */
    void Stop()
    {
        if (stoped_)
            return;
        startlock_.lock();

        stoped_ = true;
        check_delayed_event_timer_.cancel();
        io_context_.stop();

        for (auto& it : worker_threads_)
            it->join();

        worker_threads_.clear();
        cv_.notify_all();
        watch_dog_thread_->join();
        thread_active_timetab_.clear();

        startlock_.unlock();
        // log("Asynmodule stop succ!!!");
    }
    /**
     * @brief 获取io_context
     *
     * @return boost::asio::io_context&
     */
    boost::asio::io_context& get_context() { return io_context_; }
    /**
     * @brief 发送事件
     *
     * @tparam FunctionT
     * @param funcTask
     */
    template <class FunctionT>
    void PostEvent(FunctionT funcTask)
    {
        if (!stoped_)
            boost::asio::post(io_context_, funcTask);
    }
    /**
     * @brief 发送延迟事件
     *
     * @param delayed_second 延迟时间
     * @param funcTask
     */
    void PostDelayedEvent(unsigned int delayed_second, std::function<void(void)> funcTask)
    {
        if (!stoped_) {
            auto curr_time = std::chrono::steady_clock::now();
            auto timeout = curr_time + std::chrono::seconds(delayed_second);
            std::lock_guard<std::mutex> lock(delay_event_lock_);
            delay_eventtab_[timeout].push_back(funcTask);
        }
    }
    /**
     * @brief 清空延迟事件
     *
     */
    void ClearDelayedEvent()
    {
        std::lock_guard<std::mutex> lock(delay_event_lock_);
        delay_eventtab_.clear();
    }
    /**
     * @brief 构造函数
     *
     */
    AsynModuleManager() : workguard_(boost::asio::make_work_guard(io_context_)), check_delayed_event_timer_(io_context_)
    {
        stoped_ = true;
    }

private:
    /**
     * @brief 检查延迟事件回调函数
     *
     * @param result
     */
    void CheckDelayedEventCallback(boost::system::error_code result)
    {
        if (result) {
            std::lock_guard<std::mutex> lock(delay_event_lock_);
            delay_eventtab_.clear();
            // log("check delayed event timer be cancel");
            return;
        }

        {
            auto curr_time = std::chrono::steady_clock::now();
            std::lock_guard<std::mutex> lock(delay_event_lock_);
            for (auto it = delay_eventtab_.begin(); it != delay_eventtab_.end();) {
                if (curr_time >= it->first) {
                    for (auto event_it : it->second)
                        PostEvent(event_it);
                    delay_eventtab_.erase(it++);
                } else {
                    break;
                }
            }
        }

        check_delayed_event_timer_.expires_from_now(std::chrono::seconds(1));
        check_delayed_event_timer_.async_wait(
            [this](boost::system::error_code result) { CheckDelayedEventCallback(result); });
    }
    /**
     * @brief 获取系统cpu利用率
     *
     * @return float
     */
    float SystemCpuLoad()
    {
        float cpu_load;

        static struct cpu_stat_info last_cpu_stat;
        cpu_load = getSystemCpuLoad(&last_cpu_stat);
        // log("SystemCpuLoad:%f", cpu_load);
        return cpu_load;
    }

    /**
     * @brief 处理超时线程
     *
     * @param block_threads
     */
    void handleOverdue(__attribute__((__unused__)) const std::vector<long>& block_threads) { sleep(3); }

private:
    std::mutex startlock_;
    bool stoped_;

    std::mutex cv_lock_;
    std::condition_variable cv_;
    ThreadSptr watch_dog_thread_;

    std::mutex thread_active_tablock_;
    std::map<std::thread::id, thread_info> thread_active_timetab_;

    std::vector<ThreadSptr> worker_threads_;
    boost::asio::io_context io_context_;

    boost::asio::executor_work_guard<Executor_Type> workguard_;

    std::mutex delay_event_lock_;
    DelayedEventTab delay_eventtab_;

    boost::asio::steady_timer check_delayed_event_timer_;
};

#endif
