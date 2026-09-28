#include "metainfo.hpp"
#include "bencode.hpp"

#include <algorithm>
#include <openssl/sha.h>

namespace
{

static constexpr auto ANNOUNCE_TORRENT_DICT_KEY = "announce";
static constexpr auto INFO_TORRENT_DICT_KEY = "info";

static constexpr auto LENGTH_INFO_DICT_KEY = "length";
static constexpr auto PIECE_LENGTH_INFO_DICT_KEY = "piece length";
static constexpr auto PIECES_INFO_DICT_KEY = "pieces";
static constexpr auto FILES_INFO_DICT_KEY = "files";
static constexpr auto NAME_INFO_DICT_KEY = "name";

static constexpr auto LENGTH_FILES_DICT_KEY = "length";
static constexpr auto PATH_FILES_DICT_KEY = "path";

bite::torrent::InfoHash sha1(std::string_view bytes)
{
    bite::torrent::InfoHash digest{};
    ::SHA1(reinterpret_cast<const unsigned char *>(bytes.data()), bytes.size(),
           digest.data());
    return digest;
}

std::optional<std::string_view> extract_raw_info_dict(std::string_view raw,
                                                      std::string_view key_to_find)
{
    if (raw.empty() || raw.front() != 'd') {
        return std::nullopt;
    }

    std::string_view cursor = raw;
    cursor.remove_prefix(1);

    while (!cursor.empty() && cursor.front() != 'e') {
        const auto key_res = bite::bencode::decode_some(cursor);
        if (!key_res || !key_res->is<bite::bencode::String>()) {
            return std::nullopt;
        }

        const auto &key = *key_res->get<bite::bencode::String>();
        const std::string_view val_start = cursor;

        auto val_res = bite::bencode::decode_some(cursor);
        if (!val_res.has_value()) {
            return std::nullopt;
        }

        if (key == key_to_find) {
            const std::size_t val_len = cursor.data() - val_start.data();
            return std::string_view(val_start.data(), val_len);
        }
    }
    return std::nullopt;
}

std::optional<bite::torrent::MetainfoError>
parse_info_dict(const bite::bencode::Dict &info_dict, bite::torrent::Metainfo &meta)
{
    using enum bite::torrent::MetainfoError;

    const auto pl_it = info_dict.find(PIECE_LENGTH_INFO_DICT_KEY);
    if (pl_it == info_dict.end() || !pl_it->second.is<bite::bencode::Integer>()) {
        return MissingPieceLength;
    }
    meta.piece_length =
        static_cast<std::uint64_t>(*pl_it->second.get<bite::bencode::Integer>());

    const auto pieces_it = info_dict.find(PIECES_INFO_DICT_KEY);
    if (pieces_it == info_dict.end() || !pieces_it->second.is<bite::bencode::String>()) {
        return MissingPieces;
    }
    const auto &pieces_str = *pieces_it->second.get<bite::bencode::String>();
    if (pieces_str.length() % 20 != 0) {
        return InvalidPiecesLength;
    }

    meta.piece_hashes.reserve(pieces_str.size() / 20);
    for (std::size_t i = 0; i < pieces_str.size(); i += 20) {
        bite::torrent::PieceHash hash{};
        std::copy_n(pieces_str.data() + i, 20, hash.begin());
        meta.piece_hashes.push_back(hash);
    }

    if (auto name_it = info_dict.find(NAME_INFO_DICT_KEY);
        name_it != info_dict.end() && name_it->second.is<bite::bencode::String>()) {
        meta.name = *name_it->second.get<bite::bencode::String>();
    }

    const bool has_length = info_dict.contains(LENGTH_INFO_DICT_KEY);
    const bool has_files = info_dict.contains(FILES_INFO_DICT_KEY);
    if (has_files == has_length) {
        return InvalidFileStructure;
    }

    if (has_files) {
        const auto *file_list =
            info_dict.at(FILES_INFO_DICT_KEY).get<bite::bencode::List>();
        if (file_list == nullptr || file_list->empty()) {
            return InvalidFileStructure;
        }

        meta.files.reserve(file_list->size());

        for (const auto &file_info_val : *file_list) {
            const auto *file_info_dict = file_info_val.get<bite::bencode::Dict>();
            if (file_info_dict == nullptr) {
                return InvalidFileStructure;
            }

            if (!file_info_dict->contains(LENGTH_FILES_DICT_KEY) ||
                !file_info_dict->contains(PATH_FILES_DICT_KEY)) {
                return InvalidFileStructure;
            }

            const auto *flen =
                file_info_dict->at(LENGTH_FILES_DICT_KEY).get<bite::bencode::Integer>();
            if (flen == nullptr || *flen < 0) {
                return InvalidFileStructure;
            }

            const auto *path_list =
                file_info_dict->at(PATH_FILES_DICT_KEY).get<bite::bencode::List>();
            if (path_list == nullptr || path_list->empty()) {
                return InvalidFileStructure;
            }

            bite::torrent::FileInfo file_info;
            file_info.length = static_cast<std::uint64_t>(*flen);
            file_info.path.reserve(path_list->size());

            for (const auto &path_elem : *path_list) {
                const auto *path_str = path_elem.get<bite::bencode::String>();
                if (path_str == nullptr) {
                    return InvalidFileStructure;
                }
                file_info.path.push_back(*path_str);
            }

            meta.files.push_back(std::move(file_info));
        }
    } else if (has_length) {
        const auto *l = info_dict.at(LENGTH_INFO_DICT_KEY).get<bite::bencode::Integer>();
        if (l == nullptr) {
            return InvalidFileStructure;
        }
        meta.length = static_cast<std::uint64_t>(*l);
        return std::nullopt;
    }

    return std::nullopt;
}

} // namespace

std::expected<bite::torrent::Metainfo, bite::torrent::MetainfoError>
bite::torrent::parse_metainfo(std::string_view raw_bencode_bytes)
{
    using enum bite::torrent::MetainfoError;

    auto decode_res = bite::bencode::decode(raw_bencode_bytes);
    if (!decode_res.has_value() || !decode_res->is<bite::bencode::Dict>()) {
        return std::unexpected(InvalidBencode);
    }

    const auto &root_dict = *decode_res->get<bite::bencode::Dict>();

    const auto raw_info_slice =
        extract_raw_info_dict(raw_bencode_bytes, INFO_TORRENT_DICT_KEY);
    if (!raw_info_slice.has_value()) {
        return std::unexpected(MissingInfoDict);
    }

    Metainfo meta;
    meta.info_hash = sha1(*raw_info_slice);
    if (!root_dict.contains(ANNOUNCE_TORRENT_DICT_KEY) ||
        !root_dict.at(ANNOUNCE_TORRENT_DICT_KEY).is<bite::bencode::String>()) {
        return std::unexpected(MissingAnnounce);
    }
    const auto &info_dict_res = root_dict.find(INFO_TORRENT_DICT_KEY);
    if (info_dict_res == root_dict.end() ||
        !info_dict_res->second.is<bite::bencode::Dict>()) {
        return std::unexpected(MissingInfoDict);
    }

    const auto &info_dict = *info_dict_res->second.get<bite::bencode::Dict>();
    if (const auto err = parse_info_dict(info_dict, meta); err.has_value()) {
        return std::unexpected(err.value());
    }

    return meta;
}
