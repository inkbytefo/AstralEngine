#pragma once
#include <string>
#include <vector>
#include <functional>

namespace AstralEngine {

/**
 * @brief Veri sağlayıcıları için temel arayüz.
 * 
 * Bu arayüz, DevToolsSubsystem içinde veri sağlayan bileşenlerin
 * uygulaması gereken fonksiyonları tanımlar. Veri sağlayıcıları,
 * diğer araçlar tarafından tüketilebilecek verileri üretirler.
 */
class IDataProvider {
public:
    virtual ~IDataProvider() = default;
    
    /**
     * @brief Veri sağlayıcısının adını döndürür.
     * @return Sağlayıcının adı
     */
    virtual const std::string& GetProviderName() const = 0;
    
    /**
     * @brief Sağlanan veriye işaretçi döndürür.
     * @return Veriye işaretçi
     */
    virtual void* GetData() = 0;
    
    /**
     * @brief Verinin boyutunu döndürür.
     * @return Veri boyutu (bayt cinsinden)
     */
    virtual size_t GetDataSize() const = 0;
    
    /**
     * @brief Veri değiştiğinde çağrılacak callback fonksiyonunu kaydeder.
     * @param callback Veri değiştiğinde çağrılacak fonksiyon
     */
    virtual void RegisterDataChangedCallback(std::function<void()> callback) = 0;
};

} // namespace AstralEngine