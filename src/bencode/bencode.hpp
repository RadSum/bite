#pragma once

#include <cstdint>
#include <expected>
#include <map>
#include <string>
#include <variant>
#include <vector>

namespace bite::bencode
{

struct Value;

using Integer = std::int64_t;
using String = std::string;
using List = std::vector<Value>;
using Dict = std::map<std::string, Value>;

struct Value {
    std::variant<Integer, String, List, Dict> m_data;

    template <typename T> bool is() const { return std::holds_alternative<T>(m_data); }

    template <typename T, typename Self> auto *get(this Self &&self)
    {
        return std::get_if<T>(&self.m_data);
    }
};

enum class ParseError : std::uint8_t {
    InvalidFormat,
    UnexpectedEof,
    DuplicateKey,
    UnsortedKey,
};

using ParseResult = std::expected<Value, ParseError>;

ParseResult decode_some(std::string_view &input);
ParseResult decode(std::string_view input);

} // namespace bite::bencode
