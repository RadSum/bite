#include "bencode.hpp"

#include <charconv>

static constexpr char INTEGER_START_DELIM = 'i';
static constexpr char LIST_START_DELIM = 'l';
static constexpr char DICT_START_DELIM = 'd';
static constexpr char BENCODE_END_DELIM = 'e';

std::expected<std::int64_t, bite::bencode::ParseError>
decode_integer(std::string_view &input);
std::expected<bite::bencode::List, bite::bencode::ParseError>
decode_list(std::string_view &input);
std::expected<bite::bencode::String, bite::bencode::ParseError>
decode_string(std::string_view &input);
std::expected<bite::bencode::Dict, bite::bencode::ParseError>
decode_dict(std::string_view &input);

bite::bencode::ParseResult decode(std::string_view input)
{
    std::string_view cursor = input;

    auto result = bite::bencode::decode_some(cursor);
    if (!result.has_value()) {
        return result;
    }

    if (!cursor.empty()) {
        return std::unexpected(bite::bencode::ParseError::InvalidFormat);
    }

    return result;
}

bite::bencode::ParseResult decode_some(std::string_view &input)
{
    using enum bite::bencode::ParseError;
    if (input.empty()) {
        return std::unexpected(UnexpectedEof);
    }
    const auto wrap_value = [](const auto &v) {
        return bite::bencode::Value{v};
    };

    if (input.front() == INTEGER_START_DELIM) {
        return decode_integer(input).transform(wrap_value);
    }
    if (input.front() == LIST_START_DELIM) {
        return decode_list(input).transform(wrap_value);
    }
    if (input.front() == DICT_START_DELIM) {
        return decode_dict(input).transform(wrap_value);
    }
    return decode_string(input).transform(wrap_value);
}

std::expected<std::int64_t, bite::bencode::ParseError>
decode_integer(std::string_view &input)
{
    if (input.empty() || input.front() != INTEGER_START_DELIM) {
        return std::unexpected(bite::bencode::ParseError::InvalidFormat);
    }
    input.remove_prefix(1);

    const std::size_t end_pos = input.find('e');
    if (end_pos == std::string_view::npos) {
        return std::unexpected(bite::bencode::ParseError::UnexpectedEof);
    }

    std::int64_t val{0};
    auto [ptr, ec] = std::from_chars(input.data(), input.data() + end_pos, val, 10);
    if (ec != std::errc{}) {
        return std::unexpected(bite::bencode::ParseError::InvalidFormat);
    }

    input.remove_prefix(end_pos + 1);
    return val;
}

std::expected<bite::bencode::List, bite::bencode::ParseError>
decode_list(std::string_view &input)
{
    if (input.empty() || input.front() != LIST_START_DELIM) {
        return std::unexpected(bite::bencode::ParseError::InvalidFormat);
    }
    input.remove_prefix(1);

    bite::bencode::List list;

    while (!input.empty() && input.front() != BENCODE_END_DELIM) {
        auto element = bite::bencode::decode_some(input);
        if (!element.has_value()) {
            return std::unexpected(element.error());
        }
        list.push_back(std::move(*element));
    }
    if (input.empty()) {
        return std::unexpected(bite::bencode::ParseError::UnexpectedEof);
    }

    input.remove_prefix(1);
    return list;
}

std::expected<bite::bencode::String, bite::bencode::ParseError>
decode_string(std::string_view &input)
{
    std::size_t length{0};
    const auto end_ptr = input.data() + input.size();
    const auto [ptr, ec] = std::from_chars(input.data(), end_ptr, length, 10);
    if (ptr >= end_ptr) {
        return std::unexpected(bite::bencode::ParseError::UnexpectedEof);
    }
    // for now don't handle std::errc::result_out_of_range
    if (ec != std::errc{} || *ptr != ':') {
        return std::unexpected(bite::bencode::ParseError::InvalidFormat);
    }
    input.remove_prefix(ptr - input.data() + 1);
    if (input.length() < length) {
        return std::unexpected(bite::bencode::ParseError::UnexpectedEof);
    }
    const std::string_view payload = input.substr(0, length);
    input.remove_prefix(length);

    return bite::bencode::String{payload};
}

std::expected<bite::bencode::Dict, bite::bencode::ParseError>
decode_dict(std::string_view &input)
{
    if (input.empty() || input.at(0) != DICT_START_DELIM) {
        return std::unexpected(bite::bencode::ParseError::InvalidFormat);
    }

    bite::bencode::Dict dict;

    std::string last_key;
    bool first_key{true};

    while (!input.empty() && input.front() != BENCODE_END_DELIM) {
        auto key_res = decode_string(input);
        if (!key_res.has_value()) {
            return std::unexpected(key_res.error());
        }

        if (!first_key) {
            if (*key_res == last_key) {
                return std::unexpected(bite::bencode::ParseError::DuplicateKey);
            }
            if (*key_res < last_key) {
                return std::unexpected(bite::bencode::ParseError::UnsortedKey);
            }
        }

        first_key = false;
        last_key = *key_res;

        auto val_res = decode_some(input);
        if (!val_res.has_value()) {
            return std::unexpected(val_res.error());
        }

        dict.emplace(std::move(*key_res), std::move(*val_res));
    }

    return dict;
}
