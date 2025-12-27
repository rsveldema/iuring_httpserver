#pragma once

#include <expected>
#include <format>
#include <memory>
#include <string>

#include <iuring/IOUringInterface.hpp>

#include <slogger/Error.hpp>
#include <slogger/ILogger.hpp>

namespace http
{
enum class Protocol
{
    HTTP,
    HTTPS,
    SAP
};

class URL
{
public:
    std::string to_string() const;

    static std::expected<URL, error::Error> parse(logging::ILogger& logger, const std::string& url_str);

    const std::string& hostname() const
    {
        return m_host;
    }

    const std::string& endpoint() const
    {
        return m_endpoint;
    }
    
    iuring::SocketPortID  port() const
    {
        return m_port;
    }
    
    Protocol protocol() const
    {
        return m_protocol;
    }

    URL(Protocol protocol,
        const std::string& host,
        std::optional<iuring::SocketPortID> port,
        const std::string& endpoint);

private:
    Protocol m_protocol;
    std::string m_host;
    iuring::SocketPortID m_port = iuring::SocketPortID::UNENCRYPTED_WEB_PORT;
    std::string m_endpoint;

    // ctor is private to force use of parse()
    URL() = default;
};
} // namespace http

// formatter for URL
template <> struct std::formatter<http::URL>
{
    constexpr auto parse(std::format_parse_context& ctx)
    {
        return ctx.begin(); // no special parsing needed
    }

    auto format(const http::URL& url, std::format_context& ctx) const
    {
        return std::format_to(ctx.out(), "{}", url.to_string());
    }
};