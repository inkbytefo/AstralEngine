#pragma once

namespace AstralEngine {

class Engine; // Forward declaration

/**
 * @brief Tüm motor alt sistemleri için temel arayüz.
 * 
 * Her alt sistem bu arayüzü uygulamalıdır. Bu, Engine çekirdeğinin
 * tüm sistemlerin yaşam döngüsünü (başlatma, güncelleme, kapatma)
 * tek tip bir şekilde yönetmesini sağlar.
 */
class ISubsystem {
public:
    virtual ~ISubsystem() = default;

    // Motor başlatıldığında bir kez çağrılır.
    virtual void OnInitialize(Engine* owner) = 0;

    // Her frame'de ana döngü tarafından çağrılır.
    virtual void OnUpdate(float deltaTime) = 0;

    // Motor kapatıldığında bir kez çağrılır.
    virtual void OnShutdown() = 0;

    // Hata ayıklama ve tanılama için sistemin adını döndürür.
    virtual const char* GetName() const = 0;
};

} // namespace AstralEngine
