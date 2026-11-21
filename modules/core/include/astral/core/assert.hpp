#pragma once

namespace astral::detail {
[[noreturn]] void assert_fail(const char* condition, const char* message, const char* file, int line) noexcept;
}

/// Always-on invariant check for API misuse that would otherwise corrupt memory.
#define ASTRAL_ASSERT(condition, message)                                                   \
    do {                                                                                    \
        if (!(condition)) ::astral::detail::assert_fail(#condition, message, __FILE__, __LINE__); \
    } while (false)
