#include <cassert>
#include <cstring>
#include <queue>

#include <http/MbedTLS.hpp>


namespace http
{
namespace
{
    constexpr auto DEBUG_LEVEL = 0;


    void my_debug(void* ctx, [[maybe_unused]] int level, const char* file,
        int line, const char* str)
    {
        auto* tls = static_cast<MbedTLS*>(ctx);
        assert(tls != nullptr);

        tls->get_logger().info_msg(line, file, str);
    }

    int trampoline_send_cb(void* ctx, const unsigned char* buf, size_t len)
    {
        auto* tls = static_cast<MbedTLS*>(ctx);
        assert(tls != nullptr);

        return tls->send_cb(buf, len);
    }


    int trampoline_recv_cb(void* ctx, unsigned char* buf, size_t len)
    {
        auto* tls = static_cast<MbedTLS*>(ctx);
        assert(tls != nullptr);
        return tls->recv_cb(buf, len);
    }
} // namespace


enum class SocketState
{
    INITIAL,
    HANDSHAKE,
    READY,
    CLOSED
};


class MbedTlsConnectionData : public iuring::IConnectionData
{
public:
    MbedTlsConnectionData(logging::ILogger& logger)
        : m_logger(logger)
    {
    }

    logging::ILogger& get_logger()
    {
        return m_logger;
    }

    SocketState get_state() const
    {
        return m_state;
    }

    void set_state(SocketState s)
    {
        m_state = s;
    }

    bool has_encrypted_data() const
    {
        return !m_encrypted_data.empty();
    }

    size_t num_bytes_encrypted_data() const
    {
        return m_encrypted_data.size();
    }

    void push_encrypted_data(const iuring::ReceivedMessage& data)
    {
        if (data.is_empty())
        {
            return;
        }
#if __cpp_lib_containers_ranges
        m_encrypted_data.push_range(data);
#else
        for (size_t i = 0; i < data.get_size(); i++)
        {
            uint8_t byte = *(data.begin() + i);        
            m_encrypted_data.push(byte);
        }
#endif
        assert(!m_encrypted_data.empty());

    }

    size_t copy_out_encrypted_data(uint8_t* buf, size_t len)
    {
        size_t copied = 0;
        while (!m_encrypted_data.empty())
        {
            if (copied == len)
            {
                break;
            }
            buf[copied] = m_encrypted_data.front();
            copied++;
            m_encrypted_data.pop();
        }

        LOG_DEBUG(get_logger(),
            "|||||||||||| receive-cb encrypted data: {} bytes, wanted {}",
            copied, len);
        return copied;
    }

private:
    logging::ILogger& m_logger;
    SocketState m_state = SocketState::INITIAL;
    std::queue<uint8_t> m_encrypted_data;
};

int MbedTLS::send_cb(const unsigned char* buf, size_t len)
{
    LOG_DEBUG(
        get_logger(), "|||||||||||| sending encrypted data: {} bytes", len);

    assert(m_socket != nullptr);
    auto wi = get_io()->ackuire_send_workitem(m_socket);
    auto& pkt = wi->get_send_packet();
    pkt.append(buf, len);

    wi->submit_stream_data([this](const iuring::SendResult& result) {
        LOG_DEBUG(get_logger(),
            "|||||||||||| encrypted packet sent successfully: {}",
            result.status);
    });
    return len;
}

int MbedTLS::recv_cb(uint8_t* buf, size_t len)
{
    const auto& conn = std::static_pointer_cast<MbedTlsConnectionData>(
        m_socket->get_client_socket()->get_connection_data());

    const auto ret = conn->copy_out_encrypted_data(buf, len);
    if (ret == 0)
    {
        return MBEDTLS_ERR_SSL_WANT_READ;
    }
    return ret;
}


error::Error MbedTLS::do_ssl_handshake(
    const std::shared_ptr<iuring::ISocket>& client_socket)
{
    const auto& conn = std::static_pointer_cast<MbedTlsConnectionData>(
        client_socket->get_connection_data());
    assert(conn != nullptr);

    LOG_DEBUG(
        get_logger(), "||||||||||||   . Performing the SSL/TLS handshake...");

    int ret = 0;
    while ((ret = mbedtls_ssl_handshake(&ssl)) != 0)
    {
        switch (ret)
        {
        case MBEDTLS_ERR_SSL_WANT_READ:
            if (conn->has_encrypted_data())
            {
                // still have data to read, lets continue looping.
                break;
            }
            LOG_INFO(get_logger(),
                "no encrypted data anymore, will need to wait for more "
                "data to arrive");
            return error::Error::NOT_READY;

        case MBEDTLS_ERR_SSL_WANT_WRITE:
            return error::Error::NOT_READY;

        default: {
            std::array<char, 128> buf;
            mbedtls_strerror(ret, buf.data(), buf.size());
            LOG_ERROR(get_logger(),
                "||||||||||||  failed --  ! mbedtls_ssl_handshake returned {} "
                "--> {}",
                ret, buf.data());
            return error::Error::BAD_PPROTOCOL;
        }
        }
    }
    return error::Error::OK;
}


error::Error MbedTLS::add_encrypted_data(const iuring::ReceivedMessage& data,
    std::shared_ptr<iuring::ISocket> client_socket,
    tls_recv_callback_func_t handler)
{
    assert(client_socket != nullptr);

    if (client_socket->get_connection_data() == nullptr)
    {
        client_socket->set_connection_data(
            std::make_unique<MbedTlsConnectionData>(get_logger()));
    }

    const auto& conn = std::static_pointer_cast<MbedTlsConnectionData>(
        client_socket->get_connection_data());
    assert(conn != nullptr);

    if (conn->get_state() == SocketState::INITIAL)
    {
        mbedtls_ssl_session_reset(&ssl);
        conn->set_state(SocketState::HANDSHAKE);
    }

    if (conn->get_state() == SocketState::CLOSED)
    {
        if (conn->num_bytes_encrypted_data() > 0)
        {
            LOG_INFO(get_logger(),
                "|||||||||||| ignoring extra data after connection close ({} "
                "bytes)",
                conn->num_bytes_encrypted_data());
        }
        return error::Error::OK;
    }

    conn->push_encrypted_data(data);
    m_socket = std::make_shared<MbedTlsSocketWrapper>(
        client_socket, get_logger(), &ssl);

    while (conn->get_state() == SocketState::HANDSHAKE)
    {
        const auto ret = do_ssl_handshake(client_socket);
        if (ret == error::Error::NOT_READY)
        {
            return error::Error::OK;
        }
        if (ret != error::Error::OK)
        {
            return ret;
        }
        conn->set_state(SocketState::READY);
    }


    std::array<uint8_t, 1024 * 32> buf;
    const auto ret = mbedtls_ssl_read(&ssl, buf.data(), buf.size());

    if (ret < 0)
    {
        switch (ret)
        {
        case MBEDTLS_ERR_SSL_WANT_READ:
        case MBEDTLS_ERR_SSL_WANT_WRITE:
            return error::Error::OK;

        case MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY:
            LOG_DEBUG(get_logger(),
                "||||||||||||  connection was closed gracefully\n");
            conn->set_state(SocketState::CLOSED);
            return error::Error::OK;

        case MBEDTLS_ERR_NET_CONN_RESET:
            LOG_DEBUG(
                get_logger(), "||||||||||||  connection was reset by peer\n");
            conn->set_state(SocketState::CLOSED);
            return error::Error::OK;

        default:
            mbedtls_strerror(ret, (char*) buf.data(), buf.size());
            LOG_ERROR(get_logger(),
                "|||||||||||| failed to read from tls socket: {}",
                (const char*) buf.data());
            conn->set_state(SocketState::CLOSED);
            return error::Error::BAD_PPROTOCOL;
        }
    }
    else
    {
        iuring::ReceivedMessage unencrypted_msg(
            buf.data(), ret, data.get_source_address());
        auto post_action = handler(unencrypted_msg, m_socket);
        LOG_DEBUG(get_logger(),
            "|||||||||||| client tls wanted post action: {}",
            static_cast<int>(post_action));
    }

    return error::Error::OK;
}


void MbedTlsSocketWrapper::send(
    [[maybe_unused]] const std::shared_ptr<iuring::IOUringInterface>& io,
    const std::string& reply_msg, const iuring::send_callback_func_t& cb)
{
    const char* buf = reply_msg.data();
    size_t len = reply_msg.size();

    int ret = mbedtls_ssl_write(m_ssl, (unsigned char*) buf, len);
    LOG_DEBUG(get_logger(), "|||||||||||| reply from ssl-write = {}", ret);
    if (ret >= 0)
    {
        cb(iuring::SendResult(ret));
    }
}


error::Error MbedTLS::init()
{
    static const char* pers = "ssl_server";

    // mbedtls_net_context listen_fd, client_fd;
    // mbedtls_net_init(&listen_fd);
    // mbedtls_net_init(&client_fd);
    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&conf);
#if defined(MBEDTLS_SSL_CACHE_C)
    mbedtls_ssl_cache_init(&cache);
#endif
    mbedtls_x509_crt_init(&srvcert);
    mbedtls_pk_init(&pkey);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);

#if defined(MBEDTLS_USE_PSA_CRYPTO)
    psa_status_t status = psa_crypto_init();
    if (status != PSA_SUCCESS)
    {
        LOG_ERROR(get_logger(),
            "Failed to initialize PSA Crypto implementation: {}", (int) status);
        ret = MBEDTLS_ERR_SSL_HW_ACCEL_FAILED;
        return error::Error::UNKNOWN;
    }
#endif /* MBEDTLS_USE_PSA_CRYPTO */

#if defined(MBEDTLS_DEBUG_C)
    mbedtls_debug_set_threshold(DEBUG_LEVEL);
#endif

    /*
     * 1. Seed the RNG
     */
    LOG_DEBUG(get_logger(),
        "||||||||||||   . Seeding the random number generator...");

    if (int ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func,
            &entropy, (const unsigned char*) pers, strlen(pers));
        ret != 0)
    {
        LOG_ERROR(
            get_logger(), " failed  ! mbedtls_ctr_drbg_seed returned {}", ret);
        return error::Error::UNKNOWN;
    }

    /*
     * 2. Load the certificates and private RSA key
     */

    LOG_INFO(get_logger(), "reading server cert: {}",
        get_config().server_certificate_file_path);
    if (int ret = mbedtls_x509_crt_parse_file(
            &srvcert, get_config().server_certificate_file_path.c_str());
        ret != 0)
    {
        LOG_ERROR(get_logger(),
            "||||||||||||  failed  !  mbedtls_x509_crt_parse returned {}", ret);
        return error::Error::UNKNOWN;
    }


    LOG_INFO(get_logger(), "reading CA cert: {}",
        get_config().CA_certificate_file_path);
    if (int ret = mbedtls_x509_crt_parse_file(
            &srvcert, get_config().CA_certificate_file_path.c_str());
        ret != 0)
    {
        std::array<char, 128> buf;
        mbedtls_strerror(ret, buf.data(), buf.size());
        LOG_ERROR(get_logger(),
            "||||||||||||  failed  !  mbedtls_x509_crt_parse returned {} - {}",
            ret, buf.data());
        return error::Error::UNKNOWN;
    }

    const char* password = get_config().server_key_password == "" ?
        nullptr :
        get_config().server_key_password.c_str();
    LOG_INFO(get_logger(), "reading server key: {} with pass {}",
        get_config().server_key_file_path, password ? password : "<no pass>");
    if (int ret = mbedtls_pk_parse_keyfile(&pkey,
            get_config().server_key_file_path.c_str(), password,
            mbedtls_ctr_drbg_random, &ctr_drbg);
        ret != 0)
    {
        LOG_ERROR(
            get_logger(), " failed  !  mbedtls_pk_parse_key returned {}", ret);
        return error::Error::UNKNOWN;
    }

    /*
     * 4. Setup stuff
     */
    LOG_DEBUG(get_logger(), "||||||||||||   . Setting up the SSL data....");


    if (int ret = mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_SERVER,
            MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
        ret != 0)
    {
        LOG_ERROR(get_logger(),
            "||||||||||||  failed  ! mbedtls_ssl_config_defaults returned {}",
            ret);
        return error::Error::UNKNOWN;
    }

    mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);
    mbedtls_ssl_conf_dbg(&conf, my_debug, this);

#if defined(MBEDTLS_SSL_CACHE_C)
    mbedtls_ssl_conf_session_cache(
        &conf, &cache, mbedtls_ssl_cache_get, mbedtls_ssl_cache_set);
#endif

    mbedtls_ssl_conf_ca_chain(&conf, srvcert.next, NULL);
    if (int ret = mbedtls_ssl_conf_own_cert(&conf, &srvcert, &pkey); ret != 0)
    {
        LOG_ERROR(get_logger(),
            "||||||||||||  failed  ! mbedtls_ssl_conf_own_cert returned {}",
            ret);
        return error::Error::UNKNOWN;
    }

    if (int ret = mbedtls_ssl_setup(&ssl, &conf); ret != 0)
    {
        LOG_ERROR(get_logger(),
            "||||||||||||  failed  ! mbedtls_ssl_setup returned {}", ret);
        return error::Error::UNKNOWN;
    }

    mbedtls_ssl_set_bio(
        &ssl, this, trampoline_send_cb, trampoline_recv_cb, NULL);
    return error::Error::OK;
}
} // namespace http