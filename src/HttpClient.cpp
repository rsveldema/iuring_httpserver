#include "http/HttpClient.hpp"
#include "http/HttpParser.hpp"

namespace http
{
    error::Error HttpClient::send_request(const std::string& endpoint,
            HttpMethod method,
            const std::string& payload,
            const result_handler_t& result)
    {
        m_socket = m_socket_factory.create_impl(iuring::SocketType::IPV4_TCP,
            m_ip.get_port(), get_logger(), iuring::SocketKind::UNICAST_CLIENT_SOCKET);

        m_io->submit_connect(
            m_socket, m_ip,
            [this, method, payload, result, endpoint](const iuring::ConnectResult& connect_result) {
                const auto status = connect_result.to_expected();
                if (status.has_value() == false)
                {
                    HttpResult res;
                    res.status_code = StatusCode::NOT_REACHABLE;
                    LOG_ERROR(get_logger(),
                        "failed to connect to HTTP CLIENT at {}, error: {}",
                        m_ip.to_human_readable_string(),
                        static_cast<int>(status.error()));
                    res.payload = "";
                    result(res);
                    return;
                }
            
                const auto addr = status.value();

                LOG_INFO(get_logger(),
                    "connected to HTTP CLIENT at {}",
                    addr.to_human_readable_string());

                send_request_connected(endpoint, method, payload, result);
            });

        return error::Error::OK;
    }

    void HttpClient::send_request_connected(const std::string& endpoint,
            HttpMethod method,
            const std::string& payload,
            const result_handler_t& result)
    {
        assert(m_valid == VALID_MASK); // lightweight check that we've got no memory corruption...

        std::string data;
        data += StringUtils::to_upper( method_to_string(method) );
        data += " ";
        data += endpoint;
        data += " HTTP/1.1\r\n";
        data += "Host: localhost\r\n";
        data += "Content-Length: " + std::to_string(payload.size()) + "\r\n";
        data += "Application-Agent: Flex-Audio-Client/1.0\r\n";
        if (! payload.empty())
        {
            data += "Content-Type: application/json\r\n";
        }
        data += "Connection: close\r\n";
        data += "\r\n";
        data += payload;

        auto wi = m_io->ackuire_send_workitem(m_socket);

        wi->get_send_packet().append(
            reinterpret_cast<const uint8_t*>(data.data()), data.size());

        wi->submit_stream_data(
            [this, result=result](const iuring::SendResult& send_result) {
                auto status = send_result.to_expected();
                if (status.has_value() == false)
                {
                    HttpResult res;
                    res.status_code = StatusCode::NOT_REACHABLE;
                    LOG_ERROR(get_logger(),
                        "failed to send HTTP request, status: {}",
                        static_cast<int>(status.error()));
                    res.payload = "";
                    result(res);
                    return;
                }

                m_io->submit_recv(m_socket, [this, result=result](const iuring::ReceivedMessage& msg) {
                    LOG_DEBUG(get_logger(), "received: {}", msg.to_string());

                    HttpParser parser(get_logger());
                    parser.parse(msg.to_string());

                    result(HttpResult{
                        .status_code = parser.get_status_code(),
                        .payload = parser.get_payload(),
                    });

                    // todo: handle keep-alive, for now we just close the socket
                    m_io->submit_close(m_socket, [this](const iuring::CloseResult&) {
                        // mask it dead after closing so that the
                        // HttpClient can be reused.
                        mark_dead();
                    });

                    return iuring::ReceivePostAction::NONE;
                });
            });
    }
}