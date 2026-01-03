#include <sstream>
#include <utility>

#include <http/HttpServer.hpp>

namespace http
{
std::string to_string(URLParameters params)
{
    std::string ret = "{ ";
    for (const auto& [key, value] : params)
    {
        ret += key + ": " + value + ", ";
    }
    ret += " }";
    return ret;
}


std::optional<std::string> get_parameter_value(
    const URLParameters& params, const std::string& key)
{
    for (const auto& p : params)
    {
        if (p.first == key)
        {
            return p.second;
        }
    }
    return std::nullopt;
}

std::string create_json_error_msg(const std::string& msg)
{
    return std::format(R"({{"error":"{}"}})", msg);
}

void HttpServer::send_reply(const std::shared_ptr<iuring::ISocket>& socket,
    const std::string& reply_msg)
{
    socket->send(get_io(), reply_msg, [](const iuring::SendResult&) {});
}


std::vector<std::string> split(const std::string& s, char sep)
{
    std::vector<std::string> ret;

    assert(s[0] == sep);

    size_t j = 1; // skip initial '/'
    for (size_t i = j; i < s.size(); i++)
    {
        if (s[i] == sep)
        {
            const auto segment = s.substr(j, (i - j));
            ret.push_back(segment);
            j = i + 1;
        }
    }

    if (s.length() - j > 0)
    {
        auto last = s.substr(j, s.length() - j);
        ret.push_back(last);
    }

    return ret;
}


bool endpoint_matches(const std::string& impl_endpoint,
    const std::string& got_endpoint, URLParameters& params,
    logging::ILogger& logger)
{
    auto impl = split(impl_endpoint, '/');
    auto got = split(got_endpoint, '/');

    params.clear();

    if (impl.size() != got.size())
    {
        if (false)
        {
            LOG_INFO(logger, "size mismatch ({} vs {}) for {} vs {}",
                impl.size(), got.size(), impl_endpoint, got_endpoint);
        }
        return false;
    }

    for (size_t i = 0; i < impl.size(); i++)
    {
        if (impl[i][0] == '{')
        {
            // wildcard matching
            if (false)
            {
                LOG_INFO(logger,
                    "segment stored!!!! ({}[{}] vs {}) for {} vs {}\n", impl[i],
                    impl[i].substr(1, impl[i].length() - 2), got[i],
                    impl_endpoint, got_endpoint);
            }
            params.emplace_back(
                impl[i].substr(1, impl[i].length() - 2), got[i]);
            continue;
        }

        if (impl[i] != got[i])
        {
            if (false)
            {
                LOG_INFO(logger, "segment mismatch ({} vs {}) for {} vs {}",
                    impl[i], got[i], impl_endpoint, got_endpoint);
            }
            return false;
        }
    }

    return true;
}


std::optional<EndpointHandler> HttpServer::find_handler(
    const std::string& endpoint, const HttpMethod method)
{
    auto& mm = m_handler_map[method];

    for (const auto& h : mm)
    {
        URLParameters params;
        if (endpoint_matches(h.first, endpoint, params, get_logger()))
        {
            return EndpointHandler{ h.second, params };
        }
    }
    return std::nullopt;
}

/** install an endpoint handler. Note that repeated calls for the same endpoint
 * will override the earlier registered endpoint handler.
 */
void HttpServer::register_endpoint_handler(
    const std::string& endpoint, HttpMethod method, const handler_func_t& func)
{
    m_handler_map[method][endpoint] = func;
}

const char* method_to_string(HttpMethod m)
{
    switch (m)
    {
    case HttpMethod::UNKNOWN:
        return "???";
    case HttpMethod::GET:
        return "get";
    case HttpMethod::POST:
        return "post";
    case HttpMethod::PUT:
        return "put";
    case HttpMethod::DELETE:
        return "delete";
    case HttpMethod::OPTIONS:
        return "options";
    case HttpMethod::PATCH:
        return "patch";
    }
    return "unknown enum for http method";
}


std::string status_code_to_string(StatusCode c)
{
    switch (c)
    {
    default:
        break;

    case StatusCode::NOT_FOUND:
        return "Not Found";

    case StatusCode::OK:
        return "OK";

    case StatusCode::BAD_REQUEST:
        return "Bad Request";

    case StatusCode::INTERNAL_SERVER_ERROR:
        return "Internal Server Error";

    case StatusCode::FORBIDDEN:
        return "Forbidden";
    }
    return std::format("unknown: {}", static_cast<int>(c));
}

std::string HttpServer::create_reply_string(StatusCode status_code,
    const std::string& reply_payload, const header_map_t& request_headers,
    const header_map_t& extra_response_headers,
    ReplyContentType reply_content_type)
{
    const auto status_msg = status_code_to_string(status_code);

    std::array<char, 80> now;
    time_t rawtime;
    time(&rawtime);
    const auto timeinfo = localtime(&rawtime);
    strftime(now.data(), now.size(), "%c", timeinfo);


    std::string reply;
    reply += std::format(
        "HTTP/1.1 {} {}\r\n", static_cast<int>(status_code), status_msg);
    reply += std::format("Date: {}\r\n", now.data());
    reply += std::format("Server: {}\r\n", m_server_name);
    reply += std::format("Last-Modified: {}\r\n", now.data());
    reply += std::format("Content-Length: {}\r\n", reply_payload.length());

    // always set content type: if (reply_payload.length() > 0)
    switch (reply_content_type)
    {
    case ReplyContentType::APPLICATION_JSON:
        reply += "Content-Type: application/json\r\n";
        break;
    case ReplyContentType::TEXT_PLAIN:
        reply += "Content-Type: text/plain\r\n";
        break;
    case ReplyContentType::APPLICATION_SDP:
        reply += "Content-Type: application/sdp\r\n";
        break;
    }

    auto ip4 = get_adapter().get_interface_ip4();
    assert(ip4.has_value());
    // nmos tool really wants this reply header:
    reply +=
        std::format("Access-Control-Allow-Origin: http://{}\r\n", ip4.value());

    static const auto* REQUEST_HEADERS = "Access-Control-Request-Headers";
    static const auto* REQUEST_METHOD = "Access-Control-Request-Method";
    static const auto* ALLOW_HEADERS = "Access-Control-Allow-Headers";
    static const auto* ALLOW_METHODS = "Access-Control-Allow-Methods";

    bool handled_ACL_headers = false;
    bool handled_ACL_methods = false;
    for (const auto& it : extra_response_headers)
    {
        reply += it.first + ": " + it.second + "\r\n";

        if (it.first == ALLOW_HEADERS)
        {
            handled_ACL_headers = true;
        }
        if (it.first == ALLOW_METHODS)
        {
            handled_ACL_methods = true;
        }
    }

    if (!handled_ACL_headers)
    {
        // echo the request header back to the requestor
        if (request_headers.contains(REQUEST_HEADERS))
        {
            if (auto it = request_headers.find(REQUEST_HEADERS);
                it != request_headers.end())
            {
                reply += "Access-Control-Allow-Headers: " + it->second + "\r\n";
            }
            else
            {
                LOG_ERROR(get_logger(), "no req header data?");
            }
        }
    }

    if (!handled_ACL_methods)
    {
        if (request_headers.contains(REQUEST_METHOD))
        {
            if (auto it = request_headers.find(REQUEST_METHOD);
                it != request_headers.end())
            {
                reply += "Access-Control-Allow-Methods: " + it->second + "\r\n";
            }
            else
            {
                LOG_ERROR(get_logger(), "no req method data?");
            }
        }
    }

    reply += "Connection: Closed\r\n";
    reply += "\r\n";
    reply += reply_payload;

    // LOG_INFO(get_logger(), "SERVER REPLY: {}", reply_payload.c_str());

    return reply;
}


void HttpServer::handle_endpoint(const std::string& endpoint,
    const std::string& payload, HttpMethod method,
    const header_map_t& request_headers,
    const std::shared_ptr<iuring::ISocket>& socket)
{
    if (const auto handler_opt = find_handler(endpoint, method))
    {
        const auto& handler_struct = *handler_opt;
        handler_struct.handler(endpoint, payload, handler_struct.params,
            [this, socket, request_headers](const HandlerResult& ret) {
                const auto reply_msg = create_reply_string(ret.m_status,
                    ret.m_reply, request_headers, ret.m_reply_headers,
                    ret.m_content_type);

                send_reply(socket, reply_msg);
            });
    }
    else
    {
        // can immediately send a reply.
        LOG_INFO(get_logger(),
            "no endpoint handler for {}: {} -- return not-found",
            method_to_string(method), endpoint.c_str())
        // lets return an empty json
        auto status_code = http::StatusCode::NOT_FOUND;
        auto reply_payload = "{}";
        header_map_t reply_headers;

        auto reply_msg = create_reply_string(status_code, reply_payload,
            request_headers, reply_headers, ReplyContentType::APPLICATION_JSON);

        send_reply(socket, reply_msg);
    }
}

HttpSessionState HttpSession::handle_incoming_http_packet(
    const iuring::ReceivedMessage& data,
    const std::shared_ptr<iuring::ISocket>& socket)
{
    if (data.get_size() == 0)
    {
        LOG_DEBUG(socket->get_logger(),
            "http-session ---> received: 0 bytes -> connection closed");
        return HttpSessionState::COMPLETE;
    }

    const auto new_msg = data.to_string();
    m_partial_data += new_msg;
    LOG_DEBUG(socket->get_logger(), "http-session ---> received: {} bytes -> {}",
        data.get_size(), new_msg);

    parser.parse(m_partial_data);

    if (parser.get_content_size().has_value() == false)
    {
        LOG_DEBUG(socket->get_logger(),
            "http-session ---> no content-size header, assuming complete (and "
            "not handling chunked at the moment)");
        return HttpSessionState::COMPLETE;
    }

    if (parser.get_header_size() > m_partial_data.size())
    {
        LOG_INFO(socket->get_logger(),
            "http-session ---> waiting for more data: have {} need header size "
            "{}",
            m_partial_data.size(), parser.get_header_size());
        return HttpSessionState::INCOMPLETE;
    }

    if (parser.get_content_size().value() >  (m_partial_data.size() - parser.get_header_size()))
    {
        LOG_DEBUG(socket->get_logger(),
            "http-session ---> waiting for more data: have {} need {}",
            m_partial_data.size(), parser.get_content_size().value());
        return HttpSessionState::INCOMPLETE;
    }
    return HttpSessionState::COMPLETE;
}


void HttpServer::handle_incoming_http_packet(
    const iuring::ReceivedMessage& pkt_data,
    const std::shared_ptr<iuring::ISocket>& socket)
{
    if (!m_active_sessions.contains(socket->get_port()))
    {
        m_active_sessions[socket->get_port()] =
            std::make_unique<HttpSession>(get_logger());
    }

    const auto state =
        m_active_sessions[socket->get_port()]->handle_incoming_http_packet(
            pkt_data, socket);

    if (state == HttpSessionState::INCOMPLETE)
    {
        LOG_DEBUG(get_logger(),
            "http ---> session incomplete, waiting for more data");
        return;
    }

    const auto endpoint_opt = m_active_sessions[socket->get_port()]->get_endpoint();
    if (!endpoint_opt.has_value())
    {
        LOG_ERROR(get_logger(), "missing endpoint in http header");
        return;
    }

    const auto type_opt = m_active_sessions[socket->get_port()]->get_type();
    if (!type_opt.has_value())
    {
        LOG_ERROR(get_logger(), "missing type in http header");
        return;
    }

    LOG_INFO(get_logger(), "handle: {}", endpoint_opt.value());
    const auto& payload = m_active_sessions[socket->get_port()]->get_payload();
    const auto& headers = m_active_sessions[socket->get_port()]->get_headers();

    handle_endpoint(
        endpoint_opt.value(), payload, type_opt.value(), headers, socket);

    // reset so that the next request can be handled
    m_active_sessions[socket->get_port()] = nullptr;
    m_active_sessions.erase(socket->get_port());
}


[[nodiscard]] error::Error HttpServer::init()
{
    if (m_tls)
    {
        if (const auto ret = m_tls->init(); ret != error::Error::OK)
        {
            return ret;
        }
    }

    m_listen_socket = m_socket_factory.create_impl(iuring::SocketType::IPV4_TCP,
        m_port, get_logger(), iuring::SocketKind::SERVER_STREAM_SOCKET);

    LOG_INFO(get_logger(), "HttpServer: listening on port {}", m_port);


    get_io()->submit_accept(
        m_listen_socket, [this](const iuring::AcceptResult& new_conn) {
            LOG_DEBUG(get_logger(), "accept-new-connection callback called");

            auto client_socket =
                m_socket_factory.create_impl(get_logger(), new_conn);

            get_io()->submit_recv(client_socket,
                [this, client_socket](const iuring::ReceivedMessage& data) {
                    if (m_tls)
                    {
                        m_tls->add_encrypted_data(data, client_socket,
                            [this](
                                const iuring::ReceivedMessage& unencrypted_data,
                                std::shared_ptr<iuring::ISocket>
                                    tls_client_socket) {
                                handle_incoming_http_packet(
                                    unencrypted_data, tls_client_socket);
                                return (unencrypted_data.get_size() == 0) ?
                                    iuring::ReceivePostAction::NONE :
                                    iuring::ReceivePostAction::RE_SUBMIT;
                            });
                    }
                    else
                    {
                        if (data.get_size() != 0)
                        {
                            handle_incoming_http_packet(data, client_socket);
                        }
                    }

                    return (data.get_size() == 0) ?
                        iuring::ReceivePostAction::NONE :
                        iuring::ReceivePostAction::RE_SUBMIT;
                });
        });

    return error::Error::OK;
}

} // namespace http