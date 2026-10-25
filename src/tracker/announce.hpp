#pragma once

#include <chrono>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <vector>

#include <boost/asio/ip/tcp.hpp>

#include "types.hpp"

namespace bite::tracker
{

enum class AnnounceEvent {
    Started,
    Completed,
    Stopped,
};

struct AnnounceRequest {
    bite::core::InfoHash info_hash{};
    bite::core::PeerId peer_id{};
    std::uint16_t port{0};
    std::uint64_t uploaded{0};
    std::uint64_t downloaded{0};
    std::uint64_t left{0};
    std::optional<AnnounceEvent> event{};
};

struct Peer {
    boost::asio::ip::tcp::endpoint endpoint;
    std::optional<bite::core::PeerId> id{};
};

struct AnnounceResponse {
    std::chrono::seconds interval{0};
    std::vector<Peer> peers{};
};

enum class TrackerErrorKind {
    InvalidUrl,
    Network,
    Timeout,
    HttpStatus,
    MalformedResponse,
    TrackerFailure,
};

struct TrackerError {
    TrackerErrorKind kind;
    std::string message{};
};

using AnnounceResult = std::expected<AnnounceResponse, TrackerError>;

} // namespace bite::tracker
