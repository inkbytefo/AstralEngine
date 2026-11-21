#include "gpu_test_support.hpp"

using namespace astral;

TEST_CASE("headless device is created with the required features") {
    auto device = test::make_device();
    if (!device) return;
    CHECK_FALSE(device->adapter_name().empty());
    CHECK(device->supports(gpu::Feature::BufferDeviceAddress));
    CHECK(device->supports(gpu::Feature::Bindless));
}
