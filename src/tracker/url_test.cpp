#include "url.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace bite::tracker;

namespace
{

AnnounceRequest make_request()
{
    AnnounceRequest request{
        .port = 6881,
        .uploaded = 10,
        .downloaded = 20,
        .left = 30,
    };
    request.info_hash.fill('a');
    request.peer_id.fill('b');
    return request;
}

} // namespace

TEST_CASE("Announce URL Building", "[tracker][url]")
{
    SECTION("Basic request")
    {
        auto res = build_announce_url("http://tracker.example/announce", make_request());
        REQUIRE(res.has_value());
        REQUIRE(*res == "http://tracker.example/announce"
                        "?info_hash=aaaaaaaaaaaaaaaaaaaa"
                        "&peer_id=bbbbbbbbbbbbbbbbbbbb"
                        "&port=6881&uploaded=10&downloaded=20&left=30");
    }
    SECTION("Binary bytes are percent encoded")
    {
        auto request = make_request();
        request.info_hash.fill(0x00);
        request.info_hash[0] = 'a';
        request.info_hash[1] = 0xFF;
        request.info_hash[2] = '~';
        request.info_hash[3] = ' ';
        request.info_hash[4] = '&';

        auto res = build_announce_url("http://tracker.example/announce", request);
        REQUIRE(res.has_value());
        REQUIRE(res->contains("info_hash=a%FF~%20%26"
                              "%00%00%00%00%00%00%00%00%00%00%00%00%00%00%00&"));
    }
    SECTION("Existing query is preserved")
    {
        auto res = build_announce_url("https://tracker.example/announce?key=abc",
                                      make_request());
        REQUIRE(res.has_value());
        REQUIRE(res->starts_with("https://tracker.example/announce?key=abc&info_hash="));
    }
    SECTION("Event is appended when set")
    {
        auto request = make_request();
        request.event = AnnounceEvent::Started;

        auto res = build_announce_url("http://tracker.example/announce", request);
        REQUIRE(res.has_value());
        REQUIRE(res->ends_with("&event=started"));
    }
    SECTION("Event is omitted when unset")
    {
        auto res = build_announce_url("http://tracker.example/announce", make_request());
        REQUIRE(res.has_value());
        REQUIRE(!res->contains("event="));
    }
    SECTION("Invalid URL")
    {
        auto res = build_announce_url("not a url", make_request());
        REQUIRE(!res.has_value());
        REQUIRE(res.error().kind == TrackerErrorKind::InvalidUrl);
    }
}
