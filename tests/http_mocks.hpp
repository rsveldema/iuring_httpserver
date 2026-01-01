#pragma once

#include <gmock/gmock.h>

#include <memory>

#include <http/AbstractTLS.hpp>

namespace http
{

namespace mocks
{
    class TLS : public AbstractTLS
    {
    public:
        TLS(const std::shared_ptr<iuring::IOUringInterface>& io,
            logging::ILogger& logger, ServerConfig& config)
            : AbstractTLS(io, logger, config)
        {
        }


        MOCK_METHOD(error::Error, init, (), (override));

        MOCK_METHOD(error::Error, add_encrypted_data,
            (const iuring::ReceivedMessage& data,
                std::shared_ptr<iuring::ISocket> client_socket,
                tls_recv_callback_func_t handler),
            (override));
    };
} // namespace mocks
} // namespace http