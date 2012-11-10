#pragma once

#include <string>
#include <map>

namespace hurl
{
    //------------------------------------------------------------------------
    // A response describes the result of a hurl HTTP request.
    //------------------------------------------------------------------------
    struct httpresponse
    {
        int status;
        std::string body;
        // TODO: headers, MIME types, etc.
    };

    typedef std::map<std::string,std::string> httpparams;


    //------------------------------------------------------------------------
    // get (string)
    //
    // Submit an HTTP GET request to the given URL.
    //
    //  url     The URL to retrieve.
    //
    httpresponse get(std::string const& url);

    //------------------------------------------------------------------------
    // get (string, httparams)
    //
    // Submit an HTTP GET request with given parameters.
    //
    //  url     The URL to retrieve. This URL should not contain any query
    //          parameters; those are provided via params.
    //
    //  params  A string->string dictionary specifying query parameters.
    //
    httpresponse get(std::string const& url, httpparams const& params);

    //------------------------------------------------------------------------
    // post (string, httpparams)
    //
    // Submit an HTTP POST request with given parameters.
    //
    //  url     The URL to POST to.
    //
    //  params  A string->string dictionary specifying POST field elements.
    //
    httpresponse post(std::string const& url,
                      httpparams const& params = httpparams());

    //------------------------------------------------------------------------
    // post (string, string)
    //
    // Submit an HTTP POST request with raw data.
    //
    httpresponse post(std::string const& url, std::string const& data);
}

