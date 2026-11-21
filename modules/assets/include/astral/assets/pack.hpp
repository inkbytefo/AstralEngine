#pragma once
// .apak: one file holding every cooked asset of a game, each block optionally zstd-compressed.
//
//   header  "APAK" u32 version, u64 table offset, u32 entry count
//   blocks  entry data, back to back
//   table   per entry: path, u64 offset, u64 stored size, u64 size, u8 compressed, u64 content hash
//
// Entries are found by AssetId (the path, case and separators ignored). Reads are thread-safe.

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace astral {

struct PackEntry {
    std::string path;
    std::uint64_t offset = 0;
    std::uint64_t stored_size = 0; ///< Bytes in the file.
    std::uint64_t size = 0;        ///< Bytes once decompressed.
    bool compressed = false;
    std::uint64_t hash = 0;        ///< Of the decompressed bytes; checked on every read.
};

struct PackWriteOptions {
    int level = 9; ///< zstd level (1 fast .. 19 small). Blocks that do not shrink are stored raw.
};

/// Packs every file under `directory` (except the cook cache) into `pack`, paths relative to it.
/// Written to a temporary file first: a failed write never leaves a broken pack behind.
bool write_pack(const std::filesystem::path& directory, const std::filesystem::path& pack, std::string* error = nullptr,
                const PackWriteOptions& options = {});

class Pack {
public:
    /// Null on a missing or malformed file (`error` says why).
    static std::unique_ptr<Pack> open(const std::filesystem::path& file, std::string* error = nullptr);

    /// The decompressed bytes of an entry; nullopt if absent, unreadable or damaged.
    std::optional<std::vector<std::uint8_t>> read(std::string_view path, std::string* error = nullptr) const;
    [[nodiscard]] const PackEntry* find(std::string_view path) const noexcept;
    [[nodiscard]] std::span<const PackEntry> entries() const noexcept { return entries_; }
    [[nodiscard]] const std::filesystem::path& file() const noexcept { return file_; }

private:
    std::filesystem::path file_;
    std::vector<PackEntry> entries_;
    std::unordered_map<std::uint64_t, std::size_t> by_id_;
};

} // namespace astral
