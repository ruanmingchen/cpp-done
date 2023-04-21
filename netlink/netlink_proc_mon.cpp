#include "netlink_proc_mon.h"

#include <fcntl.h>
#include <linux/connector.h>
#include <linux/netlink.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <iostream>

#define MAX_RECV_BUF_SIZE 20 * 1024 * 1024
#define INVALID_SOCKET    -1

NetlinkProcMon::NetlinkProcMon()
{
    netlink_sock_ = INVALID_SOCKET;
    StopThreads_ = false;
    Started_ = false;
}

NetlinkProcMon::~NetlinkProcMon()
{
    Stop();
}

static inline ssize_t SaveRmemSize(const char* file)
{
    int fd;
    ssize_t ret;
    char buf[1204] = {0};

    fd = open(file, O_RDONLY);

    if (fd < 0) {
        return -1;
    }

    ret = read(fd, buf, sizeof(buf) - 1);
    if (ret > 0) {
        buf[ret] = '\0';
        ret = atol(buf);
    }
    close(fd);

    return ret;
}

int NetlinkProcMon::Start()
{
    if (Started_) {
        return 0;
    }

    if (connectToProcNetlink() != 0) {
        return -1;
    }

    if (subscribeToProcNetlink() != 0) {
        closeProcNetlink();
        return -1;
    }

    try {
        ProcEvProducerThread_ = std::thread(procEvRecv, this);
        CallbackDispatchThread_ = std::thread(procEvDispatcher, this);
    } catch (...) {
    }

    Started_ = true;

    return 0;
}

void NetlinkProcMon::Stop()
{
    if (Started_) {
        StopThreads_ = true;

        unSubscribeToProcNetlink();

        try {
            if (ProcEvProducerThread_.joinable()) {
                ProcEvProducerThread_.join();
            }

            {
                std::lock_guard<std::mutex> lk(ProcEvQueueMutex_);
                ProcEvQueueCv_.notify_all();
            }

            if (CallbackDispatchThread_.joinable()) {
                CallbackDispatchThread_.join();
            }
        } catch (...) {
        }

        closeProcNetlink();

        StopThreads_ = false;
        Started_ = false;
    }
}

int NetlinkProcMon::connectToProcNetlink()
{
    if (netlink_sock_ != INVALID_SOCKET) {
        return 0;
    }

    netlink_sock_ = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_CONNECTOR);
    if (netlink_sock_ == INVALID_SOCKET) {
        return -1;
    }

    struct sockaddr_nl sa_nl;

    sa_nl.nl_family = AF_NETLINK;
    sa_nl.nl_groups = CN_IDX_PROC;
    sa_nl.nl_pid = getpid();

    auto r = bind(netlink_sock_, (struct sockaddr*)&sa_nl, sizeof(sa_nl));
    if (r < 0) {
        close(netlink_sock_);
        netlink_sock_ = INVALID_SOCKET;
    }

    int rcv_size = 0;
    socklen_t optlen = sizeof(rcv_size);

    if (getsockopt(netlink_sock_, SOL_SOCKET, SO_RCVBUF, &rcv_size, &optlen) == 0) {
        if (rcv_size < MAX_RECV_BUF_SIZE) {
            rcv_size = MAX_RECV_BUF_SIZE;
            if (0 != setsockopt(netlink_sock_, SOL_SOCKET, SO_RCVBUFFORCE, (const char*)&rcv_size, optlen)) { }
            // if(getsockopt(netlink_sock_, SOL_SOCKET, SO_RCVBUF, &rcv_size, &optlen) == 0) {
            //     std::cout << "getsockopt " << rcv_size << std::endl;
            // }
        }
    }

    struct timeval tv_out;
    tv_out.tv_sec = 0;
    tv_out.tv_usec = 100000;  //  = 0.1s

    setsockopt(netlink_sock_, SOL_SOCKET, SO_RCVTIMEO, &tv_out, sizeof(tv_out));

    return r;
}

void NetlinkProcMon::closeProcNetlink()
{
    if (netlink_sock_ != INVALID_SOCKET) {
        close(netlink_sock_);
        netlink_sock_ = INVALID_SOCKET;
    }
}

int NetlinkProcMon::subscribeToProcNetlink()
{
    return setProcEvSubscriptionStatus(true);
}

int NetlinkProcMon::unSubscribeToProcNetlink()
{
    return setProcEvSubscriptionStatus(false);
}

int NetlinkProcMon::setProcEvSubscriptionStatus(bool isEnable)
{
    if (netlink_sock_ == INVALID_SOCKET) {
        return -1;
    }

    struct __attribute__((aligned(NLMSG_ALIGNTO))) {
        struct nlmsghdr nl_hdr;
        struct __attribute__((__packed__)) {
            struct cn_msg cn_msg;
            enum proc_cn_mcast_op cn_mcast;
        };
    } nlcn_msg;

    memset(&nlcn_msg, 0, sizeof(nlcn_msg));
    nlcn_msg.nl_hdr.nlmsg_len = sizeof(nlcn_msg);
    nlcn_msg.nl_hdr.nlmsg_pid = getpid();
    nlcn_msg.nl_hdr.nlmsg_type = NLMSG_DONE;

    nlcn_msg.cn_msg.id.idx = CN_IDX_PROC;
    nlcn_msg.cn_msg.id.val = CN_VAL_PROC;
    nlcn_msg.cn_msg.len = sizeof(enum proc_cn_mcast_op);

    nlcn_msg.cn_mcast = isEnable ? PROC_CN_MCAST_LISTEN : PROC_CN_MCAST_IGNORE;

    auto r = send(netlink_sock_, &nlcn_msg, sizeof(nlcn_msg), 0);
    if (r < 0) {
        return -1;
    }

    return 0;
}

uint64_t NetlinkProcMon::AddProcEvCallback(ProcEventCallback eventCallback, void* context)
{
    auto callbackId = id_generator_.generateId();

    std::lock_guard<std::mutex> lk(ProcEvCallbacksMutex_);
    ProcEv_Callbacks_.emplace_back(eventCallback, context, callbackId);

    return callbackId;
}

void NetlinkProcMon::RemoveProcEvCallback(uint64_t callbackId)
{
    std::lock_guard<std::mutex> lk(ProcEvCallbacksMutex_);

    ProcEv_Callbacks_.remove_if(
        [callbackId](const EventCallbackInfo& callbackInfo) { return callbackInfo.id_ == callbackId; });
}

void NetlinkProcMon::procEvRecvImpl()
{
    if (netlink_sock_ != INVALID_SOCKET) {
        int rc;
        struct __attribute__((aligned(NLMSG_ALIGNTO))) {
            struct nlmsghdr nl_hdr;
            struct __attribute__((__packed__)) {
                struct cn_msg cn_msg;
                struct proc_event proc_ev;
            };
        } nlcn_msg;

        while (StopThreads_ == false) {
            memset(&nlcn_msg, 0, sizeof(nlcn_msg));
            rc = recv(netlink_sock_, &nlcn_msg, sizeof(nlcn_msg), 0);

            if (rc == 0) {
                /* shutdown */
                return;
            } else if (rc == -1) {
                if (errno == EINTR)
                    continue;
                if (errno == ENOBUFS)
                    continue;
                if (errno == EAGAIN)
                    continue;

                return;
            }

            if (nlcn_msg.proc_ev.what == proc_event::PROC_EVENT_FORK
                || nlcn_msg.proc_ev.what == proc_event::PROC_EVENT_EXEC
                || nlcn_msg.proc_ev.what == proc_event::PROC_EVENT_EXIT) {
                std::lock_guard<std::mutex> lk(ProcEvQueueMutex_);

                if (ProcEvQueue_.size() > kProcEvQueueMaxSize) {
                    ProcEvQueue_.pop();
                }

                ProcEvQueue_.push(nlcn_msg.proc_ev);
                ProcEvQueueCv_.notify_all();
            }
        }
    }
}

void NetlinkProcMon::procEvRecv(NetlinkProcMon* procMonThis)
{
    if (procMonThis != nullptr) {
        procMonThis->procEvRecvImpl();
    }
}

void NetlinkProcMon::forEachCallback(ProcEvent& ev)
{
    std::lock_guard<std::mutex> lk(ProcEvCallbacksMutex_);

    for (EventCallbackInfo evElement : ProcEv_Callbacks_) {
        if (evElement.callback_ != nullptr) {
            evElement.callback_(ev, evElement.context_);
        }
    }
}

void NetlinkProcMon::procEvDispatcherImpl()
{
    do {
        std::queue<ProcEvent> tempEvQueue;

        {
            std::unique_lock<std::mutex> lck(ProcEvQueueMutex_);

            if (StopThreads_)
                break;

            if (ProcEvQueue_.size() > 0) {
                ProcEvQueue_.swap(tempEvQueue);

            } else {
                ProcEvQueueCv_.wait(lck);

                ProcEvQueue_.swap(tempEvQueue);
            }
        }

        while (!tempEvQueue.empty()) {
            auto ev = tempEvQueue.front();
            tempEvQueue.pop();

            forEachCallback(ev);
        }

    } while (true);
}

void NetlinkProcMon::procEvDispatcher(NetlinkProcMon* procMonThis)
{
    if (procMonThis != nullptr) {
        procMonThis->procEvDispatcherImpl();
    }
}