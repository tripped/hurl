#include <iostream>

#include "hurl.h"

int main()
{
    std::cout << "Testing hurl...\n";

    using namespace hurl;

    try
    {
        httpresponse result = get("http://google.com");
        std::cout << "STATUS: " << result.status << "\n";
        std::cout << "BODY: " << result.body << "\n";
    }
    catch(std::exception& e)
    {
        std::cout << e.what() << "\n";
    }

    return 0;
}

