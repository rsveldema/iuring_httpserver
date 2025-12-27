#pragma once

#include <string>
#include <functional>

#include <slogger/Error.hpp>

#include "IHttpClient.hpp"
#include "HttpStatusCodes.hpp"
#include "HttpMethod.hpp"

namespace http
{
    struct HttpResult
    {
        StatusCode status_code;
        std::string payload;
    };

    class IHttpClient
    {
    public:
        virtual ~IHttpClient() = default;

        using result_handler_t = std::function<void(const HttpResult& result)>;

        virtual error::Error send_request(const std::string& endpoint,
            HttpMethod method,
            const std::string& payload,
            const result_handler_t& result) = 0;
    };
  
}