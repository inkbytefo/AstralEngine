#include "TestFramework.hpp"
#include "AstralEngine.h"
#include "Astral/Core/TransformSystem.hpp"
#include "Astral/Core/RenderExtractionSystem.hpp"
#include "Astral/Renderer/SDFEdit.hpp"
#include <memory>
#include <cmath>

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
}

} // namespace Astral::Test
