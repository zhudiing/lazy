/**
 * @Date         : 2023-03-27 17:45:22
 * @LastEditors  : liushuai05
 * @LastEditTime : 2023-03-27 17:45:27
 */
#pragma once
#include <array>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>

#ifndef defer
struct defer_dummy {};
template <class F> struct deferrer { F f; ~deferrer() { f(); } };
template <class F> deferrer<F> operator*(defer_dummy, F f) { return {f}; }
#define DEFER_(LINE) zz_defer##LINE
#define DEFER(LINE) DEFER_(LINE)
#define defer auto DEFER(__LINE__) = defer_dummy{} *[&]()
#endif    // defer

namespace Shell
{
    struct Result
    {
        int exitCode;
        std::string output;
        bool operator==(const Result &rhs) const;
        bool operator!=(const Result &rhs) const;
        explicit operator bool() const;
        friend std::ostream &operator<<(std::ostream &os, const Result &result);
    };

    inline bool Result::operator==(const Result &rhs) const { return output == rhs.output && exitCode == rhs.exitCode; }
    inline bool Result::operator!=(const Result &rhs) const { return !(rhs == *this); }
    inline Result::operator bool() const { return exitCode == 0; }

    inline std::ostream &operator<<(std::ostream &os, const Result &result)
    {
        os << "exit code: " << result.exitCode << " output: " << result.output;
        return os;
    }

    /**
     * 执行shell命令 获取返回值和输出信息
     * 注意命令是否会阻塞
     *
     * @param command
     * @param os 
     * @return
     */
    inline Result exec(const std::string &command, std::ostream *os = nullptr)
    {
        static std::mutex mutex;

        std::array<char, 8192> buffer{};
        std::string result;
#ifdef _WIN32
    #define popen _popen
    #define pclose _pclose
    #define WIFEXITED
    #define WEXITSTATUS
#else
        struct sigaction act_tmp, act_old;    // NOLINT
        act_tmp.sa_handler = SIG_DFL;
        sigemptyset(&act_tmp.sa_mask);
        act_tmp.sa_flags = 0;

        sigaction(SIGCHLD, &act_tmp, &act_old);
        defer { sigaction(SIGCHLD, &act_old, nullptr); };
#endif

        mutex.lock();
        FILE *pipe = popen(command.c_str(), "r");
        mutex.unlock();

        if(pipe == nullptr) { return {-1, "popen() open failed!"}; }
        try {
            std::size_t bytesRead;
            while((bytesRead = std::fread(buffer.data(), sizeof(buffer.at(0)), sizeof(buffer), pipe)) != 0) {
                result += std::string(buffer.data(), bytesRead);
                if(os) { *os << std::string(buffer.data(), bytesRead); }
            }
        } catch(...) {
            pclose(pipe);
            return {-1, "popen() exec failed!"};
        }
        int ret = pclose(pipe);
        if(ret == -1) {
            return Result{-1, strerror(errno)};
        } else {
            if(WIFEXITED(ret)) {    // normal exit()
                int exitcode = WEXITSTATUS(ret);
                return Result{exitcode, result};
            } else {    // other error
                return Result{-2, result};
            }
        }
    }

}    // namespace Shell
