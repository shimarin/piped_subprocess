#include "../piped_subprocess.h"

int main()
{
    return piped_subprocess::sudo(
        "cat", {"/etc/shadow"}
    ).value_or(-1);
}