#ifndef NETLINK_PROC_MON_H
#define NETLINK_PROC_MON_H

#include <linux/cn_proc.h>

#include <condition_variable>
#include <functional>
#include <list>
#include <mutex>
#include <queue>
#include <thread>

using ProcEvent = struct proc_event;

class EventCallbackInfo
{
public:
    using ProcEventCallback = std::function<void(ProcEvent&, void*)>;

    EventCallbackInfo(ProcEventCallback callback, void* context, uint64_t id)
    {
        callback_ = callback;
        context_ = context;
        id_ = id;
    }

    ~EventCallbackInfo() = default;

public:
    uint64_t id_;

    ProcEventCallback callback_;
    void* context_;
};

class IdGenerator
{
public:
    IdGenerator() : nextId_(1) { }

    uint64_t generateId()
    {
        auto id = __sync_fetch_and_add(&nextId_, 1);
        return id;
    }

private:
    uint64_t nextId_;
};

class NetlinkProcMon
{
    using ProcEventCallback = EventCallbackInfo::ProcEventCallback;

public:
    NetlinkProcMon();
    ~NetlinkProcMon();

    int Start();
    void Stop();

    uint64_t AddProcEvCallback(ProcEventCallback eventCallback, void* context);
    void RemoveProcEvCallback(uint64_t callbackId);

private:
    int connectToProcNetlink();
    void closeProcNetlink();

    int subscribeToProcNetlink();
    int unSubscribeToProcNetlink();
    int setProcEvSubscriptionStatus(bool isEnable);

    void forEachCallback(ProcEvent& ev);

    void procEvRecvImpl();
    void procEvDispatcherImpl();

    static void procEvRecv(NetlinkProcMon* procMonThis);
    static void procEvDispatcher(NetlinkProcMon* procMonThis);

private:
    bool Started_;
    bool StopThreads_;

    IdGenerator id_generator_;
    int netlink_sock_;

    std::thread ProcEvProducerThread_;
    std::thread CallbackDispatchThread_;

    mutable std::mutex ProcEvCallbacksMutex_;
    std::list<EventCallbackInfo> ProcEv_Callbacks_;

    mutable std::mutex ProcEvQueueMutex_;
    std::queue<ProcEvent> ProcEvQueue_;
    std::condition_variable ProcEvQueueCv_;

    const unsigned int kProcEvQueueMaxSize = 8 * 1024;
};

#endif