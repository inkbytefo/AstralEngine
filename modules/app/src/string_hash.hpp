#pragma once
// Internal to Astral::App: transparent hash functor for heterogeneous std::string_view lookups.

#include <cstddef>
#include <functional>
#include <string_view>

namespace astral {

struct StringHash {
    using is_transparent = void;
    [[nodiscard]] std::size_t operator()(std::string_view sv) const noexcept {
        return std::hash<std::string_view>{}(sv);
    }
};

} // namespace astral
