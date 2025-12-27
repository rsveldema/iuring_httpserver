#pragma once

#include <memory>

#include <iuring/IOUringInterface.hpp>
#include <slogger/Error.hpp>

#include <Configuration.hpp>

namespace http
{
using tls_recv_callback_func_t =
    std::function<iuring::ReceivePostAction(const iuring::ReceivedMessage& msg,
        std::shared_ptr<iuring::ISocket> tls_client_socket)>;


class AbstractTLS
{
public:
    AbstractTLS(const std::shared_ptr<iuring::IOUringInterface>& io,
        logging::ILogger& logger, settings::Configuration& config)
        : m_logger(logger)
        , m_io(io)
        , m_config(config)
    {
    }

    const settings::Configuration& get_config() const
    {
        return m_config;
    }

    virtual error::Error init() = 0;

    virtual error::Error add_encrypted_data(const iuring::ReceivedMessage& data,
        std::shared_ptr<iuring::ISocket> client_socket,
        tls_recv_callback_func_t handler) = 0;

    const std::shared_ptr<iuring::IOUringInterface>& get_io()
    {
        return m_io;
    }

    logging::ILogger& get_logger()
    {
        return m_logger;
    }


private:
    logging::ILogger& m_logger;
    std::shared_ptr<iuring::IOUringInterface> m_io;
    settings::Configuration& m_config;
};
} // namespace http