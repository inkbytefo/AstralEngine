#include "Core/Engine.h"
#include "Core/IApplication.h"
#include "Core/Logger.h"
#include "Subsystems/Asset/AssetSubsystem.h"
#include "Subsystems/Editor/SceneEditorSubsystem.h"
#include "Subsystems/Platform/PlatformSubsystem.h"
#include "Subsystems/Renderer/Core/RenderSubsystem.h"
#include "Subsystems/UI/UISubsystem.h"
#include <iostream>

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

    // The Engine already initializes core subsystems (Platform, Render, UI,
    // Asset) We just need to ensure the Editor Subsystem is registered and
    // initialized.

    // Check if SceneEditorSubsystem is already registered.
    auto *editor = m_engine->GetSubsystem<SceneEditorSubsystem>();
    if (!editor) {
      Logger::Warning("AstralEditor", "SceneEditorSubsystem not found. Ensure "
                                      "it is registered before running.");
    }

    Logger::Info("AstralEditor", "Editor Application Started.");
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
