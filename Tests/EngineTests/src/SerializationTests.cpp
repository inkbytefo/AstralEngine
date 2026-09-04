#include "TestFramework.hpp"
#include "AstralEngine.h"
#include "Astral/Core/TransformSystem.hpp"
#include "Astral/Core/RenderExtractionSystem.hpp"
#include "Astral/Renderer/SDFEdit.hpp"
#include <memory>
#include <filesystem>
#include <fstream>
#include <cmath>

namespace Astral::Test {

void RunSerializationTests() {
    const std::string suite = "SerializationSuite";

    auto sourceScene = std::make_shared<Scene>("Binary Serialization Level");

    // 1. Varlık #0: Hero Sphere (Transform + Health)
    Entity e0 = sourceScene->CreateEntity();
    e0.AddComponent<TransformComponent>(
        glm::vec3(1.5f, 2.0f, -3.0f),
        glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
        glm::vec3(0.5f)
    );
    e0.AddComponent<HealthComponent>(300);

    // Varlık #1 (Index 1: Boşluk)
    Entity e1_gap = sourceScene->CreateEntity();

    // 2. Varlık #2: Static Box (Transform + SDF)
    Entity e2 = sourceScene->CreateEntity();
    TEST_CHECK_MSG(suite, "Entity2Index2", e2.GetID() == 2, "e2 index 2 olmali!");
    e2.AddComponent<TransformComponent>(
        glm::vec3(0.0f, 1.0f, 0.0f),
        glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
        glm::vec3(2.0f, 1.0f, 4.0f)
    );
    e2.AddComponent<SDFComponent>(
        1u, // Box
        0u, // Union
        0.0f, 1u,
        glm::vec3(0.2f, 0.4f, 0.8f),
        0.4f, 0.1f
    );
    TEST_CHECK_MSG(suite, "SetParentValid", sourceScene->SetParent(e2, e0), "Child parent'a baglanabilmeli!");
    TEST_CHECK_MSG(suite, "CycleParentRejected", !sourceScene->SetParent(e0, e2), "Dongusel parent atamasi reddedilmeli!");

    // Arada Index 3..16 arasinda 14 adet bosluk olusturuyoruz
    std::vector<Entity> dummies;
    dummies.push_back(e1_gap); // Index 1 de silinecek
    for (int i = 3; i < 17; ++i) {
        dummies.push_back(sourceScene->CreateEntity());
    }

    // 3. Varlık #17: Dynamic Rocket (Velocity + Health) - Transform ve SDF yok!
    Entity e17 = sourceScene->CreateEntity();
    TEST_CHECK_MSG(suite, "SparsityCreated", e17.GetID() == 17, "Sparsity olusturulamadi!");
    e17.AddComponent<VelocityComponent>(
        glm::vec3(12.0f, -4.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f)
    );
    e17.AddComponent<HealthComponent>(50);

    // Bosluk entity'lerini temizliyoruz (Index 1 ve 3..16 free-list'e gider)
    for (auto& dummy : dummies) {
        sourceScene->DestroyEntity(dummy);
    }

    UpdateWorldTransforms(sourceScene->GetRegistry());
    TEST_CHECK(suite, "E0HasWorldTransform", e0.HasComponent<WorldTransformComponent>());
    TEST_CHECK(suite, "E2HasWorldTransform", e2.HasComponent<WorldTransformComponent>());

    // 1. Serileştir (.astral custom binary format v2) - Static C++20 Interface
    std::filesystem::create_directories("assets/scenes");
    const std::filesystem::path testFile = "assets/scenes/level_binary.astral";
    bool serSuccess = SceneSerializer::Serialize(sourceScene, testFile);
    TEST_CHECK_MSG(suite, "BinarySerializeSuccess", serSuccess, "Binary Serialize basarisiz!");

    // 2. Yeni boş sahne oluştur ve Deserialize et - Static C++20 Interface
    auto loadedScene = std::make_shared<Scene>("Empty");
    bool deserSuccess = SceneSerializer::Deserialize(loadedScene, testFile);
    TEST_CHECK_MSG(suite, "BinaryDeserializeSuccess", deserSuccess, "Binary Deserialize basarisiz!");

    // 3. Doğrulamalar: Contiguous SparseSet Pools & Non-Sequential Entity-Component Fidelity
    auto& transforms = loadedScene->GetRegistry().GetView<TransformComponent>();
    auto& sdfs       = loadedScene->GetRegistry().GetView<SDFComponent>();
    auto& velocities = loadedScene->GetRegistry().GetView<VelocityComponent>();
    auto& healths    = loadedScene->GetRegistry().GetView<HealthComponent>();

    TEST_CHECK_MSG(suite, "TransformsCountMatches", transforms.Size() == 2, "Deserialization sonrasi Transform sayisi uyusmuyor!");
    TEST_CHECK_MSG(suite, "SdfsCountMatches", sdfs.Size() == 1, "Deserialization sonrasi SDF sayisi uyusmuyor!");
    TEST_CHECK_MSG(suite, "VelocitiesCountMatches", velocities.Size() == 1, "Deserialization sonrasi Velocity sayisi uyusmuyor!");
    TEST_CHECK_MSG(suite, "HealthsCountMatches", healths.Size() == 2, "Deserialization sonrasi Health sayisi uyusmuyor!");

    // SparseSet Invariant Dogrulamalari:
    TEST_CHECK(suite, "TransformHandlesMatch", transforms.Entities()[0] == e0.GetHandle() && transforms.Entities()[1] == e2.GetHandle());
    TEST_CHECK(suite, "HealthHandlesMatch", healths.Entities()[0] == e0.GetHandle() && healths.Entities()[1] == e17.GetHandle());
    TEST_CHECK(suite, "SdfHandleMatches", sdfs.Entities()[0] == e2.GetHandle());
    TEST_CHECK(suite, "VelocityHandleMatches", velocities.Entities()[0] == e17.GetHandle());

    // Varlık #0 doğrulaması: Transform + Health var; SDF ve Velocity YOK!
    Entity loadedE0(e0.GetHandle(), loadedScene.get());
    TEST_CHECK(suite, "LoadedE0IsValid", loadedE0.IsValid());
    TEST_CHECK(suite, "LoadedE0HasTransform", loadedE0.HasComponent<TransformComponent>());
    TEST_CHECK(suite, "LoadedE0PositionX", loadedE0.GetComponent<TransformComponent>().position.x == 1.5f);
    TEST_CHECK(suite, "LoadedE0HasHealth", loadedE0.HasComponent<HealthComponent>());
    TEST_CHECK(suite, "LoadedE0Hp", loadedE0.GetComponent<HealthComponent>().hp == 300);
    TEST_CHECK(suite, "LoadedE0NoSdf", !loadedE0.HasComponent<SDFComponent>());
    TEST_CHECK(suite, "LoadedE0NoVelocity", !loadedE0.HasComponent<VelocityComponent>());

    // Varlık #2 doğrulaması: Transform + SDF var; Health ve Velocity YOK!
    Entity loadedE2(e2.GetHandle(), loadedScene.get());
    TEST_CHECK(suite, "LoadedE2IsValid", loadedE2.IsValid());
    TEST_CHECK(suite, "LoadedE2HasTransform", loadedE2.HasComponent<TransformComponent>());
    TEST_CHECK(suite, "LoadedE2ScaleX", loadedE2.GetComponent<TransformComponent>().scale.x == 2.0f);
    TEST_CHECK(suite, "LoadedE2HasSdf", loadedE2.HasComponent<SDFComponent>());
    TEST_CHECK(suite, "LoadedE2PrimitiveType", loadedE2.GetComponent<SDFComponent>().primitiveType == 1u);
    TEST_CHECK(suite, "LoadedE2NoHealth", !loadedE2.HasComponent<HealthComponent>());
    TEST_CHECK(suite, "LoadedE2NoVelocity", !loadedE2.HasComponent<VelocityComponent>());
    TEST_CHECK_MSG(suite, "LoadedE0NoWorldTransformTransient", !loadedE0.HasComponent<WorldTransformComponent>(),
                   "Transient world transform serialize edilmemeli!");
    TEST_CHECK_MSG(suite, "LoadedE2NoWorldTransformTransient", !loadedE2.HasComponent<WorldTransformComponent>(),
                   "Transient world transform deserialize edilmemeli!");

    // Hierarchy referanslari tam 64-bit index+generation ile round-trip yapmali.
    TEST_CHECK(suite, "LoadedE0HasHierarchy", loadedE0.HasComponent<HierarchyComponent>());
    TEST_CHECK(suite, "LoadedE2HasHierarchy", loadedE2.HasComponent<HierarchyComponent>());
    const auto& loadedParentHierarchy = loadedE0.GetComponent<HierarchyComponent>();
    const auto& loadedChildHierarchy = loadedE2.GetComponent<HierarchyComponent>();
    TEST_CHECK(suite, "ChildParentMatches", loadedChildHierarchy.parent == loadedE0.GetHandle());
    TEST_CHECK(suite, "ParentChildrenCount1", loadedParentHierarchy.children.size() == 1);
    TEST_CHECK(suite, "ParentChildMatches", loadedParentHierarchy.children[0] == loadedE2.GetHandle());

    // Render extraction local child pozisyonunu degil parent.world * local sonucunu kullanmali.
    UpdateWorldTransforms(loadedScene->GetRegistry());
    TEST_CHECK(suite, "LoadedE0WorldTransformAfterUpdate", loadedE0.HasComponent<WorldTransformComponent>());
    TEST_CHECK(suite, "LoadedE2WorldTransformAfterUpdate", loadedE2.HasComponent<WorldTransformComponent>());
    std::vector<SDFEditGPU> hierarchyEdits;
    std::vector<EntityHandle> hierarchyEntities;
    ExtractRenderData(loadedScene->GetRegistry(), hierarchyEdits, hierarchyEntities);
    TEST_CHECK(suite, "HierarchyExtractionCount1", hierarchyEdits.size() == 1 && hierarchyEntities[0] == loadedE2.GetHandle());
    TEST_CHECK(suite, "HierarchyExtractionPosX", std::abs(hierarchyEdits[0].position.x - 1.5f) < 0.0001f);
    TEST_CHECK(suite, "HierarchyExtractionPosY", std::abs(hierarchyEdits[0].position.y - 2.5f) < 0.0001f);
    TEST_CHECK(suite, "HierarchyExtractionPosZ", std::abs(hierarchyEdits[0].position.z + 3.0f) < 0.0001f);

    loadedE0.GetComponent<TransformComponent>().position.x = 4.0f;
    UpdateWorldTransforms(loadedScene->GetRegistry());
    ExtractRenderData(loadedScene->GetRegistry(), hierarchyEdits, hierarchyEntities);
    TEST_CHECK_MSG(suite, "ParentMotionReflectedInChild",
                   std::abs(hierarchyEdits[0].position.x - 4.0f) < 0.0001f,
                   "Parent hareketi child world transform'una ve extraction'a yansimali!");

    // Varlık #17 doğrulaması: Velocity + Health var; Transform ve SDF YOK!
    Entity loadedE17(e17.GetHandle(), loadedScene.get());
    TEST_CHECK(suite, "LoadedE17IsValid", loadedE17.IsValid());
    TEST_CHECK(suite, "LoadedE17HasVelocity", loadedE17.HasComponent<VelocityComponent>());
    TEST_CHECK(suite, "LoadedE17LinearX", loadedE17.GetComponent<VelocityComponent>().linear.x == 12.0f);
    TEST_CHECK(suite, "LoadedE17HasHealth", loadedE17.HasComponent<HealthComponent>());
    TEST_CHECK(suite, "LoadedE17Hp", loadedE17.GetComponent<HealthComponent>().hp == 50);
    TEST_CHECK(suite, "LoadedE17NoTransform", !loadedE17.HasComponent<TransformComponent>());
    TEST_CHECK(suite, "LoadedE17NoSdf", !loadedE17.HasComponent<SDFComponent>());

    loadedScene->DestroyEntity(loadedE0);
    TEST_CHECK_MSG(suite, "CascadeDeleteVerified",
                   !loadedE0.IsValid() && !loadedE2.IsValid(),
                   "Parent silinince child cascade silinmeli!");

    // 4. Unknown Chunk Graceful Skipping (Ileri Uyumluluk) Testi
    const std::filesystem::path unknownChunkFile = "assets/scenes/unknown_chunk.astral";
    const EntityID dummyId = MakeEntityHandle(0, 1);
    {
        std::ofstream stream(unknownChunkFile, std::ios::binary | std::ios::trunc);
        // Header v2
        SceneFileHeader fh{ .magic = { 'A', 'S', 'T', 'R' }, .version = 2, .activeEntityCount = 1 };
        stream.write(reinterpret_cast<const char*>(&fh), sizeof(fh));

        // 1. Bilinmeyen Chunk (TypeID 0xCAFEBABEDEAD, 1 entity, 16 bytes component)
        ComponentChunkHeader unkChunk{
            .typeId = 0xCAFEBABEDEADull,
            .version = 1,
            .flags = 0,
            .elementCount = 1,
            .entityDataSize = sizeof(EntityID),
            .componentDataSize = 16
        };
        stream.write(reinterpret_cast<const char*>(&unkChunk), sizeof(unkChunk));
        stream.write(reinterpret_cast<const char*>(&dummyId), sizeof(dummyId));
        char dummyPayload[16] = "UNKNOWN_DATA!!!";
        stream.write(dummyPayload, 16);

        // 2. Bilinen Chunk: HealthComponent (Entity 0, hp 777)
        ComponentChunkHeader healthChunk{
            .typeId = ComponentTraits<HealthComponent>::TypeHash,
            .version = 1,
            .flags = 0,
            .elementCount = 1,
            .entityDataSize = sizeof(EntityID),
            .componentDataSize = sizeof(HealthComponent)
        };
        stream.write(reinterpret_cast<const char*>(&healthChunk), sizeof(healthChunk));
        stream.write(reinterpret_cast<const char*>(&dummyId), sizeof(dummyId));
        HealthComponent hpComp{ .hp = 777 };
        stream.write(reinterpret_cast<const char*>(&hpComp), sizeof(hpComp));
    }

    auto unkScene = std::make_shared<Scene>("UnknownChunkTest");
    bool unkSuccess = SceneSerializer::Deserialize(unkScene, unknownChunkFile);
    TEST_CHECK_MSG(suite, "UnknownChunkSkippedGracefully", unkSuccess, "Bilinmeyen chunk graceful sekilde atlanamadi!");
    TEST_CHECK(suite, "UnknownChunkHealthCount", unkScene->GetRegistry().GetView<HealthComponent>().Size() == 1);
    TEST_CHECK(suite, "UnknownChunkHealthValue", unkScene->GetRegistry().GetComponent<HealthComponent>(dummyId).hp == 777);

    // 5. Atomic Staging Rollback Testi
    auto liveScene = std::make_shared<Scene>("LiveScene");
    Entity liveE = liveScene->CreateEntity();
    liveE.AddComponent<HealthComponent>(999);

    // Bozuk dosyayı deserialize etmeye çalışalım
    const std::filesystem::path truncatedFile = "assets/scenes/truncated.astral";
    {
        std::ofstream truncStream(truncatedFile, std::ios::binary);
        char partialHeader[6] = { 'A', 'S', 'T', 'R', 2, 0 };
        truncStream.write(partialHeader, 6);
    }
    bool atomicFail = SceneSerializer::Deserialize(liveScene, truncatedFile);
    TEST_CHECK_MSG(suite, "TruncatedFileFails", !atomicFail, "Truncated file fail etmeli!");
    // Canlı sahne bozulmamış olmalı (Atomic Staging)
    TEST_CHECK(suite, "AtomicLiveSceneSizePreserved", liveScene->GetRegistry().GetView<HealthComponent>().Size() == 1);
    TEST_CHECK(suite, "AtomicLiveSceneValuePreserved", liveScene->GetRegistry().GetComponent<HealthComponent>(liveE.GetHandle()).hp == 999);

    // 6. Diger Fast-Fail Testleri
    TEST_CHECK(suite, "SerializeNullSceneFails", !SceneSerializer::Serialize(nullptr, testFile));
    TEST_CHECK(suite, "DeserializeNullSceneFails", !SceneSerializer::Deserialize(nullptr, testFile));

    const std::filesystem::path corruptMagicFile = "assets/scenes/corrupt_magic.astral";
    {
        std::ofstream corruptStream(corruptMagicFile, std::ios::binary);
        char badMagic[12] = { 'X', 'X', 'X', 'X', 2, 0, 0, 0, 2, 0, 0, 0 };
        corruptStream.write(badMagic, 12);
    }
    auto dummyScene = std::make_shared<Scene>("Dummy");
    TEST_CHECK_MSG(suite, "CorruptMagicFastFails", !SceneSerializer::Deserialize(dummyScene, corruptMagicFile),
                   "Bozuk magic bytes fast-fail etmeli!");
    TEST_CHECK(suite, "NonExistentFileFastFails", !SceneSerializer::Deserialize(dummyScene, "non_existent_file.astral"));
}

} // namespace Astral::Test
