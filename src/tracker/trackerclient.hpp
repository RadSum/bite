#pragma once

#include "announce.hpp"

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/ssl/context.hpp>

namespace bite::tracker
{

class HttpTrackerClient
{
public:
    explicit HttpTrackerClient(boost::asio::any_io_executor executor,
                               boost::asio::ssl::context &ssl_ctx);

    boost::asio::awaitable<AnnounceResult> announce(std::string_view announce_url,
                                                    const AnnounceRequest &request);

private:
    boost::asio::any_io_executor m_executor;
    boost::asio::ssl::context &m_ssl_ctx;
};

} // namespace bite::tracker
