#include "../piped_subprocess.h"

int main()
{
    piped_subprocess::exec("sed", {"s/receiver/RECEIVER/ig"}, {
        feeder: std::filesystem::path("piped_subprocess.h")
    });

    try {
        piped_subprocess::exec("sed", {"s/receiver/RECEIVER/ig"}, {
            feeder: std::filesystem::path("nosuchfile.h")
        });
    }
    catch (const std::runtime_error& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    return 0;
}