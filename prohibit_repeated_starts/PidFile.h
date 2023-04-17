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

#define kMyPidFile "/tmp/my_pid_file"
#define kBufLenForPid 64

class PidFileManager {
 public:
  PidFileManager() {}
  ~PidFileManager() { RemovePidFile(); }

  void CheckOnly() {
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
      std::cerr << e.what() << '\n';
    }
  }

  void Run() {
    while (true) {
      sleep(3);
    }
  }

 private:
  pid_t pid_;
  int fd_;

  void RemovePidFile() {
    try {
      remove(kMyPidFile);
    } catch (const std::exception& e) {
      std::cerr << e.what() << '\n';
    }
  }

  void CreatePidFile() {
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

  void CheckPidFile() {
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

  void WritePidIntoFd() {
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
};

static void SigHandler(int sig) {
  if (sig == SIGINT || sig == SIGTERM) {
    remove(kMyPidFile);
  }
  _exit(0);
}

int main() {
  PidFileManager pid_file_manager;
  pid_file_manager.CheckOnly();
  signal(SIGINT, SigHandler);
  signal(SIGTERM, SigHandler);
  pid_file_manager.Run();
  return 0;
}