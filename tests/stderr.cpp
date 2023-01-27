#include <unistd.h>
#include <iostream>
#include "../piped_subprocess.h"

static std::string trim(const std::string& string)
{
    static const char* space_chars = " \t\v\r\n";
    std::string result;
    std::string::size_type left = string.find_first_not_of(space_chars);
    if (left != std::string::npos) {
        std::string::size_type right = string.find_last_not_of(space_chars);       
        result = string.substr(left, right - left + 1);
    }
    return result;
}

int main()
{
    std::string stderr_buf;
    auto rst = piped_subprocess::exec("ls", {"-l", "/NOSUCHFILEORDIRECTORY!!"}, {
        error_receiver: [&stderr_buf](int fd) {
            int r;
            char buf[1024];
            while ((r = read(fd, buf, sizeof(buf))) > 0) {
                stderr_buf.append(buf, r);
            }
        }
    });
    std::cout << "STDERR string is \"" << trim(stderr_buf) << "\"" << std::endl;

    return rst.value_or(-1);
}