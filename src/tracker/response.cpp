#include "response.hpp"
#include "bencode.hpp"

#include <algorithm>

namespace
{
static constexpr auto FAILURE_REASON_RESPONSE_KEY = "failure reason";
static constexpr auto INTERVAL_RESPONSE_KEY = "interval";
static constexpr auto PEERS_RESPONSE_KEY = "peers";

static constexpr auto IP_PEER_DICT_KEY = "ip";
static constexpr auto PORT_PEER_DICT_KEY = "port";
static constexpr auto PEER_ID_PEER_DICT_KEY = "peer id";

static constexpr std::size_t COMPACT_PEER_SIZE = 6;

std::unexpected<bite::tracker::TrackerError> malformed_response(std::string message)
{
    return std::unexpected(bite::tracker::TrackerError{
        .kind = bite::tracker::TrackerErrorKind::MalformedResponse,
        .message = std::move(message),
    });
}

std::optional<bite::tracker::Peer> parse_dict_peer(const bite::bencode::Value &value)
{
    const auto *peer_dict = value.get<bite::bencode::Dict>();
    if (peer_dict == nullptr) {
        return std::nullopt;
    }

    const auto ip_it = peer_dict->find(IP_PEER_DICT_KEY);
    if (ip_it == peer_dict->end() || !ip_it->second.is<bite::bencode::String>()) {
        return std::nullopt;
    }
    boost::system::error_code ec;
    const auto address =
        boost::asio::ip::make_address(*ip_it->second.get<bite::bencode::String>(), ec);
    if (ec) {
        return std::nullopt;
    }

    const auto port_it = peer_dict->find(PORT_PEER_DICT_KEY);
    if (port_it == peer_dict->end() || !port_it->second.is<bite::bencode::Integer>()) {
        return std::nullopt;
    }
    const auto port = *port_it->second.get<bite::bencode::Integer>();
    if (port <= 0 || port > 65535) {
        return std::nullopt;
    }

    bite::tracker::Peer peer{
        .endpoint =
            boost::asio::ip::tcp::endpoint(address, static_cast<std::uint16_t>(port)),
    };

    if (const auto id_it = peer_dict->find(PEER_ID_PEER_DICT_KEY);
        id_it != peer_dict->end()) {
        const auto *id_str = id_it->second.get<bite::bencode::String>();
        if (id_str != nullptr &&
            id_str->size() == std::tuple_size_v<bite::core::PeerId>) {
            bite::core::PeerId id{};
            std::copy_n(id_str->data(), id.size(), id.begin());
            peer.id = id;
        }
    }

    return peer;
}

} // namespace

bite::tracker::AnnounceResult
bite::tracker::parse_announce_response(std::string_view body)
{
    const std::expected decode_result = bite::bencode::decode(body);
    if (!decode_result.has_value() || !decode_result->is<bite::bencode::Dict>()) {
        return malformed_response("Invalid bencode result");
    }

    const auto res_dict = decode_result->get<bite::bencode::Dict>();
    if (res_dict->contains(FAILURE_REASON_RESPONSE_KEY)) {
        if (const auto reason =
                res_dict->at(FAILURE_REASON_RESPONSE_KEY).get<bite::bencode::String>()) {
            return std::unexpected(bite::tracker::TrackerError{
                .kind = bite::tracker::TrackerErrorKind::TrackerFailure,
                .message = *reason,
            });
        }
        return malformed_response("Failure reason key exists, but is not a string");
    }

    if (!res_dict->contains(INTERVAL_RESPONSE_KEY)) {
        return malformed_response("Missing `interval` key");
    }
    const auto interval_res =
        res_dict->at(INTERVAL_RESPONSE_KEY).get<bite::bencode::Integer>();
    if (!interval_res || *interval_res <= 0) {
        return malformed_response("Invalid value for `interval` key");
    }
    if (!res_dict->contains(PEERS_RESPONSE_KEY)) {
        return malformed_response("Missing `peers` key");
    }

    bite::tracker::AnnounceResponse response{
        .interval = std::chrono::seconds{*interval_res},
    };

    const auto &peers_val = res_dict->at(PEERS_RESPONSE_KEY);
    if (const auto *compact_peers = peers_val.get<bite::bencode::String>()) {
        auto peers_res = bite::tracker::parse_compact_peers(*compact_peers);
        if (!peers_res.has_value()) {
            return std::unexpected(std::move(peers_res.error()));
        }
        response.peers = std::move(*peers_res);
        return response;
    }
    if (const auto *peer_list = peers_val.get<bite::bencode::List>()) {
        response.peers.reserve(peer_list->size());
        for (const auto &peer_val : *peer_list) {
            if (auto peer = parse_dict_peer(peer_val)) {
                response.peers.push_back(std::move(*peer));
            }
        }
        return response;
    }
    return malformed_response("Invalid value for `peers` key");
}

std::expected<std::vector<bite::tracker::Peer>, bite::tracker::TrackerError>
bite::tracker::parse_compact_peers(std::string_view compact_body)
{
    if (compact_body.size() % COMPACT_PEER_SIZE != 0) {
        return malformed_response("Compact peers length is not a multiple of 6");
    }

    std::vector<bite::tracker::Peer> peers;
    peers.reserve(compact_body.size() / COMPACT_PEER_SIZE);

    for (std::size_t i = 0; i < compact_body.size(); i += COMPACT_PEER_SIZE) {
        const auto *bytes =
            reinterpret_cast<const std::uint8_t *>(compact_body.data() + i);

        boost::asio::ip::address_v4::bytes_type address_bytes{};
        std::copy_n(bytes, address_bytes.size(), address_bytes.begin());

        const auto port = static_cast<std::uint16_t>((bytes[4] << 8) | bytes[5]);
        if (port == 0) {
            continue;
        }

        peers.push_back(bite::tracker::Peer{
            .endpoint = boost::asio::ip::tcp::endpoint(
                boost::asio::ip::address_v4(address_bytes), port),
        });
    }

    return peers;
}
