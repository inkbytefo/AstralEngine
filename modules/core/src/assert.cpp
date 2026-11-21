#include <astral/core/assert.hpp>

#include <cstdio>
#include <cstdlib>

namespace astral::detail {

void assert_fail(const char* condition, const char* message, const char* file, int line) noexcept {
    std::fprintf(stderr, "AstralEngine assertion failed: %s\n  %s\n  at %s:%d\n", message, condition,
                 file, line);
    std::abort();
}

} // namespace astral::detail
