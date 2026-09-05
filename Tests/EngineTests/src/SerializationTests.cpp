#include "TestFramework.hpp"
#include "AstralEngine.h"
#include "Astral/Core/TransformSystem.hpp"
#include "Astral/Core/RenderExtractionSystem.hpp"
#include "Astral/Renderer/SDFEdit.hpp"
#include <memory>
#include <filesystem>
#include <fstream>
#include <cmath>

namespace Astral {

// Oyun geliştiricisi tarafından motor çekirdeği değiştirilmeden tanımlanan örnek bileşen
struct CustomGameComponent {
    int score = 42;
    float mana = 99.5f;
};
ASTRAL_REGISTER_COMPONENT_TRAIT(CustomGameComponent);

} // namespace Astral

namespace Astral::Test {

void RunSerializationTests() {
    const std::string suite = "SerializationSuite";

    // Test dosyaları için izole geçici çalışma dizini oluşturuyoruz (repo kirliliği engellenir)
    const std::filesystem::path testDir = std::filesystem::temp_directory_path() / "astral_test_serialization";
    std::error_code ec;
    std::filesystem::remove_all(testDir, ec);
    std::filesystem::create_directories(testDir);

    // =========================================================================
    // 1. Temel DOD Binary Serialization & Hierarchy Round-Trip Testi
    // =========================================================================
    auto sourceScene = std::make_shared<Scene>("Binary Serialization Level");

    // Varlık #0: Hero Sphere (Transform + Health)
    Entity e0 = sourceScene->CreateEntity();
    e0.AddComponent<TransformComponent>(
        glm::vec3(1.5f, 2.0f, -3.0f),
        glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
        glm::vec3(0.5f)
    );
    e0.AddComponent<HealthComponent>(300);

    // Varlık #1 (Index 1: Boşluk)
    Entity e1_gap = sourceScene->CreateEntity();

    // Varlık #2: Static Box (Transform + SDF)
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

    // Varlık #17: Dynamic Rocket (Velocity + Health) - Transform ve SDF yok!
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

    // 1. Serileştir (.astral custom binary format v2)
    const std::filesystem::path testFile = testDir / "level_binary.astral";
    bool serSuccess = SceneSerializer::Serialize(sourceScene, testFile);
    TEST_CHECK_MSG(suite, "BinarySerializeSuccess", serSuccess, "Binary Serialize basarisiz!");

    // 2. Yeni boş sahne oluştur ve Deserialize et
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

    // =========================================================================
    // 2. Kapsamlı Authoring Round-Trip (Unicode Tag, Visibility, Boş Düğüm, Sahne Adı) — F01, F02
    // =========================================================================
    {
        const std::string originalSceneName = "Uzay İstasyonu Bölüm 1 - Kadıköy Rıhtım 🚀";
        auto authoringScene = std::make_shared<Scene>(originalSceneName);

        // a) Kahraman (Unicode Tag, Transform, Visibility: true)
        const std::string heroTag = "Kahraman 🚀 (İstasyon Ana Girişi - Şahin Tepesi)";
        Entity hero = authoringScene->CreateEntity();
        hero.AddComponent<TagComponent>(heroTag);
        hero.AddComponent<TransformComponent>(glm::vec3(1.0f, 2.0f, 3.0f));
        hero.AddComponent<VisibilityComponent>(true);

        // b) Gizli Ebeveyn Düğümü (Visibility: false)
        const std::string hiddenParentTag = "Gizli Radar Kulesi";
        Entity hiddenParent = authoringScene->CreateEntity();
        hiddenParent.AddComponent<TagComponent>(hiddenParentTag);
        hiddenParent.AddComponent<TransformComponent>(glm::vec3(0.0f, 10.0f, 0.0f));
        hiddenParent.AddComponent<VisibilityComponent>(false);

        // c) Gizli Ebeveynin Görünür Çocuğu (Visibility: true)
        const std::string visibleChildTag = "Görünür Alt Sensör #1";
        Entity visibleChild = authoringScene->CreateEntity();
        visibleChild.AddComponent<TagComponent>(visibleChildTag);
        visibleChild.AddComponent<TransformComponent>(glm::vec3(0.0f, 1.0f, 0.0f));
        visibleChild.AddComponent<VisibilityComponent>(true);
        TEST_CHECK(suite, "SetParentVisibleChild", authoringScene->SetParent(visibleChild, hiddenParent));

        // d) Görünür Ebeveyn (Visibility: true)
        const std::string visibleParentTag = "Görünür Ana İstasyon";
        Entity visibleParent = authoringScene->CreateEntity();
        visibleParent.AddComponent<TagComponent>(visibleParentTag);
        visibleParent.AddComponent<TransformComponent>(glm::vec3(10.0f, 0.0f, 0.0f));
        visibleParent.AddComponent<VisibilityComponent>(true);

        // e) Görünür Ebeveynin Gizli Çocuğu (Visibility: false)
        const std::string hiddenChildTag = "Gizli Savunma Birimi 👾";
        Entity hiddenChild = authoringScene->CreateEntity();
        hiddenChild.AddComponent<TagComponent>(hiddenChildTag);
        hiddenChild.AddComponent<TransformComponent>(glm::vec3(0.0f, -2.0f, 0.0f));
        hiddenChild.AddComponent<VisibilityComponent>(false);
        TEST_CHECK(suite, "SetParentHiddenChild", authoringScene->SetParent(hiddenChild, visibleParent));

        // f) Boş Düğüm (Hiçbir bileşeni olmayan saf varlık)
        Entity emptyNode = authoringScene->CreateEntity();
        const EntityHandle emptyHandle = emptyNode.GetHandle();

        // Kaydet ve yükle
        const std::filesystem::path authoringFile = testDir / "authoring_roundtrip.astral";
        bool saveOk = SceneSerializer::Serialize(authoringScene, authoringFile);
        TEST_CHECK_MSG(suite, "AuthoringSerializeSuccess", saveOk, "Authoring sahne serilestirilemedi!");

        auto loadedAuthScene = std::make_shared<Scene>("Staging");
        bool loadOk = SceneSerializer::Deserialize(loadedAuthScene, authoringFile);
        TEST_CHECK_MSG(suite, "AuthoringDeserializeSuccess", loadOk, "Authoring sahne deserialize edilemedi!");

        // Sahne adı doğrulaması
        TEST_CHECK_MSG(suite, "SceneNamePreserved", loadedAuthScene->GetName() == originalSceneName,
                       "Sahne adi Unicode olarak kayipsiz korunmali!");

        // Boş düğüm (Empty Node) doğrulaması
        Entity loadedEmpty(emptyHandle, loadedAuthScene.get());
        TEST_CHECK_MSG(suite, "EmptyNodeIsValidAndAlive", loadedEmpty.IsValid(),
                       "Bilesensiz bos dugum deserialization sonrasi canli kalmali!");
        TEST_CHECK_MSG(suite, "EmptyNodeHasNoTransform", !loadedEmpty.HasComponent<TransformComponent>(),
                       "Bos dugum gereksiz bilesen almamali!");
        TEST_CHECK_MSG(suite, "EmptyNodeHasNoTag", !loadedEmpty.HasComponent<TagComponent>(),
                       "Bos dugum gereksiz tag almamali!");

        // Hero doğrulaması
        Entity loadedHero(hero.GetHandle(), loadedAuthScene.get());
        TEST_CHECK(suite, "LoadedHeroIsValid", loadedHero.IsValid());
        TEST_CHECK(suite, "LoadedHeroHasTag", loadedHero.HasComponent<TagComponent>());
        TEST_CHECK_MSG(suite, "HeroUnicodeTagMatches", loadedHero.GetComponent<TagComponent>().tag == heroTag,
                       "Hero Unicode/Turkce etiket birebir eslesmeli!");
        TEST_CHECK(suite, "LoadedHeroHasVisibility", loadedHero.HasComponent<VisibilityComponent>());
        TEST_CHECK_MSG(suite, "HeroVisibilityTrue", loadedHero.GetComponent<VisibilityComponent>().isVisible == true,
                       "Hero gorunurlugu true olmali!");

        // Gizli Ebeveyn ve Görünür Çocuk doğrulaması
        Entity loadedHiddenParent(hiddenParent.GetHandle(), loadedAuthScene.get());
        Entity loadedVisibleChild(visibleChild.GetHandle(), loadedAuthScene.get());
        TEST_CHECK(suite, "LoadedHiddenParentValid", loadedHiddenParent.IsValid());
        TEST_CHECK(suite, "LoadedVisibleChildValid", loadedVisibleChild.IsValid());
        TEST_CHECK_MSG(suite, "HiddenParentTagMatches", loadedHiddenParent.GetComponent<TagComponent>().tag == hiddenParentTag,
                       "Gizli ebeveyn etiketi eslesmeli!");
        TEST_CHECK_MSG(suite, "HiddenParentVisibilityFalse", loadedHiddenParent.GetComponent<VisibilityComponent>().isVisible == false,
                       "Gizli ebeveyn gorunurlugu false olmali!");
        TEST_CHECK_MSG(suite, "VisibleChildTagMatches", loadedVisibleChild.GetComponent<TagComponent>().tag == visibleChildTag,
                       "Gorunur cocuk etiketi eslesmeli!");
        TEST_CHECK_MSG(suite, "VisibleChildVisibilityTrue", loadedVisibleChild.GetComponent<VisibilityComponent>().isVisible == true,
                       "Gorunur cocuk gorunurlugu true olmali!");
        TEST_CHECK(suite, "ChildParentRelationPreserved", loadedVisibleChild.GetComponent<HierarchyComponent>().parent == loadedHiddenParent.GetHandle());

        // Görünür Ebeveyn ve Gizli Çocuk doğrulaması
        Entity loadedVisibleParent(visibleParent.GetHandle(), loadedAuthScene.get());
        Entity loadedHiddenChild(hiddenChild.GetHandle(), loadedAuthScene.get());
        TEST_CHECK(suite, "LoadedVisibleParentValid", loadedVisibleParent.IsValid());
        TEST_CHECK(suite, "LoadedHiddenChildValid", loadedHiddenChild.IsValid());
        TEST_CHECK_MSG(suite, "VisibleParentVisibilityTrue", loadedVisibleParent.GetComponent<VisibilityComponent>().isVisible == true,
                       "Gorunur ebeveyn gorunurlugu true olmali!");
        TEST_CHECK_MSG(suite, "HiddenChildTagMatches", loadedHiddenChild.GetComponent<TagComponent>().tag == hiddenChildTag,
                       "Gizli cocuk Unicode etiketi eslesmeli!");
        TEST_CHECK_MSG(suite, "HiddenChildVisibilityFalse", loadedHiddenChild.GetComponent<VisibilityComponent>().isVisible == false,
                       "Gizli cocuk gorunurlugu false olmali!");
        TEST_CHECK(suite, "HiddenChildParentRelationPreserved", loadedHiddenChild.GetComponent<HierarchyComponent>().parent == loadedVisibleParent.GetHandle());
    }

    // =========================================================================
    // 3. Yeni Oyun Bileşeninin Motor `.cpp` Değiştirilmeden Kaydı & Yüklenmesi
    // =========================================================================
    {
        // Motor .cpp dosyalarini degistirmeden custom bileseni serializer'a kaydet
        SceneSerializer::RegisterComponent<CustomGameComponent>();

        auto customScene = std::make_shared<Scene>("CustomComponentLevel");
        Entity eCustom = customScene->CreateEntity();
        eCustom.AddComponent<TagComponent>("Oyuncu Karakteri");
        eCustom.AddComponent<CustomGameComponent>(CustomGameComponent{ .score = 8888, .mana = 250.75f });

        const std::filesystem::path customFile = testDir / "custom_comp.astral";
        bool customSaveOk = SceneSerializer::Serialize(customScene, customFile);
        TEST_CHECK_MSG(suite, "CustomComponentSerializeSuccess", customSaveOk,
                       "Custom oyun bileseni basariyla serialize edilmeli!");

        auto loadedCustomScene = std::make_shared<Scene>("LoadedCustom");
        bool customLoadOk = SceneSerializer::Deserialize(loadedCustomScene, customFile);
        TEST_CHECK_MSG(suite, "CustomComponentDeserializeSuccess", customLoadOk,
                       "Custom oyun bileseni basariyla deserialize edilmeli!");

        Entity loadedCustomEntity(eCustom.GetHandle(), loadedCustomScene.get());
        TEST_CHECK(suite, "LoadedCustomEntityValid", loadedCustomEntity.IsValid());
        TEST_CHECK(suite, "LoadedCustomEntityHasCustomComp", loadedCustomEntity.HasComponent<CustomGameComponent>());
        const auto& customData = loadedCustomEntity.GetComponent<CustomGameComponent>();
        TEST_CHECK_MSG(suite, "CustomCompScoreMatches", customData.score == 8888,
                       "Custom bilesen score degeri kayipsiz yuklenmeli!");
        TEST_CHECK_MSG(suite, "CustomCompManaMatches", std::abs(customData.mana - 250.75f) < 0.001f,
                       "Custom bilesen mana degeri kayipsiz yuklenmeli!");
    }

    // =========================================================================
    // 4. Unknown Chunk Graceful Skipping (Ileri Uyumluluk) Testi
    // =========================================================================
    const std::filesystem::path unknownChunkFile = testDir / "unknown_chunk.astral";
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

    // =========================================================================
    // 5. Atomic Staging Rollback ve Two-Phase Commit Testi
    // =========================================================================
    auto liveScene = std::make_shared<Scene>("LiveScene");
    Entity liveE = liveScene->CreateEntity();
    liveE.AddComponent<HealthComponent>(999);

    // Bozuk dosyayı deserialize etmeye çalışalım
    const std::filesystem::path truncatedFile = testDir / "truncated.astral";
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
    TEST_CHECK(suite, "AtomicLiveSceneNamePreserved", liveScene->GetName() == "LiveScene");

    // =========================================================================
    // 6. Güvenlik, Bütçe ve Bozuk/Aşırı Boyut Sınırları Testleri
    // =========================================================================
    TEST_CHECK(suite, "SerializeNullSceneFails", !SceneSerializer::Serialize(nullptr, testFile));
    TEST_CHECK(suite, "DeserializeNullSceneFails", !SceneSerializer::Deserialize(nullptr, testFile));

    // Corrupted Magic
    const std::filesystem::path corruptMagicFile = testDir / "corrupt_magic.astral";
    {
        std::ofstream corruptStream(corruptMagicFile, std::ios::binary);
        char badMagic[12] = { 'X', 'X', 'X', 'X', 2, 0, 0, 0, 2, 0, 0, 0 };
        corruptStream.write(badMagic, 12);
    }
    auto dummyScene = std::make_shared<Scene>("Dummy");
    TEST_CHECK_MSG(suite, "CorruptMagicFastFails", !SceneSerializer::Deserialize(dummyScene, corruptMagicFile),
                   "Bozuk magic bytes fast-fail etmeli!");

    // Non existent file
    TEST_CHECK(suite, "NonExistentFileFastFails", !SceneSerializer::Deserialize(dummyScene, testDir / "non_existent_file.astral"));

    // Aşırı payload boyutu (Dosya boyutundan büyük chunk payload)
    const std::filesystem::path oversizedChunkFile = testDir / "oversized_chunk.astral";
    {
        std::ofstream stream(oversizedChunkFile, std::ios::binary | std::ios::trunc);
        SceneFileHeader fh{ .magic = { 'A', 'S', 'T', 'R' }, .version = 2, .activeEntityCount = 1 };
        stream.write(reinterpret_cast<const char*>(&fh), sizeof(fh));
        // Payload boyutu dosyanın kalanından kat be kat büyük
        ComponentChunkHeader badChunk{
            .typeId = ComponentTraits<HealthComponent>::TypeHash,
            .version = 1,
            .flags = 0,
            .elementCount = 100,
            .entityDataSize = 100 * sizeof(EntityID),
            .componentDataSize = 100000000 // 100 MB beyan ediyor ama dosyada yok
        };
        stream.write(reinterpret_cast<const char*>(&badChunk), sizeof(badChunk));
    }
    auto safeScene = std::make_shared<Scene>("SafeScene");
    bool oversizedFail = !SceneSerializer::Deserialize(safeScene, oversizedChunkFile);
    TEST_CHECK_MSG(suite, "OversizedPayloadChunkRejected", oversizedFail,
                   "Dosya boyutunu asan bozuk chunk payload hemen reddedilmeli!");
    TEST_CHECK(suite, "SafeSceneUntouched", safeScene->GetName() == "SafeScene");

    // Aşırı uzun string içeren Tag chunk
    const std::filesystem::path oversizedTagFile = testDir / "oversized_tag.astral";
    {
        std::ofstream stream(oversizedTagFile, std::ios::binary | std::ios::trunc);
        SceneFileHeader fh{ .magic = { 'A', 'S', 'T', 'R' }, .version = 2, .activeEntityCount = 1 };
        stream.write(reinterpret_cast<const char*>(&fh), sizeof(fh));

        uint32_t badStrLen = 100000; // MAX_TAG_LENGTH = 4096 sınırından büyük
        ComponentChunkHeader badTagChunk{
            .typeId = ComponentTraits<TagComponent>::TypeHash,
            .version = 1,
            .flags = 0,
            .elementCount = 1,
            .entityDataSize = sizeof(EntityHandle),
            .componentDataSize = static_cast<uint32_t>(sizeof(uint32_t) + badStrLen)
        };
        stream.write(reinterpret_cast<const char*>(&badTagChunk), sizeof(badTagChunk));
        EntityHandle h = MakeEntityHandle(0, 1);
        stream.write(reinterpret_cast<const char*>(&h), sizeof(h));
        stream.write(reinterpret_cast<const char*>(&badStrLen), sizeof(badStrLen));
    }
    bool oversizedTagRejected = !SceneSerializer::Deserialize(safeScene, oversizedTagFile);
    TEST_CHECK_MSG(suite, "OversizedTagRejected", oversizedTagRejected,
                   "Guvenlik sinirini asan dize uzunlugu tahsis yapilmadan reddedilmeli!");

    // Test geçici dizinini temizle
    std::filesystem::remove_all(testDir, ec);
}

} // namespace Astral::Test
