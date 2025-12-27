#include <http/URL.hpp>

namespace http
{
URL::URL(Protocol protocol, const std::string& host,
    std::optional<iuring::SocketPortID> port, const std::string& endpoint)
    : m_protocol(protocol)
    , m_host(host)
    , m_port(port.value_or(iuring::SocketPortID::UNENCRYPTED_WEB_PORT))
    , m_endpoint(endpoint)
{
    if (!port.has_value())
    {
        switch (protocol)
        {
        case Protocol::HTTP:
            m_port = iuring::SocketPortID::UNENCRYPTED_WEB_PORT;
            break;
        case Protocol::HTTPS:
            m_port = iuring::SocketPortID::ENCRYPTED_WEB_PORT;
            break;
        case Protocol::SAP:
            m_port = iuring::SocketPortID::SAP_PORT_EVENT;
            break;
        }
    }
}
std::string URL::to_string() const
{
    std::string protocol_str =
        (m_protocol == Protocol::HTTP) ? "http" : "https";
    return std::format("{}://{}{}", protocol_str, m_host, m_endpoint);
}

std::expected<URL, error::Error> URL::parse(
    logging::ILogger& logger, const std::string& url_str)
{
    URL url;

    /** parses the following URLs:
     *
     *     http://hostname:port/endpoint
     *     https://hostname:port/endpoint
     *     https://hostname:port/endpoint
     *     https://hostname/endpoint
     *     sap://hostname:port/endpoint
     */

    size_t pos = 0;

    // Parse protocol
    size_t protocol_end = url_str.find("://", pos);
    if (protocol_end == std::string::npos)
    {
        LOG_ERROR(logger, "Invalid URL: missing protocol separator '://'");
        return std::unexpected(error::Error::BAD_PPROTOCOL);
    }

    std::string protocol_str = url_str.substr(pos, protocol_end - pos);
    if (protocol_str == "http")
    {
        url.m_protocol = Protocol::HTTP;
        url.m_port =
            iuring::SocketPortID::UNENCRYPTED_WEB_PORT; // default HTTP port
    }
    else if (protocol_str == "https")
    {
        url.m_protocol = Protocol::HTTPS;
        url.m_port =
            iuring::SocketPortID::ENCRYPTED_WEB_PORT; // default HTTPS port
    }
    else if (protocol_str == "sap")
    {
        url.m_protocol = Protocol::SAP;
        url.m_port = iuring::SocketPortID::SAP_PORT_EVENT; // default SAP port
    }
    else
    {
        LOG_ERROR(logger, "Unsupported protocol: {}", protocol_str);
        return std::unexpected(error::Error::BAD_PPROTOCOL);
    }

    pos = protocol_end + 3; // skip "://"

    // Parse hostname and optional port
    size_t endpoint_start = url_str.find('/', pos);
    std::string host_port;

    if (endpoint_start == std::string::npos)
    {
        // No endpoint, just hostname[:port]
        host_port = url_str.substr(pos);
        url.m_endpoint = "/";
    }
    else
    {
        host_port = url_str.substr(pos, endpoint_start - pos);
        url.m_endpoint = url_str.substr(endpoint_start);
    }

    // Check if port is specified
    size_t port_separator = host_port.find(':');
    if (port_separator != std::string::npos)
    {
        url.m_host = host_port.substr(0, port_separator);
        const auto port_str = host_port.substr(port_separator + 1);

        try
        {
            const auto p = std::stoi(port_str);
            if (p <= 0 || p > 65535)
            {
                LOG_ERROR(logger, "Invalid port number: {}", port_str);
                return std::unexpected(error::Error::RANGE);
            }
            url.m_port = static_cast<iuring::SocketPortID>(p);
        }
        catch (const std::exception& e)
        {
            LOG_ERROR(logger, "Exception parsing port: {}", e.what());
            return std::unexpected(error::Error::RANGE);
        }
    }
    else
    {
        url.m_host = host_port;
        // Port already set to default based on protocol
    }

    if (url.m_host.empty())
    {
        LOG_ERROR(logger, "Invalid URL: empty hostname");
        return std::unexpected(error::Error::RANGE);
    }

    return url;
}
} // namespace http