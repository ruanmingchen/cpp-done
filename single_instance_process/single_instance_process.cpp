#include "single_instance_process.h"

SingleInstanceProcess::~SingleInstanceProcess()
{
    RemovePidFile();
}

void SingleInstanceProcess::CheckOnly()
{
    try {
        pid_ = getpid();
        fd_ = open(kMyPidFile, O_RDWR);
        if (fd_ == -1) {
            if (errno == ENOENT) {
                CreatePidFile();
            } else {
                perror("open pid fail\n");
                exit(-1);
            }
        } else {
            CheckPidFile();
        }
    } catch (const std::exception& e) {
    }
}

[[noreturn]] void SingleInstanceProcess::Run()
{
    while (true) {
        sleep(3);
    }
}

void SingleInstanceProcess::RemovePidFile()
{
    try {
        std::remove(kMyPidFile);
    } catch (const std::exception& e) {
    } catch (...) {
    }
}

void SingleInstanceProcess::CreatePidFile()
{
    fd_ = open(kMyPidFile, O_WRONLY | O_CREAT | O_EXCL, 0666);
    if (fd_ == -1) {
        perror("Create pid fail\n");
        exit(-1);
    }

    int ret = flock(fd_, LOCK_EX);
    if (ret == -1) {
        perror("flock pid fail\n");
        close(fd_);
        exit(-1);
    }

    WritePidIntoFd();

    flock(fd_, LOCK_UN);
    close(fd_);
}

void SingleInstanceProcess::CheckPidFile()
{
    int ret = flock(fd_, LOCK_EX);
    if (ret == -1) {
        perror("flock pid fail\n");
        close(fd_);
        exit(-1);
    }

    char buf[kBufLenForPid] = {0};
    ret = read(fd_, buf, sizeof(buf) - 1);
    if (ret < 0) {
        perror("read from pid fail\n");
        exit(-1);
    } else if (ret > 0) {
        pid_t old_pid = atol(buf);
        ret = kill(old_pid, 0);
        if (ret < 0) {
            if (errno == ESRCH) {
                WritePidIntoFd();
            } else {
                perror("send signal fail\n");
                exit(-1);
            }
        } else {
            std::cout << "Program already exists, pid=" << old_pid << std::endl;
            exit(-1);
        }
    } else if (ret == 0) {
        WritePidIntoFd();
    }

    flock(fd_, LOCK_UN);
    close(fd_);
}

void SingleInstanceProcess::WritePidIntoFd()
{
    char buf[kBufLenForPid] = {0};
    sprintf(buf, "%d", pid_);
    int ret = write(fd_, buf, strlen(buf));
    if (ret <= 0) {
        if (ret == -1) {
            perror("Write pid fail\n");
        }
        exit(-1);
    } else {
        std::cout << "Create pid file ok, pid=" << pid_ << std::endl;
    }
}

void SingleInstanceProcess::SigHandler(int sig)
{
    if (sig == SIGINT || sig == SIGTERM) {
        std::remove(kMyPidFile);
    }
    _exit(0);
}