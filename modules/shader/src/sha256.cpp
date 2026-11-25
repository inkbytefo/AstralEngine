#include "sha256.hpp"

#include <algorithm>
#include <bit>

namespace astral::detail {

namespace {

constexpr std::array<std::uint32_t, 64> kRound = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

} // namespace

void Sha256::reset() noexcept {
    state_ = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    total_ = 0;
    buffered_ = 0;
}

void Sha256::block(const std::uint8_t* data) noexcept {
    std::array<std::uint32_t, 64> w{};
    for (std::size_t i = 0; i < 16; ++i) {
        w[i] = (std::uint32_t{data[i * 4]} << 24) | (std::uint32_t{data[i * 4 + 1]} << 16) |
               (std::uint32_t{data[i * 4 + 2]} << 8) | std::uint32_t{data[i * 4 + 3]};
    }
    for (std::size_t i = 16; i < 64; ++i) {
        const std::uint32_t s0 = std::rotr(w[i - 15], 7) ^ std::rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        const std::uint32_t s1 = std::rotr(w[i - 2], 17) ^ std::rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    auto [a, b, c, d, e, f, g, h] = state_;
    for (std::size_t i = 0; i < 64; ++i) {
        const std::uint32_t big1 = std::rotr(e, 6) ^ std::rotr(e, 11) ^ std::rotr(e, 25);
        const std::uint32_t choose = (e & f) ^ (~e & g);
        const std::uint32_t t1 = h + big1 + choose + kRound[i] + w[i];
        const std::uint32_t big0 = std::rotr(a, 2) ^ std::rotr(a, 13) ^ std::rotr(a, 22);
        const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        const std::uint32_t t2 = big0 + majority;
        h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    state_[0] += a; state_[1] += b; state_[2] += c; state_[3] += d;
    state_[4] += e; state_[5] += f; state_[6] += g; state_[7] += h;
}

void Sha256::update(std::span<const std::uint8_t> bytes) noexcept {
    total_ += bytes.size();
    std::size_t offset = 0;
    if (buffered_ > 0) {
        const std::size_t take = std::min(bytes.size(), buffer_.size() - buffered_);
        std::copy_n(bytes.begin(), take, buffer_.begin() + static_cast<std::ptrdiff_t>(buffered_));
        buffered_ += take;
        offset = take;
        if (buffered_ < buffer_.size()) return;
        block(buffer_.data());
        buffered_ = 0;
    }
    for (; offset + 64 <= bytes.size(); offset += 64) block(bytes.data() + offset);
    buffered_ = bytes.size() - offset;
    std::copy_n(bytes.begin() + static_cast<std::ptrdiff_t>(offset), buffered_, buffer_.begin());
}

void Sha256::update_u32(std::uint32_t value) noexcept {
    const std::uint8_t bytes[4] = {static_cast<std::uint8_t>(value), static_cast<std::uint8_t>(value >> 8),
                                   static_cast<std::uint8_t>(value >> 16), static_cast<std::uint8_t>(value >> 24)};
    update(std::span<const std::uint8_t>(bytes));
}

void Sha256::update_field(std::string_view text) noexcept {
    const std::uint64_t size = text.size();
    update_u32(static_cast<std::uint32_t>(size));
    update_u32(static_cast<std::uint32_t>(size >> 32));
    update(text);
}

Sha256Digest Sha256::finish() noexcept {
    const std::uint64_t bits = total_ * 8;
    const std::uint8_t one = 0x80;
    update(std::span<const std::uint8_t>(&one, 1));
    const std::uint8_t zero = 0;
    while (buffered_ != 56) update(std::span<const std::uint8_t>(&zero, 1));
    std::uint8_t length[8];
    for (std::size_t i = 0; i < 8; ++i) length[i] = static_cast<std::uint8_t>(bits >> (56 - 8 * i));
    update(std::span<const std::uint8_t>(length));
    Sha256Digest out{};
    for (std::size_t i = 0; i < 8; ++i) {
        out[i * 4] = static_cast<std::uint8_t>(state_[i] >> 24);
        out[i * 4 + 1] = static_cast<std::uint8_t>(state_[i] >> 16);
        out[i * 4 + 2] = static_cast<std::uint8_t>(state_[i] >> 8);
        out[i * 4 + 3] = static_cast<std::uint8_t>(state_[i]);
    }
    return out;
}

} // namespace astral::detail
