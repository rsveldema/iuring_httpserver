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


namespace http
{
enum class HttpSessionState
{
    INCOMPLETE,
    COMPLETE
};

/** gathers all http packets until a complete request is formed */
class HttpSession
{
public:
    HttpSession(logging::ILogger& logger)
        : m_logger(logger)
    {
    }

    [[nodiscard]] HttpSessionState handle_incoming_http_packet(const iuring::ReceivedMessage& data,
        const std::shared_ptr<iuring::ISocket>& socket,
        bool show_http_packets);


    const std::map<std::string, std::string>& get_headers() const
    {
        return parser.get_headers();
    }
    
    std::optional<HttpMethod> get_type()
    {
        return parser.get_type();
    }

    const std::string& get_payload() const
    {
        return parser.get_payload();
    }

    std::optional<std::string> get_endpoint()
    {
        return parser.get_endpoint();
    }

    logging::ILogger& get_logger()
    {
        return m_logger;
    }

    std::optional<std::size_t> get_content_size()
    {
        return parser.get_content_size();
    }

private:
    std::string m_partial_data;
    logging::ILogger& m_logger;
    HttpParser parser{m_logger};
};

enum class ReplyContentType
{
    APPLICATION_JSON, // application/json
    APPLICATION_SDP,  // application/sdp
    TEXT_PLAIN        // text/plain
};

using header_map_t = std::map<std::string, std::string>;

struct HandlerResult
{
    std::string m_reply;
    StatusCode m_status;
    ReplyContentType m_content_type;
    header_map_t m_reply_headers;

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



class IHttpServer
{
public:
    [[nodiscard]] virtual error::Error init() = 0;

    virtual void register_endpoint_handler(const std::string& endpoint,
        HttpMethod method, const handler_func_t& func) = 0;
};
} // namespace http