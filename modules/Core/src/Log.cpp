#include "Astral/Core/Log.hpp"

#include <atomic>
#include <cstdio>
#include <mutex>

namespace Astral::Log {

namespace {
std::mutex g_Mutex;
Sink g_Sink;
std::atomic<LogLevel> g_MinLevel{LogLevel::Info};

const char* LevelName(LogLevel level) {
    switch (level) {
        case LogLevel::Trace: return "trace";
        case LogLevel::Info:  return "info";
        case LogLevel::Warn:  return "warn";
        case LogLevel::Error: return "error";
    }
    return "?";
}
} // namespace

void SetSink(Sink sink) {
    std::lock_guard lock(g_Mutex);
    g_Sink = std::move(sink);
}

void SetMinLevel(LogLevel level) { g_MinLevel = level; }
LogLevel GetMinLevel() { return g_MinLevel; }

void Write(LogLevel level, std::string_view category, std::string_view message) {
    if (level < g_MinLevel) return;
    std::lock_guard lock(g_Mutex);
    if (g_Sink) {
        g_Sink(level, category, message);
        return;
    }
    std::fprintf(stderr, "[%s] %.*s: %.*s\n", LevelName(level),
                 static_cast<int>(category.size()), category.data(),
                 static_cast<int>(message.size()), message.data());
}

} // namespace Astral::Log
