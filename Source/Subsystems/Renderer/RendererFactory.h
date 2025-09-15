#pragma once

#include "IRenderer.h"
#include <memory>

namespace AstralEngine {

/**
 * @class RendererFactory
 * @brief Farklı renderer implementasyonları oluşturmak için factory sınıfı
 * 
 * Bu sınıf, istenen render API'sine göre uygun renderer implementasyonunu
 * oluşturur. Bu sayede motorun geri kalanı API'dan bağımsız olur.
 */
class RendererFactory {
public:
    /**
     * @brief Belirtilen API için renderer oluşturur
     * @param api Oluşturulacak renderer API'si
     * @return Oluşturulan renderer'ın unique pointer'ı
     */
    static std::unique_ptr<IRenderer> CreateRenderer(RendererAPI api);
    
    /**
     * @brief Varsayılan renderer'ı oluşturur (şimdilik Vulkan)
     * @return Varsayılan renderer'ın unique pointer'ı
     */
    static std::unique_ptr<IRenderer> CreateDefaultRenderer();
    
    /**
     * @brief Desteklenen renderer API'lerini listeler
     * @return Desteklenen API'lerin vektörü
     */
    static std::vector<RendererAPI> GetSupportedAPIs();
    
    /**
     * @brief API'nin desteklenip desteklenmediğini kontrol eder
     * @param api Kontrol edilecek API
     * @return true eğer API destekleniyorsa
     */
    static bool IsAPISupported(RendererAPI api);

private:
    // Private constructor - static sınıf
    RendererFactory() = delete;
    ~RendererFactory() = delete;
};

} // namespace AstralEngine
