#include "hurl.h"

#include <sstream>
#include <exception>
#include <stdexcept>

extern "C"
{
#include <curl/curl.h>
}

namespace hurl
{
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
                if (CURLE_OK != curl_easy_perform(handle_))
                    throw std::runtime_error("curl_easy_perform failed");
            }

            void reset()
            {
                curl_easy_reset(handle_);
            }

            template<typename T, typename U>
            void setopt(T option, U value)
            {
                if (CURLE_OK != curl_easy_setopt(handle_, option, value))
                    throw std::runtime_error("curl_easy_setopt failed");
            }

            template<typename T, typename U>
            void getinfo(T info, U* ret)
            {
                if (CURLE_OK != curl_easy_getinfo(handle_, info, ret))
                    throw std::runtime_error("curl_easy_getinfo failed");
            }

            CURL* get() const
            {
                return handle_;
            }

        private:
            CURL* handle_;
        };

        size_t writefunc(void* ptr, size_t size, size_t nmemb, httpresponse* resp)
        {
            resp->body.append(static_cast<char*>(ptr), size * nmemb);
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


        void prepare_basic(handle&              curl,
                           httpresponse &       response,
                           std::string const&   url)
        {
            curl.reset();
            curl.setopt(CURLOPT_URL, url.c_str());
            curl.setopt(CURLOPT_NOPROGRESS, 1);
            curl.setopt(CURLOPT_WRITEFUNCTION, &writefunc);
            curl.setopt(CURLOPT_WRITEDATA, &response);
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
            prepare_basic(curl, result, url);
            curl.perform();
            curl.getinfo(CURLINFO_RESPONSE_CODE, &result.status);
            return result;
        }

        httpresponse post(handle&               curl,
                          std::string const&    url,
                          std::string const&    data)
        {
            httpresponse result;
            prepare_basic(curl, result, url);
            prepare_post(curl, data.data(), data.size());
            curl.perform();
            curl.getinfo(CURLINFO_RESPONSE_CODE, &result.status);
            return result;
        }
    }

    httpresponse get(std::string const& url)
    {
        detail::handle curl;
        return detail::get(curl, url);
    }

    httpresponse get(std::string const& url, httpparams const& params)
    {
        detail::handle curl;

        std::string query(url);
        if (!params.empty())
            query.append("?" + detail::serialize(params));

        return detail::get(curl, query);
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
}

