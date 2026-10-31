#include "trackerclient.hpp"
#include "response.hpp"
#include "url.hpp"

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl/ssl_stream.hpp>
#include <boost/url/scheme.hpp>
#include <boost/url/url_view.hpp>
#include <openssl/tls1.h>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;

namespace
{

constexpr auto use_nothrow = asio::as_tuple(asio::use_awaitable);
constexpr auto IO_TIMEOUT = std::chrono::seconds{15};
constexpr std::size_t MAX_RESPONSE_BODY = 1024 * 1024;

std::unexpected<bite::tracker::TrackerError>
network_error(const boost::system::error_code &ec)
{
    return std::unexpected(bite::tracker::TrackerError{
        .kind = ec == boost::beast::error::timeout
                    ? bite::tracker::TrackerErrorKind::Timeout
                    : bite::tracker::TrackerErrorKind::Network,
        .message = ec.message(),
    });
}

template <typename Stream>
asio::awaitable<bite::tracker::AnnounceResult> do_request(Stream &stream,
                                                          boost::urls::url_view url)
{
    http::request<http::empty_body> req{http::verb::get, url.encoded_target(), 11};
    req.set(http::field::host, url.encoded_host_and_port());
    req.set(http::field::user_agent, "bite bittorrent library");
    req.keep_alive(false);

    beast::get_lowest_layer(stream).expires_after(IO_TIMEOUT);
    if (auto [ec, n] = co_await http::async_write(stream, req, use_nothrow); ec) {
        co_return network_error(ec);
    }

    beast::flat_buffer buffer;
    http::response_parser<http::string_body> parser;
    parser.body_limit(MAX_RESPONSE_BODY);

    beast::get_lowest_layer(stream).expires_after(IO_TIMEOUT);
    if (auto [ec, n] = co_await http::async_read(stream, buffer, parser, use_nothrow);
        ec) {
        co_return network_error(ec);
    }

    const auto &res = parser.get();
    if (res.result() != http::status::ok) {
        co_return std::unexpected(bite::tracker::TrackerError{
            .kind = bite::tracker::TrackerErrorKind::HttpStatus,
            .message =
                std::format("{} {}", res.result_int(), std::string_view(res.reason())),
        });
    }

    co_return bite::tracker::parse_announce_response(res.body());
}

asio::awaitable<bite::tracker::AnnounceResult>
announce_plain(asio::any_io_executor executor,
               asio::ip::tcp::resolver::results_type endpoints, boost::urls::url_view url)
{
    beast::tcp_stream stream(executor);

    stream.expires_after(IO_TIMEOUT);
    if (auto [ec, endpoint] = co_await stream.async_connect(endpoints, use_nothrow); ec) {
        co_return network_error(ec);
    }

    auto result = co_await do_request(stream, url);

    boost::system::error_code ignored;
    stream.socket().shutdown(asio::ip::tcp::socket::shutdown_both, ignored);

    co_return result;
}

asio::awaitable<bite::tracker::AnnounceResult>
announce_tls(asio::any_io_executor executor, asio::ssl::context &ctx,
             asio::ip::tcp::resolver::results_type endpoints, boost::urls::url_view url)
{
    beast::ssl_stream<beast::tcp_stream> stream(executor, ctx);
    const std::string host = url.host_address();

    if (url.host_type() == boost::urls::host_type::name &&
        !SSL_set_tlsext_host_name(stream.native_handle(), host.c_str())) {
        co_return network_error(boost::system::error_code(
            static_cast<int>(::ERR_get_error()), asio::error::get_ssl_category()));
    }

    stream.set_verify_mode(asio::ssl::verify_peer);
    stream.set_verify_callback(asio::ssl::host_name_verification(host));

    beast::get_lowest_layer(stream).expires_after(IO_TIMEOUT);
    if (auto [ec, endpoint] = co_await beast::get_lowest_layer(stream).async_connect(
            endpoints, use_nothrow);
        ec) {
        co_return network_error(ec);
    }

    beast::get_lowest_layer(stream).expires_after(IO_TIMEOUT);
    if (auto [ec] =
            co_await stream.async_handshake(asio::ssl::stream_base::client, use_nothrow);
        ec) {
        co_return network_error(ec);
    }

    auto result = co_await do_request(stream, url);

    beast::get_lowest_layer(stream).expires_after(IO_TIMEOUT);
    [[maybe_unused]] auto [shutdown_ec] = co_await stream.async_shutdown(use_nothrow);

    co_return result;
}

} // namespace

bite::tracker::HttpTrackerClient::HttpTrackerClient(boost::asio::any_io_executor executor,
                                                    boost::asio::ssl::context &ssl_ctx)
    : m_executor(std::move(executor))
    , m_ssl_ctx(ssl_ctx)
{
}

boost::asio::awaitable<bite::tracker::AnnounceResult>
bite::tracker::HttpTrackerClient::announce(std::string_view announce_url,
                                           const bite::tracker::AnnounceRequest &request)
{
    const auto url = build_announce_url(announce_url, request);
    if (!url.has_value()) {
        co_return std::unexpected(url.error());
    }

    const auto scheme = url->scheme_id();
    if (scheme != boost::urls::scheme::http && scheme != boost::urls::scheme::https) {
        co_return std::unexpected(TrackerError{
            .kind = TrackerErrorKind::InvalidUrl,
            .message = std::format("Unsupported tracker scheme: {}",
                                   std::string_view(url->scheme())),
        });
    }

    const bool is_tls = scheme == boost::urls::scheme::https;
    const auto port =
        url->has_port() ? std::string(url->port()) : (is_tls ? "443" : "80");

    asio::ip::tcp::resolver res(m_executor);
    auto [ec, endpoints] =
        co_await res.async_resolve(url->host_address(), port, use_nothrow);
    if (ec) {
        co_return network_error(ec);
    }

    if (is_tls) {
        co_return co_await announce_tls(m_executor, m_ssl_ctx, endpoints, *url);
    } else {
        co_return co_await announce_plain(m_executor, endpoints, *url);
    }
}
