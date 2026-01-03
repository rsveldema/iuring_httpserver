#include <../tests/iuring_mocks.hpp>
#include <gtest/gtest.h>

#include <http/HttpServer.hpp>
#include <slogger/DirectConsoleLogger.hpp>

using namespace testing;


class TestFixture : public testing::Test
{
};

TEST_F(TestFixture, get)
{
    logging::DirectConsoleLogger logger(
        false, true, logging::LogOutput::CONSOLE);

    auto network = std::make_shared<iuring::mocks::IOUring>();

    iuring::NetworkAdapter adapter(logger, "eth0", false);
    adapter.init();

    iuring::mocks::SocketFactory socket_factory;

    http::HttpServer server("testserver", network, logger, adapter,
        socket_factory, iuring::SocketPortID::LOCAL_WEB_PORT, nullptr);

    bool endpoint_called = false;
    server.register_endpoint_handler("/hello/{foo}/bar", http::HttpMethod::GET,
        [&](const std::string& endpoint, const std::string& payload,
            const http::URLParameters& params,
            http::reply_handler_t reply_handler) {
            ASSERT_EQ(http::get_parameter_value(params, "foo"), "42");
            ASSERT_EQ(endpoint, "/hello/42/bar");
            ASSERT_EQ(payload, "hello world");

            LOG_INFO(logger, "endpoint payload = {}", payload);

            ASSERT_EQ(socket_factory.created_sockets.size(), 2);
            EXPECT_CALL(*socket_factory.created_sockets[1], send(_, _, _))
                .WillOnce(
                    [&](const std::shared_ptr<iuring::IOUringInterface>& io,
                        const std::string& msg,
                        const iuring::send_callback_func_t& cb) {
                        ASSERT_EQ(io, network);

                        const auto split = StringUtils::split(msg, '\n');
                        ASSERT_GT(split.size(), 7);
                        ASSERT_EQ(split[0], "HTTP/1.1 200 OK\r");
                        ASSERT_EQ(split[2], "Server: testserver\r");

                        ASSERT_EQ(split[split.size() - 4], "extra-header: cheerio\r");
                        ASSERT_EQ(split[split.size() - 1], "reply from world");

                        iuring::SendResult res{1};
                        cb(res);
                    });

            http::HandlerResult reply("reply from world", http::StatusCode::OK);
            reply.m_reply_headers["extra-header"] = "cheerio";

            // calling the reply handler will cause the http-response to be sent
            // invoking the IScoket::send() call to be issued (tested with the above EXPECT_CALL(.., send(..))
            reply_handler(reply);

            endpoint_called = true;
        });

    EXPECT_CALL(*network, submit_accept(_, _))
        .WillOnce([&](const std::shared_ptr<iuring::ISocket>& socket,
                      iuring::accept_callback_func_t handler) {
            ASSERT_NE(socket, nullptr);
            ASSERT_EQ(socket, socket_factory.created_sockets[0]);
            iuring::AcceptResult res{ 123,
                iuring::IPAddress::parse("1.2.3.4").value() };

            EXPECT_CALL(*network, submit_recv(_, _))
                .WillOnce([&](const std::shared_ptr<iuring::ISocket>& socket,
                              iuring::recv_callback_func_t handler) {
                    ASSERT_NE(socket, nullptr);

                    // send a dummy http request for endpoint /hello/42/bar
                    // which will be 'received' by the above endpoint handler
                    std::string data;
                    data += "GET /hello/42/bar HTTP/1.1\r\n";
                    data += "Host: www.example.com\r\n";
                    data += "User-Agent: curl/8.15.0\r\n";
                    data += "Accept: */*\r\n";
                    data += "\r\n";
                    data += "hello world";

                    iuring::ReceivedMessage msg((const uint8_t*) data.c_str(),
                        data.length(),
                        iuring::IPAddress::parse("1.2.3.4").value());
                    auto result = handler(msg);

                    ASSERT_EQ(result, iuring::ReceivePostAction::RE_SUBMIT);
                });

            // calling the 'accept' handler will cause the submit_recv call to be made
            // by the HttpServer immediately after.
            handler(res);
        });


    ASSERT_EQ(server.init(), error::Error::OK);
    ASSERT_TRUE(endpoint_called);
}

TEST_F(TestFixture, HttpParser_ContentLength_Present)
{
    logging::DirectConsoleLogger logger(
        false, true, logging::LogOutput::CONSOLE);

    http::HttpParser parser(logger);

    // HTTP request with Content-Length header
    std::string request_with_content_length;
    request_with_content_length += "POST /api/data HTTP/1.1\r\n";
    request_with_content_length += "Host: www.example.com\r\n";
    request_with_content_length += "Content-Type: application/json\r\n";
    request_with_content_length += "Content-Length: 27\r\n";
    request_with_content_length += "\r\n";
    request_with_content_length += "{\"key\":\"value\",\"id\":123}";

    ASSERT_EQ(parser.parse(request_with_content_length), error::Error::OK);

    // Verify Content-Length is parsed correctly
    auto content_size = parser.get_content_size();
    ASSERT_TRUE(content_size.has_value());
    ASSERT_EQ(content_size.value(), 27);

    // Verify payload is correct
    ASSERT_EQ(parser.get_payload(), "{\"key\":\"value\",\"id\":123}");

    // Verify other headers are parsed correctly
    ASSERT_EQ(parser.get_type(), http::HttpMethod::POST);
    auto endpoint = parser.get_endpoint();
    ASSERT_TRUE(endpoint.has_value());
    ASSERT_EQ(endpoint.value(), "/api/data");
}

TEST_F(TestFixture, HttpParser_ContentLength_Absent)
{
    logging::DirectConsoleLogger logger(
        false, true, logging::LogOutput::CONSOLE);

    http::HttpParser parser(logger);

    // HTTP GET request without Content-Length header (typical for GET)
    std::string request_without_content_length;
    request_without_content_length += "GET /api/resource HTTP/1.1\r\n";
    request_without_content_length += "Host: www.example.com\r\n";
    request_without_content_length += "User-Agent: TestClient/1.0\r\n";
    request_without_content_length += "Accept: */*\r\n";
    request_without_content_length += "\r\n";

    ASSERT_EQ(parser.parse(request_without_content_length), error::Error::OK);

    // Verify Content-Length is not present
    auto content_size = parser.get_content_size();
    ASSERT_FALSE(content_size.has_value());

    // Verify other parsing is correct
    ASSERT_EQ(parser.get_type(), http::HttpMethod::GET);
    auto endpoint = parser.get_endpoint();
    ASSERT_TRUE(endpoint.has_value());
    ASSERT_EQ(endpoint.value(), "/api/resource");
}

TEST_F(TestFixture, HttpParser_ContentLength_Zero)
{
    logging::DirectConsoleLogger logger(
        false, true, logging::LogOutput::CONSOLE);

    http::HttpParser parser(logger);

    // HTTP request with Content-Length: 0
    std::string request_zero_content_length;
    request_zero_content_length += "POST /api/empty HTTP/1.1\r\n";
    request_zero_content_length += "Host: www.example.com\r\n";
    request_zero_content_length += "Content-Length: 0\r\n";
    request_zero_content_length += "\r\n";

    ASSERT_EQ(parser.parse(request_zero_content_length), error::Error::OK);

    // Verify Content-Length is present and is 0
    auto content_size = parser.get_content_size();
    ASSERT_TRUE(content_size.has_value());
    ASSERT_EQ(content_size.value(), 0);

    // Verify payload is empty
    ASSERT_EQ(parser.get_payload(), "");
}

TEST_F(TestFixture, HttpParser_ContentLength_LargeValue)
{
    logging::DirectConsoleLogger logger(
        false, true, logging::LogOutput::CONSOLE);

    http::HttpParser parser(logger);

    // Create a large payload
    std::string large_payload(1024, 'X');
    
    std::string request_large_content;
    request_large_content += "PUT /api/upload HTTP/1.1\r\n";
    request_large_content += "Host: www.example.com\r\n";
    request_large_content += "Content-Length: 1024\r\n";
    request_large_content += "\r\n";
    request_large_content += large_payload;

    ASSERT_EQ(parser.parse(request_large_content), error::Error::OK);

    // Verify Content-Length is parsed correctly
    auto content_size = parser.get_content_size();
    ASSERT_TRUE(content_size.has_value());
    ASSERT_EQ(content_size.value(), 1024);

    // Verify payload size matches
    ASSERT_EQ(parser.get_payload().size(), 1024);
    ASSERT_EQ(parser.get_payload(), large_payload);
}