#include "hurl.h"

#include <iostream>
#include <locale>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <exception>
#include <stdexcept>

extern "C"
{
#include <zlib.h>
#include <curl/curl.h>
#include <fcntl.h>
#include <libtar.h>
}

namespace hurl
{
    timeout::timeout()
        : std::runtime_error(curl_easy_strerror(CURLE_OPERATION_TIMEDOUT))
    { }

    resolve_error::resolve_error()
        : std::runtime_error(curl_easy_strerror(CURLE_COULDNT_RESOLVE_HOST))
    { }

    connect_error::connect_error()
        : std::runtime_error(curl_easy_strerror(CURLE_COULDNT_CONNECT))
    { }

    curl_error::curl_error(int code)
        : std::runtime_error(curl_easy_strerror(static_cast<CURLcode>(code))), code_(code)
    { }

    int curl_error::code() const
    {
        return code_;
    }


    namespace detail
    {
        // Ensure that curl_global_init gets called at program startup,
        // and that curl_global_cleanup is called before exit
        static struct ensure_init
        {
            ensure_init()
            {
                curl_global_init(CURL_GLOBAL_ALL);
            }
            ~ensure_init()
            {
                curl_global_cleanup();
            }
        } moo;

        class handle
        {
        public:
            handle()
                : handle_(curl_easy_init())
            {
                if (handle_ == NULL)
                    throw std::runtime_error("curl_easy_init failed");
            }

            ~handle()
            {
                curl_easy_cleanup(handle_);
            }

            void perform()
            {
                int code = curl_easy_perform(handle_);
                if (CURLE_OK == code)
                    return;
                if (CURLE_OPERATION_TIMEDOUT == code)
                    throw timeout();
                if (CURLE_COULDNT_RESOLVE_HOST == code)
                    throw resolve_error();
                if (CURLE_COULDNT_CONNECT == code)
                    throw connect_error();
                else
                    throw curl_error(code);
            }

            void reset()
            {
                curl_easy_reset(handle_);
            }

            template<typename T, typename U>
            void setopt(T option, U value)
            {
                int code = curl_easy_setopt(handle_, option, value);
                if (CURLE_OK != code)
                    throw curl_error(code);
            }

            template<typename T, typename U>
            void getinfo(T info, U* ret)
            {
                int code = curl_easy_getinfo(handle_, info, ret);
                if (CURLE_OK != code)
                    throw curl_error(code);
            }

            CURL* get() const
            {
                return handle_;
            }

        private:
            CURL* handle_;
        };

        // Why are these not in the standard library?
        inline std::string ltrim(std::string const& s)
        {
            using namespace std;
            return string(
                find_if(s.begin(), s.end(), not1(ptr_fun<int, int>(isspace))),
                s.end());
        }

        inline std::string rtrim(std::string const& s)
        {
            using namespace std;
            return string(
                s.begin(),
                find_if(s.rbegin(), s.rend(), not1(ptr_fun<int, int>(isspace))).base());
        }

        inline std::string trim(std::string const& s)
        {
            return ltrim(rtrim(s));
        }

        //
        // gzip compression support
        //
        std::string gzip(std::string const& input)
        {
            z_stream stream;
            int err;

            unsigned long sourceLen = input.size();
            unsigned char* source = (unsigned char*)input.data();
            stream.next_in = source;
            stream.avail_in = sourceLen;

            stream.zalloc = Z_NULL;
            stream.zfree = Z_NULL;
            stream.opaque = Z_NULL;

            if (Z_OK != deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, MAX_WBITS+16,
                    MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY))
            {
                throw std::runtime_error("error initializing deflate");
            }

            // Add 12 for old versions of zlib that don't correctly include header size
            unsigned long destLen = 12 + deflateBound(&stream, sourceLen);
            unsigned char* dest = new unsigned char[destLen];

            stream.next_out = dest;
            stream.avail_out = destLen;

            if (Z_STREAM_END != deflate(&stream, Z_FINISH))
            {
                throw std::runtime_error("failed to completely deflate");
            }

            std::string result((const char*)dest, (size_t)stream.total_out);
            deflateEnd(&stream);
            delete[] dest;
            return result;
        }

        extern "C" size_t streamfunc(void* ptr, size_t size, size_t nmemb, std::ostream* out)
        {
            (*out).write(static_cast<char*>(ptr), size * nmemb);
            return size * nmemb;
        }

        extern "C" size_t headerfunc(void* ptr, size_t size, size_t nmemb, httpresponse* resp)
        {
            std::string header(static_cast<const char*>(ptr), size * nmemb);

            // Per RFC 2616, each header line consists of a token followed
            // by a ':' and then a value, preceded by any amount of leading
            // whitespace.
            size_t cpos = header.find(':');
            if (cpos != std::string::npos)
            {
                std::string name = header.substr(0, cpos);
                std::string value = trim(header.substr(cpos+1));
                resp->headers[name] = value;
            }

            return size * nmemb;
        }

        std::string serialize(httpparams const& params)
        {
            // Serialize HTTP params in a URL-encoded form appropriate
            // for a query string or POST-fields request body.
            std::stringstream ss;
            for (httpparams::const_iterator it = params.begin();
                    it != params.end(); ++it)
            {
                if (it != params.begin())
                    ss << "&";
                char* name = curl_easy_escape(NULL, it->first.c_str(), it->first.size());
                char* value = curl_easy_escape(NULL, it->second.c_str(), it->second.size());
                ss << name << "=" << value;
                curl_free(name);
                curl_free(value);
            }
            return ss.str();
        }

        std::string query(std::string const& url, httpparams const& params)
        {
            return url + "?" + serialize(params);
        }


        void prepare_basic(handle&              curl,
                           httpresponse &       resp,
                           std::ostream &       out,
                           std::string const&   url,
                           int                  timeout)
        {
            curl.reset();
            curl.setopt(CURLOPT_URL, url.c_str());
            curl.setopt(CURLOPT_NOSIGNAL, 1);
            curl.setopt(CURLOPT_NOPROGRESS, 1);
            curl.setopt(CURLOPT_WRITEFUNCTION, &streamfunc);
            curl.setopt(CURLOPT_WRITEDATA, &out);
            curl.setopt(CURLOPT_HEADERFUNCTION, &headerfunc);
            curl.setopt(CURLOPT_HEADERDATA, &resp);
            curl.setopt(CURLOPT_COOKIEFILE, ""); // turns on cookie engine
            curl.setopt(CURLOPT_TIMEOUT, timeout);
        }

        void prepare_post(handle&               curl,
                          const void*           data,
                          size_t                size,
                          bool                  compressed = false)
        {
            curl.setopt(CURLOPT_POST, 1);
            curl.setopt(CURLOPT_POSTFIELDS, data);
            curl.setopt(CURLOPT_POSTFIELDSIZE, size);

            // In keeping with hurl's "do the wrong thing easily"
            // philosophy, disable "Expect: 100-continue" header
            static curl_slist *disable_expect = curl_slist_append(NULL, "Expect:");
            curl.setopt(CURLOPT_HTTPHEADER, disable_expect);

            // Include appropriate content-encoding with compressed POST data
            static curl_slist *enable_gzip = curl_slist_append(NULL, "Content-encoding: gzip");
            static curl_slist *disable_gzip = curl_slist_append(NULL, "Content-encoding:");
            curl.setopt(CURLOPT_HTTPHEADER, compressed? enable_gzip : disable_gzip);
        }


        httpresponse get(handle&                curl,
                         std::string const&     url,
                         int                    timeout)
        {
            httpresponse result;
            std::ostringstream ss;
            prepare_basic(curl, result, ss, url, timeout);
            curl.perform();
            curl.getinfo(CURLINFO_RESPONSE_CODE, &result.status);
            // Copy the stream buffer into the response
            result.body.assign(ss.str());
            return result;
        }

        httpresponse post(handle&               curl,
                          std::string const&    url,
                          std::string           data,
                          int                   timeout)
        {
            httpresponse result;
            std::ostringstream ss;
            prepare_basic(curl, result, ss, url, timeout);

            // TEMP: apply gzip compression to request data over 10KB
            bool compressed = false;
            if (data.size() > 10240)
            {
                data = gzip(data);
                compressed = true;
            }

            prepare_post(curl, data.data(), data.size(), compressed);
            curl.perform();
            curl.getinfo(CURLINFO_RESPONSE_CODE, &result.status);
            result.body.assign(ss.str());
            return result;
        }

        httpresponse download(handle&           curl,
                        std::string const&      url,
                        std::string const&      localpath,
                        int                     timeout)
        {
            httpresponse result;
            std::ofstream out(localpath.c_str(), std::ios::out |
                                                 std::ios::binary |
                                                 std::ios::trunc);
            prepare_basic(curl, result, out, url, timeout);
            curl.perform();
            curl.getinfo(CURLINFO_RESPONSE_CODE, &result.status);
            return result;
        }
    }

    namespace ext
    {
        void extract_tarball(std::string const& file, std::string const& extractdir)
        {
            TAR* t;

            // libtar asks for a char*? Bad libtar! Bad! No biscuit.
            if (tar_open(&t, const_cast<char*>(file.c_str()), NULL, O_RDONLY, 0777, TAR_GNU))
                throw std::runtime_error("could not open tar");

            if (tar_extract_all(t, const_cast<char*>(extractdir.c_str())))
                throw std::runtime_error("could not extract tar");

            tar_close(t);
        }
    }

    //
    // Implementations for the GET/POST free functions
    //
    httpresponse get(std::string const& url, int timeout)
    {
        detail::handle curl;
        return detail::get(curl, url, timeout);
    }

    httpresponse get(std::string const& url, httpparams const& params, int timeout)
    {
        detail::handle curl;
        return detail::get(curl, detail::query(url, params), timeout);
    }

    httpresponse post(std::string const& url, std::string const& data, int timeout)
    {
        detail::handle curl;
        return detail::post(curl, url, data, timeout);
    }

    httpresponse post(std::string const& url, httpparams const& params, int timeout)
    {
        detail::handle curl;
        return detail::post(curl, url, detail::serialize(params), timeout);
    }

    httpresponse download(std::string const& url, std::string const& localpath, int timeout)
    {
        detail::handle curl;
        return detail::download(curl, url, localpath, timeout);
    }

    httpresponse downloadtarball(std::string const& url,
                                 std::string const& localpath,
                                 std::string const& extractdir,
                                 int                timeout)
    {
        httpresponse result = download(url, localpath, timeout);
        if (result.status == 200)
            ext::extract_tarball(localpath, extractdir);
        return result;
    }


    //
    // client class implementation
    //
    class client::impl
    {
    public:
        impl(std::string const& baseurl, int timeout)
            : base_(baseurl), timeout_(timeout)
        {
        }

        detail::handle handle_;
        std::string base_;
        int timeout_;
    };

    client::client(std::string const& baseurl, int timeout)
        : impl_(new impl(baseurl, timeout))
    {
    }

    client::~client()
    {
        // This destructor is empty but vital! Without it, auto_ptr cannot
        // generate a call to impl's destructor. For an interesting look
        // at this and other PIMPL issues, see Herb Sutter's GOTW #100.
        // (http://herbsutter.com/gotw/_100)
    }

    std::string client::cookie() const
    {
        curl_slist* list = NULL;
        impl_->handle_.getinfo(CURLINFO_COOKIELIST, &list);
        std::ostringstream result;
        while(list)
        {
            result << list->data << "\n";
            list = list->next;
        }
        curl_slist_free_all(list);
        return result.str();
    }

    void client::setcookie(std::string const& data)
    {
        impl_->handle_.setopt(CURLOPT_COOKIELIST, "ALL");
        std::istringstream ss(data);
        std::string line;
        while (!ss.eof())
        {
            std::getline(ss, line);
            impl_->handle_.setopt(CURLOPT_COOKIELIST, line.c_str());
        }
    }

    httpresponse client::get(std::string const& path)
    {
        return detail::get(impl_->handle_, impl_->base_ + path, impl_->timeout_);
    }

    httpresponse client::get(std::string const& path, httpparams const& params)
    {
        return detail::get(impl_->handle_,
                           detail::query(impl_->base_ + path, params), impl_->timeout_);
    }

    httpresponse client::post(std::string const& path, std::string const& data)
    {
        return detail::post(impl_->handle_,
                            impl_->base_ + path,
                            data,
                            impl_->timeout_);
    }

    httpresponse client::post(std::string const& path, httpparams const& params)
    {
        return detail::post(impl_->handle_,
                            impl_->base_ + path,
                            detail::serialize(params),
                            impl_->timeout_);
    }

    httpresponse client::download(std::string const& path,
                                  std::string const& localpath)
    {
        return detail::download(impl_->handle_,
                                impl_->base_ + path,
                                localpath,
                                impl_->timeout_);
    }

    httpresponse client::downloadtarball(std::string const& path,
                                    std::string const& localpath,
                                    std::string const& extractdir)
    {
        httpresponse result = download(path, localpath);
        if (result.status == 200)
            ext::extract_tarball(localpath, extractdir);
        return result;
    }
}

