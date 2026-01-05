#pragma once

#include <functional>
#include <map>
#include <utility>

#include <slogger/ILogger.hpp>

#include <iuring/IOUringInterface.hpp>
#include <iuring/ISocketFactory.hpp>

#include <http/AbstractTLS.hpp>
#include <http/HttpParser.hpp>
#include <http/HttpStatusCodes.hpp>
#include <http/ParseError.hpp>

#include <http/IHttpServer.hpp>

namespace http
{
struct EndpointHandler
{
    handler_func_t handler;
    URLParameters params;
};

class HttpServer : public IHttpServer
{
public:
    /** @param tls Pass in an MbedTLS instance to switch to https.
     * @param show_http_packets If true, show incoming and outgoing http packets in info log
     */
    HttpServer(const std::string& server_name,
        const std::shared_ptr<iuring::IOUringInterface>& network,
        logging::ILogger& logger, iuring::NetworkAdapter& adapter,
        iuring::ISocketFactory& socket_factory, iuring::SocketPortID port,
        const std::shared_ptr<AbstractTLS>& tls,
        bool show_http_packets)
        : m_logger(logger)
        , m_server_name(server_name)
        , m_socket_factory(socket_factory)
        , m_adapter(adapter)
        , m_network(network)
        , m_port(port)
        , m_tls(tls)
        , m_show_http_packets(show_http_packets)
    {
    }

    logging::ILogger& get_logger()
    {
        return m_logger;
    }

    [[nodiscard]] error::Error init() override;

    void register_endpoint_handler(const std::string& endpoint,
        HttpMethod method, const handler_func_t& func) override;

private:
    logging::ILogger& m_logger;
    const std::string m_server_name;
    iuring::ISocketFactory& m_socket_factory;
    iuring::NetworkAdapter& m_adapter;

    std::map<HttpMethod, std::map<std::string, handler_func_t>> m_handler_map;

    std::shared_ptr<iuring::ISocket> m_listen_socket;
    std::shared_ptr<iuring::IOUringInterface> m_network;
    const iuring::SocketPortID m_port;
    std::shared_ptr<AbstractTLS> m_tls;
    bool m_show_http_packets = false;

    std::map<iuring::SocketPortID, std::unique_ptr<HttpSession>> m_active_sessions;

private:
    std::shared_ptr<iuring::IOUringInterface>& get_io()
    {
        return m_network;
    }


    iuring::NetworkAdapter& get_adapter()
    {
        return m_adapter;
    }


    std::optional<EndpointHandler> find_handler(
        const std::string& endpoint, const HttpMethod method);

    void handle_endpoint(const std::string& endpoint,
        const std::string& payload, HttpMethod method,
        const header_map_t& headers,
        const std::shared_ptr<iuring::ISocket>& socket);

    void handle_incoming_http_packet(const iuring::ReceivedMessage& data,
        const std::shared_ptr<iuring::ISocket>& socket);

    void send_reply(const std::shared_ptr<iuring::ISocket>& socket,
        const std::string& reply_msg);

    std::string create_reply_string(StatusCode status_code,
        const std::string& reply_payload, const header_map_t& request_headers,
        const header_map_t& response_headers,
        ReplyContentType reply_content_type);
};

std::optional<std::string> get_parameter_value(
    const URLParameters& params, const std::string& key);

} // namespace http