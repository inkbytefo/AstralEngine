#include "Core/Engine.h"
#include "Core/Logger.h"
#include "Subsystems/Platform/PlatformSubsystem.h"
#include "Subsystems/Asset/AssetSubsystem.h"
#include "Subsystems/ECS/ECSSubsystem.h"
#include "Subsystems/Renderer/RenderSubsystem.h"
#include <filesystem>

using namespace AstralEngine;

/**
 * @brief Astral Engine'in ana giriş noktası
 * 
 * Bu dosya sadece Engine'i başlatmaktan sorumludur.
 * Tüm karmaşıklık Engine sınıfı ve subsystem'ler tarafından yönetilir.
 */
int main(int argc, char* argv[]) {
    // Dosya loglamayı başlat (exe'nin yanına)
    Logger::InitializeFileLogging();
    Logger::SetLogLevel(Logger::LogLevel::Debug);
    
    Logger::Info("Main", "Starting Astral Engine...");
    Logger::Info("Main", "Engine Version: 0.1.0-alpha");
    
    try {
        // Engine instance oluştur
        Engine engine;

        // Set base path from executable path
        if (argc > 0 && argv[0]) {
            engine.SetBasePath(std::filesystem::path(argv[0]).parent_path());
        }
        
        // Subsystem'leri kaydet (sıra önemli!)
        Logger::Info("Main", "Registering subsystems...");
        
        // 1. Platform Subsystem (pencere, input)
        engine.RegisterSubsystem<PlatformSubsystem>();
        
        // 2. Asset Subsystem (varlık yönetimi)
        engine.RegisterSubsystem<AssetSubsystem>();
        
        // 3. ECS Subsystem (entity component system)
        engine.RegisterSubsystem<ECSSubsystem>();
        
        // 4. Render Subsystem (3D rendering)
        engine.RegisterSubsystem<RenderSubsystem>();
        
        // TODO: Diğer subsystem'ler
        // engine.RegisterSubsystem<ECSSubsystem>();
        // engine.RegisterSubsystem<PhysicsSubsystem>();
        // engine.RegisterSubsystem<AudioSubsystem>();
        
        Logger::Info("Main", "All subsystems registered. Starting engine...");
        
        // Engine'i çalıştır (bloklar, main loop)
        engine.Run();
        
        Logger::Info("Main", "Engine shutdown normally");
        
    } catch (const std::exception& e) {
        Logger::Critical("Main", "Fatal exception: {}", e.what());
        return -1;
    } catch (...) {
        Logger::Critical("Main", "Unknown fatal exception occurred");
        return -1;
    }
    
    Logger::Info("Main", "Astral Engine terminated");
    
    // Dosya loglamayı kapat
    Logger::ShutdownFileLogging();
    
    return 0;
}
