#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>

#include <cassert>
#include <memory>
#include <thread>
#include <ext/stdio_filebuf.h> // for __gnu_cxx::stdio_filebuf

#include "piped_subprocess.h"

static int exec(const std::string& cmd, const std::vector<std::string>& args)
{
    std::vector<const char*> argv = {cmd.c_str()};
    for (const auto& arg:args) {
        argv.push_back(arg.c_str());
    }
    argv.push_back(NULL);
    return execvp(cmd.c_str(), const_cast<char* const*>(argv.data()));
}

namespace piped_subprocess {

std::expected<int, int> fork(std::function<int(void)> subprocess, const Options& options/* = {}*/)
{
    int feeder_fd[2] = { -1, -1 }, receiver_fd[2] = { -1, -1 }, error_receiver_fd[2] = { -1, -1 };
    if ((std::holds_alternative<feeder_func_t>(options.feeder) || std::holds_alternative<const std::string>(options.feeder) 
        || std::holds_alternative<const char*>(options.feeder)) && pipe(feeder_fd) < 0) {
        throw std::runtime_error("pipe(feeder_fd) failed");
    } else if (std::holds_alternative<const std::filesystem::path>(options.feeder)) {
        const auto& path = std::get<const std::filesystem::path>(options.feeder);
        feeder_fd[0] = open(path.c_str(), O_RDONLY);
        if (feeder_fd[0] < 0) throw std::runtime_error("open(" + path.string() + ") failed. " + strerror(errno));
    }
    if (std::holds_alternative<receiver_func_t>(options.receiver) && pipe(receiver_fd) < 0) {
        throw std::runtime_error("pipe(receiver_fd) failed");
    }
    if (std::holds_alternative<receiver_func_t>(options.error_receiver) && pipe(error_receiver_fd) < 0) {
        throw std::runtime_error("pipe(error_receiver_fd) failed");
    }
    auto pid = ::fork();
    if (pid < 0) {
        if (feeder_fd[0] >= 0) close(feeder_fd[0]);
        if (feeder_fd[1] >= 0) close(feeder_fd[1]);
        if (receiver_fd[0] >= 0) close(receiver_fd[0]);
        if (receiver_fd[1] >= 0) close(receiver_fd[1]);
        if (error_receiver_fd[0] >= 0) close(error_receiver_fd[0]);
        if (error_receiver_fd[1] >= 0) close(error_receiver_fd[1]);
        throw std::runtime_error("fork() failed");
    }
    if (pid == 0) { // child
        if (feeder_fd[1] >= 0) close(feeder_fd[1]);
        if (receiver_fd[0] >= 0) close(receiver_fd[0]);
        if (error_receiver_fd[0] >= 0) close(error_receiver_fd[0]);

        if (std::holds_alternative<int>(options.feeder)) feeder_fd[0] = std::get<int>(options.feeder);
        if (feeder_fd[0] >= 0) dup2(feeder_fd[0], STDIN_FILENO);

        if (std::holds_alternative<int>(options.receiver)) receiver_fd[1] = std::get<int>(options.receiver);
        if (receiver_fd[1] >= 0) dup2(receiver_fd[1], STDOUT_FILENO);

        if (std::holds_alternative<int>(options.error_receiver)) error_receiver_fd[1] = std::get<int>(options.error_receiver);
        if (error_receiver_fd[1] >= 0) dup2(error_receiver_fd[1], STDERR_FILENO);

        _exit(subprocess());
    }
    //else(parent)
    if (feeder_fd[0] >= 0) close(feeder_fd[0]);
    if (receiver_fd[1] >= 0) close(receiver_fd[1]);
    if (error_receiver_fd[1] >= 0) close(error_receiver_fd[1]);
    std::exception_ptr feeder_exception, receiver_exception, error_receiver_exception;
    std::shared_ptr<std::thread> feeder_thread, receiver_thread, error_receiver_thread;
    if (std::holds_alternative<feeder_func_t>(options.feeder)) {
        const auto& feeder = std::get<feeder_func_t>(options.feeder);
        feeder_thread = std::make_shared<std::thread>([&](){
            try {
                if (std::holds_alternative<feeder_func_stream_t>(feeder)) {
                    __gnu_cxx::stdio_filebuf<char> filebuf(feeder_fd[1], std::ios::out);
                    feeder_fd[1] = -1; // stdio_filebuf will close it
                    std::ostream f(&filebuf);
                    std::get<feeder_func_stream_t>(feeder)(f);
                } else if (std::holds_alternative<feeder_func_fd_t>(feeder)) {
                    std::get<feeder_func_fd_t>(feeder)(feeder_fd[1]);
                } else assert(false);
            }
            catch (...) {
                feeder_exception = std::current_exception();
            }
            if (feeder_fd[1] >= 0) close(feeder_fd[1]);
        });
    } else if (std::holds_alternative<const std::string>(options.feeder)) {
        const auto& feeder = std::get<const std::string>(options.feeder);
        feeder_thread = std::make_shared<std::thread>([&]() {
            try {
                if (write(feeder_fd[1], feeder.c_str(), feeder.length()) < 0) {
                    throw std::runtime_error("write() failed");
                }
            }
            catch (...) {
                feeder_exception = std::current_exception();
            }
            close(feeder_fd[1]);
        });      
    } else if (std::holds_alternative<const char*>(options.feeder)) {
        const auto& feeder = std::get<const char*>(options.feeder);
        feeder_thread = std::make_shared<std::thread>([&]() {
            try {
                if (write(feeder_fd[1], feeder, strlen(feeder)) < 0) {
                    throw std::runtime_error("write() failed");
                }
            }
            catch (...) {
                feeder_exception = std::current_exception();
            }
            close(feeder_fd[1]);
        });      
    }
    if (std::holds_alternative<receiver_func_t>(options.receiver)) {
        const auto& receiver = std::get<receiver_func_t>(options.receiver);
        receiver_thread = std::make_shared<std::thread>([&](){
            try {
                if (std::holds_alternative<receiver_func_stream_t>(receiver)) {
                    __gnu_cxx::stdio_filebuf<char> filebuf(receiver_fd[0], std::ios::in);
                    receiver_fd[0] = -1; // stdio_filebuf will close it
                    std::istream f(&filebuf);
                    f.peek();
                    if (!f.eof()) std::get<receiver_func_stream_t>(receiver)(f);
                } else if (std::holds_alternative<receiver_func_fd_t>(receiver)) {
                    std::get<receiver_func_fd_t>(receiver)(receiver_fd[0]);
                } else assert(false);
            }
            catch (...) {
                receiver_exception = std::current_exception();
            }
            if (receiver_fd[0] >= 0) close(receiver_fd[0]);

        });
    }
    if (std::holds_alternative<receiver_func_t>(options.error_receiver)) {
        const auto& error_receiver = std::get<receiver_func_t>(options.error_receiver);
        error_receiver_thread = std::make_shared<std::thread>([&](){
            try {
                if (std::holds_alternative<receiver_func_stream_t>(error_receiver)) {
                    __gnu_cxx::stdio_filebuf<char> filebuf(error_receiver_fd[0], std::ios::in);
                    error_receiver_fd[0] = -1; // stdio_filebuf will close it
                    std::istream f(&filebuf);
                    f.peek();
                    if (!f.eof()) std::get<receiver_func_stream_t>(error_receiver)(f);
                } else if (std::holds_alternative<receiver_func_fd_t>(error_receiver)) {
                    std::get<receiver_func_fd_t>(error_receiver)(error_receiver_fd[0]);
                    close(error_receiver_fd[0]);
                } else assert(false);
            }
            catch (...) {
                error_receiver_exception = std::current_exception();
            }
            if (error_receiver_fd[0] >= 0) close(error_receiver_fd[0]);
        });
    }
    int wstatus;
    if (waitpid(pid, &wstatus, 0) < 0) throw std::runtime_error("waitpid() failed");
    if (feeder_thread) feeder_thread->join();
    if (receiver_thread) receiver_thread->join();
    if (error_receiver_thread) error_receiver_thread->join();

    if (feeder_exception) std::rethrow_exception(feeder_exception);
    if (receiver_exception) std::rethrow_exception(receiver_exception);
    if (error_receiver_exception) std::rethrow_exception(error_receiver_exception);

    if (!WIFEXITED(wstatus)) return std::unexpected(wstatus);

    return WEXITSTATUS(wstatus);
}

std::expected<int, int> exec(const std::string& cmd, const std::vector<std::string>& args/* = {}*/, const Options& options/* = {}*/)
{
    return fork([&]() {
        return ::exec(cmd, args);
    }, options);
}

std::expected<int, int> sudo(const std::string& cmd, const std::vector<std::string>& args/* = {}*/, const Options& options/* = {}*/)
{
    if (geteuid() == 0) return exec(cmd, args, options);
    //else
    std::vector<std::string> sudo_args = {cmd};
    for (const auto& arg:args) {
        sudo_args.push_back(arg);
    }
    return exec("sudo", sudo_args, options);
}

void ENSURE_OK(std::expected<int, int> result)
{
    if (result) {
        if (result.value() == 0) return;
        //else
        throw std::runtime_error("Subprocess exited with status code " + std::to_string(result.value()));
    }
    //else
    auto unexpected = result.error();
    if (WIFSIGNALED(unexpected)) {
        throw std::runtime_error("Subprocess was terminated by signal " + std::to_string((int)WTERMSIG(unexpected)) + ".");
    }
    //else
    throw std::runtime_error("Subprocess terminated abnormally.");
}

} // namespace piped_subprocess
