#include <fcntl.h>
#include "../piped_subprocess.h"

int main()
{
    piped_subprocess::fork([](){
            std::string line;
            std::cin >> line;
            std::cout << "hoge=" << line << std::endl;
            return 0;
        }, {
        feeder: [](std::ostream& out) {
            std::cerr << "feeder" << std::endl;
            out << "foo" << std::endl;
        },
        receiver: [](std::istream& in) {
            std::string line;
            in >> line;
            std::cout << "LINE=" << line << std::endl;
        }
    }).value_or(-1);

    piped_subprocess::exec("sed", {"s/foo/FOO/g"}, {
        feeder: [](std::ostream& out) {
            out << "hoge foo bar" << std::endl;
            out << "foo hoge bar" << std::endl;
        },
        receiver: [](std::istream& in) {
            while (!in.eof()) {
                char buf[16];
                in.read(buf, sizeof(buf));
                std::cout.write(buf, in.gcount());
            }
        }
    }).value_or(-1);

    piped_subprocess::exec("sed", {"s/feeder/FEEDER/g"}, {
        feeder: open("piped_subprocess.h", O_RDONLY),
        receiver: [](std::istream& in) {
            while (!in.eof()) {
                char buf[16];
                in.read(buf, sizeof(buf));
                std::cout.write(buf, in.gcount());
            }
        }
    }).value_or(-1);
}
