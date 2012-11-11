#include "hurl.h"

#include <fstream>
#include <sstream>
#include <exception>
#include <stdexcept>

extern "C"
{
#include <curl/curl.h>
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

        size_t streamfunc(void* ptr, size_t size, size_t nmemb, std::ostream* out)
        {
            (*out).write(static_cast<char*>(ptr), size * nmemb);
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
                           std::ostream &       out,
                           std::string const&   url)
        {
            curl.reset();
            curl.setopt(CURLOPT_URL, url.c_str());
            curl.setopt(CURLOPT_NOPROGRESS, 1);
            curl.setopt(CURLOPT_WRITEFUNCTION, &streamfunc);
            curl.setopt(CURLOPT_WRITEDATA, &out);
            curl.setopt(CURLOPT_COOKIEFILE, ""); // turns on cookie engine
        }

        void prepare_post(handle&               curl,
                          const void*           data,
                          size_t                size)
        {
            curl.setopt(CURLOPT_POST, 1);
            curl.setopt(CURLOPT_POSTFIELDS, data);
            curl.setopt(CURLOPT_POSTFIELDSIZE, size);
        }


        httpresponse get(handle&                curl,
                         std::string const&     url)
        {
            httpresponse result;
            std::ostringstream ss;
            prepare_basic(curl, ss, url);
            curl.perform();
            curl.getinfo(CURLINFO_RESPONSE_CODE, &result.status);
            // Copy the stream buffer into the response
            result.body.assign(ss.str());
            return result;
        }

        httpresponse post(handle&               curl,
                          std::string const&    url,
                          std::string const&    data)
        {
            httpresponse result;
            std::ostringstream ss;
            prepare_basic(curl, ss, url);
            prepare_post(curl, data.data(), data.size());
            curl.perform();
            curl.getinfo(CURLINFO_RESPONSE_CODE, &result.status);
            result.body.assign(ss.str());
            return result;
        }

        httpresponse download(handle&           curl,
                        std::string const&      url,
                        std::string const&      localpath)
        {
            httpresponse result;
            std::ofstream out(localpath.c_str(), std::ios::out |
                                                 std::ios::binary |
                                                 std::ios::trunc);
            prepare_basic(curl, out, url);
            curl.perform();
            curl.getinfo(CURLINFO_RESPONSE_CODE, &result.status);
            return result;
        }
    }

    //
    // Implementations for the GET/POST free functions
    //
    httpresponse get(std::string const& url)
    {
        detail::handle curl;
        return detail::get(curl, url);
    }

    httpresponse get(std::string const& url, httpparams const& params)
    {
        detail::handle curl;
        return detail::get(curl, detail::query(url, params));
    }

    httpresponse post(std::string const& url, std::string const& data)
    {
        detail::handle curl;
        return detail::post(curl, url, data);
    }

    httpresponse post(std::string const& url, httpparams const& params)
    {
        detail::handle curl;
        return detail::post(curl, url, detail::serialize(params));
    }

    httpresponse download(std::string const& url, std::string const& localpath)
    {
        detail::handle curl;
        return detail::download(curl, url, localpath);
    }


    //
    // client class implementation
    //
    class client::impl
    {
    public:
        impl(std::string const& baseurl)
            : base_(baseurl)
        {
        }

        detail::handle handle_;
        std::string base_;
    };

    client::client(std::string const& baseurl)
        : impl_(new impl(baseurl))
    {
    }

    client::~client()
    {
        // This destructor is empty but vital! Without it, auto_ptr cannot
        // generate a call to impl's destructor. For an interesting look
        // at this and other PIMPL issues, see Herb Sutter's GOTW #100.
        // (http://herbsutter.com/gotw/_100)
    }

    httpresponse client::get(std::string const& path)
    {
        return detail::get(impl_->handle_, impl_->base_ + path);
    }

    httpresponse client::get(std::string const& path, httpparams const& params)
    {
        return detail::get(impl_->handle_,
                           detail::query(impl_->base_ + path, params));
    }

    httpresponse client::post(std::string const& path, std::string const& data)
    {
        return detail::post(impl_->handle_,
                            impl_->base_ + path,
                            data);
    }

    httpresponse client::post(std::string const& path, httpparams const& params)
    {
        return detail::post(impl_->handle_,
                            impl_->base_ + path,
                            detail::serialize(params));
    }
}

