#include <astral/assets/asset_manager.hpp>
#include <astral/assets/pack.hpp>
#include <astral/core/bytes.hpp>

#include "file_util.hpp"

#include <zstd.h>

#include <algorithm>
#include <fstream>

namespace astral {

namespace {
constexpr std::uint8_t magic[4] = {'A', 'P', 'A', 'K'};
constexpr std::uint32_t pack_version = 1;
constexpr std::size_t header_size = 4 + 4 + 8 + 4;
constexpr std::string_view cook_cache = ".astral-cook.json";
} // namespace

bool write_pack(const std::filesystem::path& directory, const std::filesystem::path& pack, std::string* error,
                const PackWriteOptions& options) {
    auto fail = [&](const std::string& why) {
        if (error) *error = "write_pack: " + why;
        return false;
    };
    std::error_code ec;
    if (!std::filesystem::is_directory(directory, ec)) return fail(directory.string() + " is not a folder");
    std::vector<std::string> files;
    for (auto it = std::filesystem::recursive_directory_iterator(directory, ec); !ec && it != std::filesystem::recursive_directory_iterator();
         it.increment(ec)) {
        if (!it->is_regular_file()) continue;
        const std::string rel = detail::relative_path(it->path(), directory);
        if (rel == cook_cache || rel.ends_with(".tmp")) continue;
        files.push_back(rel);
    }
    if (ec) return fail(ec.message());
    std::sort(files.begin(), files.end()); // deterministic packs

    ByteWriter out;
    out.bytes(magic);
    out.u32(pack_version);
    out.u64(0); // table offset, patched below
    out.u32(static_cast<std::uint32_t>(files.size()));
    std::vector<PackEntry> entries;
    std::vector<std::uint8_t> compressed;
    for (const std::string& rel : files) {
        std::string read_error;
        auto data = detail::read_file(directory / rel, &read_error);
        if (!data) return fail(read_error);
        PackEntry e{.path = rel, .offset = out.size(), .stored_size = data->size(), .size = data->size(),
                    .compressed = false, .hash = detail::hash_bytes(*data)};
        compressed.resize(ZSTD_compressBound(data->size()));
        const std::size_t n = ZSTD_compress(compressed.data(), compressed.size(), data->data(), data->size(), options.level);
        if (!ZSTD_isError(n) && n < data->size() - data->size() / 20) { // keep it if it saves at least 5%
            e.compressed = true;
            e.stored_size = n;
            out.bytes(std::span(compressed.data(), n));
        } else {
            out.bytes(*data);
        }
        entries.push_back(std::move(e));
    }
    const std::uint64_t table = out.size();
    for (const PackEntry& e : entries) {
        out.string(e.path);
        out.u64(e.offset);
        out.u64(e.stored_size);
        out.u64(e.size);
        out.u8(e.compressed ? 1 : 0);
        out.u64(e.hash);
    }
    std::vector<std::uint8_t> bytes = out.take();
    for (int i = 0; i < 8; ++i) bytes[8 + static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(table >> (8 * i));
    std::string write_error;
    if (!detail::write_file(pack, bytes, &write_error)) return fail(write_error);
    return true;
}

std::unique_ptr<Pack> Pack::open(const std::filesystem::path& file, std::string* error) {
    auto fail = [&](const std::string& why) -> std::unique_ptr<Pack> {
        if (error) *error = file.string() + ": " + why;
        return nullptr;
    };
    std::ifstream in(file, std::ios::binary | std::ios::ate);
    if (!in) return fail("cannot open");
    const auto file_size = static_cast<std::uint64_t>(in.tellg());
    if (file_size < header_size) return fail("not a pack (too small)");
    std::vector<std::uint8_t> header(header_size);
    in.seekg(0);
    in.read(reinterpret_cast<char*>(header.data()), static_cast<std::streamsize>(header.size()));
    ByteReader h(header);
    const auto m = h.bytes(4);
    if (!std::equal(m.begin(), m.end(), magic)) return fail("not a pack (no APAK header)");
    if (h.u32() != pack_version) return fail("unsupported pack version");
    const std::uint64_t table = h.u64();
    const std::uint32_t count = h.u32();
    if (table < header_size || table > file_size) return fail("damaged (table offset)");
    std::vector<std::uint8_t> table_bytes(static_cast<std::size_t>(file_size - table));
    in.seekg(static_cast<std::streamoff>(table));
    in.read(reinterpret_cast<char*>(table_bytes.data()), static_cast<std::streamsize>(table_bytes.size()));
    if (!in) return fail("cannot read the table");

    auto pack = std::unique_ptr<Pack>(new Pack());
    pack->file_ = file;
    ByteReader r(table_bytes);
    for (std::uint32_t i = 0; i < count && r.ok(); ++i) {
        PackEntry e;
        e.path = r.string();
        e.offset = r.u64();
        e.stored_size = r.u64();
        e.size = r.u64();
        e.compressed = r.u8() != 0;
        e.hash = r.u64();
        if (!r.ok()) break;
        if (e.offset < header_size || e.offset > table || e.stored_size > table - e.offset ||
            (!e.compressed && e.size != e.stored_size)) {
            return fail("damaged (entry " + e.path + ")");
        }
        pack->by_id_[AssetId::from_path(e.path).value] = pack->entries_.size();
        pack->entries_.push_back(std::move(e));
    }
    if (!r.ok() || r.remaining() != 0) return fail("damaged (table)");
    return pack;
}

const PackEntry* Pack::find(std::string_view path) const noexcept {
    auto it = by_id_.find(AssetId::from_path(path).value);
    return it == by_id_.end() ? nullptr : &entries_[it->second];
}

std::optional<std::vector<std::uint8_t>> Pack::read(std::string_view path, std::string* error) const {
    auto fail = [&](const std::string& why) -> std::optional<std::vector<std::uint8_t>> {
        if (error) *error = std::string(path) + ": " + why;
        return std::nullopt;
    };
    const PackEntry* e = find(path);
    if (!e) return fail("not in " + file_.filename().string());
    std::ifstream in(file_, std::ios::binary); // one stream per read: thread-safe without locks
    std::vector<std::uint8_t> stored(static_cast<std::size_t>(e->stored_size));
    in.seekg(static_cast<std::streamoff>(e->offset));
    if (!in.read(reinterpret_cast<char*>(stored.data()), static_cast<std::streamsize>(stored.size()))) return fail("cannot read");
    std::vector<std::uint8_t> data;
    if (e->compressed) {
        // The frame states its size: check it before trusting the table's number with an allocation.
        if (ZSTD_getFrameContentSize(stored.data(), stored.size()) != e->size) return fail("damaged (frame size)");
        data.resize(static_cast<std::size_t>(e->size));
        const std::size_t n = ZSTD_decompress(data.data(), data.size(), stored.data(), stored.size());
        if (ZSTD_isError(n) || n != data.size()) return fail("damaged (decompression)");
    } else {
        data = std::move(stored);
    }
    if (detail::hash_bytes(data) != e->hash) return fail("damaged (content hash)");
    return data;
}

} // namespace astral
