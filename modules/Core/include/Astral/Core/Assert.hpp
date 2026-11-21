#pragma once

#include <source_location>
#include <string_view>

namespace Astral {

[[noreturn]] void AssertFailed(std::string_view expression, std::string_view message,
                               const std::source_location& where);

} // namespace Astral

// ASTRAL_ASSERT: checked in every build. Use for invariants whose violation would
// corrupt state (bad handles, out-of-range indices). Prints and aborts.
#define ASTRAL_ASSERT(expr, msg)                                                       \
    do {                                                                               \
        if (!(expr)) ::Astral::AssertFailed(#expr, msg, std::source_location::current()); \
    } while (0)
