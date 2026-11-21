#pragma once

#include <astral/core/log.hpp>
#include <astral/gpu/device.hpp>
#include <doctest/doctest.h>

#include <atomic>
#include <memory>
#include <string>

namespace astral::test {

/// Counts Vulkan validation errors so a test fails on misuse, not just on crashes.
class ErrorCountingSink final : public ILogSink {
public:
    void write(const LogRecord& record) override {
        if (record.level == LogLevel::Error) {
            ++errors;
            last = std::string(record.message).substr(0, 300);
        }
    }
    std::atomic<int> errors{0};
    std::string last;
};

/// A headless device whose validation errors fail the current test when it goes out of scope.
/// Converts to false (and the test skips its body) when no Vulkan 1.3 device exists.
class TestDevice {
public:
    TestDevice() : sink_(std::make_shared<ErrorCountingSink>()) {
        log_.add_sink(sink_);
        device_ = gpu::Device::create({.app_name = "astral_gpu_tests", .validation = true, .logger = &log_});
        if (!device_) MESSAGE("no Vulkan 1.3 device available — skipping");
    }
    ~TestDevice() {
        if (device_) device_->wait_idle();
        device_.reset();
        CHECK_MESSAGE(sink_->errors == expected_errors_, "validation/device errors: " << sink_->last);
    }
    explicit operator bool() const noexcept { return device_ != nullptr; }
    gpu::Device* operator->() const noexcept { return device_.get(); }
    gpu::Device& operator*() const noexcept { return *device_; }
    /// For tests that provoke an engine-side error message on purpose.
    void expect_errors(int count) noexcept { expected_errors_ = count; }
    [[nodiscard]] int errors() const noexcept { return sink_->errors; }

private:
    Logger log_;
    std::shared_ptr<ErrorCountingSink> sink_;
    std::unique_ptr<gpu::Device> device_;
    int expected_errors_ = 0;
};

inline TestDevice make_device() { return TestDevice(); }

} // namespace astral::test
