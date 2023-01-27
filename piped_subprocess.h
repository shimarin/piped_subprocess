#ifndef __PIPED_SUBPROCESS_H__
#define __PIPED_SUBPROCESS_H__

#include <iostream>
#include <variant>
#include <functional>
#include <expected>
#include <filesystem>

namespace piped_subprocess {
    typedef std::function<void(std::ostream&)> feeder_func_stream_t;
    typedef std::function<void(int)> feeder_func_fd_t;
    typedef std::variant<feeder_func_stream_t,feeder_func_fd_t> feeder_func_t;

    typedef std::function<void(std::istream&)> receiver_func_stream_t;
    typedef std::function<void(int)> receiver_func_fd_t;
    typedef std::variant<receiver_func_stream_t,receiver_func_fd_t> receiver_func_t;

    struct Options {
        const std::variant<std::monostate,feeder_func_t,int,const std::filesystem::path,const std::string,const char*> feeder;
        const std::variant<std::monostate,receiver_func_t,int> receiver;
        const std::variant<std::monostate,receiver_func_t,int> error_receiver;
    };

    std::expected<int, int> fork(std::function<int(void)> subprocess, const Options& options = {});
    std::expected<int, int> exec(const std::string& cmd, const std::vector<std::string>& args = {}, const Options& options = {});
    void ENSURE_OK(std::expected<int, int> result);
}

#endif // __PIPED_SUBPROCESS_H__