#include "Core/Engine.h"
#include "Core/IApplication.h"
#include "Core/Logger.h"
#include "Subsystems/Editor/SceneEditorSubsystem.h"
#include "Subsystems/Platform/PlatformSubsystem.h"
#include "Subsystems/Renderer/Core/RenderSubsystem.h"
#include "Subsystems/Asset/AssetSubsystem.h"
#include "Subsystems/UI/UISubsystem.h"
#include <iostream>

using namespace AstralEngine;

class AstralEditorApp : public IApplication {
public:
    void OnStart(Engine* owner) override {
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
        
        // The Engine already initializes core subsystems (Platform, Render, UI, Asset)
        // We just need to ensure the Editor Subsystem is registered and initialized.
        // However, subsystems are usually registered in Engine::Initialize or by the App.
        // Let's see if we can register it dynamically or if it's already there.
        // The Engine currently doesn't have a "RegisterSubsystem" public method for runtime?
        // Wait, Engine::Initialize registers default ones.
        
        // Check if SceneEditorSubsystem is already registered.
        auto* editor = m_engine->GetSubsystem<SceneEditorSubsystem>();
        if (!editor) {
            // It's likely NOT registered by default in the Engine core.
            // We should register it here.
            // But Engine::RegisterSubsystem is private/protected?
            // Let's check Engine.h.
            // If it's private, we might need to modify Engine.h or subclass Engine (not IApplication).
            // Actually, usually IApplication is just a listener.
            // The Engine structure might require us to add the subsystem in the main.cpp where Engine is created.
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
    Engine* m_engine = nullptr;
};

// Main Entry Point
// We need to implement the main function that creates the Engine and the App.
// Usually this is in a separate Main.cpp or macro.
// Let's assume we write the main() here.

int main(int argc, char** argv) {
    AstralEngine::Engine engine;
    
    // Create Application
    auto app = std::make_shared<AstralEditorApp>();
    
    // Register Editor Subsystem
    engine.RegisterSubsystem<AstralEngine::SceneEditorSubsystem>();
    
    // Run Engine
    try {
        engine.Run(app.get());
    } catch (const std::exception& e) {
        std::cerr << "Engine crashed: " << e.what() << std::endl;
        return -1;
    }
    
    return 0;
}
