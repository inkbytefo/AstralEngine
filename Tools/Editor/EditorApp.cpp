#include "Core/Engine.h"
#include "Core/IApplication.h"
#include "Core/Logger.h"
#include "Subsystems/Asset/AssetSubsystem.h"
#include "Subsystems/Editor/SceneEditorSubsystem.h"
#include "Subsystems/Platform/PlatformSubsystem.h"
#include "Subsystems/Renderer/Core/RenderSubsystem.h"
#include "Subsystems/UI/UISubsystem.h"
#include <iostream>

#include "Subsystems/Scene/Entity.h"
#include "Subsystems/Scene/Scene.h"
#include "ECS/Components.h"

using namespace AstralEngine;

class AstralEditorApp : public IApplication {
public:
  void OnStart(Engine *owner) override {
    Logger::Info("AstralEditor", "Starting Astral Editor...");
    m_engine = owner;

    // Ensure subsystems are present
    if (!m_engine->GetSubsystem<PlatformSubsystem>()) {
      Logger::Error("AstralEditor", "PlatformSubsystem missing!");
      return;
    }

    if (!m_engine->GetSubsystem<RenderSubsystem>()) {
      Logger::Error("AstralEditor", "RenderSubsystem missing!");
      return;
    }

    auto *editor = m_engine->GetSubsystem<SceneEditorSubsystem>();
    if (editor) {
        CreateTestScene();
    }

    Logger::Info("AstralEditor", "Editor Application Started with Test Scene.");
  }

  void CreateTestScene() {
    auto* editor = m_engine->GetSubsystem<SceneEditorSubsystem>();
    auto scene = editor->GetActiveScene();
    auto* assets = m_engine->GetSubsystem<AssetSubsystem>()->GetAssetManager();

    // 1. Floor
    Entity floor = scene->CreateEntity("Floor");
    auto& floorTC = floor.GetComponent<TransformComponent>();
    floorTC.scale = glm::vec3(20.0f, 0.1f, 20.0f);
    floorTC.position = glm::vec3(0.0f, -0.05f, 0.0f);
    
    auto& floorRC = floor.AddComponent<RenderComponent>();
    floorRC.modelHandle = assets->RegisterAsset("Models/Default/Cube.obj");
    floorRC.materialHandle = assets->RegisterAsset("Materials/Default.amat");
    floorRC.castsShadows = true;
    floorRC.receivesShadows = true;

    // 2. Cube
    Entity cube = scene->CreateEntity("Test Cube");
    auto& cubeTC = cube.GetComponent<TransformComponent>();
    cubeTC.position = glm::vec3(0.0f, 1.0f, 0.0f);
    
    auto& cubeRC = cube.AddComponent<RenderComponent>();
    cubeRC.modelHandle = assets->RegisterAsset("Models/Default/Cube.obj");
    cubeRC.materialHandle = assets->RegisterAsset("Materials/Default.amat");
    cubeRC.castsShadows = true;

    // 3. Main Directional Light
    Entity mainLight = scene->CreateEntity("Main Light");
    auto& lightTC = mainLight.GetComponent<TransformComponent>();
    lightTC.rotation = glm::vec3(glm::radians(-45.0f), glm::radians(45.0f), 0.0f);
    
    auto& lightComp = mainLight.AddComponent<LightComponent>();
    lightComp.type = LightComponent::LightType::Directional;
    lightComp.color = glm::vec3(1.0f, 0.95f, 0.8f);
    lightComp.intensity = 2.0f;
    lightComp.castsShadows = true;

    Logger::Info("AstralEditor", "Test scene created with Floor, Cube and Shadow-casting Light.");
  }

  void OnUpdate(float deltaTime) override {
    // Application specific update
  }

  void OnShutdown() override {
    Logger::Info("AstralEditor", "Shutting down Astral Editor...");
  }

private:
  Engine *m_engine = nullptr;
};

// Main Entry Point
int main(int argc, char **argv) {
  AstralEngine::Engine engine;

  // Create Application
  auto app = std::make_shared<AstralEditorApp>();

  // Register Core Subsystems
  engine.RegisterSubsystem<AstralEngine::PlatformSubsystem>();
  engine.RegisterSubsystem<AstralEngine::RenderSubsystem>();
  engine.RegisterSubsystem<AstralEngine::AssetSubsystem>();
  engine.RegisterSubsystem<AstralEngine::UISubsystem>();

  // Register Editor Subsystem
  engine.RegisterSubsystem<AstralEngine::SceneEditorSubsystem>();

  // Run Engine
  try {
    engine.Run(app.get());
  } catch (const std::exception &e) {
    std::cerr << "Engine crashed: " << e.what() << std::endl;
    return -1;
  }

  return 0;
}
