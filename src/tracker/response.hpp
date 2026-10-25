#pragma once

#include "announce.hpp"

#include <expected>
#include <string_view>
#include <vector>

namespace bite::tracker
{

bite::tracker::AnnounceResult parse_announce_response(std::string_view body);

std::expected<std::vector<bite::tracker::Peer>, bite::tracker::TrackerError>
parse_compact_peers(std::string_view compact_body);

} // namespace bite::tracker
