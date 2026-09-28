#include "bencode.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace bite::bencode;

TEST_CASE("Bencode Integer Parsing", "[bencode][integer]")
{
    SECTION("Positive integers")
    {
        auto res = decode("i42e");
        REQUIRE(res.has_value());
        REQUIRE(res->is<Integer>());
        REQUIRE(*res->get<Integer>() == 42);
    }
    SECTION("Negative integers")
    {
        auto res = decode("i-100e");
        REQUIRE(res.has_value());
        REQUIRE(res->is<Integer>());
        REQUIRE(*res->get<Integer>() == -100);
    }
    SECTION("Zero integer")
    {
        auto res = decode("i0e");
        REQUIRE(res.has_value());
        REQUIRE(res->is<Integer>());
        REQUIRE(*res->get<Integer>() == 0);
    }
    SECTION("Invalid format")
    {
        REQUIRE(decode("ie").error() == ParseError::InvalidFormat);
        REQUIRE(decode("i42").error() == ParseError::UnexpectedEof);
        REQUIRE(decode("iasdasdase").error() == ParseError::InvalidFormat);
    }
}

TEST_CASE("Bencode String Parsing", "[bencode][string]")
{
    using namespace std::string_view_literals;
    SECTION("Valid string")
    {
        auto res = decode("4:test");
        REQUIRE(res.has_value());
        REQUIRE(res->is<String>());
        REQUIRE(*res->get<String>() == "test"sv);
    }
    SECTION("Empty string")
    {
        auto res = decode("0:");
        REQUIRE(res.has_value());
        REQUIRE(res->is<String>());
        REQUIRE(*res->get<String>() == ""sv);
    }
    SECTION("Truncated payload")
    {
        auto res = decode("10:short");
        REQUIRE(!res.has_value());
        REQUIRE(res.error() == ParseError::UnexpectedEof);
    }
    SECTION("Missing colon")
    {
        auto res = decode("4test");
        REQUIRE(!res.has_value());
        REQUIRE(res.error() == ParseError::InvalidFormat);
    }
}

TEST_CASE("Bencode List Parsing", "[bencode][list]")
{
    SECTION("Empty list")
    {
        auto res = decode("le");
        REQUIRE(res.has_value());
        REQUIRE(res->is<List>());

        const auto *list = res->get<List>();
        REQUIRE(list != nullptr);
        REQUIRE(list->empty());
    }

    SECTION("Flat list with mixed types")
    {
        auto res = decode("li42e3:fooi-6ee");
        REQUIRE(res.has_value());

        const auto *list = res->get<List>();
        REQUIRE(list != nullptr);
        REQUIRE(list->size() == 3);

        REQUIRE(list->at(0).is<Integer>());
        REQUIRE(*list->at(0).get<Integer>() == 42);

        REQUIRE(list->at(1).is<String>());
        REQUIRE(*list->at(1).get<String>() == "foo");

        REQUIRE(list->at(2).is<Integer>());
        REQUIRE(*list->at(2).get<Integer>() == -6);
    }

    SECTION("Nested lists")
    {
        auto res = decode("ll5:lolecei100ee");
        REQUIRE(res.has_value());

        const auto *outer = res->get<List>();
        REQUIRE(outer != nullptr);
        REQUIRE(outer->size() == 2);

        REQUIRE(outer->at(0).is<List>());
        const auto *inner = outer->at(0).get<List>();
        REQUIRE(inner->size() == 1);
        REQUIRE(*inner->at(0).get<String>() == "lolec");

        REQUIRE(*outer->at(1).get<Integer>() == 100);
    }

    SECTION("Unterminated list returns UnexpectedEof")
    {
        auto res = decode("li1ei2e");
        REQUIRE(!res.has_value());
        REQUIRE(res.error() == ParseError::UnexpectedEof);
    }

    SECTION("Invalid terminator")
    {
        auto res = decode("li1ei2ew");
        REQUIRE(!res.has_value());
        REQUIRE(res.error() == ParseError::InvalidFormat);
    }
}

TEST_CASE("Bencode Dictionary Parsing", "[bencode][dict]")
{
    SECTION("Empty dict")
    {
        auto res = decode("de");
        REQUIRE(res.has_value());
        REQUIRE(res->is<Dict>());

        const auto *dict = res->get<Dict>();
        REQUIRE(dict != nullptr);
        REQUIRE(dict->empty());
    }

    SECTION("Valid dict with sorted keys")
    {
        auto res = decode("d3:bar3:foo4:testi42ee");
        REQUIRE(res.has_value());

        const auto *dict = res->get<Dict>();
        REQUIRE(dict != nullptr);
        REQUIRE(dict->size() == 2);

        auto bar_it = dict->find("bar");
        REQUIRE(bar_it != dict->end());
        REQUIRE(bar_it->second.is<String>());
        REQUIRE(*bar_it->second.get<String>() == "foo");

        auto foo_it = dict->find("test");
        REQUIRE(foo_it != dict->end());
        REQUIRE(foo_it->second.is<Integer>());
        REQUIRE(*foo_it->second.get<Integer>() == 42);
    }

    SECTION("Nested dict inside list and vice-versa")
    {
        auto res = decode("d5:firstll6:nestedeee");
        REQUIRE(res.has_value());

        const auto *dict = res->get<Dict>();
        REQUIRE(dict != nullptr);

        auto it = dict->find("first");
        REQUIRE(it != dict->end());
        REQUIRE(it->second.is<List>());

        const auto *outer_list = it->second.get<List>();
        REQUIRE(outer_list->size() == 1);

        const auto *inner_list = outer_list->at(0).get<List>();
        REQUIRE(inner_list->size() == 1);
        REQUIRE(*inner_list->at(0).get<String>() == "nested");
    }

    SECTION("Unsorted keys return UnsortedKey error")
    {
        auto res = decode("d3:fooi42e3:bari1ee");
        REQUIRE(!res.has_value());
        REQUIRE(res.error() == ParseError::UnsortedKey);
    }

    SECTION("Duplicate keys return DuplicateKey error")
    {
        auto res = decode("d3:fooi1e3:fooi2ee");
        REQUIRE(!res.has_value());
        REQUIRE(res.error() == ParseError::DuplicateKey);
    }

    SECTION("Non-string key in dict returns InvalidFormat")
    {
        auto res = decode("di42e4:spame");
        REQUIRE(!res.has_value());
        REQUIRE(res.error() == ParseError::InvalidFormat);
    }

    SECTION("Unterminated dict returns UnexpectedEof")
    {
        auto res = decode("d3:foo4:spam");
        REQUIRE(!res.has_value());
        REQUIRE(res.error() == ParseError::UnexpectedEof);
    }
}

TEST_CASE("Bencode Buffer and Framing Bounds", "[bencode][framing]")
{
    SECTION("Empty input view returns UnexpectedEof")
    {
        auto res = decode("");
        REQUIRE(!res.has_value());
        REQUIRE(res.error() == ParseError::UnexpectedEof);
    }

    SECTION("Invalid start character returns InvalidFormat")
    {
        auto res = decode("x42e");
        REQUIRE(!res.has_value());
        REQUIRE(res.error() == ParseError::InvalidFormat);
    }

    SECTION("decode enforces exact consumption (no trailing bytes)")
    {
        auto res = decode("i42e extra_garbage");
        REQUIRE(!res.has_value());
        REQUIRE(res.error() == ParseError::InvalidFormat);
    }

    SECTION("decode_some allows trailing bytes and advances cursor correctly")
    {
        std::string_view cursor = "i42e extra_garbage";
        auto res = decode_some(cursor);
        REQUIRE(res.has_value());
        REQUIRE(*res->get<Integer>() == 42);
        REQUIRE(cursor == " extra_garbage");
    }
}
