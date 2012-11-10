#include <iostream>

#include "hurl.h"

int main()
{
    std::cout << "Testing hurl...\n";

    hurl::httpresponse result = hurl::get("http://google.com");
    std::cout << "STATUS: " << result.status << "\n";
    std::cout << "BODY: " << result.body << ">>>END\n";

    return 0;
}

