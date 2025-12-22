#include "Core/Engine.h"
#include "Core/Logger.h"
#include "Core/IApplication.h"
#include "Subsystems/Platform/PlatformSubsystem.h"
#include "Subsystems/Asset/AssetSubsystem.h"
#include "Subsystems/Scene/Scene.h" // Replace ECS with Scene
#include "Subsystems/Renderer/RenderSubsystem.h"
#include "Subsystems/UI/UISubsystem.h"
#include "Subsystems/Editor/SceneEditorSubsystem.h"
#include "Subsystems/Scene/Entity.h" // Needed for Entity usage
#include "ECS/Components.h"
#include <filesystem>

using namespace AstralEngine;

/**
 * @brief Motoru test etmek için örnek bir uygulama sınıfı.
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
        
        auto* scene = m_engine->GetSubsystem<Scene>();
        auto* assets = m_engine->GetSubsystem<AssetSubsystem>()->GetAssetManager();

        if (!scene || !assets) {
            Logger::Error("SandboxApp", "Failed to get required subsystems.");
            return;
        }

        // 1. Asset'leri kaydet
        AssetHandle modelHandle = assets->RegisterAsset("Models/testobject/_VAZ2101_OBJ.obj", AssetHandle::Type::Model);
        AssetHandle materialHandle = assets->RegisterAsset("Materials/Default.amat", AssetHandle::Type::Material);

        if (!modelHandle.IsValid() || !materialHandle.IsValid()) {
            Logger::Error("SandboxApp", "Failed to register assets. Model valid: {}, Material valid: {}", 
                         modelHandle.IsValid(), materialHandle.IsValid());
            return;
        }
        
        // 2. Test Entity'sini oluştur
        Entity testEntity = scene->CreateEntity("VAZ2101");
        
        // 3. Component'leri ekle ve ayarla
        if (auto* transform = testEntity.TryGetComponent<TransformComponent>()) {
            transform->position = glm::vec3(0.0f, -1.0f, 0.0f); // Adjusted for better view
            transform->rotation = glm::vec3(glm::radians(-90.0f), 0.0f, 0.0f); // Adjusted for better view
            transform->scale = glm::vec3(1.0f);
        } else {
            Logger::Error("SandboxApp", "Failed to add TransformComponent to entity: {}", testEntity);
            return;
        }

        if (auto* render = testEntity.TryGetComponent<RenderComponent>()) {
            render->modelHandle = modelHandle;
            render->materialHandle = materialHandle;
            render->visible = true;
        } else {
            Logger::Error("SandboxApp", "Failed to add RenderComponent to entity: {}", testEntity);
            return;
        }
        
        // Name is already set in CreateEntity("VAZ2101")

        Logger::Info("SandboxApp", "Test entity created with model and material handles. Model ID: {}, Material ID: {}", 
                     modelHandle.GetID(), materialHandle.GetID());
    }

    Engine* m_engine = nullptr;
};

/**
 * @brief Astral Engine'in ana giriş noktası
 */
int main(int argc, char* argv[]) {
    Logger::InitializeFileLogging();
    Logger::SetLogLevel(Logger::LogLevel::Debug);
    
    Logger::Info("Main", "Starting Astral Engine...");
    
    try {
        Engine engine;
        SandboxApp sandbox;

        if (argc > 0 && argv) {
            engine.SetBasePath(std::filesystem::path(argv[0]).parent_path());
        }
        
        // Subsystem'leri kaydet
        engine.RegisterSubsystem<PlatformSubsystem>();
        engine.RegisterSubsystem<AssetSubsystem>();
        engine.RegisterSubsystem<Scene>(); // Register Scene instead of ECSSubsystem
        engine.RegisterSubsystem<RenderSubsystem>();
        engine.RegisterSubsystem<UISubsystem>();
        engine.RegisterSubsystem<SceneEditorSubsystem>();
        
        // Engine'i uygulama ile çalıştır
        engine.Run(&sandbox);
        
    } catch (const std::exception& e) {
        Logger::Critical("Main", "Fatal exception: {}", e.what());
        return -1;
    }
    
    Logger::Info("Main", "Astral Engine terminated.");
    Logger::ShutdownFileLogging();
    
    return 0;
}
