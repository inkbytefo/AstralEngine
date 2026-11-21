#pragma once
// Little-endian binary writing and bounds-checked reading, for cooked asset formats.
//
//     ByteWriter w;  w.u32(magic);  w.string(name);  w.bytes(pixels);
//     ByteReader r(data);  auto magic = r.u32();  auto name = r.string();
//     if (!r.ok()) { /* truncated or malformed: every read after the first failure returns zero */ }

#include <bit>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace astral {

class ByteWriter {
public:
    void u8(std::uint8_t v) { data_.push_back(v); }
    void u32(std::uint32_t v) {
        for (int i = 0; i < 4; ++i) data_.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
    }
    void u64(std::uint64_t v) {
        for (int i = 0; i < 8; ++i) data_.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
    }
    void f32(float v) { u32(std::bit_cast<std::uint32_t>(v)); }
    void f64(double v) { u64(std::bit_cast<std::uint64_t>(v)); }
    /// LEB128: 1 byte below 128, at most 10.
    void varint(std::uint64_t v) {
        while (v >= 0x80) {
            data_.push_back(static_cast<std::uint8_t>(v | 0x80));
            v >>= 7;
        }
        data_.push_back(static_cast<std::uint8_t>(v));
    }
    /// Length (varint) + UTF-8 bytes.
    void string(std::string_view s) {
        varint(s.size());
        data_.insert(data_.end(), s.begin(), s.end());
    }
    void bytes(std::span<const std::uint8_t> b) { data_.insert(data_.end(), b.begin(), b.end()); }

    [[nodiscard]] std::size_t size() const noexcept { return data_.size(); }
    [[nodiscard]] const std::vector<std::uint8_t>& data() const noexcept { return data_; }
    [[nodiscard]] std::vector<std::uint8_t> take() noexcept { return std::move(data_); }

private:
    std::vector<std::uint8_t> data_;
};

class ByteReader {
public:
    explicit ByteReader(std::span<const std::uint8_t> data) noexcept : data_(data) {}

    std::uint8_t u8() noexcept {
        if (!need(1)) return 0;
        return data_[pos_++];
    }
    std::uint32_t u32() noexcept {
        if (!need(4)) return 0;
        std::uint32_t v = 0;
        for (int i = 0; i < 4; ++i) v |= std::uint32_t{data_[pos_++]} << (8 * i);
        return v;
    }
    std::uint64_t u64() noexcept {
        if (!need(8)) return 0;
        std::uint64_t v = 0;
        for (int i = 0; i < 8; ++i) v |= std::uint64_t{data_[pos_++]} << (8 * i);
        return v;
    }
    float f32() noexcept { return std::bit_cast<float>(u32()); }
    double f64() noexcept { return std::bit_cast<double>(u64()); }
    std::uint64_t varint() noexcept {
        std::uint64_t v = 0;
        for (int shift = 0; shift < 70; shift += 7) {
            const std::uint8_t b = u8();
            if (!ok_) return 0;
            if (shift == 63 && b > 1) return fail(), 0; // more than 64 bits
            v |= std::uint64_t{b & 0x7Fu} << shift;
            if (!(b & 0x80)) return v;
        }
        return fail(), 0;
    }
    std::string string() {
        const std::uint64_t n = varint();
        const auto b = bytes(n);
        return std::string(b.begin(), b.end());
    }
    /// A view of the next `n` bytes (empty on failure).
    std::span<const std::uint8_t> bytes(std::uint64_t n) noexcept {
        if (!need(n)) return {};
        const auto out = data_.subspan(pos_, static_cast<std::size_t>(n));
        pos_ += static_cast<std::size_t>(n);
        return out;
    }

    [[nodiscard]] bool ok() const noexcept { return ok_; }
    [[nodiscard]] std::size_t remaining() const noexcept { return data_.size() - pos_; }
    [[nodiscard]] std::size_t position() const noexcept { return pos_; }
    /// Marks the data as malformed (for checks the reader cannot make itself).
    void fail() noexcept {
        ok_ = false;
        pos_ = data_.size();
    }

private:
    bool need(std::uint64_t n) noexcept {
        if (!ok_ || n > remaining()) {
            fail();
            return false;
        }
        return true;
    }
    std::span<const std::uint8_t> data_;
    std::size_t pos_ = 0;
    bool ok_ = true;
};

} // namespace astral
