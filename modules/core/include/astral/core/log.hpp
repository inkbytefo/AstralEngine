#pragma once

#include <cstdint>
#include <format>
#include <memory>
#include <mutex>
#include <source_location>
#include <string>
#include <string_view>
#include <vector>

namespace astral {

enum class LogLevel : std::uint8_t { Trace, Debug, Info, Warn, Error };

std::string_view to_string(LogLevel level) noexcept;

struct LogRecord {
    LogLevel level;
    std::string_view message;
    std::source_location where;
};

/// Destination for log records. Implementations must be safe to call from the
/// logger's lock (the logger serializes calls, sinks need no locking of their own).
class ILogSink {
public:
    virtual ~ILogSink() = default;
    virtual void write(const LogRecord& record) = 0;
};

/// Thread-safe logger. Owns no global state: create one and pass it to whoever needs it.
class Logger {
public:
    void add_sink(std::shared_ptr<ILogSink> sink);
    void set_level(LogLevel level) noexcept;
    [[nodiscard]] LogLevel level() const noexcept;
    [[nodiscard]] bool enabled(LogLevel level) const noexcept;

    void log(LogLevel level, std::string_view message,
             std::source_location where = std::source_location::current());

    template <class... Args> struct Fmt {
        std::format_string<Args...> fmt;
        std::source_location where;
        template <class S>
        consteval Fmt(const S& s, std::source_location w = std::source_location::current())
            : fmt(s), where(w) {}
    };

    template <class... Args> void trace(Fmt<std::type_identity_t<Args>...> f, Args&&... a) {
        emit(LogLevel::Trace, f.where, f.fmt, std::forward<Args>(a)...);
    }
    template <class... Args> void debug(Fmt<std::type_identity_t<Args>...> f, Args&&... a) {
        emit(LogLevel::Debug, f.where, f.fmt, std::forward<Args>(a)...);
    }
    template <class... Args> void info(Fmt<std::type_identity_t<Args>...> f, Args&&... a) {
        emit(LogLevel::Info, f.where, f.fmt, std::forward<Args>(a)...);
    }
    template <class... Args> void warn(Fmt<std::type_identity_t<Args>...> f, Args&&... a) {
        emit(LogLevel::Warn, f.where, f.fmt, std::forward<Args>(a)...);
    }
    template <class... Args> void error(Fmt<std::type_identity_t<Args>...> f, Args&&... a) {
        emit(LogLevel::Error, f.where, f.fmt, std::forward<Args>(a)...);
    }

private:
    template <class... Args>
    void emit(LogLevel level, std::source_location where, std::format_string<Args...> fmt,
              Args&&... args) {
        if (!enabled(level)) {
            return;
        }
        // Format outside the lock; only the dispatch to sinks is serialized.
        log(level, std::format(fmt, std::forward<Args>(args)...), where);
    }

    mutable std::mutex mutex_;
    std::vector<std::shared_ptr<ILogSink>> sinks_;
    LogLevel level_ = LogLevel::Info;
};

/// Writes "[LEVEL] message (file:line)" to stdout (Info and below) or stderr (Warn, Error).
class ConsoleSink final : public ILogSink {
public:
    void write(const LogRecord& record) override;
};

/// Keeps messages in memory. Intended for tests and in-game consoles.
class MemorySink final : public ILogSink {
public:
    void write(const LogRecord& record) override;
    [[nodiscard]] std::vector<std::string> lines() const;

private:
    mutable std::mutex mutex_;
    std::vector<std::string> lines_;
};

} // namespace astral
