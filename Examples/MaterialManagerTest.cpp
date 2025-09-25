#include "Core/Engine.h"
#include "Core/Logger.h"
#include "Core/IApplication.h"
#include "Subsystems/Platform/PlatformSubsystem.h"
#include "Subsystems/Asset/AssetSubsystem.h"
#include "Subsystems/Renderer/RenderSubsystem.h"
#include "Subsystems/Renderer/Material/MaterialManager.h"
#include <filesystem>
#include <chrono>

using namespace AstralEngine;

/**
 * @brief Material Manager Test Application
 *
 * This application tests the asset-backed GetMaterial(const AssetHandle&) functionality
 * with caching to ensure repeated calls return the same instance.
 */
class MaterialManagerTestApp : public IApplication {
public:
    void OnStart(Engine* owner) override {
        Logger::Info("MaterialManagerTest", "Material Manager Test starting...");
        m_engine = owner;

        TestMaterialCaching();
    }

    void OnUpdate(float deltaTime) override {
        // Test completed, just wait
        m_testCompleted = true;
    }

    void OnShutdown() override {
        Logger::Info("MaterialManagerTest", "Material Manager Test shutting down...");

        if (m_testPassed) {
            Logger::Info("MaterialManagerTest", "✓ All tests PASSED");
        } else {
            Logger::Error("MaterialManagerTest", "✗ Some tests FAILED");
        }
    }

private:
    void TestMaterialCaching() {
        Logger::Info("MaterialManagerTest", "Testing Material Manager caching functionality...");

        auto* renderSubsystem = m_engine->GetSubsystem<RenderSubsystem>();
        if (!renderSubsystem) {
            Logger::Error("MaterialManagerTest", "Failed to get RenderSubsystem");
            return;
        }

        auto* materialManager = renderSubsystem->GetMaterialManager();
        if (!materialManager) {
            Logger::Error("MaterialManagerTest", "Failed to get MaterialManager");
            return;
        }

        auto* assetManager = m_engine->GetSubsystem<AssetSubsystem>()->GetAssetManager();
        if (!assetManager) {
            Logger::Error("MaterialManagerTest", "Failed to get AssetManager");
            return;
        }

        // Test 1: Create a test material asset
        Logger::Info("MaterialManagerTest", "Test 1: Creating test material asset...");

        // Create a simple material data for testing
        auto materialData = std::make_shared<MaterialData>();
        materialData->name = "TestMaterial";
        materialData->vertexShaderPath = "Assets/Shaders/Materials/pbr_material_vertex.slang";
        materialData->fragmentShaderPath = "Assets/Shaders/Materials/pbr_material_fragment.slang";
        materialData->properties.baseColor = glm::vec3(1.0f, 0.5f, 0.0f); // Orange color
        materialData->properties.metallic = 0.8f;
        materialData->properties.roughness = 0.2f;
        materialData->isValid = true;

        // Register the material data as an asset
        AssetHandle materialHandle = assetManager->RegisterAsset("TestMaterial", AssetHandle::Type::Material);
        if (!materialHandle.IsValid()) {
            Logger::Error("MaterialManagerTest", "Failed to register test material asset");
            return;
        }

        // Store the material data in the asset manager (simulating asset loading)
        // Note: In a real implementation, this would be done by an asset importer
        // For testing purposes, we'll manually set it
        Logger::Info("MaterialManagerTest", "Test material asset registered with handle: {}", materialHandle.GetID());

        // Test 2: Get material multiple times and verify caching
        Logger::Info("MaterialManagerTest", "Test 2: Testing material caching...");

        auto startTime = std::chrono::high_resolution_clock::now();

        // First call - should create new material
        std::shared_ptr<Material> material1 = materialManager->GetMaterial(materialHandle);
        if (!material1) {
            Logger::Error("MaterialManagerTest", "Failed to get material on first call");
            return;
        }

        auto firstCallTime = std::chrono::high_resolution_clock::now();
        auto firstCallDuration = std::chrono::duration_cast<std::chrono::microseconds>(firstCallTime - startTime);

        Logger::Info("MaterialManagerTest", "First call completed in {} microseconds", firstCallDuration.count());
        Logger::Info("MaterialManagerTest", "Material name: {}, Type: {}",
                     material1->GetName(), static_cast<int>(material1->GetType()));

        // Second call - should return cached material
        std::shared_ptr<Material> material2 = materialManager->GetMaterial(materialHandle);
        if (!material2) {
            Logger::Error("MaterialManagerTest", "Failed to get material on second call");
            return;
        }

        auto secondCallTime = std::chrono::high_resolution_clock::now();
        auto secondCallDuration = std::chrono::duration_cast<std::chrono::microseconds>(secondCallTime - firstCallTime);

        Logger::Info("MaterialManagerTest", "Second call completed in {} microseconds", secondCallDuration.count());

        // Third call - should return cached material
        std::shared_ptr<Material> material3 = materialManager->GetMaterial(materialHandle);
        if (!material3) {
            Logger::Error("MaterialManagerTest", "Failed to get material on third call");
            return;
        }

        auto thirdCallTime = std::chrono::high_resolution_clock::now();
        auto thirdCallDuration = std::chrono::duration_cast<std::chrono::microseconds>(thirdCallTime - secondCallTime);

        Logger::Info("MaterialManagerTest", "Third call completed in {} microseconds", thirdCallDuration.count());

        // Test 3: Verify caching works (same instance returned)
        Logger::Info("MaterialManagerTest", "Test 3: Verifying cache functionality...");

        if (material1.get() == material2.get() && material2.get() == material3.get()) {
            Logger::Info("MaterialManagerTest", "✓ Cache test PASSED - Same instance returned for repeated calls");
        } else {
            Logger::Error("MaterialManagerTest", "✗ Cache test FAILED - Different instances returned");
            return;
        }

        // Test 4: Verify material properties are preserved
        Logger::Info("MaterialManagerTest", "Test 4: Verifying material properties...");

        const MaterialProperties& props = material1->GetProperties();
        if (props.baseColor == glm::vec3(1.0f, 0.5f, 0.0f) &&
            props.metallic == 0.8f &&
            props.roughness == 0.2f) {
            Logger::Info("MaterialManagerTest", "✓ Material properties test PASSED");
        } else {
            Logger::Error("MaterialManagerTest", "✗ Material properties test FAILED");
            Logger::Error("MaterialManagerTest", "Expected: baseColor=(1,0.5,0), metallic=0.8, roughness=0.2");
            Logger::Error("MaterialManagerTest", "Got: baseColor=({},{},{}), metallic={}, roughness={}",
                         props.baseColor.x, props.baseColor.y, props.baseColor.z,
                         props.metallic, props.roughness);
            return;
        }

        // Test 5: Test error handling with invalid handle
        Logger::Info("MaterialManagerTest", "Test 5: Testing error handling...");

        AssetHandle invalidHandle;
        std::shared_ptr<Material> invalidMaterial = materialManager->GetMaterial(invalidHandle);
        if (!invalidMaterial) {
            Logger::Info("MaterialManagerTest", "✓ Error handling test PASSED - Invalid handle correctly returned nullptr");
        } else {
            Logger::Error("MaterialManagerTest", "✗ Error handling test FAILED - Invalid handle should return nullptr");
            return;
        }

        // Test 6: Verify material count in manager
        Logger::Info("MaterialManagerTest", "Test 6: Verifying material count...");

        size_t materialCount = materialManager->GetMaterialCount();
        size_t shaderCacheCount = materialManager->GetShaderCacheCount();

        Logger::Info("MaterialManagerTest", "Material count: {}, Shader cache count: {}", materialCount, shaderCacheCount);

        if (materialCount > 0) {
            Logger::Info("MaterialManagerTest", "✓ Material count test PASSED");
        } else {
            Logger::Error("MaterialManagerTest", "✗ Material count test FAILED - Expected at least 1 material");
            return;
        }

        // All tests passed
        m_testPassed = true;
        Logger::Info("MaterialManagerTest", "✓ All material caching tests PASSED successfully!");

        // Performance summary
        Logger::Info("MaterialManagerTest", "Performance Summary:");
        Logger::Info("MaterialManagerTest", "  First call: {} μs", firstCallDuration.count());
        Logger::Info("MaterialManagerTest", "  Second call: {} μs", secondCallDuration.count());
        Logger::Info("MaterialManagerTest", "  Third call: {} μs", thirdCallDuration.count());

        if (secondCallDuration.count() < firstCallDuration.count() / 10) {
            Logger::Info("MaterialManagerTest", "  ✓ Cache performance improvement: ~{}x faster",
                        firstCallDuration.count() / secondCallDuration.count());
        }
    }

    Engine* m_engine = nullptr;
    bool m_testPassed = false;
    bool m_testCompleted = false;
};

/**
 * @brief Main entry point for Material Manager Test
 */
int main(int argc, char* argv[]) {
    // Initialize file logging
    Logger::InitializeFileLogging();
    Logger::SetLogLevel(Logger::LogLevel::Debug);

    Logger::Info("MaterialManagerTest", "Starting Material Manager Test Application...");
    Logger::Info("MaterialManagerTest", "Testing asset-backed GetMaterial(const AssetHandle&) with caching");

    try {
        Engine engine;
        MaterialManagerTestApp testApp;

        // Set base path from executable path
        if (argc > 0 && argv[0]) {
            engine.SetBasePath(std::filesystem::path(argv[0]).parent_path());
        }

        // Register subsystems
        Logger::Info("MaterialManagerTest", "Registering subsystems...");

        engine.RegisterSubsystem<PlatformSubsystem>();
        engine.RegisterSubsystem<AssetSubsystem>();
        engine.RegisterSubsystem<RenderSubsystem>();

        Logger::Info("MaterialManagerTest", "All subsystems registered. Starting engine...");

        // Run engine with test application
        engine.Run(&testApp);

        Logger::Info("MaterialManagerTest", "Engine shutdown normally");

    } catch (const std::exception& e) {
        Logger::Critical("MaterialManagerTest", "Fatal exception: {}", e.what());
        return -1;
    } catch (...) {
        Logger::Critical("MaterialManagerTest", "Unknown fatal exception occurred");
        return -1;
    }

    Logger::Info("MaterialManagerTest", "Material Manager Test terminated");

    // Shutdown file logging
    Logger::ShutdownFileLogging();

    return 0;
}