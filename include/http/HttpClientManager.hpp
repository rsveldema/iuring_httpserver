#pragma once

#include <memory>
#include <string>

#include <slogger/ILogger.hpp>

#include <iuring/ISocketFactory.hpp>

#include "HttpClient.hpp"
#include "URL.hpp"

#include "HttpMethod.hpp"

namespace http
{
class HttpClientManager
{
public:
    HttpClientManager(logging::ILogger& logger,
        const std::shared_ptr<iuring::IOUringInterface>& io,
        iuring::ISocketFactory& socket_factory)
        : m_socket_factory(socket_factory)
        , m_logger(logger)
        , m_io(io)
    {
    }

    /** Send to url the request.
     * The result handler is called when the request completes or when there's
     * an error on the way.
     */
    void send_request(const URL& url, HttpMethod method,
        const std::string& payload,
        const IHttpClient::result_handler_t& result);

    void send_request(const iuring::IPAddress& ip, const std::string endpoint,
        HttpMethod method, const std::string& payload,
        const IHttpClient::result_handler_t& result);

private:
    iuring::ISocketFactory& m_socket_factory;
    logging::ILogger& m_logger;
    std::shared_ptr<iuring::IOUringInterface> m_io;
    std::vector<std::shared_ptr<http::HttpClient>> m_http_client_registration;

    std::shared_ptr<http::HttpClient> allocate_http_client(
        const iuring::IPAddress& ip);
};
} // namespace http