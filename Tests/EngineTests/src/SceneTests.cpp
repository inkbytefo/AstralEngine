#include "TestFramework.hpp"
#include "AstralEngine.h"
#include "Astral/Scene/SceneCommands.hpp"
#include <memory>

namespace Astral::Test {

void RunSceneTests() {
    const std::string suite = "SceneManagementSuite";

    // 1. Editor Sahnesi olustur
    auto editorScene = std::make_shared<Scene>("Authoring Level");
    Entity originalShip = editorScene->CreateEntity();
    originalShip.AddComponent<TransformComponent>(glm::vec3(10.0f, 20.0f, 30.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f));
    originalShip.AddComponent<VelocityComponent>(glm::vec3(5.0f, 0.0f, 0.0f), glm::vec3(0.0f));
    originalShip.AddComponent<HealthComponent>(500);

    TEST_CHECK(suite, "OriginalEntityId0", originalShip.GetID() == 0);
    TEST_CHECK(suite, "OriginalPosX10", originalShip.GetComponent<TransformComponent>().position.x == 10.0f);
    TEST_CHECK(suite, "OriginalHp500", originalShip.GetComponent<HealthComponent>().hp == 500);

    // 2. Play dugmesine basildi: Editor -> Runtime Deep-Copy Klonlama
    auto runtimeScene = Scene::Copy(editorScene);
    TEST_CHECK(suite, "RuntimeSceneNotNull", runtimeScene != nullptr);
    TEST_CHECK(suite, "RuntimeSceneNotIdenticalPointer", runtimeScene != editorScene);

    runtimeScene->OnUpdate(2.0f);
    TEST_CHECK_MSG(suite, "NoPhysicsBeforeRuntimeStart",
                   runtimeScene->GetRegistry().GetComponent<TransformComponent>(originalShip.GetHandle()).position.x == 10.0f,
                   "Runtime baslamadan fizik transform'u degistirmemeli!");

    SceneManager sceneManager;
    sceneManager.SetActiveScene(runtimeScene);
    runtimeScene->OnRuntimeStart();

    // 3. Runtime sahnesinde simulasyon calistir ve nesneleri mutasyona ugrat
    Entity clonedShip(originalShip.GetHandle(), runtimeScene.get());
    TEST_CHECK(suite, "ClonedShipIsValid", clonedShip.IsValid());
    TEST_CHECK(suite, "ClonedShipHasTransform", clonedShip.HasComponent<TransformComponent>());

    runtimeScene->OnUpdate(2.0f); // 5.0 m/s * 2s = +10m -> pos.x = 20.0f
    clonedShip.GetComponent<HealthComponent>().hp = 120; // Can azaldi

    TEST_CHECK(suite, "ClonedPosX20", clonedShip.GetComponent<TransformComponent>().position.x == 20.0f);
    TEST_CHECK(suite, "ClonedHp120", clonedShip.GetComponent<HealthComponent>().hp == 120);

    // 4. Orijinal Editor sahnesinin bozulmadigini (Derin kopyalamanin basarisini) teyit et!
    float origX = originalShip.GetComponent<TransformComponent>().position.x;
    int origHp = originalShip.GetComponent<HealthComponent>().hp;
    TEST_CHECK_MSG(suite, "DeepCopyPreservesOriginalX", origX == 10.0f,
                   "Deep copy basarisiz! Editor sahnesi mutasyona ugradi!");
    TEST_CHECK_MSG(suite, "DeepCopyPreservesOriginalHp", origHp == 500,
                   "Deep copy basarisiz! Editor sahnesi mutasyona ugradi!");

    // 5. Runtime sahnesinde nesneyi Destroy et
    runtimeScene->DestroyEntity(clonedShip);
    TEST_CHECK_MSG(suite, "DestroyedClonedNotValid", !clonedShip.IsValid(),
                   "Destroy edilen clonedShip artik IsValid olmamalidir!");
    TEST_CHECK(suite, "DestroyedClonedNoHp", !clonedShip.HasComponent<HealthComponent>());
    TEST_CHECK_MSG(suite, "OriginalEditorEntityRemainsValid", originalShip.IsValid(),
                   "Editor nesnesi silinmemelidir ve IsValid kalmalidir!");
    TEST_CHECK_MSG(suite, "OriginalEditorEntityHasHp", originalShip.HasComponent<HealthComponent>(),
                   "Editor nesnesi silinmemelidir!");

    sceneManager.UnloadCurrentScene();

    // =========================================================================
    // A1.2 — Sahne Gecislerinde Secim ve Undo/Redo (CommandStack) Guvenligi
    // =========================================================================

    // 6. Test_Selection_And_CommandStack_Across_Scene_Clear
    auto commandScene = std::make_shared<Scene>("Command Transition Scene");
    CommandStack commandStack(20);

    Entity selected = commandScene->CreateEntity();
    TransformComponent initialTransform(glm::vec3(0.0f));
    TransformComponent modifiedTransform(glm::vec3(5.0f, 10.0f, 15.0f));
    selected.AddComponent<TransformComponent>(initialTransform);

    // Komutu calistir
    commandStack.PushAndExecute(std::make_unique<ModifyTransformCommand>(selected, initialTransform, modifiedTransform));
    TEST_CHECK(suite, "CommandModifiedTransformApplied", selected.GetComponent<TransformComponent>().position.x == 5.0f);

    // Sahneyi temizle (ornek: yeni sahne acilisi veya sahne yuklemesi)
    commandScene->Clear();

    // Secili nesne artik gecersiz olmali
    TEST_CHECK_MSG(suite, "SelectedEntityInvalidAfterClear", !selected.IsValid(),
                   "Sahne temizlendikten sonra secili nesne gecersiz olmali!");

    // Yeni bir nesne olusturalim (0. slotu yeni generation ile alacak)
    Entity newEntity = commandScene->CreateEntity();
    TransformComponent freshTransform(glm::vec3(100.0f, 200.0f, 300.0f));
    newEntity.AddComponent<TransformComponent>(freshTransform);

    // Gecmis komut stack'inde kalan eski ModifyTransformCommand'i Undo etmeyi deneyelim
    TEST_CHECK(suite, "StackCanUndo", commandStack.CanUndo());
    commandStack.Undo();

    // Stale Undo cagrisi yeni nesneyi kesinlikle BOZMAMALIDIR!
    TEST_CHECK_MSG(suite, "NewEntityNotMutatedByStaleUndo",
                   newEntity.GetComponent<TransformComponent>().position.x == 100.0f,
                   "Eski nesneye ait Undo cagrisi yeni sahnedeki nesneyi kesinlikle tahrif etmemelidir!");

    // Sahne gecisi sirasinda Editor politikasi geregi stack temizlenir
    commandStack.Clear();
    selected = Entity();
    TEST_CHECK(suite, "StackClearedAfterSceneTransition", !commandStack.CanUndo() && !commandStack.CanRedo());
    TEST_CHECK(suite, "SelectedEntityCleared", selected.GetHandle() == NullEntityHandle);
}

} // namespace Astral::Test
