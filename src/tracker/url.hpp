#pragma once

#include "announce.hpp"

#include <boost/url/url.hpp>
#include <expected>

namespace bite::tracker
{

std::expected<boost::urls::url, bite::tracker::TrackerError>
build_announce_url(std::string_view base_url, const AnnounceRequest &request);

} // namespace bite::tracker
