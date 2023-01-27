#include <unistd.h>
#include <iostream>
#include "../piped_subprocess.h"

int main()
{
    piped_subprocess::exec("sed", {"s/world/WORLD/i"}, {
        feeder: std::string("Hello, World!\n")
    });

    piped_subprocess::exec("sed", {"s/ninja/NINJA/i"}, {
        feeder: "Ninja nande!?!?\n"
    });

    return 0;
}