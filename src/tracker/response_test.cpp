#include "response.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace bite::tracker;
using namespace std::string_literals;

TEST_CASE("Compact Peers Parsing", "[tracker][response]")
{
    SECTION("Single peer")
    {
        auto res = parse_compact_peers("\x7f\x00\x00\x01\x1a\xe1"s);
        REQUIRE(res.has_value());
        REQUIRE(res->size() == 1);
        REQUIRE((*res)[0].endpoint.address().to_string() == "127.0.0.1");
        REQUIRE((*res)[0].endpoint.port() == 6881);
        REQUIRE(!(*res)[0].id.has_value());
    }
    SECTION("Multiple peers")
    {
        auto res = parse_compact_peers("\x0a\x00\x00\x01\x00\x50"
                                       "\xc0\xa8\x01\x02\xff\xff"s);
        REQUIRE(res.has_value());
        REQUIRE(res->size() == 2);
        REQUIRE((*res)[0].endpoint.address().to_string() == "10.0.0.1");
        REQUIRE((*res)[0].endpoint.port() == 80);
        REQUIRE((*res)[1].endpoint.address().to_string() == "192.168.1.2");
        REQUIRE((*res)[1].endpoint.port() == 65535);
    }
    SECTION("Empty peers")
    {
        auto res = parse_compact_peers("");
        REQUIRE(res.has_value());
        REQUIRE(res->empty());
    }
    SECTION("Port zero is skipped")
    {
        auto res = parse_compact_peers("\x7f\x00\x00\x01\x00\x00"s);
        REQUIRE(res.has_value());
        REQUIRE(res->empty());
    }
    SECTION("Invalid length")
    {
        auto res = parse_compact_peers("abcde");
        REQUIRE(!res.has_value());
        REQUIRE(res.error().kind == TrackerErrorKind::MalformedResponse);
    }
}

TEST_CASE("Announce Response Parsing", "[tracker][response]")
{
    SECTION("Compact peers")
    {
        auto res =
            parse_announce_response("d8:intervali1800e5:peers6:\x7f\x00\x00\x01\x1a\xe1"
                                    "e"s);
        REQUIRE(res.has_value());
        REQUIRE(res->interval == std::chrono::seconds{1800});
        REQUIRE(res->peers.size() == 1);
        REQUIRE(res->peers[0].endpoint.port() == 6881);
    }
    SECTION("Dictionary peers")
    {
        auto res =
            parse_announce_response("d8:intervali900e5:peersl"
                                    "d2:ip9:127.0.0.17:peer id20:-BI0001-abcdefghijkl"
                                    "4:porti6881ee"
                                    "d2:ip3:::14:porti51413ee"
                                    "ee");
        REQUIRE(res.has_value());
        REQUIRE(res->interval == std::chrono::seconds{900});
        REQUIRE(res->peers.size() == 2);
        REQUIRE(res->peers[0].endpoint.address().to_string() == "127.0.0.1");
        REQUIRE(res->peers[0].endpoint.port() == 6881);
        REQUIRE(res->peers[0].id.has_value());
        REQUIRE(res->peers[1].endpoint.address().is_v6());
        REQUIRE(res->peers[1].endpoint.port() == 51413);
        REQUIRE(!res->peers[1].id.has_value());
    }
    SECTION("Invalid dictionary peers are skipped")
    {
        auto res = parse_announce_response("d8:intervali900e5:peersl"
                                           "d2:ip9:not an ip4:porti6881ee"
                                           "d2:ip9:127.0.0.14:porti0ee"
                                           "d2:ip9:127.0.0.1e"
                                           "ee");
        REQUIRE(res.has_value());
        REQUIRE(res->peers.empty());
    }
    SECTION("Failure reason")
    {
        auto res = parse_announce_response("d14:failure reason6:bannede");
        REQUIRE(!res.has_value());
        REQUIRE(res.error().kind == TrackerErrorKind::TrackerFailure);
        REQUIRE(res.error().message == "banned");
    }
    SECTION("Malformed responses")
    {
        REQUIRE(parse_announce_response("not bencode").error().kind ==
                TrackerErrorKind::MalformedResponse);
        REQUIRE(parse_announce_response("li1ee").error().kind ==
                TrackerErrorKind::MalformedResponse);
        REQUIRE(parse_announce_response("d5:peers0:e").error().kind ==
                TrackerErrorKind::MalformedResponse);
        REQUIRE(parse_announce_response("d8:intervali0e5:peers0:e").error().kind ==
                TrackerErrorKind::MalformedResponse);
        REQUIRE(parse_announce_response("d8:intervali900ee").error().kind ==
                TrackerErrorKind::MalformedResponse);
        REQUIRE(parse_announce_response("d8:intervali900e5:peersi1ee").error().kind ==
                TrackerErrorKind::MalformedResponse);
        REQUIRE(parse_announce_response("d14:failure reasoni1ee").error().kind ==
                TrackerErrorKind::MalformedResponse);
    }
}
