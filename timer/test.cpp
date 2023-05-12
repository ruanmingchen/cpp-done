//
// Created by root on 23-5-9.
//

#include <unistd.h>

#include <iostream>

#include "timer.h"

int main()
{
    TimerTaskQueue timer_task_queue;
    timer_task_queue.Start(1);
    timer_task_queue.PostDelayedEvent(5, [&timer_task_queue]() { std::cout << 1 << std::endl; });
    timer_task_queue.ClearDelayedEvent();
    timer_task_queue.Stop();
}