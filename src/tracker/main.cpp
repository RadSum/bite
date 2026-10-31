#include "metainfo.hpp"
#include "trackerclient.hpp"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>

#include <exception>
#include <fstream>
#include <print>
#include <random>

namespace asio = boost::asio;

namespace
{

constexpr std::uint16_t LISTEN_PORT = 6881;
constexpr std::size_t MAX_PRINTED_PEERS = 10;

bite::core::PeerId generate_peer_id()
{
    static constexpr std::string_view PREFIX = "-BI0001-";
    static constexpr std::string_view ALPHABET =
        "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

    std::random_device rd;
    std::uniform_int_distribution<std::size_t> dist(0, ALPHABET.size() - 1);

    bite::core::PeerId id{};
    std::copy(PREFIX.begin(), PREFIX.end(), id.begin());
    for (std::size_t i = PREFIX.size(); i < id.size(); ++i) {
        id[i] = static_cast<std::uint8_t>(ALPHABET[dist(rd)]);
    }
    return id;
}

std::string_view error_kind_to_string(bite::tracker::TrackerErrorKind kind)
{
    using enum bite::tracker::TrackerErrorKind;
    switch (kind) {
    case InvalidUrl:
        return "Invalid URL";
    case Network:
        return "Network error";
    case Timeout:
        return "Timeout";
    case HttpStatus:
        return "HTTP status";
    case MalformedResponse:
        return "Malformed response";
    case TrackerFailure:
        return "Tracker failure";
    }
    return "Unknown error";
}

void print_error(const bite::tracker::TrackerError &error)
{
    std::println("Announce failed: {}: {}", error_kind_to_string(error.kind),
                 error.message);
}

void print_response(const bite::tracker::AnnounceResponse &response)
{
    std::println("Interval: {}s", response.interval.count());
    std::println("Peers: {}", response.peers.size());
    for (std::size_t i = 0; i < response.peers.size() && i < MAX_PRINTED_PEERS; ++i) {
        const auto &endpoint = response.peers[i].endpoint;
        std::println(" - {}:{}", endpoint.address().to_string(), endpoint.port());
    }
    if (response.peers.size() > MAX_PRINTED_PEERS) {
        std::println(" ... and {} more", response.peers.size() - MAX_PRINTED_PEERS);
    }
}

asio::awaitable<int> run(bite::tracker::HttpTrackerClient &client,
                         bite::torrent::Metainfo meta)
{
    bite::tracker::AnnounceRequest request{
        .info_hash = meta.info_hash,
        .peer_id = generate_peer_id(),
        .port = LISTEN_PORT,
        .left = meta.total_size(),
        .event = bite::tracker::AnnounceEvent::Started,
    };

    std::println("Announcing to {}", meta.announce);
    const auto result = co_await client.announce(meta.announce, request);
    if (!result.has_value()) {
        print_error(result.error());
        co_return 1;
    }
    print_response(*result);

    // Let the tracker drop us from the swarm again
    request.event = bite::tracker::AnnounceEvent::Stopped;
    if (const auto stopped = co_await client.announce(meta.announce, request);
        !stopped.has_value()) {
        print_error(stopped.error());
    }

    co_return 0;
}

} // namespace

int main(int argc, char *argv[])
{
    if (argc < 2) {
        std::println("Usage {} <path-to-torrent-file>", argv[0]);
        return 1;
    }

    const std::string file_path = argv[1];
    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
        std::println("Error: Could not open file: {}", file_path);
        return 1;
    }

    std::string raw_bytes((std::istreambuf_iterator<char>(file)),
                          std::istreambuf_iterator<char>());

    auto meta_res = bite::torrent::parse_metainfo(raw_bytes);
    if (!meta_res.has_value()) {
        std::println("Failed to parse torrent metainfo");
        return 1;
    }

    asio::io_context ioc;
    asio::ssl::context ssl_ctx(asio::ssl::context::tls_client);
    ssl_ctx.set_default_verify_paths();

    bite::tracker::HttpTrackerClient client(ioc.get_executor(), ssl_ctx);

    int exit_code = 1;
    asio::co_spawn(ioc, run(client, std::move(*meta_res)),
                   [&exit_code](std::exception_ptr e, int code) {
                       if (e) {
                           try {
                               std::rethrow_exception(e);
                           } catch (const std::exception &ex) {
                               std::println("Unhandled exception: {}", ex.what());
                           }
                           return;
                       }
                       exit_code = code;
                   });
    ioc.run();

    return exit_code;
}
