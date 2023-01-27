#include <nlohmann/json.hpp>
#include "../piped_subprocess.h"

int main()
{
    nlohmann::json json;
    piped_subprocess::ENSURE_OK(piped_subprocess::exec(
        "lsblk", {"-b", "-n", "-J", "-o", "NAME,MODEL,TYPE,RO,MOUNTPOINT,SIZE,TRAN,LOG-SEC,MAJ:MIN"}, {
        receiver: [&json](std::istream& in) {
            in >> json;
        }
    }));

    for (auto& blockdevice:json["blockdevices"]) {
        std::cout << "name=" << blockdevice["name"].get<std::string>() << ", model=" << blockdevice["model"] 
            << ", size=" << blockdevice["size"].get<uint64_t>() << std::endl;
    }

    return 0;
}