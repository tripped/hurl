#include <microhttpd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <map>
#include <string>
#include <iostream>
#include <exception>
#include <stdexcept>


// hork is a counterpart to hurl
namespace hork
{
    extern "C" int access_handler(void*, MHD_Connection*,
            const char*, const char*, const char*, const char*,
            size_t*, void**);

    typedef std::map<std::string, std::string> httpheaders;

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

        httpheaders headers;
        std::string url;
        std::string method;
        std::string version;
        std::string data;
    };

    //
    // A likewise simple model of an HTTP response
    //
    struct httpresponse
    {
        httpresponse(std::string const& body, int status = 200)
            : status(status), body(body)
        {
        }

        int status;
        httpheaders headers;
        std::string body;
    };

    //
    // A request handler is a function mapping a request to a response
    //
    typedef httpresponse (*requesthandler)(httprequest const&);

    //
    // Our very simple HTTP server model
    //
    class httpserver
    {
    public:
        explicit httpserver(int port)
            : port_(port), notfound_(NULL)
        {
            daemon_ = MHD_start_daemon(MHD_USE_THREAD_PER_CONNECTION,
                    port,
                    NULL,
                    NULL,
                    &access_handler,
                    static_cast<void*>(this),
                    MHD_OPTION_END);

            if (daemon_ == NULL)
                throw std::runtime_error("couldn't start http daemon");
        }

        ~httpserver()
        {
            MHD_stop_daemon(daemon_);
        }

        void add(std::string const& path, requesthandler handler)
        {
            handlers_[path] = handler;
        }

        void set404(requesthandler handler)
        {
            notfound_ = handler;
        }

        int handle(MHD_Connection* connection, httprequest const& request)
        {
            requesthandler handler = notfound_;

            if (handlers_.count(request.url))
                handler = handlers_[request.url];

            if (!handler)
                return MHD_NO;

            // Invoke the registered handler if one was found
            httpresponse response = handler(request);

            // Initialize response with copy of body
            MHD_Response* mhdr = MHD_create_response_from_data(
                    response.body.size(),
                    const_cast<char*>(response.body.data()),
                    MHD_NO,
                    MHD_YES);

            // Write headers
            for (httpheaders::const_iterator it = response.headers.begin();
                    it != response.headers.end(); ++it)
            {
                MHD_add_response_header(mhdr,
                        it->first.c_str(), it->second.c_str());
            }

            int ret = MHD_queue_response(connection,
                    response.status,
                    mhdr);
            MHD_destroy_response(mhdr);
            return ret;
        }

    private:
        int port_;
        MHD_Daemon* daemon_;
        std::map<std::string, requesthandler> handlers_;
        requesthandler notfound_;
    };



    extern "C" int access_handler(void* cls,
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
        {
            request->data.append(static_cast<const char*>(data), *data_size);
            *data_size -= *data_size;
            return MHD_YES;
        }

        // Hand off completed request for processing
        int result = server->handle(connection, *request);

        // Closing this request, clear context
        delete request;
        *context = NULL;

        return result;
    }
}



using namespace hork;

httpresponse root(httprequest const& request)
{
    return httpresponse("<h1>HELLO</h1>");
}

httpresponse notfound(httprequest const& request)
{
    return httpresponse("The document you requested, '" + request.url + "', "
                        "has been removed for your convenience.", 404);
}


int main(int argc, char ** argv)
{
    if (argc != 2)
    {
        printf("usage: %s <port>\n", argv[0]);
        return 1;
    }

    httpserver server(atoi(argv[1]));

    server.add("/", root);
    server.set404(notfound);

    std::cout << "Press Enter to terminate server...\n";
    fgetc(stdin);

    return 0;
}
