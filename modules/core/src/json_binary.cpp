// Json binary form (AJB1): what the cook step ships instead of text. Same values, same member
// order; several times faster to read than Json::parse(). Split from json.cpp (ADR-0030).
#include <astral/core/json.hpp>

#include <astral/core/bytes.hpp>

#include <algorithm>
#include <cmath>
#include <string>

namespace astral {

namespace {

constexpr std::uint8_t binary_magic[4] = {'A', 'J', 'B', '1'};
// TagUInt64 (ADR-0041) was added last so older files keep their meaning.
enum Tag : std::uint8_t { TagNull, TagFalse, TagTrue, TagNumber, TagInteger, TagString, TagArray, TagObject, TagUInt64 };
constexpr int max_binary_depth = 512;

void write_binary(ByteWriter& w, const Json& j) {
    switch (j.type()) {
    case Json::Type::Null: w.u8(TagNull); break;
    case Json::Type::Bool: w.u8(j.as_bool() ? TagTrue : TagFalse); break;
    case Json::Type::Number: {
        if (j.is_integer()) {
            if (const auto i = j.as_int64()) {
                w.u8(TagInteger);
                w.varint((static_cast<std::uint64_t>(*i) << 1) ^ static_cast<std::uint64_t>(*i >> 63));
            } else {
                w.u8(TagUInt64); // above int64: only uint64 storage gets here
                w.varint(*j.as_uint64());
            }
            break;
        }
        const double v = j.as_number();
        // Whole numbers (ids, counts, most colours' 0/1) as zigzag varints: 1-3 bytes instead of 9.
        if (std::isfinite(v) && v == std::floor(v) && std::fabs(v) < 9007199254740992.0 && !(v == 0 && std::signbit(v))) {
            const auto i = static_cast<std::int64_t>(v);
            w.u8(TagInteger);
            w.varint((static_cast<std::uint64_t>(i) << 1) ^ static_cast<std::uint64_t>(i >> 63));
        } else {
            w.u8(TagNumber);
            w.f64(v);
        }
        break;
    }
    case Json::Type::String:
        w.u8(TagString);
        w.string(j.as_string());
        break;
    case Json::Type::Array:
        w.u8(TagArray);
        w.varint(j.size());
        for (const Json& item : j.as_array()) write_binary(w, item);
        break;
    case Json::Type::Object:
        w.u8(TagObject);
        w.varint(j.size());
        for (const auto& [key, value] : j.as_object()) {
            w.string(key);
            write_binary(w, value);
        }
        break;
    }
}

bool read_binary(ByteReader& r, Json& out, int depth, std::string& error) {
    if (depth > max_binary_depth) {
        error = "nesting deeper than " + std::to_string(max_binary_depth);
        return false;
    }
    const std::uint8_t tag = r.u8();
    if (!r.ok()) return error = "truncated", false;
    switch (tag) {
    case TagNull: out = nullptr; return true;
    case TagFalse: out = false; return true;
    case TagTrue: out = true; return true;
    case TagNumber: out = r.f64(); break;
    case TagInteger: {
        const std::uint64_t z = r.varint();
        out = static_cast<std::int64_t>((z >> 1) ^ (~(z & 1) + 1));
        break;
    }
    case TagUInt64: out = r.varint(); break;
    case TagString: out = r.string(); break;
    case TagArray: {
        const std::uint64_t n = r.varint();
        if (n > r.remaining()) return error = "array count beyond the data", false; // each item takes >= 1 byte
        out = Json::array();
        for (std::uint64_t i = 0; i < n; ++i) {
            Json item;
            if (!read_binary(r, item, depth + 1, error)) return false;
            out.push_back(std::move(item));
        }
        break;
    }
    case TagObject: {
        const std::uint64_t n = r.varint();
        if (n > r.remaining()) return error = "member count beyond the data", false;
        out = Json::object();
        for (std::uint64_t i = 0; i < n; ++i) {
            std::string key = r.string();
            Json value;
            if (!r.ok() || !read_binary(r, value, depth + 1, error)) return error.empty() ? (error = "truncated", false) : false;
            out[key] = std::move(value);
        }
        break;
    }
    default: error = "unknown value tag " + std::to_string(tag); return false;
    }
    if (!r.ok()) return error = "truncated", false;
    return true;
}

} // namespace

std::vector<std::uint8_t> Json::to_binary() const {
    ByteWriter w;
    w.bytes(binary_magic);
    write_binary(w, *this);
    return w.take();
}

bool Json::is_binary(std::span<const std::uint8_t> data) noexcept {
    return data.size() >= 4 && std::equal(data.begin(), data.begin() + 4, binary_magic);
}

std::optional<Json> Json::from_binary(std::span<const std::uint8_t> data, std::string* error) {
    std::string message;
    if (!is_binary(data)) message = "not binary JSON (no AJB1 header)";
    ByteReader r(data.subspan(std::min<std::size_t>(4, data.size())));
    Json value;
    if (message.empty() && read_binary(r, value, 0, message)) {
        if (r.remaining() == 0) return value;
        message = "unexpected bytes after the value";
    }
    if (error) *error = "binary JSON: " + message;
    return std::nullopt;
}

} // namespace astral
