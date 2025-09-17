#include "Core/Engine.h"
#include "Core/Logger.h"
#include "Core/IApplication.h"
#include "Subsystems/Platform/PlatformSubsystem.h"
#include "Subsystems/Asset/AssetSubsystem.h"
#include "Subsystems/ECS/ECSSubsystem.h"
#include "Subsystems/Renderer/RenderSubsystem.h"
#include "ECS/Components.h"
#include <filesystem>

using namespace AstralEngine;

/**
 * @brief Astral Engine Sandbox Uygulaması
 * 
 * Bu uygulama, Astral Engine'in nasıl kullanılacağını gösteren örnek bir sandbox'tır.
 * Motorun temel özelliklerini sergilemek için basit bir sahne oluşturur.
 */
class SandboxApp : public IApplication {
public:
    void OnStart(Engine* owner) override {
        Logger::Info("SandboxApp", "Application starting...");
        m_engine = owner;
        
        // Artık alt sistemler tamamen hazır. Güvenle erişebiliriz.
        CreateTestScene();
    }

    void OnUpdate(float deltaTime) override {
        // Oyun mantığı güncellemeleri buraya gelecek.
        // Örneğin, entity'leri hareket ettirme, input kontrolü vb.
    }

    void OnShutdown() override {
        Logger::Info("SandboxApp", "Application shutting down...");
    }

private:
    void CreateTestScene() {
        Logger::Info("SandboxApp", "Creating test scene...");
        
        auto* ecs = m_engine->GetSubsystem<ECSSubsystem>();
        auto* assets = m_engine->GetSubsystem<AssetSubsystem>()->GetAssetManager();

        if (!ecs || !assets) {
            Logger::Error("SandboxApp", "Failed to get required subsystems.");
            return;
        }

        // 1. Asset'leri kaydet
        AssetHandle modelHandle = assets->RegisterAsset("Models/testobject/_VAZ2101_OBJ.obj", AssetHandle::Type::Model);
        AssetHandle textureHandle = assets->RegisterAsset("Models/testobject/VAZ2101_Body_BaseColor.png", AssetHandle::Type::Texture);

        if (!modelHandle.IsValid() || !textureHandle.IsValid()) {
            Logger::Error("SandboxApp", "Failed to register assets. Model valid: {}, Texture valid: {}", 
                         modelHandle.IsValid(), textureHandle.IsValid());
            return;
        }
        
        // 2. Test Entity'sini oluştur
        uint32_t testEntity = ecs->CreateEntity();
        
        // 3. Component'leri ekle ve ayarla
        auto& transform = ecs->AddComponent<TransformComponent>(testEntity);
        transform.position = glm::vec3(0.0f, -1.0f, 0.0f); // Adjusted for better view
        transform.rotation = glm::vec3(glm::radians(-90.0f), 0.0f, 0.0f); // Adjusted for better view
        transform.scale = glm::vec3(1.0f);

        auto& render = ecs->AddComponent<RenderComponent>(testEntity);
        render.modelHandle = modelHandle;
        render.textureHandle = textureHandle;
        render.visible = true;
        
        // Legacy string path'leri temizle (artık kullanılmıyor)
        render.modelPath.clear();
        render.texturePath.clear();
        
        // Add Name component for debugging
        auto& name = ecs->AddComponent<NameComponent>(testEntity);
        name.name = "VAZ2101";

        Logger::Info("SandboxApp", "Test entity created with model and texture handles. Model ID: {}, Texture ID: {}", 
                     modelHandle.GetID(), textureHandle.GetID());
    }

    Engine* m_engine = nullptr;
};

/**
 * @brief Ana giriş noktası
 */
int main(int argc, char* argv[]) {
    // Dosya loglamayı başlat (exe'nin yanına)
    Logger::InitializeFileLogging();
    Logger::SetLogLevel(Logger::LogLevel::Debug);
    
    Logger::Info("Sandbox", "Starting Astral Engine Sandbox...");
    Logger::Info("Sandbox", "Engine Version: 0.1.0-alpha");
    
    try {
        Engine engine;
        SandboxApp sandbox;

        // Set base path from executable path
        if (argc > 0 && argv[0]) {
            engine.SetBasePath(std::filesystem::path(argv[0]).parent_path());
        }
        
        // Subsystem'leri kaydet (sıra önemli!)
        Logger::Info("Sandbox", "Registering subsystems...");
        
        // 1. Platform Subsystem (pencere, input)
        engine.RegisterSubsystem<PlatformSubsystem>();
        
        // 2. Asset Subsystem (varlık yönetimi)
        engine.RegisterSubsystem<AssetSubsystem>();
        
        // 3. ECS Subsystem (entity component system)
        engine.RegisterSubsystem<ECSSubsystem>();
        
        // 4. Render Subsystem (3D rendering)
        engine.RegisterSubsystem<RenderSubsystem>();
        
        Logger::Info("Sandbox", "All subsystems registered. Starting engine...");
        
        // Engine'i uygulama ile çalıştır
        engine.Run(&sandbox);
        
        Logger::Info("Sandbox", "Engine shutdown normally");
        
    } catch (const std::exception& e) {
        Logger::Critical("Sandbox", "Fatal exception: {}", e.what());
        return -1;
    } catch (...) {
        Logger::Critical("Sandbox", "Unknown fatal exception occurred");
        return -1;
    }
    
    Logger::Info("Sandbox", "Astral Engine Sandbox terminated");
    
    // Dosya loglamayı kapat
    Logger::ShutdownFileLogging();
    
    return 0;
}
