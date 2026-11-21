#pragma once
// Internal helpers shared by the cook, the pack and the asset manager.

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace astral::detail {

/// FNV-1a 64 over bytes (content hashes: cook cache, pack integrity).
inline std::uint64_t hash_bytes(std::span<const std::uint8_t> data, std::uint64_t h = 14695981039346656037ull) {
    for (std::uint8_t b : data) {
        h ^= b;
        h *= 1099511628211ull;
    }
    return h;
}

inline std::optional<std::vector<std::uint8_t>> read_file(const std::filesystem::path& path, std::string* error = nullptr) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) {
        if (error) *error = "cannot open " + path.string();
        return std::nullopt;
    }
    const std::streamoff size = in.tellg();
    std::vector<std::uint8_t> data(static_cast<std::size_t>(size < 0 ? 0 : size));
    in.seekg(0);
    if (!data.empty() && !in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()))) {
        if (error) *error = "cannot read " + path.string();
        return std::nullopt;
    }
    return data;
}

/// Writes through a temporary file and renames it over the target: readers never see half a file.
inline bool write_file(const std::filesystem::path& path, std::span<const std::uint8_t> data, std::string* error = nullptr) {
    std::error_code ec;
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path(), ec);
    const std::filesystem::path tmp = path.string() + ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
        if (!out) {
            if (error) *error = "cannot write " + tmp.string();
            return false;
        }
    }
    std::filesystem::rename(tmp, path, ec);
    if (ec) {
        if (error) *error = "cannot replace " + path.string() + ": " + ec.message();
        std::filesystem::remove(tmp, ec);
        return false;
    }
    return true;
}

/// "a/b/c.png" with forward slashes, relative to `root`.
inline std::string relative_path(const std::filesystem::path& file, const std::filesystem::path& root) {
    return std::filesystem::relative(file, root).generic_string();
}

} // namespace astral::detail
