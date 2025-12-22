#include "Logger.h"
#include "FileLogger.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <chrono>

namespace AstralEngine {

Logger::LogLevel Logger::s_currentLevel = Logger::LogLevel::Info;
std::unique_ptr<FileLogger> Logger::s_fileLogger = nullptr;
Logger::LogCallback Logger::s_logCallback = nullptr;

void Logger::Log(LogLevel level, const std::string& category, const std::string& message) {
    if (level < s_currentLevel) {
        return;
    }
    
    std::string timestamp = GetTimeString();
    std::string levelStr = GetLevelString(level);
    
    // Callback (Editor Console vb.)
    if (s_logCallback) {
        s_logCallback(level, category, message);
    }

    // Konsola yaz
    std::cout << "[" << timestamp << "] "
              << "[" << levelStr << "] "
              << "[" << category << "] "
              << message << std::endl;
    
    // Dosyaya yaz (eğer açıksa)
    if (s_fileLogger && s_fileLogger->IsOpen()) {
        s_fileLogger->WriteLog(timestamp, levelStr, category, message);
    }
}

std::string Logger::GetLevelString(LogLevel level) {
    switch (level) {
        case LogLevel::Trace:    return "TRACE";
        case LogLevel::Debug:    return "DEBUG";
        case LogLevel::Info:     return "INFO";
        case LogLevel::Warning:  return "WARN";
        case LogLevel::Error:    return "ERROR";
        case LogLevel::Critical: return "CRITICAL";
        default:                 return "UNKNOWN";
    }
}

std::string Logger::GetTimeString() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::ostringstream oss;
    
#ifdef _WIN32
    struct tm timeinfo;
    localtime_s(&timeinfo, &time_t);
    oss << std::put_time(&timeinfo, "%H:%M:%S");
#else
    oss << std::put_time(std::localtime(&time_t), "%H:%M:%S");
#endif
    
    oss << '.' << std::setfill('0') << std::setw(3) << milliseconds.count();
    
    return oss.str();
}

void Logger::SetLogCallback(LogCallback callback) {
    s_logCallback = callback;
}

void Logger::SetLogLevel(LogLevel level) {
    s_currentLevel = level;
}

bool Logger::InitializeFileLogging(const std::string& logDirectory) {
    if (!s_fileLogger) {
        s_fileLogger = std::make_unique<FileLogger>();
    }
    
    bool success = s_fileLogger->OpenLogFile(logDirectory);
    if (success) {
        Info("Logger", "File logging initialized successfully");
    } else {
        Error("Logger", "Failed to initialize file logging");
    }
    
    return success;
}

void Logger::ShutdownFileLogging() {
    if (s_fileLogger) {
        s_fileLogger->CloseLogFile();
        s_fileLogger.reset();
        Info("Logger", "File logging shutdown complete");
    }
}

} // namespace AstralEngine
