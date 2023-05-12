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

using namespace std;

class TimerTaskQueue
{
    using DelayedEventTab = std::map<std::chrono::steady_clock::time_point, std::list<std::function<void(void)>>>;
    using Executor = boost::asio::io_service::executor_type;

    struct thread_info {
        long tid{};
        std::chrono::steady_clock::time_point time;
    };

public:
    TimerTaskQueue();
    ~TimerTaskQueue() = default;

    void Start(int thread_num = 1);
    void Stop();

    template <class FunctionT>
    void PostEvent(FunctionT funcTask)
    {
        if (!stoped_) {
            boost::asio::post(io_context_, funcTask);
        }
    }

    void PostDelayedEvent(unsigned int delayed_second, const std::function<void(void)>& funcTask);

    void ClearDelayedEvent();

private:
    void CheckDelayedEventCallback(boost::system::error_code result);

private:
    const int kSeconds = 60;

    std::mutex startlock;
    bool stoped_;

    std::mutex thread_active_tablock_;
    std::map<std::thread::id, thread_info> thread_active_timetab_;

    std::vector<std::shared_ptr<std::thread>> worker_threads_;
    boost::asio::io_context io_context_;

    boost::asio::executor_work_guard<Executor> workguard_;

    std::mutex delay_event_lock_;
    DelayedEventTab delay_eventtab_;

    boost::asio::steady_timer check_delayed_event_timer_;
};

#endif
