#include "url.hpp"

#include <array>
#include <boost/url/encode.hpp>
#include <boost/url/parse.hpp>
#include <boost/url/rfc/unreserved_chars.hpp>
#include <boost/url/url.hpp>

namespace
{

template <std::size_t N>
std::string percent_encode(const std::array<std::uint8_t, N> &bytes)
{
    const std::string_view raw(reinterpret_cast<const char *>(bytes.data()),
                               bytes.size());
    return boost::urls::encode(raw, boost::urls::unreserved_chars);
}

std::string_view event_to_string(bite::tracker::AnnounceEvent event)
{
    using enum bite::tracker::AnnounceEvent;
    switch (event) {
    case Started:
        return "started";
    case Completed:
        return "completed";
    case Stopped:
        return "stopped";
    }
    return "";
}

} // namespace

std::expected<std::string, bite::tracker::TrackerError>
bite::tracker::build_announce_url(std::string_view base_url,
                                  const AnnounceRequest &request)
{
    const auto parsed = boost::urls::parse_uri(base_url);
    if (!parsed) {
        return std::unexpected(TrackerError{
            .kind = TrackerErrorKind::InvalidUrl,
            .message = parsed.error().message(),
        });
    }

    boost::urls::url url = *parsed;
    auto params = url.encoded_params();
    params.append({"info_hash", percent_encode(request.info_hash)});
    params.append({"peer_id", percent_encode(request.peer_id)});
    params.append({"port", std::to_string(request.port)});
    params.append({"uploaded", std::to_string(request.uploaded)});
    params.append({"downloaded", std::to_string(request.downloaded)});
    params.append({"left", std::to_string(request.left)});
    if (request.event.has_value()) {
        params.append({"event", event_to_string(*request.event)});
    }

    return std::string(url.buffer());
}
