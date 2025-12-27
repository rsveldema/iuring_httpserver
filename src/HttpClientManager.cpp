#include <http/HttpClientManager.hpp>

namespace http
{
std::shared_ptr<http::HttpClient> HttpClientManager::allocate_http_client(
    const iuring::IPAddress& ip)
{
    for (auto it : m_http_client_registration)
    {
        if (it->can_be_recycled())
        {
            it->restart(ip);
            return it;
        }
    }

    return m_http_client_registration.emplace_back(
        std::make_shared<http::HttpClient>(m_logger, ip, m_io, m_socket_factory));
}


void HttpClientManager::send_request(const iuring::IPAddress& ip, const std::string endpoint, HttpMethod method,
        const std::string& payload,
        const IHttpClient::result_handler_t& result)
{
    auto client = allocate_http_client(ip);
    client->send_request(endpoint, method, payload, result);
}

void HttpClientManager::send_request(const URL& url,
    HttpMethod method, const std::string& payload,
    const IHttpClient::result_handler_t& result)
{
    LOG_INFO(m_logger, "resolving hostname: {}", url.hostname());

    m_io->resolve_hostname(
        url.hostname(), [this, method, payload,
            result, url](const iuring::IOUringInterface::resolve_hostname_arg_t& host_resolved) {

            if (!host_resolved)
            {
                LOG_ERROR(m_logger, "Hostname resolution failed: {}",
                    static_cast<int>(host_resolved.error()));
                result({ .status_code = StatusCode::SERVICE_UNAVAILABLE,
                    .payload = "" });
                return;
            }

            if (host_resolved.value().empty())
            {
                LOG_ERROR(m_logger, "Hostname resolution returned no addresses");
                result({ .status_code = StatusCode::SERVICE_UNAVAILABLE,
                    .payload = "" });
                return;
            }

            const auto ip = host_resolved.value().front();

            iuring::IPAddress new_ip(ip);
            new_ip.set_port( url.port());

            send_request(new_ip, url.endpoint(), method, payload, result);
        });
}


} // namespace http