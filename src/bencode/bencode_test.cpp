#include "bencode.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace bite::bencode;

TEST_CASE("Bencode Integer Parsing", "[bencode][integer]") {
    SECTION("Valid integers") {
        std::string_view input = "i42e";
        auto res = decode(input);
        REQUIRE(res.has_value());
        REQUIRE(res->is<Integer>());
        REQUIRE(*res->get<Integer>() == 42);
    }
};