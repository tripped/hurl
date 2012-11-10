#pragma once

#include <map>
#include <string>
#include <memory>

namespace hurl
{
    //
    // A response describes the result of a hurl HTTP request.
    //
    struct httpresponse
    {
        int status;
        std::string body;
        // TODO: headers, MIME types, etc.
    };

    typedef std::map<std::string,std::string> httpparams;


    //
    // get (string)
    // Submit an HTTP GET request to the given URL.
    //
    //  url     The URL to retrieve.
    //
    httpresponse get(std::string const& url);

    //
    // get (string, httparams)
    // Submit an HTTP GET request with given parameters.
    //
    //  url     The URL to retrieve. This URL should not contain any query
    //          parameters; those are provided via params.
    //  params  A string->string dictionary specifying query parameters.
    //
    httpresponse get(std::string const& url, httpparams const& params);

    //
    // post (string, httpparams)
    //  Submit an HTTP POST request with given parameters. The parameters
    //  will be URL-encoded and serialized as standard form-data.
    //
    //  url     The URL to POST to.
    //  params  A string->string dictionary specifying POST field elements.
    //
    httpresponse post(std::string const& url, httpparams const& params);

    //
    // post (string, string)
    //  Submit an HTTP POST request with raw data.
    //
    //  url     The URL to POST to.
    //  data    String containing raw data to submit. Will not be
    //          URL-encoded or modified in any way.
    //
    httpresponse post(std::string const& url, std::string const& data);


    //
    // client
    //  A convenience class representing a client session, used to perform
    //  multiple requests to a service with a given base URL. Uses a single
    //  cURL handle, which has two important consequences:
    //
    //  1. Cookies are saved between requests.
    //  2. Requests made on the same client are NOT THREAD-SAFE.
    //
    //  The parameters for client's member functions are the same as those
    //  for the corresponding free functions above, except that the first
    //  parameter, path, is appended to the client's base URL to form the
    //  effective URL for the request.
    //
    //  E.g.,
    //
    //      get("http://example.com/foo");
    //
    //  is equivalent to:
    //
    //      client example("http://example.com");
    //      example.get("/foo");
    //
    //  But note that because of the shared session, subsequent requests
    //  made on the example client may have different results.
    //
    class client
    {
    public:
        explicit client(std::string const& baseurl);
        ~client();

        httpresponse get(std::string const& path);
        httpresponse get(std::string const& path, httpparams const& params);
        httpresponse post(std::string const& path, std::string const& data);
        httpresponse post(std::string const& path, httpparams const& params);

    private:
        class impl;
        std::auto_ptr<impl> impl_;

        // Noncopyable
        client(client const&);
        client& operator=(client const&);
    };
}

