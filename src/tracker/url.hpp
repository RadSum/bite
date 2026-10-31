#pragma once

#include "announce.hpp"

#include <expected>

namespace bite::tracker
{

std::expected<std::string, bite::tracker::TrackerError>
build_announce_url(std::string_view base_url, const AnnounceRequest &request);

} // namespace bite::tracker
