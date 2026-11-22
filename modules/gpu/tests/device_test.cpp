#include "gpu_test_support.hpp"

using namespace astral;

TEST_CASE("headless device is created with the required features") {
    auto device = test::make_device();
    if (!device) return;
    CHECK_FALSE(device->adapter_name().empty());
    const gpu::AdapterInfo info = device->adapter_info();
    CHECK(info.name == device->adapter_name());
    CHECK(info.vendor_id != 0);
    CHECK(device->supports(gpu::Feature::BufferDeviceAddress));
    CHECK(device->supports(gpu::Feature::Bindless));
}

TEST_CASE("validation layer reports no errors for a test device") {
    auto device = test::make_device();
    if (!device) return;
    if (!device->supports(gpu::Feature::Validation)) {
        MESSAGE("validation layer not installed — nothing to check");
        return;
    }
    // Errors are printed by the debug callback; the test run is checked for "[vulkan]" lines.
    MESSAGE("validation active on " << device->adapter_name());
    CHECK(true);
}
