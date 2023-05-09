#include "single_instance_process.h"

PidFileManager::~PidFileManager()
{
    RemovePidFile();
}

void PidFileManager::CheckOnly()
{
    try {
        pid_ = getpid();
        fd_ = open(kMyPidFile, O_RDWR);
        if (fd_ == -1) {
            if (errno == ENOENT) {
                CreatePidFile();
            } else {
                perror("open " kMyPidFile " fail\n");
                exit(-1);
            }
        } else {
            CheckPidFile();
        }
    } catch (const std::exception& e) {
    }
}

void PidFileManager::Run()
{
    while (true) {
        sleep(3);
    }
}

void PidFileManager::RemovePidFile()
{
    try {
        std::remove(kMyPidFile);
    } catch (const std::exception& e) {
    } catch (...) {
    }
}

void PidFileManager::CreatePidFile()
{
    fd_ = open(kMyPidFile, O_WRONLY | O_CREAT | O_EXCL, 0666);
    if (fd_ == -1) {
        perror("Create " kMyPidFile " fail\n");
        exit(-1);
    }

    int ret = flock(fd_, LOCK_EX);
    if (ret == -1) {
        perror("flock " kMyPidFile " fail\n");
        close(fd_);
        exit(-1);
    }

    WritePidIntoFd();

    flock(fd_, LOCK_UN);
    close(fd_);
}

void PidFileManager::CheckPidFile()
{
    int ret = flock(fd_, LOCK_EX);
    if (ret == -1) {
        perror("flock " kMyPidFile " fail\n");
        close(fd_);
        exit(-1);
    }

    char buf[kBufLenForPid] = {0};
    ret = read(fd_, buf, sizeof(buf) - 1);
    if (ret < 0) {
        perror("read from " kMyPidFile " fail\n");
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

void PidFileManager::WritePidIntoFd()
{
    char buf[kBufLenForPid] = {0};
    sprintf(buf, "%d", pid_);
    int ret = write(fd_, buf, strlen(buf));
    if (ret <= 0) {
        if (ret == -1) {
            perror("Write " kMyPidFile " fail\n");
        }
        exit(-1);
    } else {
        std::cout << "Create " kMyPidFile " ok, pid=" << pid_ << std::endl;
    }
}

void PidFileManager::SigHandler(int sig)
{
    if (sig == SIGINT || sig == SIGTERM) {
        remove(kMyPidFile);
    }
    _exit(0);
}