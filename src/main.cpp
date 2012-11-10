#include <iostream>

#include "hurl.h"

int main()
{
    std::cout << "Testing hurl...\n";

    using namespace hurl;

    httpresponse result = download("http://google.com", "google.html");

    std::cout << "STATUS: " << result.status << "\n";
    std::cout << "BODY: " << result.body << "\n";

    return 0;
}

