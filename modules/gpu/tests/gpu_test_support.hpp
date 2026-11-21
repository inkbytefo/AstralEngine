#pragma once

#include <astral/gpu/device.hpp>
#include <doctest/doctest.h>

#include <memory>

namespace astral::test {

/// Creates a headless device, or returns nullptr (and the test skips its body) when the
/// machine has no Vulkan 1.3 device. Validation is on when the layer is installed.
inline std::unique_ptr<gpu::Device> make_device() {
    auto device = gpu::Device::create({.app_name = "astral_gpu_tests", .validation = true});
    if (!device) {
        MESSAGE("no Vulkan 1.3 device available — skipping");
    }
    return device;
}

} // namespace astral::test
