#pragma once

namespace AstralEngine {

class Engine;

/**
 * @brief Motor tarafından çalıştırılacak uygulamalar için temel arayüz.
 * 
 * Bu arayüz, motor ile oyun/uygulama mantığı arasında temiz bir
 * ayrım sağlar. Engine, bu arayüzü implemente eden bir sınıfın
 * yaşam döngüsünü yönetir.
 */
class IApplication {
public:
    virtual ~IApplication() = default;

    /**
     * @brief Motor ve tüm alt sistemler başlatıldıktan sonra bir kez çağrılır.
     * Sahne kurulumu, başlangıç varlıklarının yüklenmesi gibi işlemler için idealdir.
     * @param owner Engine'in sahibi olduğu uygulama.
     */
    virtual void OnStart(Engine* owner) = 0;

    /**
     * @brief Her frame'de oyun mantığını güncellemek için çağrılır.
     * @param deltaTime Bir önceki frame'den geçen süre (saniye).
     */
    virtual void OnUpdate(float deltaTime) = 0;

    /**
     * @brief Motor kapatılmadan hemen önce çağrılır.
     * Kaynak temizleme ve durum kaydetme işlemleri burada yapılabilir.
     */
    virtual void OnShutdown() = 0;
};

} // namespace AstralEngine
