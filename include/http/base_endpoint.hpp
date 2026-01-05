#pragma once

#include <map>
#include <string>
#include <regex>

#include <http/HttpServer.hpp>
#include <domainmodel/Node.hpp>
#include <nmos/nmos_codegen_types.hpp>

namespace http
{

class BaseEndpoint
{
public:
    virtual ~BaseEndpoint() = default;

    virtual std::string get_endpoint_path() const = 0;
};

} // namespace http