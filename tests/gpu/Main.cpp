#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include "GpuTestSupport.hpp"
#include "Astral/Core/Log.hpp"

#include <cstdio>
#include <string>

namespace GpuTest {
int g_VulkanErrors = 0;
}

// GPU tests run with validation (ASTRAL_VALIDATION=1 via CTest). Any validation error
// or warning fails the run, even if every individual check passed.
int main(int argc, char** argv) {
    Astral::Log::SetSink([](Astral::LogLevel level, std::string_view category, std::string_view message) {
        if (category == "vulkan" && level >= Astral::LogLevel::Warn) ++GpuTest::g_VulkanErrors;
        if (level >= Astral::LogLevel::Warn)
            std::fprintf(stderr, "[%.*s] %.*s\n", int(category.size()), category.data(), int(message.size()), message.data());
    });
    doctest::Context context(argc, argv);
    int result = context.run();
    if (GpuTest::g_VulkanErrors > 0) {
        std::fprintf(stderr, "FAILED: %d Vulkan validation messages\n", GpuTest::g_VulkanErrors);
        return 1;
    }
    return result;
}
