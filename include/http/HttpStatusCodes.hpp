#pragma once

#include <cstdint>

namespace http
{
enum class StatusCode : uint16_t
{
    CONTINUE = 100,

    OK = 200,
    CREATED = 201,
    NO_CONTENT = 204,

    MOVED_PERMANENTLY = 301,
    FOUND = 302,
    NOT_MODIFIED = 304,

    BAD_REQUEST = 400,
    NOT_FOUND = 404,
    FORBIDDEN = 403,
    NOT_REACHABLE = 444,

    NOT_IMPLEMENTED = 501,
    INTERNAL_SERVER_ERROR = 500,
    SERVICE_UNAVAILABLE = 503
};

    std::string status_code_to_string(StatusCode c);

} // namespace http




template <>
struct std::formatter<http::StatusCode> {
    constexpr auto parse(std::format_parse_context& ctx) {
        return ctx.begin();
    }

    auto format(const http::StatusCode& c, std::format_context& ctx) const {
        return std::format_to(ctx.out(), "{}", http::status_code_to_string(c));
    }
};

