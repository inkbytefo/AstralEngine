#pragma once

// Golden test device identification: parses adapter diagnostics emitted by capture runs and
// classifies Vulkan devices into lavapipe (software reference), other software, or hardware GPUs.
// Test infrastructure only (not part of the engine's public API).

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace astral::golden {

enum class DeviceClass : std::uint8_t {
    Unknown,
    Lavapipe,
    OtherSoftware,
    Hardware,
};

struct CaptureDevice {
    std::string name;
    std::uint32_t vendor_id = 0;
    std::uint32_t device_id = 0;
    std::uint32_t driver_id = 0;
    std::string driver_name;
    std::string type; // "discrete", "integrated", "virtual", "cpu", "other"
};

// Parses the "capture device: name=... | vendor=0x... | ..." line from capture output.
[[nodiscard]] std::optional<CaptureDevice> parse_capture_device(std::string_view output);

// Classifies the Vulkan adapter for software vs hardware golden routing.
[[nodiscard]] DeviceClass classify_device(const CaptureDevice& device);

[[nodiscard]] std::string_view device_class_name(DeviceClass device_class) noexcept;

// Single-line diagnostic string with adapter name, vendor id, driver id, type, and class.
[[nodiscard]] std::string format_device(const CaptureDevice& device);

} // namespace astral::golden
