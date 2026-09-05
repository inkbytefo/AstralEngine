#include "TestFramework.hpp"
#include "AstralEngine.h"
#include "Astral/Core/TransformSystem.hpp"
#include "Astral/Core/RenderExtractionSystem.hpp"
#include "Astral/Renderer/SDFEdit.hpp"
#include <memory>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace Astral::Test {

void RunGenerationalIdentityTests() {
    const std::string suite = "GenerationalIdentitySuite";

    auto scene = std::make_shared<Scene>("Identity Test Scene");
    auto& reg = scene->GetRegistry();

    // 1. Test_Create_IsAlive
    Entity e0 = scene->CreateEntity();
    TEST_CHECK_MSG(suite, "NewEntityIsValid", e0.IsValid(), "Yeni olusturulan varlik IsValid olmali!");
    TEST_CHECK_MSG(suite, "NewEntityIsAlive", reg.IsAlive(e0.GetHandle()), "Registry IsAlive true donmeli!");
    TEST_CHECK_MSG(suite, "NewEntityIndex0", e0.GetIndex() == 0, "Ilk entity index 0 olmali!");
    TEST_CHECK_MSG(suite, "NewEntityGen1", e0.GetGeneration() == 1, "Ilk entity generation 1 olmali!");
    e0.AddComponent<HealthComponent>(100);

    // 2. Test_Destroy_Invalidation
    scene->DestroyEntity(e0);
    TEST_CHECK_MSG(suite, "DestroyedEntityNotValid", !e0.IsValid(), "Destroy sonrasi e0.IsValid false olmali!");
    TEST_CHECK_MSG(suite, "DestroyedEntityNotAlive", !reg.IsAlive(e0.GetHandle()), "Destroy sonrasi reg.IsAlive false olmali!");
    TEST_CHECK_MSG(suite, "DestroyedEntityNoComponents", !e0.HasComponent<HealthComponent>(), "Bilesenler temizlenmis olmali!");

    // 3. Test_FreeList_Recycling & Stale Handle Protection
    // Ayni indekse sahip yeni entity olustur (Gen 2 olacak)
    Entity e0_recycled = scene->CreateEntity();
    TEST_CHECK_MSG(suite, "RecycledEntitySameIndex", e0_recycled.GetIndex() == 0, "Free-list ayni indeksi (0) recycle etmeli!");
    TEST_CHECK_MSG(suite, "RecycledEntityGen2", e0_recycled.GetGeneration() == 2, "Recycle edilen entity'nin generation'i 2 olmali!");
    TEST_CHECK_MSG(suite, "HandlesAreDifferent", e0_recycled.GetHandle() != e0.GetHandle(), "Eski ve yeni 64-bit handle'lar farkli olmali!");
    TEST_CHECK_MSG(suite, "RecycledEntityIsValid", e0_recycled.IsValid(), "Recycle entity gecerli olmali!");
    TEST_CHECK_MSG(suite, "OldHandleStaysInvalid", !e0.IsValid(), "Eski handle (e0) gecersiz kalmali!");
    TEST_CHECK_MSG(suite, "OldHandleRejectedByRegistry", !reg.IsAlive(e0.GetHandle()), "Eski handle Registry tarafindan kesinlikle reddedilmeli!");

    // 4. Ghost Mutation Engeli: e0_recycled'a can verelim
    e0_recycled.AddComponent<HealthComponent>(500);
    TEST_CHECK(suite, "RecycledEntityHpSet", e0_recycled.GetComponent<HealthComponent>().hp == 500);
    // Eski e0 handle'i uzerinden HasComponent cagrisi IsValid assert'i / IsAlive korumasi saglar:
    TEST_CHECK_MSG(suite, "GhostMutationBlocked", !e0.HasComponent<HealthComponent>(), "Eski handle yeni component'e erisememeli!");

    // 5. Test_Double_Destroy: Eski handle uzerinden tekrar destroy cagrisi hicbir seyi bozmamali
    scene->DestroyEntity(e0); // Gecersiz handle, sessizce yok sayilmali
    TEST_CHECK_MSG(suite, "DoubleDestroySafe", e0_recycled.IsValid(), "Gecersiz destroy cagrisi yasayan entity'yi etkilememeli!");
    TEST_CHECK(suite, "RecycledEntityStillAlive", reg.IsAlive(e0_recycled.GetHandle()));

    // 6. Test_Multiple_Recycling_Generations: Birden fazla geri donusum dongusu
    Entity eA = scene->CreateEntity(); // Index 1, Gen 1
    TEST_CHECK(suite, "EntityAIndex1Gen1", eA.GetIndex() == 1 && eA.GetGeneration() == 1);
    scene->DestroyEntity(eA);
    Entity eB = scene->CreateEntity(); // Index 1, Gen 2
    TEST_CHECK(suite, "EntityBIndex1Gen2", eB.GetIndex() == 1 && eB.GetGeneration() == 2);
    scene->DestroyEntity(eB);

    // 7. Test_RenderExtraction_Generation_Preservation:
    // ExtractRenderData fonksiyonunun 64-bit Generation bilgisini korudugunu ve picking nesnesinin IsValid oldugunu dogrula
    Entity eC = scene->CreateEntity(); // Index 1, Gen 3
    TEST_CHECK(suite, "EntityCIndex1Gen3", eC.GetIndex() == 1 && eC.GetGeneration() == 3);
    eC.AddComponent<TransformComponent>(glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f));
    eC.AddComponent<SDFComponent>();
    std::vector<SDFEditGPU> extractedEdits;
    std::vector<EntityHandle> extractedHandles;
    UpdateWorldTransforms(scene->GetRegistry());
    TEST_CHECK(suite, "EntityCHasWorldTransform", eC.HasComponent<WorldTransformComponent>());
    TEST_CHECK(suite, "EntityCMatrixValid", std::abs(eC.GetComponent<WorldTransformComponent>().matrix[3].x) < 0.0001f);
    ExtractRenderData(scene->GetRegistry(), extractedEdits, extractedHandles);
    TEST_CHECK(suite, "ExtractionCount1", extractedHandles.size() == 1);
    TEST_CHECK_MSG(suite, "ExtractionExact64BitHandle", extractedHandles[0] == eC.GetHandle(),
                   "ExtractRenderData tam 64-bit kanonik handle'i korumali!");
    TEST_CHECK_MSG(suite, "ExtractionGen3Preserved", GetEntityGeneration(extractedHandles[0]) == 3,
                   "Generation 3 korunmus olmali (uint32 kirpilmamali)!");
    Entity pickedEntity(extractedHandles[0], scene.get());
    TEST_CHECK_MSG(suite, "PickedEntityIsValid", pickedEntity.IsValid(),
                   "Extracted handle ile kurulan Entity IsValid() olmali!");
    TEST_CHECK_MSG(suite, "PickedEntityGen3", pickedEntity.GetGeneration() == 3,
                   "Picked entity generation 3 olmali!");

    // =========================================================================
    // A1.2 — F03: Clear Generation Korunumu ve Handle Invalidation Testleri
    // =========================================================================

    // 8. Test_Clear_Invalidates_Outstanding_Handles (F03 Temel Senaryo)
    Entity preClearEntity = scene->CreateEntity();
    EntityHandle preClearHandle = preClearEntity.GetHandle();
    preClearEntity.AddComponent<HealthComponent>(250);
    TEST_CHECK(suite, "PreClearEntityValid", preClearEntity.IsValid());

    // Sahneyi temizle
    scene->Clear();

    // Clear sonrasi eski handle artik olu olmali
    TEST_CHECK_MSG(suite, "PreClearHandleDeadAfterClear", !reg.IsAlive(preClearHandle),
                   "Clear sonrasi eski EntityHandle Registry tarafindan reddedilmeli!");
    TEST_CHECK_MSG(suite, "PreClearEntityInvalidAfterClear", !preClearEntity.IsValid(),
                   "Clear sonrasi eski Entity nesnesi IsValid() false olmali!");

    // Yeniden nesne olustur: free-list 0. indeksi verecek ama nesli (generation) artmis olmali!
    Entity postClearEntity = scene->CreateEntity();
    TEST_CHECK_MSG(suite, "PostClearEntityValid", postClearEntity.IsValid(),
                   "Clear sonrasi olusturulan nesne IsValid olmali!");
    TEST_CHECK_MSG(suite, "HandlesMustDifferAcrossClear", postClearEntity.GetHandle() != preClearHandle,
                   "Clear oncesi ve sonrasi olusan nesnelerin 64-bit handle'lari kesinlikle esit olmamali!");
    TEST_CHECK_MSG(suite, "OldHandleStillInvalidAfterRecreate", !reg.IsAlive(preClearHandle),
                   "Yeni nesne olussa bile eski handle kesinlikle olu kalmali!");
    TEST_CHECK_MSG(suite, "OldEntityObjectStillInvalid", !preClearEntity.IsValid(),
                   "Eski Entity nesnesi IsValid false kalmali!");

    // Eski handle uzerinden yeni nesneye Ghost Mutation engeli
    postClearEntity.AddComponent<HealthComponent>(777);
    TEST_CHECK(suite, "PostClearEntityHpSet", postClearEntity.GetComponent<HealthComponent>().hp == 777);
    TEST_CHECK_MSG(suite, "PreClearEntityCannotAccessHp", !preClearEntity.HasComponent<HealthComponent>(),
                   "Eski entity referansi yeni nesnenin bilesenine erisememeli!");

    // Eski handle uzerinden Registry mutasyon cagrilari reddedilmeli
    bool addCompSuccess = reg.AddComponent<HealthComponent>(preClearHandle, HealthComponent{999});
    TEST_CHECK_MSG(suite, "RegAddComponentRejectsDeadHandle", !addCompSuccess,
                   "Registry::AddComponent olu handle uzerinde false donmeli!");
    TEST_CHECK_MSG(suite, "NewEntityHpNotOverwritten", postClearEntity.GetComponent<HealthComponent>().hp == 777,
                   "Eski handle yeni nesnenin verisini ezen bir mutasyon gerceklestirememeli!");

    // 9. Test_Multiple_Clear_Cycles (Ardisik temizleme donguleri)
    std::vector<EntityHandle> epoch1Handles;
    for (int i = 0; i < 5; ++i) {
        epoch1Handles.push_back(scene->CreateEntity().GetHandle());
    }
    TEST_CHECK(suite, "Epoch1Count5", epoch1Handles.size() == 5);

    scene->Clear();

    for (EntityHandle h : epoch1Handles) {
        TEST_CHECK_MSG(suite, "Epoch1HandleInvalid", !reg.IsAlive(h),
                       "Ilk epok handle'larinin tamami Clear sonrasi gecersiz olmali!");
    }

    std::vector<EntityHandle> epoch2Handles;
    for (int i = 0; i < 5; ++i) {
        EntityHandle h = scene->CreateEntity().GetHandle();
        epoch2Handles.push_back(h);
        for (EntityHandle oldH : epoch1Handles) {
            TEST_CHECK_MSG(suite, "NoEpochCollision", h != oldH,
                           "Yeni epok handle'i onceki epok handle'i ile asla cakismamali!");
        }
    }

    scene->Clear();

    // Hem Epoch 1 hem Epoch 2 tumuyle gecersiz olmali
    for (EntityHandle h : epoch1Handles) {
        TEST_CHECK(suite, "Epoch1StillInvalid", !reg.IsAlive(h));
    }
    for (EntityHandle h : epoch2Handles) {
        TEST_CHECK(suite, "Epoch2Invalid", !reg.IsAlive(h));
    }

    // 10. Test_Public_Mutation_API_Rejects_Invalid_Handle_In_Release
    EntityHandle bogusHandle = MakeEntityHandle(99999, 1);
    TEST_CHECK_MSG(suite, "BogusHandleNotAlive", !reg.IsAlive(bogusHandle), "Hayali handle IsAlive olmamali!");

    // Registry seviyesinde mutasyon kontrolleri
    TEST_CHECK(suite, "RegAddCompRejectsBogus", !reg.AddComponent<HealthComponent>(bogusHandle, HealthComponent{10}));
    TEST_CHECK(suite, "RegRemoveCompRejectsBogus", !reg.RemoveComponent<HealthComponent>(bogusHandle));
    reg.DestroyEntity(bogusHandle); // Cokmemeli
    TEST_CHECK(suite, "RegTryGetCompNullOnBogus", reg.TryGetComponent<HealthComponent>(bogusHandle) == nullptr);

    bool regGetThrew = false;
    try {
        (void)reg.GetComponent<HealthComponent>(bogusHandle);
    } catch (const std::runtime_error&) {
        regGetThrew = true;
    }
    TEST_CHECK_MSG(suite, "RegGetComponentThrowsOnDead", regGetThrew,
                   "Registry::GetComponent gecersiz handle uzerinde std::runtime_error firlatmali!");

    // Entity seviyesinde mutasyon kontrolleri
    Entity bogusEntity(bogusHandle, scene.get(), scene->GetInstanceId());
    TEST_CHECK(suite, "BogusEntityNotValid", !bogusEntity.IsValid());
    TEST_CHECK(suite, "BogusEntityTryGetNull", bogusEntity.TryGetComponent<HealthComponent>() == nullptr);
    TEST_CHECK(suite, "BogusEntityRemoveFalse", !bogusEntity.RemoveComponent<HealthComponent>());

    bool entityAddThrew = false;
    try {
        bogusEntity.AddComponent<HealthComponent>(100);
    } catch (const std::runtime_error&) {
        entityAddThrew = true;
    }
    TEST_CHECK_MSG(suite, "EntityAddComponentThrowsOnDead", entityAddThrew,
                   "Entity::AddComponent gecersiz nesne uzerinde std::runtime_error firlatmali!");

    bool entityGetThrew = false;
    try {
        (void)bogusEntity.GetComponent<HealthComponent>();
    } catch (const std::runtime_error&) {
        entityGetThrew = true;
    }
    TEST_CHECK_MSG(suite, "EntityGetComponentThrowsOnDead", entityGetThrew,
                   "Entity::GetComponent gecersiz nesne uzerinde std::runtime_error firlatmali!");

    // Scene seviyesinde mutasyon kontrolleri
    Entity dupResult = scene->DuplicateEntity(bogusEntity);
    TEST_CHECK_MSG(suite, "DuplicateBogusEntityFails", !dupResult.IsValid(),
                   "Gecersiz entity duplicate edildiginde IsValid false olmali!");

    TEST_CHECK(suite, "SetParentBogusFails", !scene->SetParent(bogusEntity, bogusEntity));
    TEST_CHECK(suite, "ClearParentBogusFails", !scene->ClearParent(bogusEntity));
    scene->DestroyEntity(bogusEntity); // Cokmemeli

    // 11. Test_Scene_Destruction_Lifetime_Contract (Use-After-Free Korumasi)
    Entity orphanEntity;
    {
        auto tempScene = std::make_unique<Scene>("Temporary Scene");
        orphanEntity = tempScene->CreateEntity();
        TEST_CHECK_MSG(suite, "TempEntityValidWhileSceneAlive", orphanEntity.IsValid(),
                       "Gecici sahne yasarken nesnesi gecerli olmali!");
        TEST_CHECK(suite, "TempSceneRegisteredAlive", Scene::IsSceneAlive(tempScene.get()));
        // tempScene scope sonu -> destructor calisir ve IsSceneAlive tablosundan cikar
    }
    TEST_CHECK_MSG(suite, "TempSceneUnregisteredOnDestruct", !Scene::IsSceneAlive(orphanEntity.GetScene()),
                   "Yikilan sahne IsSceneAlive sorgusunda false donmeli!");
    TEST_CHECK_MSG(suite, "OrphanEntityInvalidAfterSceneDestruct", !orphanEntity.IsValid(),
                   "Sahnesi yok edilen Entity nesnesi IsValid false donmeli ve serbest bellege erismemelidir!");

    bool orphanAddThrew = false;
    try {
        orphanEntity.AddComponent<HealthComponent>(50);
    } catch (const std::runtime_error&) {
        orphanAddThrew = true;
    }
    TEST_CHECK_MSG(suite, "OrphanEntityThrowsOnMutation", orphanAddThrew,
                   "Sahnesi yok edilen entity mutasyon denemesinde exception uretmeli ve cokmemelidir!");

    // 12. Test_Scene_Instance_Epoch_And_Swap
    auto sceneA = std::make_shared<Scene>("Scene Alpha");
    auto sceneB = std::make_shared<Scene>("Scene Beta");
    Entity entityA = sceneA->CreateEntity();
    Entity entityB = sceneB->CreateEntity();
    TEST_CHECK(suite, "EntityAValidPreSwap", entityA.IsValid());
    TEST_CHECK(suite, "EntityBValidPreSwap", entityB.IsValid());

    // Swap sonrasi instance id'ler yenilenir; eski Entity referanslari yeni icerige ghost erisim yapamaz
    sceneA->Swap(*sceneB);
    TEST_CHECK_MSG(suite, "PreSwapEntityAInvalidated", !entityA.IsValid(),
                   "Swap oncesi Entity A referansi swap sonrasi gecersiz kalmali!");
    TEST_CHECK_MSG(suite, "PreSwapEntityBInvalidated", !entityB.IsValid(),
                   "Swap oncesi Entity B referansi swap sonrasi gecersiz kalmali!");

    // Swap sonrasi sahne uzerinden yeni olusturulan nesneler gecerli olmali
    Entity newEntityA = sceneA->CreateEntity();
    Entity newEntityB = sceneB->CreateEntity();
    TEST_CHECK(suite, "NewEntityAValidPostSwap", newEntityA.IsValid());
    TEST_CHECK(suite, "NewEntityBValidPostSwap", newEntityB.IsValid());
}

} // namespace Astral::Test
