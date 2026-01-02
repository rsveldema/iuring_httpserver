// included by generated endpoint code in its namespace
//
// NOTE: no pragma-once here as its specifically included multiple times but in
// different class-impls
//

[[maybe_unused]] static std::string serialize(const http::EmptyObject&)
{
    return "";
}
[[maybe_unused]] static void deserialize(const http::EmptyObject&, const json&)
{
}

// hostname
[[maybe_unused]] static std::string serialize(const http::hostname_t& t)
{
    return serialize(t.value);
}

[[maybe_unused]] static void deserialize(
    http::hostname_t& t, const json& payload)
{
    std::string s;
    deserialize(s, payload);
    t.value = s;
}

// ipv4
[[maybe_unused]] static std::string serialize(const http::ipv4_t& t)
{
    return serialize(t.value);
}
[[maybe_unused]] static void deserialize(http::ipv4_t& t, const json& payload)
{
    deserialize(t.value, payload);
}

// ipv6
[[maybe_unused]] static std::string serialize(const http::ipv6_t& t)
{
    return serialize(t.value);
}
[[maybe_unused]] static void deserialize(http::ipv6_t& t, const json& payload)
{
    deserialize(t.value, payload);
}

// regex

[[maybe_unused]] static std::string serialize(const http::RegEx& r)
{
    return serialize(r.value);
}

[[maybe_unused]] static void deserialize(http::RegEx& r, const json& payload)
{
    deserialize(r.value, payload);
}


[[maybe_unused]] static std::string serialize(const std::string& obj)
{
    return '\"' + obj + '\"';
}

[[maybe_unused]] static void deserialize(std::string& data, const json& payload)
{
    if (payload == json::object())
    {
        // object without properties maps to empty string
        // for example: caps{ properties: properties not yet defined}
        // from the schema.
        data = "";
        return;
    }
    if (!payload.is_string())
    {
        THROW_ERROR("not a json string: " + payload.dump());
    }
    data = payload.get<std::string>();
}

[[maybe_unused]] static std::string serialize(bool b)
{
    return b ? "true" : "false";
}

[[maybe_unused]] static void deserialize(bool& b, const json& payload)
{
    b = payload.get<bool>();
}

[[maybe_unused]] static std::string serialize(int64_t v)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%ld", v);
    return buf;
}

[[maybe_unused]] static void deserialize(int64_t& v, const json& payload)
{
    v = payload.get<int>();
}

[[maybe_unused]] static std::string serialize(const http::null_t&)
{
    return "null";
}

[[maybe_unused]] static void deserialize(http::null_t&, const json& p)
{
    if (!p.is_null())
    {
        THROW_ERROR("not a null json obj");
    }
}


//          double


[[maybe_unused]] static std::string serialize(double v)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%f", v);
    return buf;
}

[[maybe_unused]] static void deserialize(double& v, const json& payload)
{
    v = payload.get<double>();
}


#define EMPTY_SER(CODE)                                                        \
    [[maybe_unused]] static std::string serialize(                             \
        const http::EmptyObject_##CODE&)                                       \
    {                                                                          \
        return "";                                                             \
    }                                                                          \
    [[maybe_unused]] static void deserialize(                                  \
        const http::EmptyObject_##CODE&, const json&)                          \
    {                                                                          \
    }

EMPTY_SER(200)
EMPTY_SER(202)
EMPTY_SER(204)

EMPTY_SER(307)

EMPTY_SER(403)
EMPTY_SER(404)
EMPTY_SER(405)
EMPTY_SER(409)
EMPTY_SER(423)

EMPTY_SER(500)

template <typename T>
[[maybe_unused]] static std::string serialize(const std::optional<T>& obj)
{
    if (obj.has_value())
    {
        return serialize(obj.value());
    }
    return "";
}

template <typename T>
[[maybe_unused]] static void deserialize(
    std::optional<T>& obj, const json& payload)
{
    const json empty;
    if (payload == empty)
    {
        return;
    }

    T data;
    deserialize(data, payload);
    obj = data;
}


template <typename... Types>
static std::string serialize(const std::variant<Types...>& obj)
{
    return std::visit([](const auto& elt) { return serialize(elt); }, obj);
}

template <std::size_t I, typename... Types>
static void deserialize_recurs_variant(std::variant<Types...>& obj, const json& payload)
{
    using V = std::variant<Types...>;
    if constexpr (I < std::variant_size_v<V>)
    {
        try
        {
            using A = std::variant_alternative_t<I, V>;

            A val{};
            deserialize(val, payload);
            obj = val;
            return;
        }
        catch (const ParseError& e)
        {
            fprintf(stderr, "was not variant alternative %ld\n", I);
        }

        return deserialize_recurs_variant<I + 1, Types...>(obj, payload);
    }
    THROW_ERROR("failed to parse tuple variant");
}
template <typename... Types>
static void deserialize(std::variant<Types...>& obj, const json& payload)
{
    deserialize_recurs_variant<0, Types...>(obj, payload);
};

template <typename T>
[[maybe_unused]] static std::string serialize(const std::vector<T>& obj)
{
    std::string result = "[";
    const char* comma = "";
    for (const auto& item : obj)
    {
        if (const auto k = serialize(item); k != "")
        {
            result += comma;
            result += k;
            comma = ", ";
        }
    }
    result += "]";
    return result;
}

template <typename T>
[[maybe_unused]] static void deserialize(
    std::vector<T>& obj, const json& payload)
{
    for (size_t i = 0; i < payload.size(); i++)
    {
        T elt{};
        deserialize(elt, payload[i]);
        obj.push_back(elt);
    }
}

template <typename K, typename V>
[[maybe_unused]] static std::string serialize(const std::map<K, V>& obj)
{
    std::string result = "{";
    const char* comma = "";
    for (const auto& item : obj)
    {
        result += comma;
        result += serialize(item.first);
        result += ":";
        result += serialize(item.second);
        comma = ", ";
    }
    result += "}";
    return result;
}


template <typename K, typename V>
[[maybe_unused]] static void deserialize(
    std::map<K, V>& obj, const json& payload)
{
    if (!payload.is_object())
    {
        THROW_ERROR("not a json object");
    }

    for (const auto& it : payload.items())
    {
        auto jkey = it.key();
        auto jvalue = it.value();

        V value;

        deserialize(value, jvalue);

        obj[jkey] = value;
    }
}
