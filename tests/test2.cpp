#include <unistd.h>
#include <iostream>
#include "../piped_subprocess.h"

int main()
{
    return piped_subprocess::exec("sed", {"s/world/WORLD/gi"}, {
        feeder: [](int fd) {
            write(fd, "Hello, World!\n", 14);
        },
        receiver: [](int fd) {
            int r;
            char buf[1024];
            while ((r = read(fd, buf, sizeof(buf))) > 0) {
                std::cout.write(buf, r);
            }
        }
    }).value_or(-1);
}