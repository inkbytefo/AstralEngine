#pragma once
#include <string>

namespace AstralEngine {

/**
 * @brief Veri tüketicileri için temel arayüz.
 * 
 * Bu arayüz, DevToolsSubsystem içinde veri tüketen bileşenlerin
 * uygulaması gereken fonksiyonları tanımlar. Veri tüketicileri,
 * veri sağlayıcılarından gelen verileri işlerler.
 */
class IDataConsumer {
public:
    virtual ~IDataConsumer() = default;
    
    /**
     * @brief Veri tüketicisinin adını döndürür.
     * @return Tüketici adı
     */
    virtual const std::string& GetConsumerName() const = 0;
    
    /**
     * @brief Veri sağlayıcısından gelen veriyi ayarlar.
     * @param data Gelen veriye işaretçi
     * @param size Veri boyutu (bayt cinsinden)
     */
    virtual void SetData(void* data, size_t size) = 0;
};

} // namespace AstralEngine