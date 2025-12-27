#pragma once

namespace http
{
enum class HttpMethod
{
    UNKNOWN,
    GET,
    POST,
    PUT,
    DELETE,
    OPTIONS,
    PATCH
};

const char* method_to_string(HttpMethod m);


} // namespace http