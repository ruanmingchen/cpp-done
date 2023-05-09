//
// Created by root on 23-5-9.
//
#include "timer.h"

Timer::Timer(): stoped_(true),
    workguard_(boost::asio::make_work_guard(io_context_)),
    check_delayed_event_timer_(io_context_)
{}

void Timer::Start(int workerThreadCount)
{
    if (stoped_){
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

        check_delayed_event_timer_.expires_from_now(std::chrono::seconds(1));
        check_delayed_event_timer_.async_wait(
            [this](boost::system::error_code result) { CheckDelayedEventCallback(result); });

        stoped_ = false;
        startlock_.unlock();
    }
}

void Timer::Stop()
{
    if (!stoped_){
        startlock_.lock();

        stoped_ = true;
        check_delayed_event_timer_.cancel();
        io_context_.stop();

        for (auto& it : worker_threads_)
            it->join();

        worker_threads_.clear();
        cv_.notify_all();
        thread_active_timetab_.clear();

        startlock_.unlock();
    }


    // log("Asynmodule stop succ!!!");
}

void Timer::PostDelayedEvent(unsigned int delayed_second, std::function<void(void)> funcTask)
{
    if (!stoped_) {
        auto curr_time = std::chrono::steady_clock::now();
        auto timeout = curr_time + std::chrono::seconds(delayed_second);
        std::lock_guard<std::mutex> lock(delay_event_lock_);
        delay_eventtab_[timeout].push_back(funcTask);
    }
}

void Timer::ClearDelayedEvent()
{
    std::lock_guard<std::mutex> lock(delay_event_lock_);
    delay_eventtab_.clear();
}

void Timer::CheckDelayedEventCallback(boost::system::error_code result)
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