#include "device_info.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdio>

namespace astral::golden {

namespace {

// Khronos VkDriverId constants (stable Vulkan ABI).
constexpr std::uint32_t vk_driver_id_google_swiftshader = 10;
constexpr std::uint32_t vk_driver_id_mesa_llvmpipe = 13;

std::string_view trim(std::string_view text) noexcept {
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t' || text.front() == '\r' || text.front() == '\n')) {
        text.remove_prefix(1);
    }
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\r' || text.back() == '\n')) {
        text.remove_suffix(1);
    }
    return text;
}

char lower_ascii(char c) noexcept {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
}

bool equals_icase(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (lower_ascii(a[i]) != lower_ascii(b[i])) return false;
    }
    return true;
}

bool contains_icase(std::string_view haystack, std::string_view needle) noexcept {
    if (needle.empty() || needle.size() > haystack.size()) return false;
    for (std::size_t i = 0; i + needle.size() <= haystack.size(); ++i) {
        if (equals_icase(haystack.substr(i, needle.size()), needle)) return true;
    }
    return false;
}

std::optional<std::uint32_t> parse_uint32(std::string_view text) noexcept {
    text = trim(text);
    int base = 10;
    if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        text.remove_prefix(2);
        base = 16;
    }
    if (text.empty()) return std::nullopt;
    std::uint32_t value = 0;
    const auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), value, base);
    if (ec != std::errc{} || ptr != text.data() + text.size()) return std::nullopt;
    return value;
}

bool apply_field(CaptureDevice& out, std::string_view token) {
    const std::size_t eq = token.find('=');
    if (eq == std::string_view::npos) return false;
    const std::string_view key = trim(token.substr(0, eq));
    const std::string_view val = trim(token.substr(eq + 1));
    if (key == "name") {
        out.name = std::string(val);
        return true;
    }
    if (key == "vendor") {
        if (const auto v = parse_uint32(val)) {
            out.vendor_id = *v;
            return true;
        }
        return false;
    }
    if (key == "device") {
        if (const auto v = parse_uint32(val)) {
            out.device_id = *v;
            return true;
        }
        return false;
    }
    if (key == "driver") {
        if (const auto v = parse_uint32(val)) {
            out.driver_id = *v;
            return true;
        }
        return false;
    }
    if (key == "driver_name") {
        out.driver_name = std::string(val);
        return true;
    }
    if (key == "type") {
        out.type = std::string(val);
        return true;
    }
    return false;
}

} // namespace

std::optional<CaptureDevice> parse_capture_device(std::string_view output) {
    constexpr std::string_view prefix = "capture device:";
    std::size_t pos = 0;
    while (pos < output.size()) {
        const std::size_t eol = output.find('\n', pos);
        const std::string_view line = trim(output.substr(pos, eol == std::string_view::npos ? output.size() - pos : eol - pos));
        pos = eol == std::string_view::npos ? output.size() : eol + 1;
        if (line.rfind(prefix, 0) != 0) continue;

        CaptureDevice dev;
        std::string_view rest = trim(line.substr(prefix.size()));
        bool parsed_any = false;
        while (!rest.empty()) {
            const std::size_t bar = rest.find('|');
            const std::string_view token = trim(rest.substr(0, bar));
            if (apply_field(dev, token)) parsed_any = true;
            if (bar == std::string_view::npos) break;
            rest.remove_prefix(bar + 1);
        }
        if (parsed_any) return dev;
    }
    return std::nullopt;
}

DeviceClass classify_device(const CaptureDevice& device) {
    if (device.name.empty() && device.vendor_id == 0 && device.driver_id == 0 && device.driver_name.empty() &&
        device.type.empty()) {
        return DeviceClass::Unknown;
    }
    if (device.driver_id == vk_driver_id_mesa_llvmpipe || contains_icase(device.driver_name, "llvmpipe") ||
        contains_icase(device.driver_name, "lavapipe") || contains_icase(device.name, "llvmpipe") ||
        contains_icase(device.name, "lavapipe")) {
        return DeviceClass::Lavapipe;
    }
    if (equals_icase(device.type, "cpu") || device.driver_id == vk_driver_id_google_swiftshader ||
        contains_icase(device.driver_name, "swiftshader") || contains_icase(device.name, "swiftshader") ||
        contains_icase(device.name, "software") || contains_icase(device.name, "microsoft basic render")) {
        return DeviceClass::OtherSoftware;
    }
    return DeviceClass::Hardware;
}

std::string_view device_class_name(DeviceClass device_class) noexcept {
    switch (device_class) {
    case DeviceClass::Lavapipe: return "lavapipe";
    case DeviceClass::OtherSoftware: return "software";
    case DeviceClass::Hardware: return "hardware";
    default: return "unknown";
    }
}

std::string format_device(const CaptureDevice& device) {
    char buf[512];
    const std::string_view name = device.name.empty() ? "<unknown>" : std::string_view(device.name);
    const std::string_view drv = device.driver_name.empty() ? "-" : std::string_view(device.driver_name);
    const std::string_view type = device.type.empty() ? "other" : std::string_view(device.type);
    const std::string_view cls = device_class_name(classify_device(device));
    std::snprintf(buf, sizeof(buf), "%.*s (vendor=0x%04x, driver=%u/%.*s, type=%.*s, class=%.*s)",
                  static_cast<int>(name.size()), name.data(), device.vendor_id, device.driver_id,
                  static_cast<int>(drv.size()), drv.data(), static_cast<int>(type.size()), type.data(),
                  static_cast<int>(cls.size()), cls.data());
    return std::string(buf);
}

} // namespace astral::golden
