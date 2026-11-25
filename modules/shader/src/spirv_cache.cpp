#include <astral/shader/spirv_cache.hpp>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <functional>
#include <system_error>
#include <thread>

namespace astral {

namespace {

constexpr char kMagic[4] = {'A', 'S', 'P', 'V'};
constexpr std::uint32_t kFormatVersion = 1;
constexpr std::uint32_t kSpirvMagic = 0x07230203u;
constexpr std::size_t kHeaderSize = 4 + 4 + 32 + 8 + 8;
constexpr std::size_t kSpirvHeaderWords = 5;
constexpr std::uint64_t kMaxWords = std::uint64_t{1} << 28; // 1 GiB: far beyond any shader

std::uint64_t checksum(const std::uint8_t* data, std::size_t size) {
    std::uint64_t h = 14695981039346656037ull; // FNV-1a 64: detects truncation and bit rot, not an attack
    for (std::size_t i = 0; i < size; ++i) h = (h ^ data[i]) * 1099511628211ull;
    return h;
}

void put_u32(std::uint8_t* p, std::uint32_t v) { for (int i = 0; i < 4; ++i) p[i] = static_cast<std::uint8_t>(v >> (8 * i)); }
void put_u64(std::uint8_t* p, std::uint64_t v) { for (int i = 0; i < 8; ++i) p[i] = static_cast<std::uint8_t>(v >> (8 * i)); }
std::uint32_t get_u32(const std::uint8_t* p) {
    std::uint32_t v = 0;
    for (int i = 0; i < 4; ++i) v |= std::uint32_t{p[i]} << (8 * i);
    return v;
}
std::uint64_t get_u64(const std::uint8_t* p) {
    std::uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v |= std::uint64_t{p[i]} << (8 * i);
    return v;
}

/// Reads and validates one entry; false for anything that is not exactly what store() wrote for `key`.
bool read_entry(const std::filesystem::path& file, const SpirvCacheKey& key, std::vector<std::uint32_t>& spirv) {
    std::ifstream in(file, std::ios::binary | std::ios::ate);
    if (!in) return false;
    const std::streamoff total = in.tellg();
    if (total < static_cast<std::streamoff>(kHeaderSize)) return false;
    std::uint8_t header[kHeaderSize];
    in.seekg(0);
    if (!in.read(reinterpret_cast<char*>(header), kHeaderSize)) return false;
    if (std::memcmp(header, kMagic, 4) != 0 || get_u32(header + 4) != kFormatVersion) return false;
    if (std::memcmp(header + 8, key.bytes.data(), key.bytes.size()) != 0) return false;
    const std::uint64_t words = get_u64(header + 40);
    if (words < kSpirvHeaderWords || words > kMaxWords) return false;
    if (static_cast<std::uint64_t>(total) != kHeaderSize + words * 4) return false;
    std::vector<std::uint32_t> data(static_cast<std::size_t>(words));
    if (!in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(words * 4))) return false;
    // The payload is stored little-endian; every supported platform is.
    if (checksum(reinterpret_cast<const std::uint8_t*>(data.data()), data.size() * 4) != get_u64(header + 48)) return false;
    if (data[0] != kSpirvMagic) return false;
    spirv = std::move(data);
    return true;
}

} // namespace

std::string SpirvCacheKey::hex() const {
    static constexpr char digits[] = "0123456789abcdef";
    std::string out;
    out.reserve(bytes.size() * 2);
    for (const std::uint8_t b : bytes) {
        out.push_back(digits[b >> 4]);
        out.push_back(digits[b & 15]);
    }
    return out;
}

SpirvCache::SpirvCache(std::filesystem::path directory) : directory_(std::move(directory)) {}

SpirvCache::Stats SpirvCache::stats() const noexcept {
    return {hits_.load(), misses_.load(), rejected_.load(), stores_.load()};
}

std::filesystem::path SpirvCache::path_for(const SpirvCacheKey& key) const { return directory_ / (key.hex() + ".spv"); }

bool SpirvCache::lookup(const SpirvCacheKey& key, std::vector<std::uint32_t>& spirv) {
    if (directory_.empty()) {
        ++misses_;
        return false;
    }
    const std::filesystem::path file = path_for(key);
    std::error_code ec;
    if (!std::filesystem::exists(file, ec)) {
        ++misses_;
        return false;
    }
    if (!read_entry(file, key, spirv)) {
        ++misses_;
        ++rejected_;
        return false;
    }
    ++hits_;
    std::filesystem::last_write_time(file, std::filesystem::file_time_type::clock::now(), ec); // mark as used (best effort)
    return true;
}

void SpirvCache::store(const SpirvCacheKey& key, std::span<const std::uint32_t> spirv) {
    if (directory_.empty() || spirv.size() < kSpirvHeaderWords) return;
    std::error_code ec;
    std::filesystem::create_directories(directory_, ec);
    if (ec) return;

    std::uint8_t header[kHeaderSize];
    std::memcpy(header, kMagic, 4);
    put_u32(header + 4, kFormatVersion);
    std::memcpy(header + 8, key.bytes.data(), key.bytes.size());
    put_u64(header + 40, spirv.size());
    put_u64(header + 48, checksum(reinterpret_cast<const std::uint8_t*>(spirv.data()), spirv.size() * 4));

    // Unique per writer (thread, call, moment) so two compiles of the same key never share a temporary file.
    const std::filesystem::path file = path_for(key);
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    std::filesystem::path temp = file;
    temp += "." + std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id()) ^ static_cast<std::size_t>(stamp)) +
            "." + std::to_string(temp_counter_.fetch_add(1)) + ".tmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out) return;
        out.write(reinterpret_cast<const char*>(header), kHeaderSize);
        out.write(reinterpret_cast<const char*>(spirv.data()), static_cast<std::streamsize>(spirv.size() * 4));
        if (!out.good()) {
            out.close();
            std::filesystem::remove(temp, ec);
            return;
        }
    }
    std::filesystem::rename(temp, file, ec); // replaces an existing (possibly corrupt) entry
    if (ec) {
        std::filesystem::remove(temp, ec);
        return;
    }
    ++stores_;
}

std::uint64_t SpirvCache::prune(std::uint64_t max_bytes) {
    struct Entry {
        std::filesystem::path path;
        std::filesystem::file_time_type used;
        std::uint64_t size;
    };
    std::vector<Entry> entries;
    std::uint64_t total = 0;
    std::error_code ec;
    if (directory_.empty()) return 0;
    for (std::filesystem::directory_iterator it(directory_, ec), end; !ec && it != end; it.increment(ec)) {
        if (it->path().extension() != ".spv") continue;
        std::error_code entry_ec;
        const auto size = it->file_size(entry_ec);
        const auto used = it->last_write_time(entry_ec);
        if (entry_ec) continue;
        entries.push_back({it->path(), used, size});
        total += size;
    }
    std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) { return a.used < b.used; });
    for (const Entry& e : entries) {
        if (total <= max_bytes) break;
        std::error_code remove_ec;
        if (std::filesystem::remove(e.path, remove_ec)) total -= e.size;
    }
    return total;
}

} // namespace astral
