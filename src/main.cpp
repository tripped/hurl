#include <iostream>

#include "hurl.h"

int main(int argc, char** argv)
{
    using namespace hurl;

    if (argc < 2)
    {
        std::cout << "usage: " << argv[0] << " url [localpath]\n";
        return 1;
    }

    try
    {
        httpresponse result;

        if (argc == 2)
            result = get(argv[1]);
        else
            result = download(argv[1], argv[2]);

        std::cout << "STATUS: " << result.status << "\n";
        std::cout << "BODY: " << result.body << "\n";
    }
    catch(std::exception& e)
    {
        std::cout << e.what() << "\n";
    }

    return 0;
}

