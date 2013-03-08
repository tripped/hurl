#include <iostream>
#include <fstream>
#include <vector>

#include "hurl.h"

namespace hurl {
    namespace detail {
        std::string gzip(std::string const&);
        std::string gunzip(std::string const&);
    }
}

std::string readfile(std::string const& name)
{
    std::ifstream f(name.c_str());
    f.seekg(0, std::ifstream::end);
    size_t size = f.tellg();
    f.seekg(0);
    std::vector<char> buf(size);
    f.read(&buf.front(), size);
    return std::string(&buf.front(), size);
}

int main(int argc, char** argv)
{
    using namespace hurl;
    using namespace hurl::detail;

    if (argc < 3) {
        std::cout << "usage: " << argv[0] << " <cmd> params\n";
        return 1;
    }
    try {
        std::string cmd(argv[1]);

        if (cmd == "get") {
            httpresponse result = get(argv[2]);
            std::cerr << "-----------------------\n";
            std::cerr << "STATUS = " << result.status << "\n";
            std::cerr << "-----------------------\n";
            std::cout << result.body;
        }
        else if (cmd == "post") {
            httpresponse result = post(argv[2], (argc > 3)? argv[3] : "");
            std::cerr << "-----------------------\n";
            std::cerr << "STATUS = " << result.status << "\n";
            std::cerr << "-----------------------\n";
            std::cout << result.body;
        }
        else if (cmd == "zip") {
            std::string src = readfile(argv[2]);
            std::string out = gzip(src);
            std::cerr << "Deflated " << src.size() << " bytes to " << out.size() << "\n";
            std::cout << out;
        }
        else if (cmd == "unzip") {
            std::string src = readfile(argv[2]);
            std::string out = gunzip(src);
            std::cerr << "Inflated " << src.size() << " bytes to " << out.size() << "\n";
            std::cout << out;
        }
        else {
            std::cerr << "Unrecognized command.\n";
            return 1;
        }
    }
    catch(std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }

    return 0;
}

