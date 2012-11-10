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


        void prepare_basic(CURL*                curl,
                           httpresponse &       response,
                           std::string const&   url)
        {
            curl_easy_reset(curl);
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &writefunc);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        }

        void prepare_post(CURL*                 curl,
                          const void*           data,
                          size_t                size)
        {
            curl_easy_setopt(curl, CURLOPT_POST, 1);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, size);
        }


        httpresponse get(CURL*                  curl,
                         std::string const&     url)
        {
            httpresponse result;
            prepare_basic(curl, result, url);

            if (CURLE_OK != curl_easy_perform(curl))
                throw std::runtime_error("curl_easy_perform failed");

            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &result.status);

            return result;
        }

        httpresponse post(CURL*                 curl,
                          std::string const&    url,
                          std::string const&    data)
        {
            httpresponse result;
            prepare_basic(curl, result, url);
            prepare_post(curl, data.data(), data.size());

            if (CURLE_OK != curl_easy_perform(curl))
                throw std::runtime_error("curl_easy_perform failed");

            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &result.status);

            return result;
        }
    }

    httpresponse get(std::string const& url)
    {
        // TODO: these aren't exception-safe!
        CURL* curl = curl_easy_init();
        httpresponse result = detail::get(curl, url);
        curl_easy_cleanup(curl);
        return result;
    }

    httpresponse get(std::string const& url, httpparams const& params)
    {
        CURL* curl = curl_easy_init();

        std::string query(url);
        if (!params.empty())
            query.append("?" + detail::serialize(params));

        httpresponse result = detail::get(curl, query);

        curl_easy_cleanup(curl);
        return result;
    }

    httpresponse post(std::string const& url, std::string const& data)
    {
        CURL* curl = curl_easy_init();
        httpresponse result = detail::post(curl, url, data);
        curl_easy_cleanup(curl);
        return result;
    }

    httpresponse post(std::string const& url, httpparams const& params)
    {
        CURL* curl = curl_easy_init();
        httpresponse result = detail::post(curl, url, detail::serialize(params));
        curl_easy_cleanup(curl);
        return result;
    }
}

