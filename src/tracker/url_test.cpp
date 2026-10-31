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

std::string_view query_of(const boost::urls::url &url)
{
    const auto query = url.encoded_query();
    return {query.data(), query.size()};
}

std::string_view target_of(const boost::urls::url &url)
{
    const auto target = url.encoded_target();
    return {target.data(), target.size()};
}

std::optional<std::string> param_of(const boost::urls::url &url, std::string_view key)
{
    const auto params = url.params();
    const auto it = params.find(key);
    if (it == params.end()) {
        return std::nullopt;
    }
    return (*it).value;
}

} // namespace

TEST_CASE("Announce URL Building", "[tracker][url]")
{
    SECTION("Basic request")
    {
        auto res = build_announce_url("http://tracker.example/announce", make_request());
        REQUIRE(res.has_value());
        REQUIRE(res->scheme() == "http");
        REQUIRE(res->host() == "tracker.example");
        REQUIRE(!res->has_port());
        REQUIRE(res->path() == "/announce");
        REQUIRE(query_of(*res) == "info_hash=aaaaaaaaaaaaaaaaaaaa"
                                  "&peer_id=bbbbbbbbbbbbbbbbbbbb"
                                  "&port=6881&uploaded=10&downloaded=20&left=30");
    }
    SECTION("Explicit port and target")
    {
        auto res =
            build_announce_url("https://tracker.example:6969/announce", make_request());
        REQUIRE(res.has_value());
        REQUIRE(res->scheme() == "https");
        REQUIRE(res->port_number() == 6969);
        REQUIRE(target_of(*res).starts_with("/announce?info_hash="));
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
        REQUIRE(query_of(*res).starts_with(
            "info_hash=a%FF~%20%26%00%00%00%00%00%00%00%00%00%00%00%00%00%00%00&"));
    }
    SECTION("Binary bytes decode back to the original")
    {
        auto request = make_request();
        for (std::size_t i = 0; i < request.info_hash.size(); ++i) {
            request.info_hash[i] = static_cast<std::uint8_t>(i * 13);
        }

        auto res = build_announce_url("http://tracker.example/announce", request);
        REQUIRE(res.has_value());
        const auto info_hash = param_of(*res, "info_hash");
        REQUIRE(info_hash.has_value());
        REQUIRE(*info_hash ==
                std::string(reinterpret_cast<const char *>(request.info_hash.data()),
                            request.info_hash.size()));
    }
    SECTION("Existing query is preserved")
    {
        auto res = build_announce_url("https://tracker.example/announce?key=abc",
                                      make_request());
        REQUIRE(res.has_value());
        REQUIRE(query_of(*res).starts_with("key=abc&info_hash="));
        REQUIRE(param_of(*res, "key") == "abc");
    }
    SECTION("Event is appended when set")
    {
        auto request = make_request();
        request.event = AnnounceEvent::Started;

        auto res = build_announce_url("http://tracker.example/announce", request);
        REQUIRE(res.has_value());
        REQUIRE(param_of(*res, "event") == "started");
    }
    SECTION("Event is omitted when unset")
    {
        auto res = build_announce_url("http://tracker.example/announce", make_request());
        REQUIRE(res.has_value());
        REQUIRE(!param_of(*res, "event").has_value());
    }
    SECTION("Invalid URL")
    {
        auto res = build_announce_url("not a url", make_request());
        REQUIRE(!res.has_value());
        REQUIRE(res.error().kind == TrackerErrorKind::InvalidUrl);
    }
}
