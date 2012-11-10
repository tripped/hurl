#include <iostream>


#include <map>
#include <string>
#include <sstream>
#include <exception>
#include <stdexcept>

extern "C"
{
#include <curl/curl.h>
}

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
    // get
    //
    // Submit an HTTP GET request with given parameters.
    //------------------------------------------------------------------------
    httpresponse get(std::string const& url, httpparams const& params = httpparams());

    //------------------------------------------------------------------------
    // post
    //
    // Submit an HTTP POST request with given parameters.
    //------------------------------------------------------------------------
    httpresponse post(std::string const& url, httpparams const& params = httpparams());



    // TODO: cut along the dotted line
    //........................................................................

    namespace detail
    {
        size_t writefunc(void* ptr, size_t size, size_t nmemb, httpresponse* resp)
        {
            resp->body.append(static_cast<char*>(ptr), size * nmemb);
            return size * nmemb;
        }


        // Serialize HTTP params into a URL-encoded form appropriate for
        // a query string or a POST body.
        std::string serialize(httpparams const& params)
        {
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

        httpresponse get(CURL*                  curl,
                         std::string const&     url,
                         httpparams const&      params = httpparams())
        {
            httpresponse result;
            if (curl == NULL)
                throw std::runtime_error("get called with null CURL handle");

            std::string query(url);
            if (!params.empty())
                query.append("?" + serialize(params));

            curl_easy_setopt(curl, CURLOPT_URL, query.c_str());
            curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &writefunc);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);

            if (CURLE_OK != curl_easy_perform(curl))
                throw std::runtime_error("curl_easy_perform failed");

            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &result.status);

            return result;
        }


        httpresponse post(CURL*                 curl,
                          std::string const&    url,
                          httpparams const&     params = httpparams())
        {
            httpresponse result;
            if (curl == NULL)
                throw std::runtime_error("post called with null CURL handle");

            std::string fields = serialize(params);

            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &writefunc);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);
            curl_easy_setopt(curl, CURLOPT_POST, 1);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, fields.data());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, fields.size());

            if (CURLE_OK != curl_easy_perform(curl))
                throw std::runtime_error("curl_easy_perform failed");

            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &result.status);

            return result;
        }
    }

    httpresponse get(std::string const& url, httpparams const& params)
    {
        // TODO: this isn't exception-safe!
        CURL* curl = curl_easy_init();
        httpresponse result = detail::get(curl, url, params);
        curl_easy_cleanup(curl);
        return result;
    }

    httpresponse post(std::string const& url, httpparams const& params)
    {
        CURL* curl = curl_easy_init();
        httpresponse result = detail::post(curl, url, params);
        curl_easy_cleanup(curl);
        return result;
    }
}

int main()
{
    std::cout << "Testing hurl...\n";

    hurl::httpresponse result = hurl::get("http://google.com");
    std::cout << "STATUS: " << result.status << "\n";
    std::cout << "BODY: " << result.body << ">>>END\n";

    return 0;
}

