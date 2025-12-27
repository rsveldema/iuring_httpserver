#pragma once

#include <queue>

#include "mbedtls/ctr_drbg.h"
#include "mbedtls/debug.h"
#include "mbedtls/entropy.h"
#include "mbedtls/error.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/ssl_cache.h"
#include "mbedtls/x509.h"

#include <slogger/ILogger.hpp>

#include "AbstractTLS.hpp"

namespace http
{
class MbedTlsSocketWrapper : public iuring::ISocket
{
public:
    MbedTlsSocketWrapper(const std::shared_ptr<iuring::ISocket>& client_socket,
        logging::ILogger& logger, mbedtls_ssl_context* ssl)
        : iuring::ISocket(client_socket->get_type(), client_socket->get_port(),
              logger, client_socket->get_kind(), client_socket->get_fd())
        , m_client_socket(client_socket)
        , m_ssl(ssl)
    {
        assert(ssl != nullptr);
    }

    void send(const std::shared_ptr<iuring::IOUringInterface>& io,
        const std::string& reply_msg,
        const iuring::send_callback_func_t& cb) override;

    int mcast_bind() override
    {
        return m_client_socket->mcast_bind();
    }

    void join_multicast_group(
        const std::string& ip_address, const std::string& source_iface) override
    {
        m_client_socket->join_multicast_group(ip_address, source_iface);
    }

    const std::shared_ptr<iuring::ISocket>& get_client_socket()
    {
        return m_client_socket;
    }

private:
    std::shared_ptr<iuring::ISocket> m_client_socket;
    mbedtls_ssl_context* m_ssl;
};


class MbedTLS : public AbstractTLS
{
public:
    MbedTLS(const std::shared_ptr<iuring::IOUringInterface>& io,
        logging::ILogger& logger, ServerConfig& config)
        : AbstractTLS(io, logger, config)
    {
    }

    error::Error init() override;

    error::Error add_encrypted_data(const iuring::ReceivedMessage& data,
        std::shared_ptr<iuring::ISocket> client_socket,
        tls_recv_callback_func_t handler) override;

private:
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_x509_crt srvcert;
    mbedtls_pk_context pkey;
#if defined(MBEDTLS_SSL_CACHE_C)
    mbedtls_ssl_cache_context cache;
#endif

    std::shared_ptr<MbedTlsSocketWrapper> m_socket;

    error::Error do_ssl_handshake(const std::shared_ptr<iuring::ISocket>& client_socket);

public:
    int send_cb(const unsigned char* buf, size_t len);
    int recv_cb(unsigned char* buf, size_t len);
};

} // namespace http