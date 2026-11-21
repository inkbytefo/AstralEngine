#include "Astral/Core/Assert.hpp"
#include "Astral/Core/Log.hpp"

#include <cstdlib>

namespace Astral {

void AssertFailed(std::string_view expression, std::string_view message, const std::source_location& where) {
    Log::Error("assert", "{} ({}) at {}:{}", message, expression, where.file_name(), where.line());
    std::abort();
}

} // namespace Astral
