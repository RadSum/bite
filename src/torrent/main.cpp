#include "metainfo.hpp"
#include <fstream>
#include <iomanip>
#include <print>
#include <sstream>

namespace
{

std::string to_hex(const bite::torrent::InfoHash &hash)
{
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (std::uint8_t byte : hash) {
        oss << std::setw(2) << static_cast<int>(byte);
    }
    return oss.str();
}

std::string format_bytes(std::uint64_t bytes)
{
    constexpr double KiB = 1024.0;
    constexpr double MiB = KiB * 1024.0;
    constexpr double GiB = MiB * 1024.0;

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);

    if (bytes >= GiB) {
        oss << (bytes / GiB) << " GiB";
    } else if (bytes >= MiB) {
        oss << (bytes / MiB) << " MiB";
    } else if (bytes >= KiB) {
        oss << (bytes / KiB) << " KiB";
    } else {
        oss << bytes << " Bytes";
    }
    return oss.str();
}

std::string_view error_to_string(bite::torrent::MetainfoError err)
{
    using enum bite::torrent::MetainfoError;
    switch (err) {
    case InvalidBencode:
        return "Invalid Bencode syntax";
    case MissingInfoDict:
        return "Missing or malformed 'info' dictionary";
    case MissingAnnounce:
        return "Missing or malformed 'announce' URL";
    case MissingPieceLength:
        return "Missing or invalid 'piece length'";
    case MissingPieces:
        return "Missing or malformed 'pieces' binary string";
    case InvalidPiecesLength:
        return "Pieces length is not a multiple of 20 bytes";
    case InvalidFileStructure:
        return "Invalid single-file or multi-file layout";
    }
    return "Unknown error";
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
        std::println("Failed to parse torrent metainfo: {}",
                     error_to_string(meta_res.error()));
        return 1;
    }

    const auto &meta = meta_res.value();

    std::println("Name: {}", meta.name.empty() ? "<unnamed>" : meta.name);
    std::println("Info Hash: {}", to_hex(meta.info_hash));
    std::println("Announce: {}", meta.announce);

    std::println("Piece Length: {}", format_bytes(meta.piece_length));
    std::println("Piece count: {}", meta.num_pieces());
    std::println("Total size: {}", format_bytes(meta.total_size()));

    std::println("Layout: {}", (meta.is_multi_file() ? "Multi-file" : "Single-file"));

    if (meta.is_multi_file()) {
        std::println("Files ({})", meta.files.size());
        for (const auto &file : meta.files) {
            std::string relative_path;
            for (std::size_t i = 0; i < file.path.size(); ++i) {
                relative_path += file.path[i];
                if (i + 1 < file.path.size())
                    relative_path += "/";
            }
            std::println(" - {} ({})", relative_path, format_bytes(file.length));
        }
    } else if (meta.length.has_value()) {
        std::println("File size: {}", format_bytes(*meta.length));
    }

    return 0;
}
