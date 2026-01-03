#pragma once

#include <map>
#include <sstream>

#include <iuring/IOUringInterface.hpp>

#include <slogger/ILogger.hpp>

#include <http/HttpMethod.hpp>
#include <http/HttpStatusCodes.hpp>

namespace http
{
class HttpParser
{
public:
    HttpParser(logging::ILogger& logger)
        : m_logger(logger)
    {
    }

    error::Error parse(const std::string& input)
    {
        if (input.empty())
        {
            return error::Error::UNKNOWN;
        }
        std::istringstream stream(input);
        parse_headers(stream);

        m_header_size = stream.tellg();

        const auto pos = m_header_size;
        if (pos == -1)
        {
            LOG_ERROR(get_logger(), "http parser: no payload found: {}",
                input);
            return error::Error::UNKNOWN;
        }
        m_payload = input.substr(stream.tellg());
        return error::Error::OK;
    }

    std::optional<std::string> get_endpoint()
    {
        const auto& method = m_headers[METHOD];
        const auto split = StringUtils::split(method, ' ');
        if (split.size() < 2)
        {
            return std::nullopt;
        }
        return split[1];
    }

    /// @returns http/1.1
    std::optional<std::string> get_protocol()
    {
        const auto& method = m_headers[METHOD];
        const auto split = StringUtils::split(method, ' ');
        if (split.size() < 3)
        {
            return std::nullopt;
        }
        return split[2];
    }

    
    std::optional<std::size_t> get_content_size()
    {
        const auto content_length_it = m_headers.find("Content-Length");
        if (content_length_it == m_headers.end())
        {
            return std::nullopt;
        }
        return static_cast<std::size_t>(
            std::stoul(content_length_it->second));
    }

    std::optional<HttpMethod> get_type()
    {
        const auto& method = m_headers[METHOD];
        const auto split = StringUtils::split(method, ' ');

        if (split.size() < 1)
        {
            return std::nullopt;
        }
        if (split[0] == "GET")
            return HttpMethod::GET;
        if (split[0] == "POST")
            return HttpMethod::POST;
        if (split[0] == "PUT")
            return HttpMethod::PUT;
        if (split[0] == "DELETE")
            return HttpMethod::DELETE;
        if (split[0] == "OPTIONS")
            return HttpMethod::OPTIONS;
        if (split[0] == "PATCH")
            return HttpMethod::PATCH;
        return HttpMethod::UNKNOWN;
    }

    const std::string& get_payload() const
    {
        return m_payload;
    }

    const std::map<std::string, std::string>& get_headers() const
    {
        return m_headers;
    }

    /** The replies status is in the first line as stored in the method header under 'METHOD
     * its format is:
     *     HTTP/1.1 200 OK
     * We want to extract the 200 part.
     */
    StatusCode get_status_code()
    {
        const auto& header_line = m_headers[METHOD];
        const auto split = StringUtils::split(header_line, ' ');
        if (split.size() < 3)
        {
            LOG_ERROR(get_logger(),
                "bad http response status line: {}", header_line);
            return StatusCode::BAD_REQUEST;
        }
        const int code = std::stoi(split[1]);
        return static_cast<StatusCode>(code);
    }

    std::int64_t get_header_size() const
    {
        return m_header_size;
    }

private:
    static constexpr const char* METHOD = "method";
    logging::ILogger& m_logger;
    std::map<std::string, std::string> m_headers;
    std::string m_payload;
    std::int64_t m_header_size = 0;

    void parse_headers(std::istringstream& stream)
    {
        int ix = 0;
        std::string header;
        while (std::getline(stream, header) && header != "\r")
        {
            if (ix++ == 0)
            {
                m_headers[METHOD] = StringUtils::trim(header);
                continue;
            }
            const auto index = header.find(':', 0);
            if (index != std::string::npos)
            {
                m_headers.insert(
                    std::make_pair(StringUtils::trim(header.substr(0, index)),
                        StringUtils::trim(header.substr(index + 1))));
            }
        }
    }

    logging::ILogger& get_logger()
    {
        return m_logger;
    }
};


} // namespace http