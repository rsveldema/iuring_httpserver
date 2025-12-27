#pragma once

#include <slogger/Error.hpp>
#include <slogger/Logger.hpp>

#include "HttpMethod.hpp"
#include "HttpStatusCodes.hpp"

#include <iuring/IOUringInterface.hpp>
#include <iuring/ISocketFactory.hpp>

#include "IHttpClient.hpp"

namespace http
{
class HttpClient : public IHttpClient
{
public:
    HttpClient(logging::ILogger& logger, const iuring::IPAddress& ip,
        const std::shared_ptr<iuring::IOUringInterface>& io,
        iuring::ISocketFactory& socket_factory)
        : m_socket_factory(socket_factory)
        , m_logger(logger)
        , m_ip(ip)
        , m_io(io)
    {
    }

    /** The http client instance is passed around in
     * various callbacks. This means that it should not be copied
     * by value.
     */
    HttpClient(const HttpClient&) = delete;
    void operator=(const HttpClient&) = delete;
    HttpClient(HttpClient&&) = delete;
    void operator=(HttpClient&&) = delete;

    ~HttpClient()
    {
        assert(m_valid == VALID_MASK);
        m_valid = 0xbaafaad;
    }

    void restart(const iuring::IPAddress& ip)
    {
        assert(m_can_be_recycled);
        m_can_be_recycled = false;
        m_socket = nullptr;
        m_ip = ip;
    }

    void mark_dead()
    {
        m_can_be_recycled = true;
    }

    bool can_be_recycled() const
    {
        return m_can_be_recycled;
    }

    error::Error send_request(const std::string& endpoint, HttpMethod method,
        const std::string& payload, const result_handler_t& result) override;

private:
    iuring::ISocketFactory& m_socket_factory;
    logging::ILogger& m_logger;
    iuring::IPAddress m_ip;
    std::shared_ptr<iuring::ISocket> m_socket;
    std::shared_ptr<iuring::IOUringInterface> m_io;
    uint32_t m_valid = VALID_MASK;
    bool m_can_be_recycled = false;

    static constexpr uint32_t VALID_MASK = 0xdeadbeed;

    logging::ILogger& get_logger()
    {
        return m_logger;
    }

    void send_request_connected(const std::string& endpoint, HttpMethod method,
        const std::string& payload, const result_handler_t& result);
};

} // namespace http