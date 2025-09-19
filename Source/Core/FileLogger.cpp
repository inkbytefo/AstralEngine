#include "FileLogger.h"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#elif __linux__
#include <unistd.h>
#include <limits.h>
#endif

namespace AstralEngine {

FileLogger::FileLogger() {
    // Constructor - dosya henüz açık değil
}

FileLogger::~FileLogger() {
    CloseLogFile();
}

bool FileLogger::OpenLogFile(const std::string& directory) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Mevcut dosyayı kapat
    if (m_file.is_open()) {
        m_file.close();
    }
    
    // Dizin belirtilmediyse exe'nin yanına koy
    std::string targetDir = directory;
    if (targetDir.empty()) {
        targetDir = GetExecutableDirectory();
    }
    
    // Log dosyası adı oluştur
    std::string fileName = GenerateLogFileName();
    
    // Dosya yolunu oluştur (platforma göre ayırıcı kullan)
#ifdef _WIN32
    m_logFilePath = targetDir + "\\" + fileName;
#else
    m_logFilePath = targetDir + "/" + fileName;
#endif
    
    try {
        // Dosyayı aç (append modunda)
        m_file.open(m_logFilePath, std::ios::out | std::ios::app);
        
        if (!m_file.is_open()) {
            return false;
        }
        
        // UTF-8 BOM ekle (Windows için)
        #ifdef _WIN32
        m_file << "\xEF\xBB\xBF";
        #endif
        
        // Başlık yaz
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        
        m_file << "========================================" << std::endl;
        m_file << "Astral Engine Log File" << std::endl;
        
#ifdef _WIN32
        struct tm timeinfo;
        localtime_s(&timeinfo, &time_t);
        m_file << "Started: " << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S") << std::endl;
#else
        m_file << "Started: " << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") << std::endl;
#endif
        
        m_file << "========================================" << std::endl;
        m_file << std::endl;
        
        m_file.flush();
        
        return true;
        
    } catch (const std::exception& e) {
        return false;
    }
}

void FileLogger::CloseLogFile() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_file.is_open()) {
        try {
            // Kapanış mesajı yaz
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            
            m_file << std::endl;
            m_file << "========================================" << std::endl;
            
#ifdef _WIN32
            struct tm timeinfo;
            localtime_s(&timeinfo, &time_t);
            m_file << "Log session ended: " << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S") << std::endl;
#else
            m_file << "Log session ended: " << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") << std::endl;
#endif
            
            m_file << "========================================" << std::endl;
            
            m_file.close();
            
        } catch (const std::exception& e) {
            if (m_file.is_open()) {
                m_file.close();
            }
        }
    }
}

void FileLogger::WriteLog(const std::string& timestamp, const std::string& level, 
                         const std::string& category, const std::string& message) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_file.is_open()) {
        return;
    }
    
    try {
        m_file << "[" << timestamp << "] "
               << "[" << level << "] "
               << "[" << category << "] "
               << message << std::endl;
        
        // Her log mesajından sonra flush et (önemli mesajlar için)
        if (level == "ERROR" || level == "CRITICAL") {
            m_file.flush();
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Exception writing to log file: " << e.what() << std::endl;
    }
}

std::string FileLogger::GetExecutableDirectory() {
    try {
        #ifdef _WIN32
        char buffer[MAX_PATH];
        GetModuleFileNameA(NULL, buffer, MAX_PATH);
        
        // Dosya yolundan son ayırıcıya kadar olan kısmı al
        std::string fullPath(buffer);
        size_t lastSlash = fullPath.find_last_of("\\/");
        if (lastSlash != std::string::npos) {
            return fullPath.substr(0, lastSlash);
        }
        
        #elif __linux__
        char buffer[PATH_MAX];
        ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
        if (len != -1) {
            buffer[len] = '\0';
            std::string fullPath(buffer);
            size_t lastSlash = fullPath.find_last_of('/');
            if (lastSlash != std::string::npos) {
                return fullPath.substr(0, lastSlash);
            }
        }
        #endif
        
        // Fallback: çalışma dizini
        return ".";
        
    } catch (const std::exception& e) {
        return ".";
    }
}

std::string FileLogger::GenerateLogFileName() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::ostringstream oss;
    oss << "AstralEngine_";
    
#ifdef _WIN32
    struct tm timeinfo;
    localtime_s(&timeinfo, &time_t);
    oss << std::put_time(&timeinfo, "%Y%m%d_%H%M%S");
#else
    oss << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");
#endif
    
    oss << ".log";
    
    return oss.str();
}

} // namespace AstralEngine
