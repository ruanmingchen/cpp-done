#include "crash_handler.h"

#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <boost/filesystem.hpp>
#include <vector>

#ifndef strstartswith
#define strstartswith(a, b) (strncmp((a), (b), strlen(b)) == 0)
#endif

#ifndef streq
#define streq(a, b) (strcmp((a), (b)) == 0)
#endif

#define CRASH_FILE_NAME         "crash_trace"
#define CRASH_PROCESS_MAPS      "crash_process_maps"
#define PROC_SELF_EXE_PATH      "/proc/self/exe"
#define ALOG_CRASH_FILE_MAX_NUM 6
#define FILE_PATH_MAX_LEN       1024

CrashHandler::CrashHandler() : back_trace_(true), kCrashLogPath_()
{
}

void CrashHandler::Init()
{
    ClearCrashFiles();

    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_sigaction = CrashSighandler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_RESTART;

    sigaction(SIGINT, &act, &oldSigAct_[SIGINT]);  // 程序终止(interrupt)信号,通常是Ctrl-C时发出 deamon进程不会出现
    sigaction(SIGILL, &act, &oldSigAct_[SIGILL]);    // 信号4    非法指令
    sigaction(SIGABRT, &act, &oldSigAct_[SIGABRT]);  // 信号6    来自abort函数的终止信号
    sigaction(SIGBUS, &act, &oldSigAct_[SIGBUS]);    // 信号7    总线错误
    sigaction(SIGFPE, &act, &oldSigAct_[SIGFPE]);    // 信号8    浮点异常
    sigaction(SIGSEGV, &act, &oldSigAct_[SIGSEGV]);  // 信号11   无效的存储器引用(段故障) 往没有写权限的内存地址写数据
    sigaction(SIGSTKFLT, &act, &oldSigAct_[SIGSTKFLT]);  // 信号16   协处理器上的栈故障
    // sigaction(SIGURG, &act, &oldSigAct_[SIGURG]);     // 信号23 有"紧急"数据，
    // 目前内部堆栈回溯使用 sigaction(SIGPIPE, &act, &oldSigAct_[SIGPIPE]);  //
    // 信号13   向一个没有读用户的管道做写操作 不作为异常退出 Broken pipe
}

/*
 * @brief Set the backTraceEnabled_ switch.
 */

void CrashHandler::set_back_trace(bool enable)
{
    back_trace_.store(enable, std::memory_order_release);
}

/*
 * @brief Get the backTraceEnabled_ switch.
 */
bool CrashHandler::back_trace() const
{
    return back_trace_.load(std::memory_order_acquire);
}

/*
 * @brief 将程序指令地址转换为所对应的函数名、以及函数所在的源文件名和行号.
 */
bool CrashHandler::Addr2line(uintptr_t pc, char line[], int line_size)
{
    bool ret = false;

    if (line_size > 0) {
#ifndef BUILD_RELEASE
        char cmd[256] = {0};

        snprintf(cmd, sizeof(cmd) - 1, "Addr2line -C -f -e /proc/%d/exe 0x%lx", getpid(), pc);
        int fd = v_popen(cmd, "r");
        if ((fd >= 0) && (read(fd, line, line_size - 1) >= 0)) {
            ret = true;
        }

        if (fd >= 0) {
            close(fd);
        }
#else
        line[0] = '\n';

        ret = true;
#endif
    }

    return ret;
}

/*
 * @brief Create the crash_time_stamp file.
 */
void CrashHandler::CrashTimeStampCreate(const std::string& file_path, const struct tm* ptm)
{
    if (nullptr != ptm) {
        int fd = open(file_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd >= 0) {
            char crash_time[256] = {0};
            snprintf(crash_time, sizeof(crash_time) - 1, "%04d-%02d-%02d %02d:%02d:%02d", ptm->tm_year + 1900,
                     ptm->tm_mon + 1, ptm->tm_mday, ptm->tm_hour, ptm->tm_min, ptm->tm_sec);

            ssize_t ret = write(fd, crash_time, strlen(crash_time));
            close(fd);
        }
    }
}

/*
 * @brief Create the crash_trace file.
 */
int CrashHandler::CrashFileOpenAndWriteCommon(int signo, const char* type)
{
    int fd = -1;

    do {
        time_t now;
        time(&now);
        struct tm time_now;
        struct tm* ptm = localtime_r(&now, &time_now);
        if (nullptr == ptm)
            break;

        char filename[256] = {0};

        snprintf(filename, sizeof(filename) - 1, "%s%s.%04d-%02d-%02d_%02d_%02d_%02d", kCrashLogPath_.c_str(),
                 CRASH_FILE_NAME, ptm->tm_year + 1900, ptm->tm_mon + 1, ptm->tm_mday, ptm->tm_hour, ptm->tm_min,
                 ptm->tm_sec);

        fd = open(filename, O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (fd < 0) {
            break;
        }

        ssize_t wbytes = 0;

        // 输出程序的绝对路径
        char buffer[512] = {0};
        int count = readlink(PROC_SELF_EXE_PATH, buffer, sizeof(buffer) - 2);
        if (count > 0) {
            buffer[count] = '\n';
            buffer[count + 1] = 0;

            wbytes = write(fd, buffer, count + 1);
        }

        // 输出信息的时间
        memset(buffer, 0, sizeof(buffer));
        snprintf(buffer, sizeof(buffer) - 1, "Dump Time: %04d-%02d-%02d %02d:%02d:%02d\n", ptm->tm_year + 1900,
                 ptm->tm_mon + 1, ptm->tm_mday, ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
        wbytes = write(fd, buffer, strlen(buffer));

        // 线程和信号
        int mytid = syscall(SYS_gettid);
        snprintf(buffer, sizeof(buffer) - 1, "Current thread: %u, Catch signal: %d Type: %s\n", mytid, signo,
                 (type == nullptr) ? "unknown" : type);
        wbytes = write(fd, buffer, strlen(buffer));

        wbytes = write(fd, "\n", 1);  // 插入空行

        // 写入到设备
        fsync(fd);

    } while (false);

    return fd;
}

/*
 * @brief Close the fd file.
 */
void CrashHandler::CrashFileClose(int fd)
{
    if (fd >= 0) {
        close(fd);
    }
}

/*
 * @brief Copy the file from dest to src.
 */
void CrashHandler::CrashFileCopy(const char* src, const char* dest)
{
    int srcFd = -1;
    int dstFd = -1;

    do {
        if (!src || !dest)
            break;

        srcFd = open(src, O_RDONLY);  // open src file
        if (srcFd == -1)
            break;

        dstFd = open(dest, O_WRONLY | O_CREAT | O_EXCL, 0664);  // open dest file
        if (dstFd == -1)
            break;

        int len = 0;
        char buf[1024] = {0};

        while ((len = read(srcFd, buf, sizeof(buf))) > 0) {
            ssize_t wbytes = write(dstFd, buf, len);
        }

    } while (false);

    if (srcFd != -1) {
        close(srcFd);  // close src fd
        srcFd = -1;
    }

    if (dstFd != -1) {
        close(dstFd);  // close dest fd
        dstFd = -1;
    }

    return;
}

/*
 * @brief 发生crash，将Maps读出来放log文件里面
 */
void CrashHandler::SaveProcessMapsToLog()
{
    time_t now;
    time(&now);
    struct tm time_now;
    struct tm* ptm = localtime_r(&now, &time_now);

    if (nullptr != ptm) {
        char src_file[256] = {0};
        snprintf(src_file, sizeof(src_file) - 1, "/proc/%d/maps", getpid());

        char dest_file[256] = {0};
        snprintf(dest_file, sizeof(dest_file) - 1, "%s%s.%04d-%02d-%02d_%02d_%02d_%02d", kCrashLogPath_,
                 CRASH_PROCESS_MAPS, ptm->tm_year + 1900, ptm->tm_mon + 1, ptm->tm_mday, ptm->tm_hour, ptm->tm_min,
                 ptm->tm_sec);

        CrashFileCopy(src_file, dest_file);
    }
}

/*
 * @brief 发生crash，write backtrace to log
 */
void CrashHandler::SaveBacktraceForSig(int signo)
{
    do {
        if (!back_trace()) {  // check the backtrace switch
            break;
        }

        int fd = CrashFileOpenAndWriteCommon(signo, "crash");  // create the file
        if (fd < 0) {
            break;
        }

        // 锁定文件
        struct flock fl;
        fl.l_type = F_WRLCK;
        fl.l_start = 0;
        fl.l_whence = SEEK_SET;
        fl.l_len = 0;
        fl.l_pid = getpid();
        fcntl(fd, F_SETLKW, &fl);

        unw_context_t unw_ctx;
        unw_cursor_t unw_cur;
        unw_getcontext(&unw_ctx);
        unw_init_local(&unw_cur, &unw_ctx);

        char line[4096];
        int count = 1;

        do {
            ssize_t wbytes = 0;
            unw_word_t ip;
            unw_get_reg(&unw_cur, UNW_REG_IP, &ip);  // get the crash ip info

            char buf[50] = {0};
            snprintf(buf, sizeof(buf) - 1, "%02d: [0x%lx]", count, ip);
            wbytes = write(fd, buf, strlen(buf));
            wbytes = write(fd, " ", 1);

            memset(line, 0, sizeof(line));
            if (Addr2line(ip, line,
                          sizeof(line))) {  // get function info by Addr2line
                wbytes = write(fd, line, strlen(line));
                wbytes = write(fd, "\n", 1);
            }

            int next_res = unw_step(&unw_cur);  // get next

            if (next_res <= 0) {
                break;
            }

            ++count;
        } while (true);

        // 文件解锁后关闭
        fl.l_type = F_UNLCK;
        fcntl(fd, F_SETLK, &fl);

        CrashFileClose(fd);  // close fd

    } while (false);
}

/*
 * @brief Process when get crash sig.
 */
void CrashHandler::ProcessCrashSig(int signo, siginfo_t* info, void* reserved)
{
    if (CheckDirExistOrMake(kCrashLogPath_.c_str())) {
        SaveProcessMapsToLog();
        SaveBacktraceForSig(signo);
    }

    if (signo != SIGURG) {
        signal(signo, SIG_DFL); /* 恢复信号默认处理 */

#ifdef BUILD_RELEASE
        raise(signo); /* 重新发送信号 */
#else
        raise(SIGSTOP); /* 重新发送STOP信号，暂停进程执行，方便调试 */
#endif
    }
    // oldSigAct_[signo].sa_handler(signo);
}

/*
 * @brief Crash sig handler.
 */
static void CrashSighandler(int signo, siginfo_t* info, void* reserved)
{
    CrashHandler::GetInstance().ProcessCrashSig(signo, info, reserved);
}

void CrashHandler::ClearCrashFiles()
{
    try {
        std::vector<std::string> crashFiles;
        std::vector<std::string> crashProcessMaps;

        boost::filesystem::path log_directory_path(kCrashLogPath_);
        boost::filesystem::directory_iterator iterator_end;

        // enum the crash files
        for (boost::filesystem::directory_iterator iterator(log_directory_path); iterator != iterator_end; ++iterator) {
            std::string name = iterator->path().filename().string();

            if (strstartswith(name.c_str(), CRASH_FILE_NAME)) {
                crashFiles.push_back(name);
            } else if (strstartswith(name.c_str(), CRASH_PROCESS_MAPS)) {
                crashProcessMaps.push_back(name);
            }
        }

        // 删除crash文件
        if (crashFiles.size() >= ALOG_CRASH_FILE_MAX_NUM) {
            // 排序，按从小到大排序
            std::sort(crashFiles.begin(), crashFiles.end());

            for (int i = 0; i < (ALOG_CRASH_FILE_MAX_NUM / 2); i++) {
                std::string path = kCrashLogPath_;
                path += crashFiles.at(i);
                RemoveSingleFile(path.c_str());
            }
        }

        // 删除process_maps文件
        if (crashProcessMaps.size() >= ALOG_CRASH_FILE_MAX_NUM) {
            // 排序，按从小到大排序
            std::sort(crashProcessMaps.begin(), crashProcessMaps.end());

            for (int i = 0; i < (ALOG_CRASH_FILE_MAX_NUM / 2); i++) {
                std::string path = kCrashLogPath_;
                path += crashProcessMaps.at(i);

                RemoveSingleFile(path.c_str());
            }
        }
    } catch (...) {
    }
}

int CrashHandler::v_popen(const char* cmd, const char* type)
{
    int ret = -1;

    do {
        int pfd[2];

        /* only allow "r" or "w" */
        if ((type[0] != 'r' && type[0] != 'w') || type[1] != 0)
            break;

        if (pipe2(pfd, O_CLOEXEC) < 0)  // make pipe
            break;

        pid_t pid = vfork();
        if (pid < 0) {
            close(pfd[0]);
            close(pfd[1]);
            break;
        } else if (pid == 0) {
            if (*type == 'r') {
                close(pfd[0]);  // close read pipe
                if (pfd[1] != STDOUT_FILENO) {
                    // exec函数的执行结果将会通过标准输出写到控制台上，但这里我们不需要在控制台输出，
                    // 而是需要将结果返回，因此将标准输出重定向到管道写端
                    dup2(pfd[1], STDOUT_FILENO);
                    close(pfd[1]);
                }
            } else {
                close(pfd[1]);                   // close write pipe
                if (pfd[0] != STDIN_FILENO) {
                    dup2(pfd[0], STDIN_FILENO);  // 将标准输入重定向到管道读端
                    close(pfd[0]);
                }
            }

            execl("/bin/sh", "sh", "-c", cmd, nullptr);  // 用exec函数执行命令
            _exit(127);
        }

        if (*type == 'r') {
            close(pfd[1]);
            ret = pfd[0];  // return read pipe in parent process
        } else {
            close(pfd[0]);
            ret = pfd[1];  // return write pipe in parent process
        }

    } while (false);

    return ret;
}

bool CrashHandler::RemoveSingleFile(const char* filepath)
{
    bool bres = false;

    do {
        if (filepath == NULL) {
            break;
        }

        // 不能删除根目录
        if (streq(filepath, "/")) {
            break;
        }

        // 必须使用绝对路径，开头不是/的就不是绝对路径
        if (filepath[0] != '/') {
            break;
        }

        if (access(filepath, F_OK) == 0) {
            bres = (0 == unlink(filepath));
        } else {
            bres = true;  // The file does not exist and return true.
        }

    } while (false);

    return bres;
}

bool CrashHandler::CheckDirExistOrMake(const char* dir)
{
    if (dir == nullptr) {
        return false;
    }

    // Check if the directory exists
    if (access(dir, F_OK) == 0) {
        return true;
    }

    // Make the directory
    char dest_dir[FILE_PATH_MAX_LEN] = {0};
    strncpy(dest_dir, dir, sizeof(dir) - 2);  // 预留'/'空间
    int len = strlen(dir);
    if (dir[len - 1] != '/') {
        strcat(dest_dir, "/");
    }

    len = strlen(dest_dir);
    for (int i = 1; i < len; i++) {
        if (dest_dir[i] == '/') {
            dest_dir[i] = 0;  // 处理当前目录
            if (access(dest_dir, F_OK) != 0) {
                /* 文件夹other用户可进入
                 * 最终产生的文件（或目录）的权限值是mode值与umask的反值（即~umask）
                 * 做AND运算后的结果，这个值才真正表示了创建出的文件或目录的权限。
                 */
                mode_t old = umask(022);
                int ret = mkdir(dest_dir, 0755);
                umask(old);
                if (ret == -1) {
                    return false;  // Failed to make directory
                }
            }

            dest_dir[i] = '/';  // 恢复
        }
    }

    return true;
}
