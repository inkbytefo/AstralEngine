#include "Astral/Core/Application.hpp"
#include "Astral/Core/Registry.hpp"
#include "Astral/Scene/Scene.hpp"
#include "Astral/Scene/SceneManager.hpp"
#include "Astral/Scene/SceneSerializer.hpp"
#include "Astral/Core/Components.hpp"
#include "Astral/Core/RenderExtractionSystem.hpp"
#include "Astral/Renderer/SDFEdit.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>

// Fizik Sistemi: yalnizca verileri isler, kendi icinde durum tutmaz.
// SparseSet'in dense (kontigu) dizisini gezer -> cache dostu.
static void PhysicsSystem(Astral::Registry& registry, float deltaTime) {
    auto& transforms = registry.GetView<Astral::TransformComponent>();

    for (auto&& [entity, transform] : transforms) {
        if (registry.HasComponent<Astral::VelocityComponent>(entity)) {
            auto& velocity = registry.GetComponent<Astral::VelocityComponent>(entity);
            transform.position += velocity.linear * deltaTime;
        }
    }
}

static void RunEcsTests() {
    std::cout << "=== [Astral Engine: ECS Dogrulama Testi] ===\n";
    Astral::Registry registry;

    // 1. Oyuncu gemisi (Transform + Velocity + Health)
    Astral::EntityID playerShip = registry.CreateEntity();
    registry.AddComponent<Astral::TransformComponent>(playerShip, {{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}});
    registry.AddComponent<Astral::VelocityComponent>(playerShip, {{15.0f, 5.0f, 0.0f}, {0.0f, 0.0f, 0.0f}});
    registry.AddComponent<Astral::HealthComponent>(playerShip, {200});

    // 2. Sabit uzay istasyonu (yalnizca Transform)
    Astral::EntityID spaceStation = registry.CreateEntity();
    registry.AddComponent<Astral::TransformComponent>(spaceStation, {{100.0f, 100.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}});

    // 3. Goktasi (Transform + Velocity)
    Astral::EntityID asteroid = registry.CreateEntity();
    registry.AddComponent<Astral::TransformComponent>(asteroid, {{0.0f, 50.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}});
    registry.AddComponent<Astral::VelocityComponent>(asteroid, {{2.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}});

    std::cout << "[ECS Test] playerShip.hp = "
              << registry.GetComponent<Astral::HealthComponent>(playerShip).hp << "\n";
    std::cout << "[ECS Test] station has hp? = "
              << (registry.HasComponent<Astral::HealthComponent>(spaceStation) ? "evet" : "hayir") << "\n";

    PhysicsSystem(registry, 1.0f);

    const bool removed = registry.RemoveComponent<Astral::VelocityComponent>(asteroid);
    std::cout << "[ECS Test] Goktasi hizi kaldirildi mi? -> " << (removed ? "evet" : "hayir") << "\n";

    registry.DestroyEntity(spaceStation);
    std::cout << "[ECS Test] Istasyon DestroyEntity sonrasi var mi? -> "
              << (registry.HasComponent<Astral::TransformComponent>(spaceStation) ? "evet" : "hayir") << "\n";
    std::cout << "=== [ECS Dogrulama Testi Basariyla Tamamlandi] ===\n\n";
}

static void RunSceneTests() {
    std::cout << "=== [Astral Engine: Scene Management & Deep-Copy Dogrulama Testi] ===\n";

    // 1. Editor Sahnesi olustur
    auto editorScene = std::make_shared<Astral::Scene>("Authoring Level");
    Astral::Entity originalShip = editorScene->CreateEntity();
    originalShip.AddComponent<Astral::TransformComponent>(glm::vec3(10.0f, 20.0f, 30.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f));
    originalShip.AddComponent<Astral::VelocityComponent>(glm::vec3(5.0f, 0.0f, 0.0f), glm::vec3(0.0f));
    originalShip.AddComponent<Astral::HealthComponent>(500);

    std::cout << "[Scene Test] Editor Seviyesi Hazirlandi. Nesne ID: " << originalShip.GetID()
              << ", Orijinal Pos X: " << originalShip.GetComponent<Astral::TransformComponent>().position.x
              << ", HP: " << originalShip.GetComponent<Astral::HealthComponent>().hp << "\n";

    // 2. Play dugmesine basildi: Editor -> Runtime Deep-Copy Klonlama
    auto runtimeScene = Astral::Scene::Copy(editorScene);
    assert(runtimeScene != nullptr);
    assert(runtimeScene != editorScene);

    Astral::SceneManager sceneManager;
    sceneManager.SetActiveScene(runtimeScene);
    runtimeScene->OnRuntimeStart();

    // 3. Runtime sahnesinde simulasyon calistir ve nesneleri mutasyona ugrat
    Astral::Entity clonedShip(originalShip.GetHandle(), runtimeScene.get());
    assert(clonedShip.IsValid());
    assert(clonedShip.HasComponent<Astral::TransformComponent>());

    runtimeScene->OnUpdate(2.0f); // 5.0 m/s * 2s = +10m -> pos.x = 20.0f
    clonedShip.GetComponent<Astral::HealthComponent>().hp = 120; // Can azaldi

    std::cout << "[Scene Test] Runtime Simulasyon Sonrasi: Cloned Pos X: " 
              << clonedShip.GetComponent<Astral::TransformComponent>().position.x
              << ", Cloned HP: " << clonedShip.GetComponent<Astral::HealthComponent>().hp << "\n";

    // 4. Orijinal Editor sahnesinin bozulmadigini (Derin kopyalamanin basarisini) teyit et!
    float origX = originalShip.GetComponent<Astral::TransformComponent>().position.x;
    int origHp = originalShip.GetComponent<Astral::HealthComponent>().hp;
    std::cout << "[Scene Test] Orijinal Editor Sahnesi Durumu: Pos X = " << origX << " (Beklenen: 10.0), HP = " << origHp << " (Beklenen: 500)\n";
    assert(origX == 10.0f && "Deep copy basarisiz! Editor sahnesi mutasyona ugradi!");
    assert(origHp == 500 && "Deep copy basarisiz! Editor sahnesi mutasyona ugradi!");

    // 5. Runtime sahnesinde nesneyi Destroy et
    runtimeScene->DestroyEntity(clonedShip);
    assert(!clonedShip.IsValid() && "Destroy edilen clonedShip artik IsValid olmamalidir!");
    assert(!clonedShip.HasComponent<Astral::HealthComponent>());
    assert(originalShip.IsValid() && "Editor nesnesi silinmemelidir ve IsValid kalmalidir!");
    assert(originalShip.HasComponent<Astral::HealthComponent>() && "Editor nesnesi silinmemelidir!");

    sceneManager.UnloadCurrentScene();
    std::cout << "=== [Scene Management & Deep-Copy Dogrulama Testi Basariyla Tamamlandi] ===\n\n";
}

static void RunGenerationalIdentityTests() {
    std::cout << "=== [Astral Engine: Generational Entity & Lifetime Tests] ===\n";
    auto scene = std::make_shared<Astral::Scene>("Identity Test Scene");
    auto& reg = scene->GetRegistry();

    // 1. Test_Create_IsAlive
    Astral::Entity e0 = scene->CreateEntity();
    assert(e0.IsValid() && "Yeni olusturulan varlik IsValid olmali!");
    assert(reg.IsAlive(e0.GetHandle()) && "Registry IsAlive true donmeli!");
    assert(e0.GetIndex() == 0 && "Ilk entity index 0 olmali!");
    assert(e0.GetGeneration() == 1 && "Ilk entity generation 1 olmali!");
    e0.AddComponent<Astral::HealthComponent>(100);

    // 2. Test_Destroy_Invalidation
    scene->DestroyEntity(e0);
    assert(!e0.IsValid() && "Destroy sonrasi e0.IsValid false olmali!");
    assert(!reg.IsAlive(e0.GetHandle()) && "Destroy sonrasi reg.IsAlive false olmali!");
    assert(!e0.HasComponent<Astral::HealthComponent>() && "Bilesenler temizlenmis olmali!");

    // 3. Test_FreeList_Recycling & Stale Handle Protection
    // Ayni indekse sahip yeni entity olustur (Gen 2 olacak)
    Astral::Entity e0_recycled = scene->CreateEntity();
    assert(e0_recycled.GetIndex() == 0 && "Free-list ayni indeksi (0) recycle etmeli!");
    assert(e0_recycled.GetGeneration() == 2 && "Recycle edilen entity'nin generation'i 2 olmali!");
    assert(e0_recycled.GetHandle() != e0.GetHandle() && "Eski ve yeni 64-bit handle'lar farkli olmali!");
    assert(e0_recycled.IsValid() && "Recycle entity gecerli olmali!");
    assert(!e0.IsValid() && "Eski handle (e0) gecersiz kalmali!");
    assert(!reg.IsAlive(e0.GetHandle()) && "Eski handle Registry tarafindan kesinlikle reddedilmeli!");

    // 4. Ghost Mutation Engeli: e0_recycled'a can verelim
    e0_recycled.AddComponent<Astral::HealthComponent>(500);
    assert(e0_recycled.GetComponent<Astral::HealthComponent>().hp == 500);
    // Eski e0 handle'i uzerinden HasComponent cagrisi IsValid assert'i / IsAlive korumasi saglar:
    assert(!e0.HasComponent<Astral::HealthComponent>() && "Eski handle yeni component'e erisememeli!");

    // 5. Test_Double_Destroy: Eski handle uzerinden tekrar destroy cagrisi hicbir seyi bozmamali
    scene->DestroyEntity(e0); // Gecersiz handle, sessizce yok sayilmali
    assert(e0_recycled.IsValid() && "Gecersiz destroy cagrisi yasayan entity'yi etkilememeli!");
    assert(reg.IsAlive(e0_recycled.GetHandle()));

    // 6. Test_Multiple_Recycling_Generations: Birden fazla geri donusum dongusu
    Astral::Entity eA = scene->CreateEntity(); // Index 1, Gen 1
    assert(eA.GetIndex() == 1 && eA.GetGeneration() == 1);
    scene->DestroyEntity(eA);
    Astral::Entity eB = scene->CreateEntity(); // Index 1, Gen 2
    assert(eB.GetIndex() == 1 && eB.GetGeneration() == 2);
    scene->DestroyEntity(eB);
    // 7. Test_RenderExtraction_Generation_Preservation:
    // ExtractRenderData fonksiyonunun 64-bit Generation bilgisini korudugunu ve picking nesnesinin IsValid oldugunu dogrula
    Astral::Entity eC = scene->CreateEntity(); // Index 1, Gen 3
    assert(eC.GetIndex() == 1 && eC.GetGeneration() == 3);
    eC.AddComponent<Astral::TransformComponent>(glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f));
    eC.AddComponent<Astral::SDFComponent>();
    std::vector<Astral::SDFEditGPU> extractedEdits;
    std::vector<Astral::EntityHandle> extractedHandles;
    Astral::ExtractRenderData(scene->GetRegistry(), extractedEdits, extractedHandles);
    assert(extractedHandles.size() == 1);
    assert(extractedHandles[0] == eC.GetHandle() && "ExtractRenderData tam 64-bit kanonik handle'i korumali!");
    assert(Astral::GetEntityGeneration(extractedHandles[0]) == 3 && "Generation 3 korunmus olmali (uint32 kirpilmamali)!");
    Astral::Entity pickedEntity(extractedHandles[0], scene.get());
    assert(pickedEntity.IsValid() && "Extracted handle ile kurulan Entity IsValid() olmali!");
    assert(pickedEntity.GetGeneration() == 3 && "Picked entity generation 3 olmali!");

    std::cout << "[Identity Test] 64-bit Generational Handle, FreeList ve Ghost Mutation korumasi %100 dogrulandi!\n";
    std::cout << "[Identity Test] ExtractRenderData 64-bit Generation korumasi ve Picking nesne gecerliligi dogrulandi!\n";
    std::cout << "=== [Generational Entity & Lifetime Testleri Basariyla Tamamlandi] ===\n\n";
}

static void RunSerializationTests() {
    std::cout << "=== [Astral Engine: Custom DOD Binary Scene Serialization Testi (v2)] ===\n";

    auto sourceScene = std::make_shared<Astral::Scene>("Binary Serialization Level");

    // 1. Varlık #0: Hero Sphere (Transform + Health)
    Astral::Entity e0 = sourceScene->CreateEntity();
    e0.AddComponent<Astral::TransformComponent>(
        glm::vec3(1.5f, 2.0f, -3.0f),
        glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
        glm::vec3(0.5f)
    );
    e0.AddComponent<Astral::HealthComponent>(300);

    // Varlık #1 (Index 1: Boşluk)
    Astral::Entity e1_gap = sourceScene->CreateEntity();

    // 2. Varlık #2: Static Box (Transform + SDF)
    Astral::Entity e2 = sourceScene->CreateEntity();
    assert(e2.GetID() == 2 && "e2 index 2 olmali!");
    e2.AddComponent<Astral::TransformComponent>(
        glm::vec3(0.0f, 1.0f, 0.0f),
        glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
        glm::vec3(2.0f, 1.0f, 4.0f)
    );
    e2.AddComponent<Astral::SDFComponent>(
        1u, // Box
        0u, // Union
        0.0f, 1u,
        glm::vec3(0.2f, 0.4f, 0.8f),
        0.4f, 0.1f
    );

    // Arada Index 3..16 arasinda 14 adet bosluk olusturuyoruz
    std::vector<Astral::Entity> dummies;
    dummies.push_back(e1_gap); // Index 1 de silinecek
    for (int i = 3; i < 17; ++i) {
        dummies.push_back(sourceScene->CreateEntity());
    }

    // 3. Varlık #17: Dynamic Rocket (Velocity + Health) - Transform ve SDF yok!
    Astral::Entity e17 = sourceScene->CreateEntity();
    assert(e17.GetID() == 17 && "Sparsity olusturulamadi!");
    e17.AddComponent<Astral::VelocityComponent>(
        glm::vec3(12.0f, -4.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f)
    );
    e17.AddComponent<Astral::HealthComponent>(50);

    // Bosluk entity'lerini temizliyoruz (Index 1 ve 3..16 free-list'e gider)
    for (auto& dummy : dummies) {
        sourceScene->DestroyEntity(dummy);
    }

    // 1. Serileştir (.astral custom binary format v2) - Static C++20 Interface
    const std::filesystem::path testFile = "assets/scenes/level_binary.astral";
    bool serSuccess = Astral::SceneSerializer::Serialize(sourceScene, testFile);
    assert(serSuccess && "Binary Serialize basarisiz!");

    // 2. Yeni boş sahne oluştur ve Deserialize et - Static C++20 Interface
    auto loadedScene = std::make_shared<Astral::Scene>("Empty");
    bool deserSuccess = Astral::SceneSerializer::Deserialize(loadedScene, testFile);
    assert(deserSuccess && "Binary Deserialize basarisiz!");

    // 3. Doğrulamalar: Contiguous SparseSet Pools & Non-Sequential Entity-Component Fidelity
    auto& transforms = loadedScene->GetRegistry().GetView<Astral::TransformComponent>();
    auto& sdfs       = loadedScene->GetRegistry().GetView<Astral::SDFComponent>();
    auto& velocities = loadedScene->GetRegistry().GetView<Astral::VelocityComponent>();
    auto& healths    = loadedScene->GetRegistry().GetView<Astral::HealthComponent>();

    assert(transforms.Size() == 2 && "Deserialization sonrasi Transform sayisi uyusmuyor!");
    assert(sdfs.Size() == 1 && "Deserialization sonrasi SDF sayisi uyusmuyor!");
    assert(velocities.Size() == 1 && "Deserialization sonrasi Velocity sayisi uyusmuyor!");
    assert(healths.Size() == 2 && "Deserialization sonrasi Health sayisi uyusmuyor!");

    // SparseSet Invariant Dogrulamalari:
    assert(transforms.Entities()[0] == e0.GetHandle() && transforms.Entities()[1] == e2.GetHandle());
    assert(healths.Entities()[0] == e0.GetHandle() && healths.Entities()[1] == e17.GetHandle());
    assert(sdfs.Entities()[0] == e2.GetHandle());
    assert(velocities.Entities()[0] == e17.GetHandle());

    // Varlık #0 doğrulaması: Transform + Health var; SDF ve Velocity YOK!
    Astral::Entity loadedE0(e0.GetHandle(), loadedScene.get());
    assert(loadedE0.IsValid());
    assert(loadedE0.HasComponent<Astral::TransformComponent>());
    assert(loadedE0.GetComponent<Astral::TransformComponent>().position.x == 1.5f);
    assert(loadedE0.HasComponent<Astral::HealthComponent>());
    assert(loadedE0.GetComponent<Astral::HealthComponent>().hp == 300);
    assert(!loadedE0.HasComponent<Astral::SDFComponent>());
    assert(!loadedE0.HasComponent<Astral::VelocityComponent>());

    // Varlık #2 doğrulaması: Transform + SDF var; Health ve Velocity YOK!
    Astral::Entity loadedE2(e2.GetHandle(), loadedScene.get());
    assert(loadedE2.IsValid());
    assert(loadedE2.HasComponent<Astral::TransformComponent>());
    assert(loadedE2.GetComponent<Astral::TransformComponent>().scale.x == 2.0f);
    assert(loadedE2.HasComponent<Astral::SDFComponent>());
    assert(loadedE2.GetComponent<Astral::SDFComponent>().primitiveType == 1u);
    assert(!loadedE2.HasComponent<Astral::HealthComponent>());
    assert(!loadedE2.HasComponent<Astral::VelocityComponent>());

    // Varlık #17 doğrulaması: Velocity + Health var; Transform ve SDF YOK!
    Astral::Entity loadedE17(e17.GetHandle(), loadedScene.get());
    assert(loadedE17.IsValid());
    assert(loadedE17.HasComponent<Astral::VelocityComponent>());
    assert(loadedE17.GetComponent<Astral::VelocityComponent>().linear.x == 12.0f);
    assert(loadedE17.HasComponent<Astral::HealthComponent>());
    assert(loadedE17.GetComponent<Astral::HealthComponent>().hp == 50);
    assert(!loadedE17.HasComponent<Astral::TransformComponent>());
    assert(!loadedE17.HasComponent<Astral::SDFComponent>());

    std::cout << "[Serialization Test] DOD Binary Dogrulama (v2): Non-sequential seyrek Entity-Component eslemesi %100 dogrulandi!\n";

    // 4. Unknown Chunk Graceful Skipping (Ileri Uyumluluk) Testi
    const std::filesystem::path unknownChunkFile = "assets/scenes/unknown_chunk.astral";
    const Astral::EntityID dummyId = Astral::MakeEntityHandle(0, 1);
    {
        std::ofstream stream(unknownChunkFile, std::ios::binary | std::ios::trunc);
        // Header v2
        Astral::SceneFileHeader fh{ .magic = { 'A', 'S', 'T', 'R' }, .version = 2, .activeEntityCount = 1 };
        stream.write(reinterpret_cast<const char*>(&fh), sizeof(fh));

        // 1. Bilinmeyen Chunk (TypeID 0xCAFEBABEDEAD, 1 entity, 16 bytes component)
        Astral::ComponentChunkHeader unkChunk{
            .typeId = 0xCAFEBABEDEADull,
            .version = 1,
            .flags = 0,
            .elementCount = 1,
            .entityDataSize = sizeof(Astral::EntityID),
            .componentDataSize = 16
        };
        stream.write(reinterpret_cast<const char*>(&unkChunk), sizeof(unkChunk));
        stream.write(reinterpret_cast<const char*>(&dummyId), sizeof(dummyId));
        char dummyPayload[16] = "UNKNOWN_DATA!!!";
        stream.write(dummyPayload, 16);

        // 2. Bilinen Chunk: HealthComponent (Entity 0, hp 777)
        Astral::ComponentChunkHeader healthChunk{
            .typeId = Astral::ComponentTraits<Astral::HealthComponent>::TypeHash,
            .version = 1,
            .flags = 0,
            .elementCount = 1,
            .entityDataSize = sizeof(Astral::EntityID),
            .componentDataSize = sizeof(Astral::HealthComponent)
        };
        stream.write(reinterpret_cast<const char*>(&healthChunk), sizeof(healthChunk));
        stream.write(reinterpret_cast<const char*>(&dummyId), sizeof(dummyId));
        Astral::HealthComponent hpComp{ .hp = 777 };
        stream.write(reinterpret_cast<const char*>(&hpComp), sizeof(hpComp));
    }

    auto unkScene = std::make_shared<Astral::Scene>("UnknownChunkTest");
    bool unkSuccess = Astral::SceneSerializer::Deserialize(unkScene, unknownChunkFile);
    assert(unkSuccess && "Bilinmeyen chunk graceful sekilde atlanamadi!");
    assert(unkScene->GetRegistry().GetView<Astral::HealthComponent>().Size() == 1);
    assert(unkScene->GetRegistry().GetComponent<Astral::HealthComponent>(dummyId).hp == 777);
    std::cout << "[Serialization Test] Forward-Compatibility: Bilinmeyen chunk basariyla atlandi ve sahne yuklendi!\n";

    // 5. Atomic Staging Rollback Testi
    auto liveScene = std::make_shared<Astral::Scene>("LiveScene");
    Astral::Entity liveE = liveScene->CreateEntity();
    liveE.AddComponent<Astral::HealthComponent>(999);

    // Bozuk dosyayı deserialize etmeye çalışalım
    const std::filesystem::path truncatedFile = "assets/scenes/truncated.astral";
    {
        std::ofstream truncStream(truncatedFile, std::ios::binary);
        char partialHeader[6] = { 'A', 'S', 'T', 'R', 2, 0 };
        truncStream.write(partialHeader, 6);
    }
    bool atomicFail = Astral::SceneSerializer::Deserialize(liveScene, truncatedFile);
    assert(!atomicFail && "Truncated file fail etmeli!");
    // Canlı sahne bozulmamış olmalı (Atomic Staging)
    assert(liveScene->GetRegistry().GetView<Astral::HealthComponent>().Size() == 1);
    assert(liveScene->GetRegistry().GetComponent<Astral::HealthComponent>(liveE.GetHandle()).hp == 999);
    std::cout << "[Serialization Test] Atomic Staging Rollback: Hatali dosya canli sahneyi bozmadi!\n";

    // 6. Diger Fast-Fail Testleri
    assert(!Astral::SceneSerializer::Serialize(nullptr, testFile));
    assert(!Astral::SceneSerializer::Deserialize(nullptr, testFile));

    const std::filesystem::path corruptMagicFile = "assets/scenes/corrupt_magic.astral";
    {
        std::ofstream corruptStream(corruptMagicFile, std::ios::binary);
        char badMagic[12] = { 'X', 'X', 'X', 'X', 2, 0, 0, 0, 2, 0, 0, 0 };
        corruptStream.write(badMagic, 12);
    }
    auto dummyScene = std::make_shared<Astral::Scene>("Dummy");
    assert(!Astral::SceneSerializer::Deserialize(dummyScene, corruptMagicFile) && "Bozuk magic bytes fast-fail etmeli!");
    assert(!Astral::SceneSerializer::Deserialize(dummyScene, "non_existent_file.astral"));

    std::cout << "[Serialization Test] Fast-Fail Guvenlik Testleri: Gecersiz magic, EOF ve null kontrolleri basarili!\n";
    std::cout << "=== [Custom DOD Binary Scene Serialization Testi Basariyla Tamamlandi] ===\n\n";
}

int main(int argc, char* argv[]) {
    Astral::AppConfig config;
    int maxFrames = -1;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--test" || arg == "--test-only") {
            maxFrames = 10;
        } else if (arg == "--bench") {
            config.benchMode = true;
        } else if (arg == "--bench-frames" && i + 1 < argc) {
            config.benchFrames = std::stoi(argv[++i]);
            config.benchMode = true;
        } else if (arg == "--bench-out" && i + 1 < argc) {
            config.benchOutputFile = argv[++i];
            config.benchMode = true;
        } else if (arg == "--frames" && i + 1 < argc) {
            maxFrames = std::stoi(argv[++i]);
        } else if (arg == "--width" && i + 1 < argc) {
            config.width = std::stoi(argv[++i]);
        } else if (arg == "--height" && i + 1 < argc) {
            config.height = std::stoi(argv[++i]);
        } else if (arg == "--normal" && i + 1 < argc) {
            std::string mode = argv[++i];
            if (mode == "tetra" || mode == "tetrahedron" || mode == "1") {
                config.normalMode = 1;
            } else {
                config.normalMode = 0;
            }
        } else if (arg == "--shader" && i + 1 < argc) {
            config.shaderPath = argv[++i];
        } else if (arg == "--legacy-map") {
            config.legacyMap = true;
        } else if (arg == "--grid") {
            config.useGrid = true;
        } else if (arg == "--no-grid") {
            config.useGrid = false;
        } else if (arg == "--stress") {
            config.stressTest = true;
        } else if (arg == "--opt-shadow") {
            config.optShadow = true;
        } else if (arg == "--no-opt-shadow") {
            config.optShadow = false;
        } else if (arg == "--taa") {
            config.enableTAA = true;
        } else if (arg == "--no-taa") {
            config.enableTAA = false;
        }
    }

    // 1. ECS Cekirdek Testlerini Calistir
    RunEcsTests();

    // 2. Generational Entity & Lifetime Testlerini Calistir (Phase 4)
    RunGenerationalIdentityTests();

    // 3. Scene Management & Deep-Copy Testlerini Calistir
    RunSceneTests();

    // 4. Scene Serialization & Deserialization Testlerini Calistir
    RunSerializationTests();

    // 4. Astral Engine Vulkan 1.4 & Pencere Uygulamasini Calistir
    Astral::Application app(config);
    app.Run(maxFrames);

    return 0;
}