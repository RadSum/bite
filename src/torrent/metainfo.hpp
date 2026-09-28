#pragma once

#include <array>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <vector>

namespace bite::torrent
{

struct FileInfo {
    std::uint64_t length;
    std::vector<std::string> path;
};

using InfoHash = std::array<std::uint8_t, 20>;
using PieceHash = std::array<std::uint8_t, 20>;

struct Metainfo {
    std::string announce;

    std::string name;
    std::uint64_t piece_length{0};
    std::vector<PieceHash> piece_hashes;

    std::optional<std::uint64_t> length;
    std::vector<FileInfo> files;

    InfoHash info_hash{};

    bool is_multi_file() const { return !files.empty(); }

    std::uint64_t total_size() const
    {
        if (length.has_value()) {
            return *length;
        }
        std::uint64_t size{0};
        for (const auto &file : files) {
            size += file.length;
        }
        return size;
    }

    std::size_t num_pieces() const { return piece_hashes.size(); }
};

enum class MetainfoError {
    InvalidBencode,
    MissingInfoDict,
    MissingAnnounce,
    MissingPieceLength,
    InvalidPiecesLength,
    InvalidFileStructure,
    MissingPieces,
};

std::expected<Metainfo, MetainfoError> parse_metainfo(std::string_view raw_bencode_bytes);

} // namespace bite::torrent
