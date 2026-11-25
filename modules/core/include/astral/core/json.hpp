#pragma once
// A small JSON value for engine data (scenes, prefabs, settings): human-readable, diffable,
// easy for tools and AI to read and write. Objects keep insertion order so saved files diff cleanly.
// Not a high-throughput parser: shipped games load cooked binary data instead.

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace astral {

class Json {
public:
    enum class Type : std::uint8_t { Null, Bool, Number, String, Array, Object };
    using Array = std::vector<Json>;
    using Object = std::vector<std::pair<std::string, Json>>;

    Json() noexcept = default;
    Json(std::nullptr_t) noexcept {}
    Json(bool b) noexcept : value_(b) {}
    Json(double n) noexcept : value_(n) {}
    // Integers are kept exactly (ADR-0041); they are still Type::Number and as_number() reads them.
    Json(int n) noexcept : value_(static_cast<std::int64_t>(n)) {}
    Json(std::int64_t n) noexcept : value_(n) {}
    Json(std::uint32_t n) noexcept : value_(static_cast<std::int64_t>(n)) {}
    Json(std::uint64_t n) noexcept {
        if (n <= static_cast<std::uint64_t>(INT64_MAX)) value_ = static_cast<std::int64_t>(n);
        else value_ = n;
    }
    Json(float n) noexcept : value_(static_cast<double>(n)) {}
    Json(const char* s) : value_(std::string(s)) {}
    Json(std::string s) noexcept : value_(std::move(s)) {}
    Json(std::string_view s) : value_(std::string(s)) {}

    static Json array(std::initializer_list<Json> items = {});
    static Json object();

    [[nodiscard]] Type type() const noexcept {
        const std::size_t i = value_.index();
        return i >= int64_index ? Type::Number : static_cast<Type>(i);
    }
    [[nodiscard]] bool is_null() const noexcept { return type() == Type::Null; }
    [[nodiscard]] bool is_bool() const noexcept { return type() == Type::Bool; }
    [[nodiscard]] bool is_number() const noexcept { return type() == Type::Number; }
    [[nodiscard]] bool is_string() const noexcept { return type() == Type::String; }
    [[nodiscard]] bool is_array() const noexcept { return type() == Type::Array; }
    [[nodiscard]] bool is_object() const noexcept { return type() == Type::Object; }

    // Reads never throw: a wrong type gives the fallback.
    [[nodiscard]] bool as_bool(bool fallback = false) const noexcept;
    [[nodiscard]] double as_number(double fallback = 0.0) const noexcept;
    /// True if the number is held as an integer (written without fraction or exponent, or built from one).
    [[nodiscard]] bool is_integer() const noexcept { return value_.index() >= int64_index; }
    /// The exact value when it is a whole number representable as T; nullopt otherwise (or not a number).
    [[nodiscard]] std::optional<std::int64_t> as_int64() const noexcept;
    [[nodiscard]] std::optional<std::uint64_t> as_uint64() const noexcept;
    [[nodiscard]] const std::string& as_string() const noexcept;
    [[nodiscard]] const Array& as_array() const noexcept;
    [[nodiscard]] const Object& as_object() const noexcept;
    /// Elements of an array or members of an object; 0 otherwise.
    [[nodiscard]] std::size_t size() const noexcept;

    /// Member lookup; nullptr if absent or not an object.
    [[nodiscard]] const Json* find(std::string_view key) const noexcept;
    /// Missing member / index / wrong type: a shared null value.
    const Json& operator[](std::string_view key) const noexcept;
    const Json& operator[](std::size_t index) const noexcept;
    const Json& operator[](int index) const noexcept { return (*this)[static_cast<std::size_t>(index)]; }
    // Non-const index reads: without these, `doc[0]` on a non-const Json would also match the
    // const char* key overload (0 is a null pointer constant) and be ambiguous.
    const Json& operator[](std::size_t index) noexcept { return static_cast<const Json&>(*this)[index]; }
    const Json& operator[](int index) noexcept { return static_cast<const Json&>(*this)[static_cast<std::size_t>(index)]; }
    /// Member access that inserts (turns a null into an object first).
    Json& operator[](std::string_view key);
    Json& operator[](const char* key) { return (*this)[std::string_view(key)]; }
    /// Appends (turns a null into an array first).
    void push_back(Json value);
    /// Removes an object member; false if absent (or not an object).
    bool erase(std::string_view key);

    /// Compact (indent < 0... = 0) or pretty-printed with `indent` spaces.
    [[nodiscard]] std::string dump(int indent = 0) const;
    /// nullopt on invalid input; `error` gets "line L, column C: message".
    static std::optional<Json> parse(std::string_view text, std::string* error = nullptr);

    /// Compact binary form ("AJB1" + tagged values): what the cook step ships instead of text.
    /// Same values, same member order; several times faster to read than parse().
    [[nodiscard]] std::vector<std::uint8_t> to_binary() const;
    /// nullopt on damaged input (never crashes, never over-allocates; nesting limited to 512 levels).
    static std::optional<Json> from_binary(std::span<const std::uint8_t> data, std::string* error = nullptr);
    /// True if `data` starts with the binary form's magic.
    static bool is_binary(std::span<const std::uint8_t> data) noexcept;

    /// Numbers compare by value whatever their storage (Json(1) == Json(1.0)).
    friend bool operator==(const Json& a, const Json& b) noexcept;

private:
    static constexpr std::size_t int64_index = 6;
    // The first six alternatives follow Type; the integer ones come last and report Type::Number.
    std::variant<std::nullptr_t, bool, double, std::string, Array, Object, std::int64_t, std::uint64_t> value_{nullptr};
};

} // namespace astral
