#pragma once
// Content-addressed on-disk cache of compiled SPIR-V, so a shader is compiled by glslang once per machine and
// not once per launch (docs/modules/shader.md). The key is a SHA-256 over everything the compiler sees: GLSL
// text, every include, stage, entry name, compiler version and target options. A changed engine header or
// generated template therefore never finds a stale entry: it simply misses.
//
// Thread-safe. There is no global instance: the owner (App, a tool, a test) creates one SpirvCache and passes
// a pointer through ShaderCompileServices. Cache problems never fail a compile: a missing, truncated or
// corrupt file counts as a miss and is overwritten by the freshly compiled SPIR-V.

#include <array>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace astral {

/// SHA-256 of one compile request (see SpirvCache).
struct SpirvCacheKey {
    std::array<std::uint8_t, 32> bytes{};
    [[nodiscard]] std::string hex() const;
    friend bool operator==(const SpirvCacheKey&, const SpirvCacheKey&) = default;
};

class SpirvCache {
public:
    struct Stats {
        std::uint64_t hits = 0;     ///< Compiles answered from disk.
        std::uint64_t misses = 0;   ///< Lookups that found nothing usable (the caller compiled).
        std::uint64_t rejected = 0; ///< Of the misses: a file existed but was truncated, corrupt or for another key.
        std::uint64_t stores = 0;   ///< Entries written.
    };

    /// `directory` is created on the first store. An empty path makes a cache that never hits and never writes.
    explicit SpirvCache(std::filesystem::path directory);
    SpirvCache(const SpirvCache&) = delete;
    SpirvCache& operator=(const SpirvCache&) = delete;

    [[nodiscard]] const std::filesystem::path& directory() const noexcept { return directory_; }
    [[nodiscard]] Stats stats() const noexcept;
    /// The file an entry lives in (exposed for tools and tests that inspect or damage entries).
    [[nodiscard]] std::filesystem::path path_for(const SpirvCacheKey& key) const;

    /// True and `spirv` filled when a valid entry exists; counts a hit, or a miss (and a rejection for a bad file).
    [[nodiscard]] bool lookup(const SpirvCacheKey& key, std::vector<std::uint32_t>& spirv);
    /// Writes an entry atomically (temporary file + rename: concurrent readers and writers never see half a file).
    /// Best effort: an unwritable directory is ignored.
    void store(const SpirvCacheKey& key, std::span<const std::uint32_t> spirv);

    /// Deletes the least recently used entries (a hit or a store counts as a use) until the entries take at most
    /// `max_bytes`; returns the size that remains. Call it once at start-up, not per compile.
    std::uint64_t prune(std::uint64_t max_bytes);

private:
    std::filesystem::path directory_;
    std::atomic<std::uint64_t> hits_{0}, misses_{0}, rejected_{0}, stores_{0}, temp_counter_{0};
};

} // namespace astral
