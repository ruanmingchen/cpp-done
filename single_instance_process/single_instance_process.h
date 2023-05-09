// 1.
// 进程在启动时，判断/tmp/my_pid_file是否存在；如果不存在，则将当前进程的pid写入，程序继续运行；

// 2.
// 如果/tmp/my_pid_file已经存在但无内容，则将当前进程的pid写入，程序继续运行；

// 3.
// 如果/tmp/my_pid_file已经存在且有内容，读取并判断该值指向的pid是否正在运行。如果没有，则将当前进程的pid写入，程序继续运行；否则，当前进程退出。使用了
// kill(pid, 0) 进行检测．

// 4. 在收到SIGINT和SIGTERM时，先删除/tmp/my_pid_file，再退出．
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <fstream>
#include <iostream>

#define kMyPidFile    "/tmp/my_pid_file"
#define kBufLenForPid 64

class PidFileManager
{
public:
    PidFileManager() = default;
    ~PidFileManager();

    void CheckOnly();
    void Run();

    static void SigHandler(int sig);

private:
    void RemovePidFile();
    void CreatePidFile();
    void CheckPidFile();
    void WritePidIntoFd();

private:
    pid_t pid_;
    int fd_;
};

int main()
{
    PidFileManager pid_file_manager;
    pid_file_manager.CheckOnly();
    signal(SIGINT, PidFileManager::SigHandler);
    signal(SIGTERM, PidFileManager::SigHandler);
    pid_file_manager.Run();
    return 0;
}