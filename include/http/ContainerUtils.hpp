#pragma once

#include <vector>

namespace ContainerUtils
{
template<typename T>
bool contains(const std::vector<T>& l, const T elt)
{
    for (auto it : l)
    {
        if (it == elt)
        {
            return true;
        }
    }
    return false;
}
}