#include "TestFramework.hpp"
#include "AstralEngine.h"
#include "Astral/Scene/Scene.hpp"
#include "Astral/Scene/SceneSerializer.hpp"
#include "Astral/Scene/SceneCommands.hpp"
#include "Astral/Scene/SDFWorldQuery.hpp"
#include "Astral/Core/CommandStack.hpp"
#include "Astral/Core/Components.hpp"
#include "PuzzleGameSubsystem.hpp"

#include <filesystem>
#include <iostream>

namespace Astral::Test {

#define ASTRAL_TEST_ASSERT(cond, msg) TEST_CHECK_MSG("GameplayCycleSuite", #cond, (cond), (msg))
void TestAuthoringPlayStopCycle() {
    auto authoringScene = Sandbox::PuzzleGameSubsystem::CreatePuzzleScene();
    ASTRAL_TEST_ASSERT(authoringScene != nullptr, "Authoring sahnesi basariyla olusturulmali");

    // Authoring sahnesindeki baslangic degerlerini kaydet
    Entity playerAuth = authoringScene->FindEntityByTag("Player");
    ASTRAL_TEST_ASSERT(playerAuth.IsValid(), "Authoring sahnesinde Player bulunmali");
    glm::vec3 initialPlayerPos = playerAuth.GetComponent<TransformComponent>().position;

    Entity cutterAuth = authoringScene->FindEntityByTag("CutterDoorway");
    ASTRAL_TEST_ASSERT(cutterAuth.IsValid(), "Authoring sahnesinde CutterDoorway bulunmali");
    ASTRAL_TEST_ASSERT(cutterAuth.GetComponent<SDFComponent>().isVisible == 0, "Authoring sahnesinde cutter baslangicta gizli olmali");

    size_t initialEntityCount = 0;
    for (auto&& [e, tag] : authoringScene->GetRegistry().GetView<TagComponent>()) {
        (void)e; (void)tag;
        initialEntityCount++;
    }

    // 1. Play Mode Baslat: Sahne klonlanir
    std::shared_ptr<Scene> playScene = authoringScene->Clone();
    ASTRAL_TEST_ASSERT(playScene != nullptr, "Klonlanan Play sahnesi gecersiz olamaz");
    ASTRAL_TEST_ASSERT(playScene.get() != authoringScene.get(), "Play sahnesi bagimsiz bir ornek olmali");

    // 2. Play sahnesinde mutasyonlar yap:
    // a. Oyuncu hareket ettirilsin
    Entity playerPlay = playScene->FindEntityByTag("Player");
    ASTRAL_TEST_ASSERT(playerPlay.IsValid(), "Play sahnesinde Player bulunmali");
    playerPlay.GetComponent<TransformComponent>().position = glm::vec3(5.0f, 10.0f, -15.0f);

    // b. Yeni calisma zamani nesnesi olusturulsun
    Entity bullet = playScene->CreateEntity("RuntimeBullet");
    bullet.AddComponent<TransformComponent>(glm::vec3(1.0f, 2.0f, 3.0f));

    // c. CSG Kapi oyulsun (cutter gorunur yapilsin)
    Entity cutterPlay = playScene->FindEntityByTag("CutterDoorway");
    cutterPlay.GetComponent<SDFComponent>().isVisible = 1;
    cutterPlay.GetComponent<VisibilityComponent>().isVisible = true;

    // d. Bir nesne yok edilsin (GoalOrb)
    Entity goalPlay = playScene->FindEntityByTag("GoalOrb");
    ASTRAL_TEST_ASSERT(goalPlay.IsValid(), "Play sahnesinde GoalOrb bulunmali");
    playScene->DestroyEntity(goalPlay);

    // Play sahnesindeki degisiklikleri teyit et
    ASTRAL_TEST_ASSERT(playerPlay.GetComponent<TransformComponent>().position.x == 5.0f, "Play sahnesinde oyuncu degismis olmali");
    ASTRAL_TEST_ASSERT(playScene->FindEntityByTag("RuntimeBullet").IsValid(), "RuntimeBullet play sahnesinde var olmali");
    ASTRAL_TEST_ASSERT(!playScene->FindEntityByTag("GoalOrb").IsValid(), "GoalOrb play sahnesinde yok edilmis olmali");

    // 3. Stop Mode: Play sahnesi birakilir ve Authoring sahnesine donulur
    playScene.reset();

    // 4. Authoring sahnesinin 100% bakir ve bozulmamis kaldigini dogrula
    ASTRAL_TEST_ASSERT(playerAuth.IsValid(), "Authoring player hala gecerli olmali");
    glm::vec3 afterStopPos = playerAuth.GetComponent<TransformComponent>().position;
    ASTRAL_TEST_ASSERT(glm::distance(initialPlayerPos, afterStopPos) < 0.0001f,
                       "Authoring sahnesindeki oyuncu konumu Play modundan hicbir sekilde etkilenmemeli");

    ASTRAL_TEST_ASSERT(!authoringScene->FindEntityByTag("RuntimeBullet").IsValid(),
                       "Play modunda olusturulan nesneler authoring sahnesine sizmamali");

    Entity goalAuth = authoringScene->FindEntityByTag("GoalOrb");
    ASTRAL_TEST_ASSERT(goalAuth.IsValid(), "Authoring sahnesindeki GoalOrb yok olmamis olmali");

    Entity cutterAuthAfter = authoringScene->FindEntityByTag("CutterDoorway");
    ASTRAL_TEST_ASSERT(cutterAuthAfter.GetComponent<SDFComponent>().isVisible == 0,
                       "Authoring sahnesindeki cutter gorunurlugu gizli kalmali");

    size_t finalEntityCount = 0;
    for (auto&& [e, tag] : authoringScene->GetRegistry().GetView<TagComponent>()) {
        (void)e; (void)tag;
        finalEntityCount++;
    }
    ASTRAL_TEST_ASSERT(finalEntityCount == initialEntityCount,
                       "Authoring sahnesindeki toplam varlik sayisi degismemeli");
}

// ============================================================================
// 2. Input Odak Korumasi (UI Kullanirken Oyun Girdisi Sizmamasi)
// ============================================================================
void TestInputFocusIsolation() {
    auto scene = Sandbox::PuzzleGameSubsystem::CreatePuzzleScene();
    Sandbox::PuzzleGameSubsystem subsystem;
    subsystem.OnInit();

    InputSystem input;
    ActionMap actions;
    EventBus events;
    JobSystem jobs;
    FrameContext ctx{
        .registry = scene->GetRegistry(),
        .input = input,
        .actions = actions,
        .events = events,
        .jobSystem = jobs,
        .window = nullptr,
        .deltaTime = 0.016f
    };

    // 'W' tusuna basilsin
    input.SimulateKey(GLFW_KEY_W, GLFW_PRESS);
    input.BeginFrame();

    // Durum A: UI Odakli / Input Bloke (Orn: ImGui penceresine tiklaniyor veya text giriliyor)
    subsystem.SetInputBlocked(true);
    glm::vec3 posBefore = subsystem.GetPlayerPosition();

    subsystem.OnUpdate(ctx);
    glm::vec3 posAfterBlocked = subsystem.GetPlayerPosition();

    // Karakter ileriye (Z ekseninde) hareket ETMEMELIDIR
    ASTRAL_TEST_ASSERT(std::abs(posAfterBlocked.z - posBefore.z) < 0.001f,
                       "UI input capture devredeyken oyuncu WASD girdisine yanit vermemeli");

    // 'E' tusu (kapi oyma) denenir
    input.SimulateKey(GLFW_KEY_E, GLFW_PRESS);
    input.BeginFrame();
    subsystem.OnUpdate(ctx);
    ASTRAL_TEST_ASSERT(!subsystem.IsDoorCarved(),
                       "UI input capture devredeyken 'E' etkilesim tusu calismamali");

    // Durum B: Input Blokesi Kaldirilir (Kullanici oyun sahnesine odaklandi)
    subsystem.SetInputBlocked(false);
    subsystem.OnUpdate(ctx);
    ASTRAL_TEST_ASSERT(subsystem.IsDoorCarved(),
                       "Oyun sahnesi odakliyken 'E' tusu kapi oymayi basariyla tetiklemeli");

    // 'W' tusu ile hareket dogrulamasi
    input.SimulateKey(GLFW_KEY_W, GLFW_PRESS);
    input.BeginFrame();
    float zBeforeMove = subsystem.GetPlayerPosition().z;
    subsystem.OnUpdate(ctx);
    float zAfterMove = subsystem.GetPlayerPosition().z;
    ASTRAL_TEST_ASSERT(zAfterMove < zBeforeMove,
                       "Oyun sahnesi odakliyken 'W' tusu oyuncuyu ileri hareket ettirmeli");
}

// ============================================================================
// 3. Analitik CPU SDF Mesafe ve Carpisma Cozumu
// ============================================================================
void TestCpuSdfDistanceAndCollision() {
    Scene scene("CollisionScene");

    // Zemin kutusu: y = -0.5, yukseklik = 1.0 (ust yuzey y = 0.0)
    Entity floor = scene.CreateEntity("Floor");
    floor.AddComponent<TransformComponent>(glm::vec3(0.0f, -0.5f, 0.0f), glm::quat(1,0,0,0), glm::vec3(10.0f, 0.5f, 10.0f));
    auto& floorSdf = floor.AddComponent<SDFComponent>();
    floorSdf.primitiveType = 1; // Box
    floorSdf.operation = 0;     // Union
    floorSdf.isVisible = 1;

    // 1. Analitik mesafe testi
    glm::vec3 testPoint(0.0f, 2.0f, 0.0f);
    float dist = SDFWorldQuery::QueryDistance(scene, testPoint);
    // Nokta (0, 2, 0)'da, zemin y=0'da -> mesafe 2.0 olmali
    ASTRAL_TEST_ASSERT(std::abs(dist - 2.0f) < 0.02f, "Zemine olan analitik mesafe 2.0f civarinda olmali");

    // 2. Normal gradyan testi
    glm::vec3 normal = SDFWorldQuery::QueryNormal(scene, glm::vec3(0.0f, 0.1f, 0.0f));
    ASTRAL_TEST_ASSERT(normal.y > 0.98f, "Zemin uzerindeki normal dusey yukari (0, 1, 0) olmali");

    // 3. Carpisma cozumu (ResolveSphereCollision)
    // Kure yaricap 0.5, konumu (0.0, 0.2, 0.0) -> zemine 0.3 batmis durumda
    glm::vec3 spherePos(0.0f, 0.2f, 0.0f);
    float sphereRadius = 0.5f;
    bool resolved = SDFWorldQuery::ResolveSphereCollision(scene, spherePos, sphereRadius, 0.01f);

    ASTRAL_TEST_ASSERT(resolved, "Carpisma tespit edilip cozulmeli");
    ASTRAL_TEST_ASSERT(spherePos.y >= sphereRadius,
                       "Carpisma sonrasi kure yuzeyin ustune cikarilmis olmali (y >= 0.5)");
}

// ============================================================================
// 4. CSG Dunya Modifikasyonu ve Carpisma Uyum Testi (Kapi Oyma)
// ============================================================================
void TestCsgCarveCollisionFidelity() {
    auto scene = Sandbox::PuzzleGameSubsystem::CreatePuzzleScene();
    Sandbox::PuzzleGameSubsystem subsystem;
    subsystem.OnInit();

    glm::vec3 doorwayPoint(0.0f, 1.0f, 0.0f); // Engel duvardaki gecit noktasi

    // 1. Kapi oyulmadan once: Kapi noktasinda engel duvari vardir (SDF mesafesi negatif veya kucuk)
    float distBefore = SDFWorldQuery::QueryDistance(*scene, doorwayPoint);
    ASTRAL_TEST_ASSERT(distBefore <= 0.01f, "Kapi oyulmadan once gecit noktasinda kati duvar olmali");

    // Bir kure gecit noktasinda test edildiginde carpisma gerceklesmeli
    glm::vec3 testSphere = doorwayPoint;
    bool blockedBefore = SDFWorldQuery::ResolveSphereCollision(*scene, testSphere, 0.3f);
    ASTRAL_TEST_ASSERT(blockedBefore, "Kapi oyulmadan once kure duvara carpmali");

    // 2. CSG Kapi Oymasi Gerceklestirilir (Subtract Cutter aktiflesir)
    subsystem.CarveDoorway(*scene);
    ASTRAL_TEST_ASSERT(subsystem.IsDoorCarved(), "Kapi oyulmus olmali");

    // 3. Kapi oyulduktan sonra: Gecit noktasindaki SDF mesafesi bosluga (pozitif) donusmeli
    float distAfter = SDFWorldQuery::QueryDistance(*scene, doorwayPoint);
    ASTRAL_TEST_ASSERT(distAfter > 0.1f,
                       "Kapi oyulduktan sonra gecit noktasinda artik bosluk bulunmali (CSG Subtraction)");

    // Kure artik kapi boslugundan rahatlikla gecmeli, duvara batma duzeltmesi yapilmamali
    glm::vec3 openSphere = doorwayPoint;
    bool blockedAfter = SDFWorldQuery::ResolveSphereCollision(*scene, openSphere, 0.15f);
    ASTRAL_TEST_ASSERT(!blockedAfter, "Kapi oyulduktan sonra kure gecit merkezinde serbestce bulunabilmeli");
}

// ============================================================================
// 5. Undo/Redo Komut Yigini Kapsami (Rename, Visibility, Reparent)
// ============================================================================
void TestUndoRedoCommandStack() {
    Scene scene("UndoRedoScene");
    CommandStack cmdStack;

    Entity boxA = scene.CreateEntity("BoxAlpha");
    boxA.AddComponent<TransformComponent>(glm::vec3(0.0f));
    boxA.AddComponent<SDFComponent>();
    boxA.AddComponent<VisibilityComponent>(true);

    Entity boxB = scene.CreateEntity("BoxBeta");
    boxB.AddComponent<TransformComponent>(glm::vec3(2.0f, 0.0f, 0.0f));

    // 1. RenameEntityCommand
    {
        auto renameCmd = std::make_unique<RenameEntityCommand>(boxA, "BoxAlpha", "RenamedAlpha");
        cmdStack.PushAndExecute(std::move(renameCmd));
        ASTRAL_TEST_ASSERT(boxA.GetComponent<TagComponent>().tag == "RenamedAlpha", "Yeniden adlandirma basarili olmali");

        cmdStack.Undo();
        ASTRAL_TEST_ASSERT(boxA.GetComponent<TagComponent>().tag == "BoxAlpha", "Undo ile eski isim geri yuklenmeli");

        cmdStack.Redo();
        ASTRAL_TEST_ASSERT(boxA.GetComponent<TagComponent>().tag == "RenamedAlpha", "Redo ile yeni isim tekrar atanmali");
    }

    // 2. SetVisibilityCommand
    {
        auto visCmd = std::make_unique<SetVisibilityCommand>(boxA, true, false);
        cmdStack.PushAndExecute(std::move(visCmd));
        ASTRAL_TEST_ASSERT(!boxA.GetComponent<VisibilityComponent>().isVisible, "Gorunurluk false yapilmali");
        ASTRAL_TEST_ASSERT(boxA.GetComponent<SDFComponent>().isVisible == 0, "SDFComponent isVisible 0 olmali");

        cmdStack.Undo();
        ASTRAL_TEST_ASSERT(boxA.GetComponent<VisibilityComponent>().isVisible, "Undo sonrasi gorunurluk true olmali");
        ASTRAL_TEST_ASSERT(boxA.GetComponent<SDFComponent>().isVisible == 1, "Undo sonrasi SDF isVisible 1 olmali");

        cmdStack.Redo();
        ASTRAL_TEST_ASSERT(!boxA.GetComponent<VisibilityComponent>().isVisible, "Redo sonrasi gorunurluk tekrar false olmali");
    }

    // 3. ReparentEntityCommand
    {
        auto reparentCmd = std::make_unique<ReparentEntityCommand>(scene, boxB.GetHandle(), NullEntityHandle, boxA.GetHandle());
        cmdStack.PushAndExecute(std::move(reparentCmd));

        ASTRAL_TEST_ASSERT(scene.GetParent(boxB.GetHandle()) == boxA.GetHandle(), "boxB'nin ebeveyni boxA olmali");
        auto children = scene.GetChildren(boxA.GetHandle());
        ASTRAL_TEST_ASSERT(std::find(children.begin(), children.end(), boxB.GetHandle()) != children.end(),
                           "boxA'nin cocuk listesinde boxB bulunmali");

        cmdStack.Undo();
        ASTRAL_TEST_ASSERT(scene.GetParent(boxB.GetHandle()) == NullEntityHandle, "Undo ile boxB koksizlesmeli");

        cmdStack.Redo();
        ASTRAL_TEST_ASSERT(scene.GetParent(boxB.GetHandle()) == boxA.GetHandle(), "Redo ile ebeveyn baglantisi yeniden kurulmali");
    }
}

// ============================================================================
// 6. Kaydet -> Kapat -> Yeniden Ac Bütünlük Testi
// ============================================================================
void TestSaveCloseReopenWorldStateFidelity() {
    const std::filesystem::path testDir = std::filesystem::temp_directory_path() / "astral_test_gameplay";
    std::filesystem::create_directories(testDir);
    const std::filesystem::path testFilePath = testDir / "test_puzzle_world.astral";
    if (std::filesystem::exists(testFilePath)) {
        std::filesystem::remove(testFilePath);
    }

    // Orijinal bulmaca sahnesini olustur
    auto originalScene = Sandbox::PuzzleGameSubsystem::CreatePuzzleScene();

    // Sahneyi diske kaydet
    bool saved = SceneSerializer::Serialize(originalScene, testFilePath);
    ASTRAL_TEST_ASSERT(saved, "Sahne basariyla diske serilestirilmelidir");
    ASTRAL_TEST_ASSERT(std::filesystem::exists(testFilePath), "Serilestirilen dosya diskte bulunmalidir");

    // Orijinal sahne kapatilir / bellekten duser
    originalScene.reset();

    // Sifirdan yeni bos sahne olusturup yukle
    auto reloadedScene = std::make_shared<Scene>("ReloadedWorld");
    bool loaded = SceneSerializer::Deserialize(reloadedScene, testFilePath);
    ASTRAL_TEST_ASSERT(loaded, "Sahne diskten basariyla okunmalidir");

    // Tum temel varliklarin eksiksiz ve ayni bilesenlerle yuklendigini dogrula
    Entity floor = reloadedScene->FindEntityByTag("Floor");
    Entity wall = reloadedScene->FindEntityByTag("ObstacleWall");
    Entity cutter = reloadedScene->FindEntityByTag("CutterDoorway");
    Entity goal = reloadedScene->FindEntityByTag("GoalOrb");
    Entity player = reloadedScene->FindEntityByTag("Player");
    Entity camera = reloadedScene->FindEntityByTag("MainCamera");

    ASTRAL_TEST_ASSERT(floor.IsValid(), "Floor yeniden yuklenmis olmali");
    ASTRAL_TEST_ASSERT(wall.IsValid(), "ObstacleWall yeniden yuklenmis olmali");
    ASTRAL_TEST_ASSERT(cutter.IsValid(), "CutterDoorway yeniden yuklenmis olmali");
    ASTRAL_TEST_ASSERT(goal.IsValid(), "GoalOrb yeniden yuklenmis olmali");
    ASTRAL_TEST_ASSERT(player.IsValid(), "Player yeniden yuklenmis olmali");
    ASTRAL_TEST_ASSERT(camera.IsValid(), "MainCamera yeniden yuklenmis olmali");

    // Bilesen degerlerini karsilastir
    ASTRAL_TEST_ASSERT(cutter.GetComponent<SDFComponent>().operation == 1, "Cutter operation Subtract (1) olmali");
    ASTRAL_TEST_ASSERT(cutter.GetComponent<SDFComponent>().isVisible == 0, "Cutter isVisible 0 olmali");
    ASTRAL_TEST_ASSERT(camera.GetComponent<CameraComponent>().primary == 1, "Camera primary ozelligi korunmali");
    ASTRAL_TEST_ASSERT(std::abs(player.GetComponent<TransformComponent>().position.z - 3.5f) < 0.001f,
                       "Player konumu dogru yuklenmeli");

    // Gecici dosyayi temizle
    std::error_code ec;
    std::filesystem::remove_all(testDir, ec);
}

// ============================================================================
// Test Paketi Giris Noktasi
// ============================================================================
void RunEditorGameplayCycleTests() {
    std::cout << "\n--- [A3] Editor Gameplay Cycle & Reference Playable Suite ---\n";
    TestAuthoringPlayStopCycle();
    TestInputFocusIsolation();
    TestCpuSdfDistanceAndCollision();
    TestCsgCarveCollisionFidelity();
    TestUndoRedoCommandStack();
    TestSaveCloseReopenWorldStateFidelity();
    std::cout << "--- [A3] Editor Gameplay Cycle Testleri Basariyla Tamamlandi! ---\n\n";
}

} // namespace Astral::Test
