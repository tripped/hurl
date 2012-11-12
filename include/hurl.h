#pragma once

#include <map>
#include <string>
#include <memory>
#include <stdexcept>

namespace hurl
{
    typedef std::map<std::string,std::string> httpparams;
    typedef std::map<std::string,std::string> httpheaders;

    //
    // A response describes the result of a hurl HTTP request.
    //
    struct httpresponse
    {
        int status;
        httpheaders headers;
        std::string body;
    };


    //
    // Exceptions:
    //  Any of these may be thrown by the functions in this module that
    //  perform HTTP requests if certain problematic conditions arise.
    //
    class timeout : public std::runtime_error
    {
    public:
        timeout();
    };

    class resolve_error: public std::runtime_error
    {
    public:
        resolve_error();
    };

    class connect_error : public std::runtime_error
    {
    public:
        connect_error();
    };

    // A fallback exception for all other errors; code() returns the
    // underlying CURLcode and can be used for more information
    class curl_error : public std::runtime_error
    {
    public:
        explicit curl_error(int);
        int code() const;
    private:
        int code_;
    };


    //
    // get (string)
    //  Submit an HTTP GET request to the given URL.
    //
    //  url     The URL to retrieve.
    //  timeout Time, in seconds, to wait before failing. A timeout of 0
    //          means the request will never time out.
    //
    //          Failure takes the form of a thrown hurl::timeout exception.
    //
    httpresponse get            (std::string const&     url,
                                 int                    timeout = 0);

    //
    // get (string, httparams)
    //  Submit an HTTP GET request with given parameters.
    //
    //  url     The URL to retrieve. This URL should not contain any query
    //          parameters; those are provided via params.
    //  params  A string->string dictionary specifying query parameters.
    //  timeout Time, in seconds, to wait before failing
    //
    httpresponse get            (std::string const&     url,
                                 httpparams const&      params,
                                 int                    timeout = 0);

    //
    // post (string, httpparams)
    //  Submit an HTTP POST request with given parameters. The parameters
    //  will be URL-encoded and serialized as standard form-data.
    //
    //  url     The URL to POST to.
    //  params  A string->string dictionary specifying POST field elements.
    //  timeout Time, in seconds, to wait before failing
    //
    httpresponse post           (std::string const&     url,
                                 httpparams const&      params,
                                 int                    timeout = 0);

    //
    // post (string, string)
    //  Submit an HTTP POST request with raw data.
    //
    //  url     The URL to POST to.
    //  data    String containing raw data to submit. Will not be
    //          URL-encoded or modified in any way.
    //  timeout Time, in seconds, to wait before failing
    //
    httpresponse post           (std::string const&     url,
                                 std::string const&     data,
                                 int                    timeout = 0);

    //
    // download (string, string)
    //  Download a file via HTTP GET to the local filesystem. If a file with
    //  the specified path already exists and is writable, it will be
    //  truncated before the download begins, even if the server returns an
    //  error status.
    //
    //  url         The URL of the file to download
    //  localpath   Path in the local filesystem to save the file to
    //  timeout     Time, in seconds, to wait before failing
    //
    httpresponse download       (std::string const&     url,
                                 std::string const&     localpath,
                                 int                    timeout = 0);

    //
    // downloadtarball (string, string, string)
    //  Download a tar-encoded archive to the specified path and extract it
    //  in the specified directory. This function does not follow redirects,
    //  and if the server responds with any HTTP status besides 200 OK, it
    //  will not attempt to extract from the downloaded file.
    //
    //  url         The URL of the tarball to retrieve
    //  localpath   Path in local filesystem to save tarball to
    //  extractdir  Path in local filesystem to extract tarball contents to
    //  timeout     Time, in seconds, to wait before failing.
    //
    // NOTE:
    //  This is a temporary convenience function; it will probably be
    //  factored out of this module in a reasonable way soon.
    //
    httpresponse downloadtarball(std::string const& url,
                                 std::string const& localpath,
                                 std::string const& extractdir,
                                 int timeout = 0);

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
    //  Another difference is that the client member functions omit the
    //  timeout parameter; instead this is set in the client constructor
    //  and used for all requests.
    //
    class client
    {
    public:
        explicit client(std::string const& baseurl, int timeout = 0);
        ~client();

        httpresponse get        (std::string const&     path);

        httpresponse get        (std::string const&     path,
                                 httpparams const&      params);

        httpresponse post       (std::string const&     path,
                                 std::string const&     data);

        httpresponse post       (std::string const&     path,
                                 httpparams const&      params);

        httpresponse download   (std::string const&     path,
                                 std::string const&     localpath);

        httpresponse downloadtarball(std::string const& path,
                                 std::string const&     localpath,
                                 std::string const&     extractdir);

    private:
        class impl;
        std::auto_ptr<impl> impl_;

        // Noncopyable
        client(client const&);
        client& operator=(client const&);
    };
}

