#include <astral/core/log.hpp>

#include <cstdio>

namespace astral {

std::string_view to_string(LogLevel level) noexcept {
    switch (level) {
    case LogLevel::Trace: return "TRACE";
    case LogLevel::Debug: return "DEBUG";
    case LogLevel::Info: return "INFO";
    case LogLevel::Warn: return "WARN";
    case LogLevel::Error: return "ERROR";
    }
    return "?";
}

void Logger::add_sink(std::shared_ptr<ILogSink> sink) {
    if (!sink) {
        return;
    }
    std::scoped_lock lock(mutex_);
    sinks_.push_back(std::move(sink));
}

void Logger::set_level(LogLevel level) noexcept { level_.store(level, std::memory_order_relaxed); }

LogLevel Logger::level() const noexcept { return level_.load(std::memory_order_relaxed); }

bool Logger::enabled(LogLevel level) const noexcept { return level >= this->level(); }

void Logger::log(LogLevel level, std::string_view message, std::source_location where) {
    std::scoped_lock lock(mutex_);
    if (level < level_.load(std::memory_order_relaxed)) {
        return;
    }
    const LogRecord record{level, message, where};
    for (const auto& sink : sinks_) {
        sink->write(record);
    }
}

void ConsoleSink::write(const LogRecord& record) {
    std::FILE* out = record.level >= LogLevel::Warn ? stderr : stdout;
    const auto level = to_string(record.level);
    std::fprintf(out, "[%.*s] %.*s (%s:%u)\n", static_cast<int>(level.size()), level.data(),
                 static_cast<int>(record.message.size()), record.message.data(),
                 record.where.file_name(), static_cast<unsigned>(record.where.line()));
}

void MemorySink::write(const LogRecord& record) {
    std::scoped_lock lock(mutex_);
    lines_.emplace_back(record.message);
}

std::vector<std::string> MemorySink::lines() const {
    std::scoped_lock lock(mutex_);
    return lines_;
}

} // namespace astral
