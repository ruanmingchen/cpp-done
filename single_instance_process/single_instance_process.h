// 1.
// 进程在启动时，判断/tmp/my_pid_file是否存在；如果不存在，则将当前进程的pid写入，程序继续运行；

// 2.
// 如果/tmp/my_pid_file已经存在但无内容，则将当前进程的pid写入，程序继续运行；

// 3.
// 如果/tmp/my_pid_file已经存在且有内容，读取并判断该值指向的pid是否正在运行。如果没有，则将当前进程的pid写入，程序继续运行；否则，当前进程退出。使用了
// kill(pid, 0) 进行检测．

// 4. 在收到SIGINT和SIGTERM时，先删除/tmp/my_pid_file，再退出．
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <csignal>
#include <cstring>
#include <fstream>
#include <iostream>

class SingleInstanceProcess
{
public:
    SingleInstanceProcess() = default;
    ~SingleInstanceProcess();

    void CheckOnly();
    [[noreturn]] static void Run();

    static void SigHandler(int sig);

private:
    static void RemovePidFile();
    void CreatePidFile();
    void CheckPidFile();
    void WritePidIntoFd();

private:
    static constexpr char *kMyPidFile = "/tmp/my_pid_file";
    static constexpr int kBufLenForPid = 64;

    pid_t pid_{};
    int fd_{};
};