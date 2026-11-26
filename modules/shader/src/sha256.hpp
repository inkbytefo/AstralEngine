#pragma once
// Private to Astral::Shader: SHA-256 for content-addressed cache keys (not used for security).

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace astral::detail {

using Sha256Digest = std::array<std::uint8_t, 32>;

/// Incremental SHA-256 (FIPS 180-4).
class Sha256 {
public:
    Sha256() noexcept { reset(); }
    void reset() noexcept;
    void update(std::span<const std::uint8_t> bytes) noexcept;
    void update(std::string_view text) noexcept {
        update(std::span(reinterpret_cast<const std::uint8_t*>(text.data()), text.size()));
    }
    /// Length-prefixed: update_field("ab"), update_field("c") differs from ("a"), ("bc").
    void update_field(std::string_view text) noexcept;
    void update_u32(std::uint32_t value) noexcept;
    [[nodiscard]] Sha256Digest finish() noexcept;

private:
    void block(const std::uint8_t* data) noexcept;

    std::array<std::uint32_t, 8> state_{};
    std::array<std::uint8_t, 64> buffer_{};
    std::uint64_t total_ = 0;
    std::size_t buffered_ = 0;
};

} // namespace astral::detail
