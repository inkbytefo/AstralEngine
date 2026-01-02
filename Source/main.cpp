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
        Logger::Info("SandboxApp", "Creating test scene with BMW M5 E34...");
        
        auto* editor = m_engine->GetSubsystem<SceneEditorSubsystem>();
        if (!editor) {
            Logger::Error("SandboxApp", "SceneEditorSubsystem not found!");
            return;
        }

        auto scene = editor->GetActiveScene();
        auto* assets = m_engine->GetSubsystem<AssetSubsystem>()->GetAssetManager();

        if (!scene || !assets) {
            Logger::Error("SandboxApp", "Failed to get required subsystems.");
            return;
        }

        // 1. Asset'leri kaydet
        // BMW modelini kaydet (scene.gltf)
        AssetHandle modelHandle = assets->RegisterAsset("3DObjects/bmw_m5_e34/scene.gltf");
        AssetHandle materialHandle = assets->RegisterAsset("Materials/Default.amat");

        // 2. Zemin (Büyük bir zemin ki araba üstünde dursun)
        Entity floor = scene->CreateEntity("Floor");
        auto& floorTransform = floor.GetComponent<TransformComponent>();
        floorTransform.position = glm::vec3(0.0f, -0.5f, 0.0f);
        floorTransform.scale = glm::vec3(50.0f, 0.1f, 50.0f);
        
        auto& floorRender = floor.AddComponent<RenderComponent>();
        floorRender.modelHandle = assets->RegisterAsset("Models/Default/Cube.obj");
        floorRender.materialHandle = materialHandle;
        floorRender.visible = true;

        // 3. BMW M5 E34
        Entity bmw = scene->CreateEntity("BMW_M5_E34");
        auto& bmwTransform = bmw.GetComponent<TransformComponent>();
        bmwTransform.position = glm::vec3(0.0f, 0.0f, 0.0f);
        bmwTransform.scale = glm::vec3(1.0f); // glTF scale genelde 1.0'dır
        
        auto& bmwRender = bmw.AddComponent<RenderComponent>();
        bmwRender.modelHandle = modelHandle;
        bmwRender.materialHandle = materialHandle; // Şimdilik default material, ilerde glTF material desteği eklenebilir
        bmwRender.visible = true;
        bmwRender.castsShadows = true;

        // 4. Işıklandırma
        Entity mainLight = scene->CreateEntity("Sun");
        auto& lightTransform = mainLight.GetComponent<TransformComponent>();
        lightTransform.rotation = glm::vec3(glm::radians(-45.0f), glm::radians(45.0f), 0.0f);
        
        auto& lightComp = mainLight.AddComponent<LightComponent>();
        lightComp.type = LightComponent::LightType::Directional;
        lightComp.color = glm::vec4(1.0f, 0.95f, 0.9f, 1.0f);
        lightComp.intensity = 3.0f;
        lightComp.castsShadows = true;

        Logger::Info("SandboxApp", "BMW M5 E34 scene created.");
        
        editor->SetSelectedEntity((uint32_t)bmw);
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
