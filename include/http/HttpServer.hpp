#pragma once


#include <functional>
#include <utility>

#include <slogger/ILogger.hpp>

#include <iuring/IOUringInterface.hpp>
#include <iuring/ISocketFactory.hpp>

#include <http/AbstractTLS.hpp>
#include <http/HttpParser.hpp>
#include <http/HttpStatusCodes.hpp>
#include <http/ParseError.hpp>

namespace http
{
enum class ReplyContentType
{
    APPLICATION_JSON, // application/json
    APPLICATION_SDP,  // application/sdp
    TEXT_PLAIN        // text/plain
};

struct HandlerResult
{
    std::string m_reply;
    StatusCode m_status;
    ReplyContentType m_content_type;

    HandlerResult()
        : m_reply("")
        , m_status(StatusCode::OK)
        , m_content_type(ReplyContentType::APPLICATION_JSON)
    {
    }

    HandlerResult(std::string reply, StatusCode status,
        ReplyContentType content_type = ReplyContentType::APPLICATION_JSON)
        : m_reply(std::move(reply))
        , m_status(status)
        , m_content_type(content_type)
    {
    }
};

using reply_handler_t = std::function<void(const HandlerResult&)>;

std::string create_json_error_msg(const std::string& msg);


using URLParameters = std::vector<std::pair<std::string, std::string>>;

std::string to_string(http::URLParameters params);

using handler_func_t =
    std::function<void(const std::string& endpoint, const std::string& payload,
        const URLParameters& params, reply_handler_t reply_handler)>;

struct EndpointHandler
{
    handler_func_t handler;
    URLParameters params;
};

class HttpServer
{
public:
    HttpServer(const std::string& server_name,
        const std::shared_ptr<iuring::IOUringInterface>& network,
        logging::ILogger& logger, iuring::NetworkAdapter& adapter,
        iuring::ISocketFactory& socket_factory, iuring::SocketPortID port,
        const std::shared_ptr<AbstractTLS>& tls)
        : m_logger(logger)
        , m_server_name(server_name)
        , m_socket_factory(socket_factory)
        , m_adapter(adapter)
        , m_network(network)
        , m_port(port)
        , m_tls(tls)
    {
    }

    logging::ILogger& get_logger()
    {
        return m_logger;
    }

    [[nodiscard]] error::Error init();

    void register_endpoint_handler(const std::string& endpoint,
        HttpMethod method, const handler_func_t& func);

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
        const std::map<std::string, std::string>& headers,
        const std::shared_ptr<iuring::ISocket>& socket);

    void handle_incoming_http_packet(const iuring::ReceivedMessage& data,
        const std::shared_ptr<iuring::ISocket>& socket);

    void send_reply(const std::shared_ptr<iuring::ISocket>& socket,
        const std::string& reply_msg);

    std::string create_reply_string(StatusCode status_code,
        const std::string& reply_payload,
        const std::map<std::string, std::string>& headers,
        ReplyContentType reply_content_type);
};

std::optional<std::string> get_parameter_value(
    const URLParameters& params, const std::string& key);

} // namespace http