#ifndef CRASH_HANDLER_H
#define CRASH_HANDLER_H

#include <signal.h>

#include <atomic>
#include <string>

class CrashHandler final
{
public:
    static CrashHandler& GetInstance()
    {
        static CrashHandler signal;
        return signal;
    }

public:
    void Init();
    void set_back_trace(bool enable);                                  // Set the backTraceEnabled_ switch.
    bool back_trace() const;                                           // Get the backTraceEnabled_ switch.
    void ProcessCrashSig(int signo, siginfo_t* info, void* reserved);  // Process when get crash sig.

private:
    CrashHandler();
    ~CrashHandler() = default;

    CrashHandler(const CrashHandler&) = delete;
    CrashHandler& operator=(const CrashHandler&) = delete;

    bool Addr2line(uintptr_t pc, char line[], int line_size);
    void CrashTimeStampCreate(const std::string& file_path, const struct tm* ptm);

    int CrashFileOpenAndWriteCommon(int signo, const char* type);
    void CrashFileClose(int fd);
    void CrashFileCopy(const char* src, const char* dest);

    void SaveProcessMapsToLog();          // write maps to log
    void SaveBacktraceForSig(int signo);  // write backtrace to log

    void ClearCrashFiles();

    int v_popen(const char* cmd, const char* type);
    bool CheckDirExistOrMake(const char* dir);
    bool RemoveSingleFile(const char* filepath);

private:
    struct sigaction oldSigAct_[NSIG];
    std::atomic_bool back_trace_;
    const std::string kCrashLogPath_;
};

#endif