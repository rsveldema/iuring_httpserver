#pragma once

#include <stdexcept>

class ParseError : public std::runtime_error
{
public:
    ParseError(const std::string& msg) : std::runtime_error(msg) {}
};

#define THROW_ERROR(X) throw ParseError(X)
#define INTERNAL_ERROR(X) throw std::runtime_error(X)
