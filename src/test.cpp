#include <microhttpd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <iostream>
#include <exception>
#include <stdexcept>


// hork is a counterpart to hurl
namespace hork
{
    int request_handler(void*, MHD_Connection*,
            const char*, const char*, const char*, const char*,
            size_t*, void**);

    class httpserver
    {
        // woof!
        friend int request_handler(void*, MHD_Connection*,
                const char*, const char*, const char*, const char*,
                size_t*, void**);
    public:
        explicit httpserver(int port)
            : port_(port)
        {
            daemon_ = MHD_start_daemon(MHD_USE_THREAD_PER_CONNECTION,
                    port,
                    NULL,
                    NULL,
                    &request_handler,
                    static_cast<void*>(this),
                    MHD_OPTION_END);

            if (daemon_ == NULL)
                throw std::runtime_error("couldn't start http daemon");
        }

        ~httpserver()
        {
            MHD_stop_daemon(daemon_);
        }

    private:
        int port_;
        MHD_Daemon* daemon_;
    };

    //
    // A very simple model of an HTTP request. Because this is only for
    // testing purposes, we do the stupid thing and just accumulate POST
    // data in a buffer inside the request until it's all accounted for.
    //
    struct httprequest
    {
        httprequest(const char* url, const char* method, const char* version)
            : url(url), method(method), version(version)
        {
            std::cout << "> " << method << " " << url << " " << version << "\n";
        }

        enum state
        {
            HEADER_RECEIVED,
            DATA_INCOMING,
            FINISHED
        } state;

        std::string url;
        std::string method;
        std::string version;
        std::string data;
    };

    int request_handler(void* cls,
            MHD_Connection* connection,
            const char* url,
            const char* method,
            const char* version,
            const char* data,
            size_t* data_size,
            void** context)
    {
        httpserver* server = static_cast<httpserver*>(cls);
        httprequest* request = static_cast<httprequest*>(*context);

        if (request == NULL)
        {
            request = new httprequest(url, method, version);
            *context = request;
            return MHD_YES;
        }

        if (0 != *data_size)
            return MHD_NO;

        // Closing this request, clear context
        delete request;
        *context = NULL;

        const char* hello = "<html><h1>HELLO<h1></html>";

        MHD_Response* response = MHD_create_response_from_data(
                strlen(hello), const_cast<char*>(hello), MHD_NO, MHD_NO);

        int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);

        MHD_destroy_response(response);

        return ret;
    }
}


int main(int argc, char ** argv)
{
    if (argc != 2)
    {
        printf("usage: %s <port>\n", argv[0]);
        return 1;
    }

    hork::httpserver server(atoi(argv[1]));

    std::cout << "Press Enter to terminate server...\n";
    fgetc(stdin);

    return 0;
}
