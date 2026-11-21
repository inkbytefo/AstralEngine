#include <astral/core/json.hpp>

#include <astral/core/bytes.hpp>
#include <algorithm>

#include <charconv>
#include <cmath>
#include <cstdio>

namespace astral {

namespace {

const Json& null_json() {
    static const Json null;
    return null;
}

void escape(std::string& out, const std::string& s) {
    out += '"';
    for (unsigned char c : s) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        default:
            if (c < 0x20) {
                char buf[8];
                std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                out += buf;
            } else {
                out += static_cast<char>(c); // UTF-8 passes through
            }
        }
    }
    out += '"';
}

void number(std::string& out, double v) {
    if (!std::isfinite(v)) { // JSON has no NaN/Inf
        out += "null";
        return;
    }
    char buf[32];
    // Shortest representation that round-trips exactly.
    const auto [end, ec] = std::to_chars(buf, buf + sizeof(buf), v);
    out.append(buf, ec == std::errc{} ? end : buf);
}

void write(std::string& out, const Json& j, int indent, int depth) {
    auto newline = [&](int d) {
        if (indent <= 0) return;
        out += '\n';
        out.append(static_cast<std::size_t>(indent * d), ' ');
    };
    switch (j.type()) {
    case Json::Type::Null: out += "null"; break;
    case Json::Type::Bool: out += j.as_bool() ? "true" : "false"; break;
    case Json::Type::Number: number(out, j.as_number()); break;
    case Json::Type::String: escape(out, j.as_string()); break;
    case Json::Type::Array: {
        const auto& a = j.as_array();
        // Short arrays of numbers (vectors, colours, quaternions) stay on one line: readable diffs.
        const bool inline_numbers = a.size() <= 16 && std::all_of(a.begin(), a.end(), [](const Json& v) {
            return v.is_number() || v.is_bool() || v.is_null();
        });
        out += '[';
        for (std::size_t i = 0; i < a.size(); ++i) {
            if (i) out += inline_numbers && indent > 0 ? ", " : ",";
            if (!inline_numbers) newline(depth + 1);
            write(out, a[i], indent, depth + 1);
        }
        if (!a.empty() && !inline_numbers) newline(depth);
        out += ']';
        break;
    }
    case Json::Type::Object: {
        const auto& o = j.as_object();
        out += '{';
        for (std::size_t i = 0; i < o.size(); ++i) {
            if (i) out += ',';
            newline(depth + 1);
            escape(out, o[i].first);
            out += indent > 0 ? ": " : ":";
            write(out, o[i].second, indent, depth + 1);
        }
        if (!o.empty()) newline(depth);
        out += '}';
        break;
    }
    }
}

class Parser {
public:
    explicit Parser(std::string_view text) : text_(text) {}

    std::optional<Json> run(std::string* error) {
        Json value;
        if (parse_value(value, 0)) {
            skip_ws();
            if (pos_ == text_.size()) return value;
            fail("unexpected characters after the value");
        }
        if (error) {
            std::size_t line = 1, column = 1;
            for (std::size_t i = 0; i < err_pos_ && i < text_.size(); ++i) {
                if (text_[i] == '\n') {
                    ++line;
                    column = 1;
                } else {
                    ++column;
                }
            }
            *error = "line " + std::to_string(line) + ", column " + std::to_string(column) + ": " + err_;
        }
        return std::nullopt;
    }

private:
    static constexpr int max_depth = 256;

    bool fail(const char* message) {
        if (err_.empty()) {
            err_ = message;
            err_pos_ = pos_;
        }
        return false;
    }
    void skip_ws() {
        while (pos_ < text_.size() && (text_[pos_] == ' ' || text_[pos_] == '\t' || text_[pos_] == '\n' || text_[pos_] == '\r')) ++pos_;
    }
    bool literal(std::string_view word) {
        if (text_.substr(pos_, word.size()) != word) return fail("invalid literal");
        pos_ += word.size();
        return true;
    }

    bool parse_value(Json& out, int depth) {
        if (depth > max_depth) return fail("nesting too deep");
        skip_ws();
        if (pos_ >= text_.size()) return fail("unexpected end of input");
        const char c = text_[pos_];
        if (c == '{') return parse_object(out, depth);
        if (c == '[') return parse_array(out, depth);
        if (c == '"') {
            std::string s;
            if (!parse_string(s)) return false;
            out = Json(std::move(s));
            return true;
        }
        if (c == 't') { out = true; return literal("true"); }
        if (c == 'f') { out = false; return literal("false"); }
        if (c == 'n') { out = nullptr; return literal("null"); }
        if (c == '-' || (c >= '0' && c <= '9')) return parse_number(out);
        return fail("unexpected character");
    }

    bool parse_number(Json& out) {
        const std::size_t start = pos_;
        if (text_[pos_] == '-') ++pos_;
        auto digits = [&] {
            const std::size_t d = pos_;
            while (pos_ < text_.size() && text_[pos_] >= '0' && text_[pos_] <= '9') ++pos_;
            return pos_ > d;
        };
        if (!digits()) return fail("invalid number");
        if (pos_ < text_.size() && text_[pos_] == '.') {
            ++pos_;
            if (!digits()) return fail("invalid number");
        }
        if (pos_ < text_.size() && (text_[pos_] == 'e' || text_[pos_] == 'E')) {
            ++pos_;
            if (pos_ < text_.size() && (text_[pos_] == '+' || text_[pos_] == '-')) ++pos_;
            if (!digits()) return fail("invalid number");
        }
        double v = 0;
        const auto [end, ec] = std::from_chars(text_.data() + start, text_.data() + pos_, v);
        if (ec != std::errc{} || end != text_.data() + pos_) {
            pos_ = start;
            return fail("invalid number");
        }
        out = v;
        return true;
    }

    static void append_utf8(std::string& s, std::uint32_t cp) {
        if (cp < 0x80) {
            s += static_cast<char>(cp);
        } else if (cp < 0x800) {
            s += static_cast<char>(0xC0 | (cp >> 6));
            s += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            s += static_cast<char>(0xE0 | (cp >> 12));
            s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            s += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            s += static_cast<char>(0xF0 | (cp >> 18));
            s += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            s += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }

    bool hex4(std::uint32_t& out) {
        if (pos_ + 4 > text_.size()) return fail("invalid \\u escape");
        out = 0;
        for (int i = 0; i < 4; ++i) {
            const char h = text_[pos_++];
            out <<= 4;
            if (h >= '0' && h <= '9') out |= static_cast<std::uint32_t>(h - '0');
            else if (h >= 'a' && h <= 'f') out |= static_cast<std::uint32_t>(h - 'a' + 10);
            else if (h >= 'A' && h <= 'F') out |= static_cast<std::uint32_t>(h - 'A' + 10);
            else return fail("invalid \\u escape");
        }
        return true;
    }

    bool parse_string(std::string& out) {
        ++pos_; // opening quote
        while (pos_ < text_.size()) {
            const char c = text_[pos_++];
            if (c == '"') return true;
            if (static_cast<unsigned char>(c) < 0x20) return fail("control character in string");
            if (c != '\\') {
                out += c;
                continue;
            }
            if (pos_ >= text_.size()) break;
            const char e = text_[pos_++];
            switch (e) {
            case '"': out += '"'; break;
            case '\\': out += '\\'; break;
            case '/': out += '/'; break;
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case 'n': out += '\n'; break;
            case 'r': out += '\r'; break;
            case 't': out += '\t'; break;
            case 'u': {
                std::uint32_t cp = 0;
                if (!hex4(cp)) return false;
                if (cp >= 0xD800 && cp < 0xDC00) { // surrogate pair
                    std::uint32_t lo = 0;
                    if (text_.substr(pos_, 2) != "\\u") return fail("unpaired surrogate");
                    pos_ += 2;
                    if (!hex4(lo) || lo < 0xDC00 || lo > 0xDFFF) return fail("unpaired surrogate");
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                }
                append_utf8(out, cp);
                break;
            }
            default: return fail("invalid escape");
            }
        }
        return fail("unterminated string");
    }

    bool parse_array(Json& out, int depth) {
        ++pos_;
        out = Json::array();
        skip_ws();
        if (pos_ < text_.size() && text_[pos_] == ']') {
            ++pos_;
            return true;
        }
        for (;;) {
            Json item;
            if (!parse_value(item, depth + 1)) return false;
            out.push_back(std::move(item));
            skip_ws();
            if (pos_ >= text_.size()) return fail("unterminated array");
            if (text_[pos_] == ',') { ++pos_; continue; }
            if (text_[pos_] == ']') { ++pos_; return true; }
            return fail("expected ',' or ']'");
        }
    }

    bool parse_object(Json& out, int depth) {
        ++pos_;
        out = Json::object();
        skip_ws();
        if (pos_ < text_.size() && text_[pos_] == '}') {
            ++pos_;
            return true;
        }
        for (;;) {
            skip_ws();
            if (pos_ >= text_.size() || text_[pos_] != '"') return fail("expected a member name");
            std::string key;
            if (!parse_string(key)) return false;
            skip_ws();
            if (pos_ >= text_.size() || text_[pos_] != ':') return fail("expected ':'");
            ++pos_;
            Json value;
            if (!parse_value(value, depth + 1)) return false;
            out[key] = std::move(value);
            skip_ws();
            if (pos_ >= text_.size()) return fail("unterminated object");
            if (text_[pos_] == ',') { ++pos_; continue; }
            if (text_[pos_] == '}') { ++pos_; return true; }
            return fail("expected ',' or '}'");
        }
    }

    std::string_view text_;
    std::size_t pos_ = 0;
    std::string err_;
    std::size_t err_pos_ = 0;
};

} // namespace

Json Json::array(std::initializer_list<Json> items) {
    Json j;
    j.value_ = Array(items);
    return j;
}

Json Json::object() {
    Json j;
    j.value_ = Object{};
    return j;
}

bool Json::as_bool(bool fallback) const noexcept {
    const bool* b = std::get_if<bool>(&value_);
    return b ? *b : fallback;
}

double Json::as_number(double fallback) const noexcept {
    const double* d = std::get_if<double>(&value_);
    return d ? *d : fallback;
}

const std::string& Json::as_string() const noexcept {
    static const std::string empty;
    const std::string* s = std::get_if<std::string>(&value_);
    return s ? *s : empty;
}

const Json::Array& Json::as_array() const noexcept {
    static const Array empty;
    const Array* a = std::get_if<Array>(&value_);
    return a ? *a : empty;
}

const Json::Object& Json::as_object() const noexcept {
    static const Object empty;
    const Object* o = std::get_if<Object>(&value_);
    return o ? *o : empty;
}

std::size_t Json::size() const noexcept {
    if (const Array* a = std::get_if<Array>(&value_)) return a->size();
    if (const Object* o = std::get_if<Object>(&value_)) return o->size();
    return 0;
}

const Json* Json::find(std::string_view key) const noexcept {
    const Object* o = std::get_if<Object>(&value_);
    if (!o) return nullptr;
    for (const auto& [k, v] : *o) {
        if (k == key) return &v;
    }
    return nullptr;
}

const Json& Json::operator[](std::string_view key) const noexcept {
    const Json* j = find(key);
    return j ? *j : null_json();
}

const Json& Json::operator[](std::size_t index) const noexcept {
    const Array* a = std::get_if<Array>(&value_);
    return (a && index < a->size()) ? (*a)[index] : null_json();
}

Json& Json::operator[](std::string_view key) {
    if (is_null()) value_ = Object{};
    Object* o = std::get_if<Object>(&value_);
    if (!o) {
        value_ = Object{}; // writing a member replaces a non-object value
        o = std::get_if<Object>(&value_);
    }
    for (auto& [k, v] : *o) {
        if (k == key) return v;
    }
    o->emplace_back(std::string(key), Json{});
    return o->back().second;
}

bool Json::erase(std::string_view key) {
    auto* object = std::get_if<Object>(&value_);
    if (!object) return false;
    for (auto it = object->begin(); it != object->end(); ++it) {
        if (it->first == key) {
            object->erase(it);
            return true;
        }
    }
    return false;
}

void Json::push_back(Json value) {
    if (!is_array()) value_ = Array{};
    std::get<Array>(value_).push_back(std::move(value));
}

std::string Json::dump(int indent) const {
    std::string out;
    write(out, *this, indent, 0);
    return out;
}

std::optional<Json> Json::parse(std::string_view text, std::string* error) {
    return Parser(text).run(error);
}

// --- binary form ---------------------------------------------------------------------------------

namespace {

constexpr std::uint8_t binary_magic[4] = {'A', 'J', 'B', '1'};
enum Tag : std::uint8_t { TagNull, TagFalse, TagTrue, TagNumber, TagInteger, TagString, TagArray, TagObject };
constexpr int max_binary_depth = 512;

void write_binary(ByteWriter& w, const Json& j) {
    switch (j.type()) {
    case Json::Type::Null: w.u8(TagNull); break;
    case Json::Type::Bool: w.u8(j.as_bool() ? TagTrue : TagFalse); break;
    case Json::Type::Number: {
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
        out = static_cast<double>(static_cast<std::int64_t>((z >> 1) ^ (~(z & 1) + 1)));
        break;
    }
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

