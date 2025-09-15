#pragma once

#include <string>
#include <fstream>
#include <filesystem>
#include <mutex>
#include <memory>

namespace AstralEngine {

/**
 * @brief Dosya bazlı loglama sistemi
 * 
 * Logger sınıfına dosya yazma yeteneği ekler.
 * Exe'nin yanına otomatik olarak log dosyası oluşturur.
 */
class FileLogger {
public:
    FileLogger();
    ~FileLogger();

    // Dosyayı aç/kapat
    bool OpenLogFile(const std::string& directory = "");
    void CloseLogFile();
    bool IsOpen() const { return m_file.is_open(); }

    // Log mesajı yaz
    void WriteLog(const std::string& timestamp, const std::string& level, 
                  const std::string& category, const std::string& message);

    // Exe'nin bulunduğu dizini al
    static std::string GetExecutableDirectory();

private:
    std::ofstream m_file;
    std::mutex m_mutex;
    std::string m_logFilePath;

    // Log dosyası adı oluştur
    std::string GenerateLogFileName();
};

} // namespace AstralEngine
