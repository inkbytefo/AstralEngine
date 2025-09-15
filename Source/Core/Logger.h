#pragma once

#include <string>
#include <format>
#include <iostream>
#include <sstream>
#include <memory>

// Forward declaration
namespace AstralEngine {
class FileLogger;
}

namespace AstralEngine {

/**
 * @brief Motorun merkezi loglama sistemi.
 * 
 * Farklı log seviyelerini destekler ve hem konsol hem dosya çıkışı sağlar.
 * Dosya loglaması otomatik olarak exe'nin yanına log dosyası oluşturur.
 */
class Logger {
public:
    enum class LogLevel {
        Trace = 0,
        Debug = 1,
        Info = 2,
        Warning = 3,
        Error = 4,
        Critical = 5
    };

    // Static interface
    template<typename... Args>
    static void Trace(const std::string& category, const std::string& format, Args&&... args);
    
    template<typename... Args>
    static void Debug(const std::string& category, const std::string& format, Args&&... args);
    
    template<typename... Args>
    static void Info(const std::string& category, const std::string& format, Args&&... args);
    
    template<typename... Args>
    static void Warning(const std::string& category, const std::string& format, Args&&... args);
    
    template<typename... Args>
    static void Error(const std::string& category, const std::string& format, Args&&... args);
    
    template<typename... Args>
    static void Critical(const std::string& category, const std::string& format, Args&&... args);

    // Log seviyesini ayarla
    static void SetLogLevel(LogLevel level);

    // Dosya loglamayı başlat/durdur
    static bool InitializeFileLogging(const std::string& logDirectory = "");
    static void ShutdownFileLogging();

private:
    static void Log(LogLevel level, const std::string& category, const std::string& message);
    static std::string GetLevelString(LogLevel level);
    static std::string GetTimeString();
    
    static LogLevel s_currentLevel;
    static std::unique_ptr<FileLogger> s_fileLogger;
};

// Template implementations
template<typename... Args>
void Logger::Trace(const std::string& category, const std::string& format, Args&&... args) {
    if constexpr (sizeof...(args) > 0) {
        Log(LogLevel::Trace, category, std::vformat(format, std::make_format_args(args...)));
    } else {
        Log(LogLevel::Trace, category, format);
    }
}

template<typename... Args>
void Logger::Debug(const std::string& category, const std::string& format, Args&&... args) {
    if constexpr (sizeof...(args) > 0) {
        Log(LogLevel::Debug, category, std::vformat(format, std::make_format_args(args...)));
    } else {
        Log(LogLevel::Debug, category, format);
    }
}

template<typename... Args>
void Logger::Info(const std::string& category, const std::string& format, Args&&... args) {
    if constexpr (sizeof...(args) > 0) {
        Log(LogLevel::Info, category, std::vformat(format, std::make_format_args(args...)));
    } else {
        Log(LogLevel::Info, category, format);
    }
}

template<typename... Args>
void Logger::Warning(const std::string& category, const std::string& format, Args&&... args) {
    if constexpr (sizeof...(args) > 0) {
        Log(LogLevel::Warning, category, std::vformat(format, std::make_format_args(args...)));
    } else {
        Log(LogLevel::Warning, category, format);
    }
}

template<typename... Args>
void Logger::Error(const std::string& category, const std::string& format, Args&&... args) {
    if constexpr (sizeof...(args) > 0) {
        Log(LogLevel::Error, category, std::vformat(format, std::make_format_args(args...)));
    } else {
        Log(LogLevel::Error, category, format);
    }
}

template<typename... Args>
void Logger::Critical(const std::string& category, const std::string& format, Args&&... args) {
    if constexpr (sizeof...(args) > 0) {
        Log(LogLevel::Critical, category, std::vformat(format, std::make_format_args(args...)));
    } else {
        Log(LogLevel::Critical, category, format);
    }
}

} // namespace AstralEngine
