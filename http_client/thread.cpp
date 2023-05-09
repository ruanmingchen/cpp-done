#include "thread.h"

#include <stdio.h>

using namespace net;

std::atomic_int Thread::numCreated_;

Thread::Thread(const ThreadFunc& func, const std::string& name) :
    func_(func),
    name_(name),
    started_(false),
    joined_(false)
{
}

Thread::Thread(ThreadFunc&& func, const std::string& name) :
    func_(std::move(func)),
    name_(name),
    started_(false),
    joined_(false)
{
}

Thread::~Thread()
{
    if (started_ && !joined_) {
        pThread_->detach();
    }
}

void Thread::start()
{
    started_ = true;
    pThread_ = std::make_shared<std::thread>(func_);
}

void Thread::join()
{
    joined_ = true;
    pThread_->join();
}